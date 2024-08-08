// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/cws_info_service.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/queue.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/extensions/cws_item_service.pb.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr int kMaxExtensionIdsPerRequest = 3;
constexpr int kMaxRetriesPerRequest = 2;

// Default check and fetch intervals.
constexpr int kCheckIntervalSeconds = 1 * 60 * 60;
constexpr int kFetchIntervalSeconds = 24 * 60 * 60;
// Fast mode check and fetch intervals. These intervals are used to
// facilitate end-end testing.
constexpr int kFastStartupCheckDelaySeconds = 30;
constexpr int kFastCheckIntervalSeconds = 1 * 60;
constexpr int kFastFetchIntervalSeconds = 3 * 60;

constexpr char kRequestUrl[] =
    "https://chromewebstore.googleapis.com/v2/items/-/storeMetadata:batchGet";
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cws_info_service", R"(
      semantics {
        sender: "CWS Info Service"
        description:
          "Sends ids of currently installed extensions that update from the "
          "the Chrome Web Store to fetch their store metadata. The metadata "
          "includes information such as an extension's current publish status "
          "which is used to enforce the ExtensionUnpublishedAvailability "
          "policy to disable the extension. "
        trigger:
          "Periodic fetch of metadata information once every 24 hours. A fetch "
          "is also triggered at Chrome or profile startup and when the "
          "ExtensionUnpublishedAvailability policy setting changes."
        user_data {
          type: PROFILE_DATA
        }
        data:
          "Ids of the currently installed extensions that update from the "
          "Chrome Web Store."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-04-06"
        internal {
          contacts {
            email: "anunoy@chromium.org"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled in settings. It will only be "
          "triggered if the user has installed extensions from the store."
        policy_exception_justification: "Not implemented."
      })");

// CWS Info pref keys.
constexpr char kCWSInfo[] = "cws-info";
constexpr char kIsPresent[] = "is-present";
constexpr char kIsLive[] = "is-live";
constexpr char kLastUpdateTimeMillis[] = "last-updated-time-millis";
constexpr char kViolationType[] = "violation-type";
constexpr char kUnpublishedLongAgo[] = "unpublished-long-ago";
constexpr char kNoPrivacyPractice[] = "no-privacy-practice";
constexpr const char* kLabels[] = {kUnpublishedLongAgo, kNoPrivacyPractice};

// Proto conversion helpers.
// Helpers to convert extension id <-> name field in protos.
// name format: items/{itemId}/storeMetadata
std::string GetIdFromName(const std::string& name) {
  std::string id;
  base::StringTokenizer t(name, "/");
  if (t.GetNext() && t.GetNext()) {
    id = t.token();
  }
  return id;
}
std::string GetNameFromId(const std::string& id) {
  return "items/" + id + "/storeMetadata";
}

// Whether or not to skip the check if the build includes the Google Chrome API
// key. Used for testing.
bool skip_api_key_check_for_testing = false;

// Histogram helpers.
void RecordFetchSuccess(bool success) {
  base::UmaHistogramBoolean("Extensions.CWSInfoService.FetchSuccess", success);
}
void RecordMetadataChanged(bool changed) {
  base::UmaHistogramBoolean("Extensions.CWSInfoService.MetadataChanged",
                            changed);
}
void RecordNumRequestsInFetch(int num_requests) {
  base::UmaHistogramCounts100("Extensions.CWSInfoService.NumRequestsInFetch",
                              num_requests);
}
void RecordNetworkHistograms(const network::SimpleURLLoader* url_loader) {
  int net_error = url_loader->NetError();
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }
  base::UmaHistogramSparse(
      "Extensions.CWSInfoService.NetworkResponseCodeOrError",
      net_error == net::OK || net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE
          ? response_code
          : net_error);
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    base::UmaHistogramExactLinear(
        "Extensions.CWSInfoService.NetworkRetriesTillSuccess",
        url_loader->GetNumRetries(), kMaxRetriesPerRequest + 1);
  } else {
    DVLOG(1) << "Request net error:" << net_error
             << ", response code:" << response_code;
  }
}

}  // namespace

namespace extensions {

// Allow periodic retrieval of extensions metadata from the Chrome Web Store
// (CWS). This is effectively a kill-switch for the feature.
BASE_FEATURE(kCWSInfoService,
             "CWSInfoService",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Increase the frequency of periodic retrieval of extensions metadata from
// CWS. This feature is used only for testing purposes.
BASE_FEATURE(kCWSInfoFastCheck,
             "CWSInfoFastCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

base::Value::Dict GetDictFromStoreMetadataProto(const StoreMetadata* metadata) {
  base::Value::Dict dict;
  if (!metadata) {
    dict.Set(kIsPresent, false);
  } else {
    dict.Set(kIsPresent, true);
    dict.Set(kIsLive, metadata->is_live());
    dict.Set(kLastUpdateTimeMillis,
             base::NumberToString(metadata->last_update_time_millis()));
    dict.Set(kViolationType,
             static_cast<int>(CWSInfoService::GetViolationTypeFromString(
                 metadata->violation_type())));

    const auto& proto_labels = metadata->labels();
    for (const auto* label : kLabels) {
      dict.Set(label, base::Contains(proto_labels, label));
    }
  }

  return dict;
}

// Saves CWS info if it is different from that currently saved in extension
// prefs.
bool SaveInfoIfChanged(ExtensionPrefs* extension_prefs,
                       const std::string& id,
                       const StoreMetadata* new_info) {
  bool saved = false;

  const base::Value::Dict* saved_dict =
      extension_prefs->ReadPrefAsDict(id, kCWSInfo);
  base::Value::Dict new_dict = GetDictFromStoreMetadataProto(new_info);
  if (!saved_dict || *saved_dict != new_dict) {
    // The metadata is new or is different from that saved in extension prefs.
    saved = true;
    extension_prefs->SetDictionaryPref(
        id, {kCWSInfo, kDictionary, PrefScope::kExtensionSpecific},
        std::move(new_dict));
  }

  return saved;
}

int GetNextFetchInterval() {
  // jitter fetch interval by +/- 25%
  double jitter_factor = base::RandDouble() * 0.5 + 0.75;
  return base::FeatureList::IsEnabled(kCWSInfoFastCheck)
             ? kFastFetchIntervalSeconds
             : kFetchIntervalSeconds * jitter_factor;
}

}  // namespace

// Stores context information about a CWS info fetch operation.
struct CWSInfoService::FetchContext {
  struct Request {
    ExtensionIdSet ids;
    BatchGetStoreMetadatasRequest proto;
  };
  base::queue<Request> requests;
  // Indicates if the metadata retrieved is different from that currently saved.
  bool metadata_changed = false;
};

// static
CWSInfoService* CWSInfoService::Get(Profile* profile) {
  return CWSInfoServiceFactory::GetInstance()->GetForProfile(profile);
}

CWSInfoService::CWSInfoService(Profile* profile)
    : profile_(profile),
      pref_service_(profile->GetPrefs()),
      extension_prefs_(ExtensionPrefs::Get(profile)),
      extension_registry_(ExtensionRegistry::Get(profile)),
      url_loader_factory_(profile->GetURLLoaderFactory()),
      max_ids_per_request_(kMaxExtensionIdsPerRequest),
      current_fetch_interval_secs_(GetNextFetchInterval()) {
  // Vary the startup check out between 30s to 10min, unless FastCheck
  // option is enabled.
  startup_delay_secs_ = base::FeatureList::IsEnabled(kCWSInfoFastCheck)
                            ? kFastStartupCheckDelaySeconds
                            : base::RandInt(/*min=*/30, /*max=*/600);
  ScheduleCheck(startup_delay_secs_);
}

CWSInfoService::CWSInfoService() = default;
CWSInfoService::~CWSInfoService() = default;

void CWSInfoService::Shutdown() {
  info_check_timer_.Stop();
}

std::optional<bool> CWSInfoService::IsLiveInCWS(
    const Extension& extension) const {
  const base::Value::Dict* cws_info_dict =
      extension_prefs_->ReadPrefAsDict(extension.id(), kCWSInfo);
  if (cws_info_dict == nullptr) {
    return std::nullopt;
  }
  if (!cws_info_dict->FindBool(kIsPresent).value_or(false)) {
    return false;
  }
  return cws_info_dict->FindBool(kIsLive).value_or(false);
}

std::optional<CWSInfoService::CWSInfo> CWSInfoService::GetCWSInfo(
    const Extension& extension) const {
  const base::Value::Dict* cws_info_dict =
      extension_prefs_->ReadPrefAsDict(extension.id(), kCWSInfo);
  if (cws_info_dict == nullptr) {
    return std::nullopt;
  }
  CWSInfo info;
  info.is_present = cws_info_dict->FindBool(kIsPresent).value_or(false);

  if (info.is_present) {
    info.is_live = cws_info_dict->FindBool(kIsLive).value_or(false);
    const std::string* last_update_time_millis_str =
        cws_info_dict->FindString(kLastUpdateTimeMillis);
    int64_t last_update_time_millis = 0;
    if (last_update_time_millis_str &&
        base::StringToInt64(*last_update_time_millis_str,
                            &last_update_time_millis)) {
      info.last_update_time =
          base::Time::FromMillisecondsSinceUnixEpoch(last_update_time_millis);
    }

    info.violation_type = static_cast<CWSViolationType>(
        cws_info_dict->FindInt(kViolationType).value_or(0));
    info.unpublished_long_ago =
        cws_info_dict->FindBool(kUnpublishedLongAgo).value_or(false);
    info.no_privacy_practice =
        cws_info_dict->FindBool(kNoPrivacyPractice).value_or(false);
  }

  return std::make_optional<CWSInfo>(info);
}

void CWSInfoService::CheckAndMaybeFetchInfo() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Do nothing unless an official api key is configured OR
  // the api key check is skipped for testing.
  // Note that there will be no periodic checking after this since we
  // return immediately without scheduling a future check.
  if (!google_apis::IsGoogleChromeAPIKeyUsed() &&
      !skip_api_key_check_for_testing) {
    return;
  }

  // If a fetch is already in progress, don't do anything.
  if (active_fetch_) {
    return;
  }

  base::TimeDelta elapsed_time =
      base::Time::Now() -
      pref_service_->GetTime(prefs::kCWSInfoFetchErrorTimestamp);
  // If there was a previous fetch error, wait a full fetch interval before
  // retrying.
  if (elapsed_time >= base::Seconds(current_fetch_interval_secs_)) {
    elapsed_time =
        base::Time::Now() - pref_service_->GetTime(prefs::kCWSInfoTimestamp);
    // Enough time has elapsed since the last successful fetch.
    bool data_refresh_needed =
        elapsed_time >= base::Seconds(current_fetch_interval_secs_);

    bool new_info_requested = false;
    std::unique_ptr<FetchContext> fetch_context =
        CreateRequests(new_info_requested);

    if ((data_refresh_needed || new_info_requested) && fetch_context) {
      // Stop the check timer in case it is running. This can happen if we got
      // here because of an out-of-cycle fetch.
      info_check_timer_.Stop();
      // Save the fetch context and send the (first) request.
      active_fetch_ = std::move(fetch_context);
      RecordNumRequestsInFetch(active_fetch_->requests.size());
      current_fetch_interval_secs_ = GetNextFetchInterval();
      SendRequest();
      return;
    }
  }

  // No info request necessary at this time. Schedule the next check.
  int check_interval_seconds = base::FeatureList::IsEnabled(kCWSInfoFastCheck)
                                   ? kFastCheckIntervalSeconds
                                   : kCheckIntervalSeconds;
  ScheduleCheck(check_interval_seconds);
}

void CWSInfoService::ScheduleCheck(int seconds) {
  info_check_timer_.Start(FROM_HERE, base::Seconds(seconds), this,
                          &CWSInfoService::CheckAndMaybeFetchInfo);
}

std::unique_ptr<CWSInfoService::FetchContext> CWSInfoService::CreateRequests(
    bool& new_info_requested) {
  new_info_requested = false;

  auto* extension_mgmt =
      ExtensionManagementFactory::GetForBrowserContext(profile_);
  if (!extension_mgmt) {
    return nullptr;
  }
  ExtensionSet installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  if (installed_extensions.empty()) {
    return nullptr;
  }

  auto fetch_context = std::make_unique<FetchContext>();
  FetchContext::Request* request = nullptr;
  int num_ids_added_in_request = 0;
  for (const auto& extension : installed_extensions) {
    if (extension_mgmt->UpdatesFromWebstore(*extension) == false) {
      continue;
    }
    if (extension_prefs_->ReadPrefAsDict(extension->id(), kCWSInfo) ==
        nullptr) {
      // This extension does not already have CWS info saved. Flag this as a new
      // info request.
      new_info_requested = true;
    }
    if (num_ids_added_in_request == 0) {
      // Create a new request context.
      fetch_context->requests.emplace();
      request = &fetch_context->requests.back();
      request->proto.set_parent("items/-");
    }
    request->proto.add_names(GetNameFromId(extension->id()));
    request->ids.emplace(extension->id());
    num_ids_added_in_request++;
    if (num_ids_added_in_request == max_ids_per_request_) {
      // Max ids reached for the request context. Reset the count to create
      // a new context for the remaining ids.
      num_ids_added_in_request = 0;
    }
  }

  if (fetch_context->requests.empty()) {
    // No extensions require a CWS info fetch.
    return nullptr;
  }

  // Return the fetch context - contains information about number of requests to
  // send and which ids are included in each request.
  return fetch_context;
}

void CWSInfoService::SendRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kRequestUrl);
  // A POST request is sent with an override to GET due to server requirements.
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->headers.SetHeader("X-HTTP-Method-Override", "GET");
  google_apis::AddAPIKeyToRequest(*resource_request, google_apis::GetAPIKey());
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetRetryOptions(kMaxRetriesPerRequest,
                               network::SimpleURLLoader::RETRY_ON_5XX);
  std::string request_str =
      active_fetch_->requests.front().proto.SerializeAsString();
  url_loader_->AttachStringForUpload(request_str, "application/x-protobuf");
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&CWSInfoService::OnResponseReceived,
                     weak_factory_.GetWeakPtr()));
  info_requests_++;
}

void CWSInfoService::OnResponseReceived(std::unique_ptr<std::string> response) {
  CHECK(url_loader_);
  RecordNetworkHistograms(url_loader_.get());

  bool error = false;
  if (response) {
    BatchGetStoreMetadatasResponse response_proto;
    if (response_proto.ParseFromString(*response) == true) {
      info_responses_++;
      if (MaybeSaveResponseToPrefs(response_proto)) {
        info_changes_++;
        active_fetch_->metadata_changed = true;
      }
    } else {
      DVLOG(1) << "Failed to parse response: " << *response;
      info_errors_++;
      error = true;
    }
  } else {
    info_errors_++;
    error = true;
  }

  if (error) {
    // Record the fetch error timestamp. This timestamp is used to
    // wait at least one fetch interval after an error before
    // attempting another fetch.
    pref_service_->SetTime(prefs::kCWSInfoFetchErrorTimestamp,
                           base::Time::Now());
  } else {
    // Info response received without any errors. Remove the request object
    // from the request queue.
    active_fetch_->requests.pop();
    if (!active_fetch_->requests.empty()) {
      // Request info for the next batch of extension ids.
      SendRequest();
      return;
    }

    // All requests completed. Store "freshness" timestamp in global extension
    // prefs.
    pref_service_->SetTime(prefs::kCWSInfoTimestamp, base::Time::Now());

    RecordMetadataChanged(active_fetch_->metadata_changed);
    if (active_fetch_->metadata_changed) {
      // Notify observers if the metadata changed.
      for (auto& observer : observers_) {
        observer.OnCWSInfoChanged();
      }
    }
  }

  // All requests completed successfully OR a request failed. In either case,
  // schedule the next check.
  RecordFetchSuccess(!error);
  active_fetch_.reset();
  int check_interval_seconds = base::FeatureList::IsEnabled(kCWSInfoFastCheck)
                                   ? kFastCheckIntervalSeconds
                                   : kCheckIntervalSeconds;
  ScheduleCheck(check_interval_seconds);
}

bool CWSInfoService::MaybeSaveResponseToPrefs(
    const BatchGetStoreMetadatasResponse& response_proto) {
  bool store_metadata_changed = false;

  for (const auto& metadata : response_proto.store_metadatas()) {
    std::string id = GetIdFromName(metadata.name());
    active_fetch_->requests.front().ids.erase(id);
    if (extension_prefs_->HasPrefForExtension(id) == false) {
      continue;
    }
    if (SaveInfoIfChanged(extension_prefs_, id, &metadata)) {
      store_metadata_changed = true;
    }
  }

  // Process any resquested ids missing from the response. These ids represent
  // extensions that are no longer available from the store.
  for (const auto& id : active_fetch_->requests.front().ids) {
    if (extension_prefs_->HasPrefForExtension(id) == false) {
      continue;
    }
    if (SaveInfoIfChanged(extension_prefs_, id, nullptr)) {
      store_metadata_changed = true;
    }
  }

  return store_metadata_changed;
}

void CWSInfoService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CWSInfoService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

static_assert(static_cast<int>(CWSInfoService::CWSViolationType::kUnknown) == 4,
              "GetViolationTypeFromString needs to be updated to match "
              "CWSInfoService::CWSViolationType");
// static:
CWSInfoService::CWSViolationType CWSInfoService::GetViolationTypeFromString(
    const std::string& violation_type_str) {
  static constexpr auto violation_type_str_map =
      base::MakeFixedFlatMap<std::string_view,
                             CWSInfoService::CWSViolationType>(
          {{"none", CWSInfoService::CWSViolationType::kNone},
           {"malware", CWSInfoService::CWSViolationType::kMalware},
           {"policy-violation", CWSInfoService::CWSViolationType::kPolicy},
           {"minor-policy-violation",
            CWSInfoService::CWSViolationType::kMinorPolicy}});

  const auto it = violation_type_str_map.find(violation_type_str);
  return it != violation_type_str_map.end() ? it->second
                                            : CWSViolationType::kUnknown;
}

void CWSInfoService::SetMaxExtensionIdsPerRequestForTesting(int max) {
  max_ids_per_request_ = max;
}

std::string CWSInfoService::GetRequestURLForTesting() const {
  return kRequestUrl;
}

int CWSInfoService::GetFetchIntervalForTesting() const {
  return current_fetch_interval_secs_;
}

int CWSInfoService::GetStartupDelayForTesting() const {
  return startup_delay_secs_;
}

int CWSInfoService::GetCheckIntervalForTesting() const {
  return kCheckIntervalSeconds;
}

base::Time CWSInfoService::GetCWSInfoTimestampForTesting() const {
  return pref_service_->GetTime(prefs::kCWSInfoTimestamp);
}

base::Time CWSInfoService::GetCWSInfoFetchErrorTimestampForTesting() const {
  return pref_service_->GetTime(prefs::kCWSInfoFetchErrorTimestamp);
}

// static
void CWSInfoService::SetSkipApiCheckForTesting(bool skip_api_key_check) {
  skip_api_key_check_for_testing = skip_api_key_check;
}

}  // namespace extensions
