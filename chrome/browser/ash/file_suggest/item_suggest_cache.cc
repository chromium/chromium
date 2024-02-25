// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/item_suggest_cache.h"

#include <algorithm>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/drive/drive_pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash {
namespace {

// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("launcher_item_suggest", R"(
      semantics {
        sender: "Launcher suggested drive files"
        description:
          "The Chrome OS launcher requests suggestions for Drive files from "
          "the Drive ItemSuggest API. These are displayed in the launcher."
        trigger:
          "Once on login after Drive FS is mounted. Afterwards, whenever the "
          "Chrome OS launcher is opened."
        data:
          "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "This cannot be disabled except by policy."
        chrome_policy {
          DriveDisabled {
            DriveDisabled: true
          }
        }
      })");

bool IsDisabledByPolicy(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive);
}

base::Time GetLastRequestTime(Profile* profile) {
  return profile->GetPrefs()->GetTime(
      ash::prefs::kLauncherLastContinueRequestTime);
}

void SetLastRequestTime(Profile* profile, const base::Time& time) {
  profile->GetPrefs()->SetTime(ash::prefs::kLauncherLastContinueRequestTime,
                               time);
}

//------------------
// Metrics utilities
//------------------

void LogStatus(ItemSuggestCache::Status status) {
  base::UmaHistogramEnumeration("Apps.AppList.ItemSuggestCache.Status", status);
}

void LogResponseSize(const int size) {
  base::UmaHistogramCounts100000("Apps.AppList.ItemSuggestCache.ResponseSize",
                                 size);
}

void LogLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Apps.AppList.ItemSuggestCache.UpdateCacheLatency",
                          latency);
}

//----------------------
// JSON response parsing
//----------------------

std::optional<std::string> GetString(const base::Value::Dict& value,
                                     const std::string& key) {
  const auto* field = value.FindString(key);
  if (!field) {
    return std::nullopt;
  }
  return *field;
}

std::optional<ItemSuggestCache::Result> ConvertResult(
    const base::Value* value) {
  if (!value->is_dict()) {
    return std::nullopt;
  }
  const auto& value_dict = value->GetDict();

  // Get the item ID and display name.
  const auto& item_id = GetString(value_dict, "itemId");
  const auto& display_text = GetString(value_dict, "displayText");
  if (!item_id || !display_text) {
    return std::nullopt;
  }

  ItemSuggestCache::Result result(item_id.value(), display_text.value(),
                                  /*prediction_reason=*/std::nullopt);

  // Get the justification string. We allow this to be empty, so return the
  // previously-created `result` on failure.
  const auto* justification_dict = value_dict.FindDict("justification");
  if (!justification_dict) {
    return result;
  }

  // We use `unstructuredJustificationDescription` because justifications are
  // displayed on one line, and `justificationDescription` is intended for
  // multi-line formatting.
  const auto* description =
      justification_dict->FindDict("unstructuredJustificationDescription");
  if (!description) {
    return result;
  }

  // `unstructuredJustificationDescription` contains only one text segment by
  // convention.
  const auto* text_segments = description->FindList("textSegment");
  if (!text_segments || text_segments->empty() ||
      !(*text_segments)[0].is_dict()) {
    return result;
  }
  const auto& text_segment = (*text_segments)[0].GetDict();

  const auto justification = GetString(text_segment, "text");
  if (!justification) {
    return result;
  }

  result.prediction_reason = justification;
  return result;
}

std::optional<ItemSuggestCache::Results> ConvertResults(
    const base::Value* value) {
  if (!value->is_dict()) {
    return std::nullopt;
  }
  const auto& value_dict = value->GetDict();

  const auto suggestion_id = GetString(value_dict, "suggestionSessionId");
  if (!suggestion_id) {
    return std::nullopt;
  }

  ItemSuggestCache::Results results(suggestion_id.value());

  const auto* items = value_dict.FindList("item");
  if (!items) {
    // Return empty results if there are no items.
    return results;
  }

  for (const auto& result_value : *items) {
    auto result = ConvertResult(&result_value);
    // If any result fails conversion, fail completely and return std::nullopt,
    // rather than just skipping this result. This makes clear the distinction
    // between a response format issue and the response containing no results.
    if (!result) {
      return std::nullopt;
    }
    results.results.push_back(std::move(result.value()));
  }

  return results;
}

}  // namespace

BASE_FEATURE(kLauncherItemSuggest,
             "LauncherItemSuggest",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> ItemSuggestCache::kEnabled;
constexpr base::FeatureParam<std::string> ItemSuggestCache::kServerUrl;
constexpr base::FeatureParam<std::string> ItemSuggestCache::kModelName;
constexpr base::FeatureParam<bool> ItemSuggestCache::kMultipleQueriesPerSession;
constexpr base::FeatureParam<int> ItemSuggestCache::kLongDelayMinutes;

ItemSuggestCache::Result::Result(
    const std::string& id,
    const std::string& title,
    const std::optional<std::string>& prediction_reason)
    : id(id), title(title), prediction_reason(prediction_reason) {}

ItemSuggestCache::Result::Result(const Result& other)
    : id(other.id),
      title(other.title),
      prediction_reason(other.prediction_reason) {}

ItemSuggestCache::Result::~Result() = default;

ItemSuggestCache::Results::Results(const std::string& suggestion_id)
    : suggestion_id(suggestion_id) {}

ItemSuggestCache::Results::Results(const Results& other)
    : suggestion_id(other.suggestion_id), results(other.results) {}

ItemSuggestCache::Results::~Results() = default;

ItemSuggestCache::ItemSuggestCache(
    const std::string& locale,
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : made_request_(false),
      enabled_(kEnabled.Get()),
      server_url_(kServerUrl.Get()),
      multiple_queries_per_session_(kMultipleQueriesPerSession.Get()),
      locale_(locale),
      profile_(profile),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ItemSuggestCache::~ItemSuggestCache() = default;

base::CallbackListSubscription ItemSuggestCache::RegisterCallback(
    ItemSuggestCache::OnResultsCallback callback) {
  return on_results_callback_list_.Add(std::move(callback));
}

std::optional<ItemSuggestCache::Results> ItemSuggestCache::GetResults() {
  // Return a copy because a pointer to |results_| will become invalid whenever
  // the cache is updated.
  return results_;
}

std::string ItemSuggestCache::GetRequestBody() {
  // We request that ItemSuggest serve our request via particular model by
  // specifying the model name in client_tags. This is a non-standard part of
  // the API, implemented so we can experiment with model backends. The
  // client_tags can be set via Finch based on what is expected by the
  // ItemSuggest backend, and unexpected tags will be assigned a default model.
  static constexpr char kRequestBody[] = R"({
        "client_info": {
          "platform_type": "CHROME_OS",
          "scenario_type": "CHROME_OS_ZSS_FILES",
          "language_code": "$1",
          "request_type": "BACKGROUND_REQUEST",
          "client_tags": {
            "name": "$2"
          }
        },
        "max_suggestions": 10,
        "type_detail_fields": "drive_item.title,justification.display_text"
      })";

  const std::string& model = kModelName.Get();
  return base::ReplaceStringPlaceholders(kRequestBody, {locale_, model},
                                         nullptr);
}

base::TimeDelta ItemSuggestCache::GetDelay() {
  bool use_long_delay = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kLauncherUseLongContinueDelay);
  return base::Minutes(use_long_delay ? kLongDelayMinutes.Get()
                                      : kShortDelayMinutes);
}

void ItemSuggestCache::MaybeUpdateCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_start_time_ = base::TimeTicks::Now();

  if (base::Time::Now() - GetLastRequestTime(profile_) < GetDelay()) {
    return;
  }

  // Make no requests and exit in these cases:
  // - Item suggest has been disabled via experiment.
  // - Item suggest has been disabled by policy.
  // - The server url is not https or not trusted by Google.
  // - We've already made a request this session and we are not configured to
  //   query multiple times.
  if (!enabled_) {
    LogStatus(Status::kDisabledByExperiment);
    return;
  } else if (IsDisabledByPolicy(profile_)) {
    LogStatus(Status::kDisabledByPolicy);
    return;
  } else if (!server_url_.SchemeIs(url::kHttpsScheme) ||
             !google_util::IsGoogleAssociatedDomainUrl(server_url_)) {
    LogStatus(Status::kInvalidServerUrl);
    return;
  } else if (made_request_ && !multiple_queries_per_session_) {
    LogStatus(Status::kPostLaunchUpdateIgnored);
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    LogStatus(Status::kNoIdentityManager);
    return;
  }

  // Fetch an OAuth2 access token.
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "launcher_item_suggest", identity_manager,
      signin::ScopeSet({GaiaConstants::kDriveReadOnlyOAuth2Scope}),
      base::BindOnce(&ItemSuggestCache::OnTokenReceived,
                     weak_factory_.GetWeakPtr()),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void ItemSuggestCache::UpdateCacheWithJsonForTest(
    const std::string json_response) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      json_response, base::BindOnce(&ItemSuggestCache::OnJsonParsed,
                                    weak_factory_.GetWeakPtr()));
}

void ItemSuggestCache::OnTokenReceived(GoogleServiceAuthError error,
                                       signin::AccessTokenInfo token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LogStatus(Status::kGoogleAuthError);
    return;
  }

  // Make a new request. This destroys any existing |url_loader_| which will
  // cancel that request if it is in-progress.
  SetLastRequestTime(profile_, base::Time::Now());
  made_request_ = true;
  url_loader_ = MakeRequestLoader(token_info.token);
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  url_loader_->AttachStringForUpload(GetRequestBody(), "application/json");

  // Perform the request.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ItemSuggestCache::OnSuggestionsReceived,
                     weak_factory_.GetWeakPtr()),
      kMaxResponseSize);
}

void ItemSuggestCache::OnSuggestionsReceived(
    const std::unique_ptr<std::string> json_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = url_loader_->NetError();
  if (net_error != net::OK) {
    if (!url_loader_->ResponseInfo() || !url_loader_->ResponseInfo()->headers) {
      if (net_error == net::ERR_INSUFFICIENT_RESOURCES) {
        LogStatus(Status::kResponseTooLarge);
      } else {
        // Note that requests ending in kNetError don't count towards
        // ItemSuggest QPS, but the last request time is still updated.
        LogStatus(Status::kNetError);
      }
    } else {
      const int status = url_loader_->ResponseInfo()->headers->response_code();
      if (status >= 500) {
        LogStatus(Status::k5xxStatus);
      } else if (status >= 400) {
        LogStatus(Status::k4xxStatus);
      } else if (status >= 300) {
        LogStatus(Status::k3xxStatus);
      }
    }

    return;
  } else if (!json_response || json_response->empty()) {
    LogStatus(Status::kEmptyResponse);
    return;
  }

  LogResponseSize(json_response->size());

  // Parse the JSON response from ItemSuggest.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *json_response, base::BindOnce(&ItemSuggestCache::OnJsonParsed,
                                     weak_factory_.GetWeakPtr()));
}

void ItemSuggestCache::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    LogStatus(Status::kJsonParseFailure);
    return;
  }

  // Convert the JSON value into a Results object. If the conversion fails, or
  // if the conversion contains no results, we shouldn't update the stored
  // results.
  const auto& results = ConvertResults(&*result);
  if (!results) {
    LogStatus(Status::kJsonConversionFailure);
  } else if (results->results.empty()) {
    LogStatus(Status::kNoResultsInResponse);
    if (!results_) {
      // Make sure that |results_| is non-null to indicate that an update was
      // successful.
      results_ = std::move(results.value());
    }
  } else {
    LogStatus(Status::kOk);
    LogLatency(base::TimeTicks::Now() - update_start_time_);
    results_ = std::move(results.value());
    on_results_callback_list_.Notify();
  }
}

std::unique_ptr<network::SimpleURLLoader> ItemSuggestCache::MakeRequestLoader(
    const std::string& token) {
  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->method = "POST";
  resource_request->url = server_url_;
  // Do not allow cookies.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Ignore the cache because we always want fresh results.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  DCHECK(resource_request->url.is_valid());

  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token);

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kTrafficAnnotation);
}

// static
std::optional<ItemSuggestCache::Results> ItemSuggestCache::ConvertJsonForTest(
    const base::Value* value) {
  return ConvertResults(value);
}

}  // namespace ash
