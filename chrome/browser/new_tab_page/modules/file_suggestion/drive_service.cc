// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"

namespace {
#if BUILDFLAG(IS_LINUX)
constexpr char kPlatform[] = "LINUX";
#elif BUILDFLAG(IS_WIN)
constexpr char kPlatform[] = "WINDOWS";
#elif BUILDFLAG(IS_MAC)
constexpr char kPlatform[] = "MAC_OS";
#elif BUILDFLAG(IS_CHROMEOS)
constexpr char kPlatform[] = "CHROME_OS";
#else
constexpr char kPlatform[] = "UNSPECIFIED_PLATFORM";
#endif
// TODO(crbug.com/40749413): Add language code to request.
constexpr char kRequestBody[] = R"({
  "client_info": {
    "platform_type": "%s",
    "scenario_type": "CHROME_NTP_FILES",
    "language_code": "%s",
    "request_type": "LIVE_REQUEST",
    "client_tags": {
      "name": "%s"
    }
  },
  "max_suggestions": %d,
  "type_detail_fields": "drive_item.title,drive_item.mimeType"
})";
// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;
const char server_url[] = "https://appsitemsuggest-pa.googleapis.com/v1/items";
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("drive_service", R"(
      semantics {
        sender: "Drive Service"
        description:
          "The Drive Service requests suggestions for Drive files from "
          "the Drive ItemSuggest API. The response will be displayed in NTP's "
          "Drive Module."
        trigger:
          "Each time a user navigates to the NTP while "
          "the Drive module is enabled and the user is "
          "signed in."
        data:
          "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can control this feature by (1) selecting "
          "a non-Google default search engine in Chrome "
          "settings under 'Search Engine', (2) signing out, "
          "or (3) disabling the Drive module."
        chrome_policy {
          DefaultSearchProviderEnabled {
            policy_options {mode: MANDATORY}
            DefaultSearchProviderEnabled: false
          }
          BrowserSignin {
            policy_options {mode: MANDATORY}
            BrowserSignin: 0
          }
          NTPCardsVisible {
            NTPCardsVisible: false
          }
        }
      })");
constexpr char kFakeDataWithThreeFiles[] = R"({
  "item": [
    {
      "itemId": "foo",
      "url": "https://docs.google.com",
      "driveItem": {
        "title": "Drive Module Design Doc",
        "mimeType": "application/vnd.google-apps.document"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened yesterday"}]
        }
      }
    },
    {
      "itemId": "bar",
      "url": "https://sheets.google.com",
      "driveItem": {
        "title": "Monthly Presentation Schedule",
        "mimeType": "application/vnd.google-apps.spreadsheet"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened today"}]
        }
      }
    },
    {
      "itemId": "baz",
      "url": "https://slides.google.com",
      "driveItem": {
        "title": "File With A Really Really Really Really Really Long Name",
        "mimeType": "application/vnd.google-apps.presentation"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened on Monday"}]
        }
      }
    }
  ]
}
)";
constexpr char kFakeDataWithSixFiles[] = R"({
  "item": [
    {
      "itemId": "foo",
      "url": "https://docs.google.com",
      "driveItem": {
        "title": "Drive Module Design Doc",
        "mimeType": "application/vnd.google-apps.document"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened yesterday"}]
        }
      }
    },
    {
      "itemId": "bar",
      "url": "https://sheets.google.com",
      "driveItem": {
        "title": "Monthly Presentation Schedule",
        "mimeType": "application/vnd.google-apps.spreadsheet"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened today"}]
        }
      }
    },
    {
      "itemId": "baz",
      "url": "https://slides.google.com",
      "driveItem": {
        "title": "File With A Really Really Really Really Really Long Name",
        "mimeType": "application/vnd.google-apps.presentation"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened on Monday"}]
        }
      }
    },
    {
      "itemId": "qux",
      "url": "https://slides.google.com",
      "driveItem": {
        "title": "Cutest Kittens on the Web",
        "mimeType": "application/vnd.google-apps.presentation"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened on Monday"}]
        }
      }
    },
    {
      "itemId": "foobar",
      "url": "https://docs.google.com",
      "driveItem": {
        "title": "Budgeting Notes",
        "mimeType": "application/vnd.google-apps.document"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened yesterday"}]
        }
      }
    },
    {
      "itemId": "bazqux",
      "url": "https://sheets.google.com",
      "driveItem": {
        "title": "1",
        "mimeType": "application/vnd.google-apps.spreadsheet"
      },
      "justification": {
        "unstructuredJustificationDescription": {
          "textSegment": [{"text": "You opened today"}]
        }
      }
    }
  ]
})";
}  // namespace

// static
const char DriveService::kLastDismissedTimePrefName[] =
    "NewTabPage.Drive.LastDimissedTime";

// static
const base::TimeDelta DriveService::kDismissDuration = base::Days(14);

DriveService::~DriveService() = default;

DriveService::DriveService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    const std::string& application_locale,
    PrefService* pref_service)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      segmentation_platform_service_(segmentation_platform_service),
      application_locale_(application_locale),
      pref_service_(pref_service) {}

// static
void DriveService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastDismissedTimePrefName, base::Time());
}

void DriveService::GetDriveFiles(GetFilesCallback get_files_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.push_back(std::move(get_files_callback));
  if (callbacks_.size() > 1) {
    return;
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpDriveModuleSegmentation)) {
    GetDriveModuleSegmentationData();
  } else {
    GetDriveFilesInternal();
  }
}

bool DriveService::GetDriveModuleSegmentationData() {
  segmentation_platform::PredictionOptions options;
  options.on_demand_execution = true;
  segmentation_platform_service_->GetClassificationResult(
      segmentation_platform::kDesktopNtpModuleKey, options, nullptr,
      base::IgnoreArgs<const segmentation_platform::ClassificationResult&>(
          base::BindOnce(&DriveService::GetDriveFilesInternal,
                         base::Unretained(this))));
  return true;
}

void DriveService::GetDriveFilesInternal() {
  const base::Time last_dismissed_time =
      pref_service_->GetTime(kLastDismissedTimePrefName);
  // Bail if module is still dismissed.
  if (!last_dismissed_time.is_null()) {
    base::TimeDelta elapsed_time = base::Time::Now() - last_dismissed_time;
    if (elapsed_time < kDismissDuration) {
      const std::string remaining_hours =
          base::NumberToString((kDismissDuration - elapsed_time).InHours());
      LogModuleDismissed(ntp_features::kNtpDriveModule, true, remaining_hours);

      for (auto& callback : callbacks_) {
        std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
      }
      callbacks_.clear();
      return;
    }
  }

  LogModuleDismissed(ntp_features::kNtpDriveModule, false,
                     /*remaining_hours=*/"0");

  // Skip fetch and jump straight to data parsing when serving fake data.
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpDriveModule,
          ntp_features::kNtpDriveModuleDataParam) == "fake") {
    base::FeatureList::IsEnabled(ntp_features::kNtpDriveModuleShowSixFiles)
        ? data_decoder::DataDecoder::ParseJsonIsolated(
              kFakeDataWithSixFiles, base::BindOnce(&DriveService::OnJsonParsed,
                                                    weak_factory_.GetWeakPtr()))
        : data_decoder::DataDecoder::ParseJsonIsolated(
              kFakeDataWithThreeFiles,
              base::BindOnce(&DriveService::OnJsonParsed,
                             weak_factory_.GetWeakPtr()));
    return;
  }

  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "ntp_drive_module", identity_manager_,
      signin::ScopeSet({GaiaConstants::kDriveReadOnlyOAuth2Scope}),
      base::BindOnce(&DriveService::OnTokenReceived,
                     weak_factory_.GetWeakPtr()),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void DriveService::DismissModule() {
  pref_service_->SetTime(kLastDismissedTimePrefName, base::Time::Now());
}

void DriveService::RestoreModule() {
  pref_service_->SetTime(kLastDismissedTimePrefName, base::Time());
}

void DriveService::OnTokenReceived(GoogleServiceAuthError error,
                                   signin::AccessTokenInfo token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    LogModuleError(ntp_features::kNtpDriveModule, error.error_message());
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    }
    callbacks_.clear();
    return;
  }

  // Skip fetch if data is cached and not expired.
  // TODO(crbug.com/40773636): Leverage the standard HTTP cache once ItemSuggest
  // supports GET requests.
  if (cached_json_ && cached_json_token_ == token_info.token &&
      /* We use std::max to guard against negative cache ages. This can happen,
         for instance, when modifying the local clock. */
      std::max((base::Time::Now() - cached_json_time_).InSeconds(),
               INT64_C(0)) <
          base::GetFieldTrialParamByFeatureAsInt(
              ntp_features::kNtpDriveModule,
              ntp_features::kNtpDriveModuleCacheMaxAgeSParam, 0)) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        *cached_json_, base::BindOnce(&DriveService::OnJsonParsed,
                                      weak_factory_.GetWeakPtr()));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = GURL(server_url);
  // Cookies should not be allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Ignore cache for fresh results.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token_info.token);

  DCHECK(!url_loader_);
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  const int kNumFilesRequested =
      base::FeatureList::IsEnabled(ntp_features::kNtpDriveModuleShowSixFiles)
          ? 6
          : 3;
  url_loader_->AttachStringForUpload(
      base::StringPrintf(kRequestBody, kPlatform, application_locale_.c_str(),
                         base::GetFieldTrialParamValueByFeature(
                             ntp_features::kNtpDriveModule,
                             ntp_features::kNtpDriveModuleExperimentGroupParam)
                             .c_str(),
                         kNumFilesRequested),
      "application/json");
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DriveService::OnJsonReceived, weak_factory_.GetWeakPtr(),
                     token_info.token),
      kMaxResponseSize);
  base::UmaHistogramSparse("NewTabPage.Modules.DataRequest",
                           base::PersistentHash("drive"));
}

void DriveService::OnJsonReceived(const std::string& token,
                                  std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = url_loader_->NetError();
  url_loader_.reset();

  if (net_error == net::OK && response_body) {
    cached_json_ = std::move(response_body);
    cached_json_time_ = base::Time::Now();
    cached_json_token_ = token;
    data_decoder::DataDecoder::ParseJsonIsolated(
        *cached_json_, base::BindOnce(&DriveService::OnJsonParsed,
                                      weak_factory_.GetWeakPtr()));
    return;
  }

  if (net_error != net::OK) {
    LogModuleError(
        ntp_features::kNtpDriveModule,
        base::StrCat({"net error ", base::NumberToString(net_error)}));
  } else if (!response_body) {
    LogModuleError(ntp_features::kNtpDriveModule, "no JSON response body");
  }
    base::UmaHistogramEnumeration("NewTabPage.Drive.ItemSuggestRequestResult",
                                  ItemSuggestRequestResult::kNetworkError);
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    }
    callbacks_.clear();
}

void DriveService::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    LogModuleError(ntp_features::kNtpDriveModule, "JSON parse error");
    base::UmaHistogramEnumeration("NewTabPage.Drive.ItemSuggestRequestResult",
                                  ItemSuggestRequestResult::kJsonParseError);
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    }
    callbacks_.clear();
    return;
  }
  auto* items = result->GetDict().FindList("item");
  if (!items) {
    LogModuleError(ntp_features::kNtpDriveModule, "no items in JSON");
    base::UmaHistogramEnumeration("NewTabPage.Drive.ItemSuggestRequestResult",
                                  ItemSuggestRequestResult::kContentError);
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    }
    callbacks_.clear();
    return;
  }
  ItemSuggestRequestResult request_result = ItemSuggestRequestResult::kSuccess;
  std::vector<file_suggestion::mojom::FilePtr> document_list;
  for (const auto& item : *items) {
    const auto& item_dict = item.GetDict();
    auto* title = item_dict.FindStringByDottedPath("driveItem.title");
    auto* mime_type = item_dict.FindStringByDottedPath("driveItem.mimeType");
    auto* justification_text_segments = item_dict.FindListByDottedPath(
        "justification.unstructuredJustificationDescription.textSegment");
    if (!justification_text_segments ||
        justification_text_segments->size() == 0) {
      request_result = ItemSuggestRequestResult::kContentError;
      continue;
    }
    std::string justification_text;
    for (auto& text_segment : *justification_text_segments) {
      auto* justification_text_path = text_segment.GetDict().FindString("text");
      if (!justification_text_path) {
        request_result = ItemSuggestRequestResult::kContentError;
        continue;
      }
      justification_text += *justification_text_path;
    }
    auto* id = item_dict.FindString("itemId");
    auto* item_url = item_dict.FindString("url");
    if (!title || !mime_type || justification_text.empty() || !id ||
        !item_url || !GURL(*item_url).is_valid()) {
      request_result = ItemSuggestRequestResult::kContentError;
      continue;
    }
    auto mojo_drive_doc = file_suggestion::mojom::File::New();
    mojo_drive_doc->title = *title;
    mojo_drive_doc->mime_type = *mime_type;
    mojo_drive_doc->justification_text = justification_text;
    mojo_drive_doc->id = *id;
    mojo_drive_doc->item_url = GURL(*item_url);
    document_list.push_back(std::move(mojo_drive_doc));
  }
  base::UmaHistogramEnumeration("NewTabPage.Drive.ItemSuggestRequestResult",
                                request_result);
  base::UmaHistogramCounts100("NewTabPage.Drive.FileCount",
                              document_list.size());
  for (auto& callback : callbacks_) {
    std::move(callback).Run(mojo::Clone(document_list));
  }
  callbacks_.clear();
}
