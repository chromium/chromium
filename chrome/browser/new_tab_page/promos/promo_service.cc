// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/promos/promo_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "extensions/common/extension_features.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

namespace {

// The number of days until a blocklist entry expires.
const int kDaysThatBlocklistExpiresIn = 28;

const char kNewTabPromosApiPath[] = "/async/newtab_promos";

const char kXSSIResponsePreamble[] = ")]}'";

constexpr char kWarningSymbol[] =
    "data:image/"
    "svg+xml;base64,"
    "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9Ii01IC"
    "01IDU4IDU4IiBmaWxsPSIjZmRkNjMzIj48cGF0aCBkPSJNMiA0Mmg0NEwyNCA0IDIgNDJ6"
    "bTI0LTZoLTR2LTRoNHY0em0wLThoLTR2LThoNHY4eiIvPjwvc3ZnPg==";
constexpr char kFakePromo[] = R"({
  "update": {
    "promos": {
      "middle": "test",
      "middle_announce_payload": {
        "hidden": false,
        "part": [{
          "image": {
            "image_url": "%s",
            "target": "command:%s"
          }
        },{
          "link": {
            "url": "command:%s",
            "text": "Test command: %s"
          }
        }]
      },
      "id": "test%s"
    }
  }
})";

bool CanBlockPromos() {
  return base::FeatureList::IsEnabled(
      ntp_features::kNtpMiddleSlotPromoDismissal);
}

GURL GetGoogleBaseUrl() {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = GURL(google_util::kGoogleHomepageURL);
  }
  return google_base_url;
}

GURL GetApiUrl() {
  return GetGoogleBaseUrl().Resolve(kNewTabPromosApiPath);
}

// Parses an update proto from |value|. Will return false if |value| is not of
// the form: {"update":{"promos":{"middle_announce_payload": ""}}}, and true
// otherwise.
// Additionally, there can be a "log_url" or "id" field in the promo. Those are
// populated if found. They're not set for emergency promos. |data| will never
// be std::nullopt if top level dictionary keys of "update" and "promos" are
// present. Note: the "log_url" (if found), is resolved against
// GetGoogleBaseUrl() to form a valid GURL.
bool JsonToPromoData(const base::Value& value, std::optional<PromoData>* data) {
  *data = std::nullopt;

  if (!value.is_dict()) {
    DVLOG(1) << "Parse error: top-level dictionary not found";
    return false;
  }
  const base::Value::Dict& dict = value.GetDict();

  const base::Value::Dict* update = dict.FindDict("update");
  if (!update) {
    DVLOG(1) << "Parse error: no update";
    return false;
  }

  const base::Value::Dict* promos = update->FindDict("promos");
  if (!promos) {
    DVLOG(1) << "Parse error: no promos";
    return false;
  }

  PromoData result;
  *data = result;

  const base::Value::Dict* middle_announce_payload =
      promos->FindDict("middle_announce_payload");
  if (!middle_announce_payload) {
    DVLOG(1) << "No middle announce payload";
    return false;
  }
  JSONStringValueSerializer serializer(&result.middle_slot_json);
  serializer.Serialize(*middle_announce_payload);

  const std::string* maybe_log_url = promos->FindString("log_url");
  // Emergency promos don't have these, so it's OK if this key is missing.
  std::string log_url = maybe_log_url ? *maybe_log_url : std::string();

  GURL promo_log_url;
  if (!log_url.empty())
    promo_log_url = GetGoogleBaseUrl().Resolve(log_url);

  std::string promo_id;
  if (CanBlockPromos()) {
    const std::string* maybe_promo_id = promos->FindString("id");
    if (maybe_promo_id)
      promo_id = *maybe_promo_id;
    else
      net::GetValueForKeyInQuery(promo_log_url, "id", &promo_id);
  }

  // Emergency promos may not have IDs, which is OK. They also can't be
  // dismissed (because of this).

  result.promo_log_url = promo_log_url;
  result.promo_id = promo_id;

  *data = result;

  return true;
}

}  // namespace

PromoService::PromoService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile)
    : url_loader_factory_(url_loader_factory), profile_(profile) {}

PromoService::~PromoService() = default;

void PromoService::Refresh() {
  std::string command_id;
  // Replace the promo URL with "command:<id>" if such a command ID is set
  // via the feature params.
  // If fake data is being used, we set the command_id to 7, which corresponds
  // to kNoOpCommand in
  // ui/webui/resources/js/browser_command/browser_command.mojom
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpMiddleSlotPromoDismissal,
          ntp_features::kNtpMiddleSlotPromoDismissalParam) == "fake") {
    command_id = base::NumberToString(
        static_cast<int>(browser_command::mojom::Command::kNoOpCommand));
  } else {
    command_id = base::GetFieldTrialParamValueByFeature(
        features::kPromoBrowserCommands, features::kBrowserCommandIdParam);
  }

  if (!command_id.empty()) {
    auto fake_promo_json = std::make_unique<std::string>(base::StringPrintf(
        kFakePromo, kWarningSymbol, command_id.c_str(), command_id.c_str(),
        command_id.c_str(), command_id.c_str()));
    OnLoadDone(std::move(fake_promo_json));
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("promo_service", R"(
        semantics {
          sender: "Promo Service"
          description: "Downloads promos."
          trigger:
            "Displaying the new tab page on Desktop, if Google is the "
            "configured search provider."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetApiUrl();
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PromoService::OnLoadDone, base::Unretained(this)),
      1024 * 1024);
}

void PromoService::OnLoadDone(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DVLOG(1) << "Request failed with error: " << simple_loader_->NetError();
    PromoDataLoaded(Status::TRANSIENT_ERROR, std::nullopt);
    return;
  }

  std::string response;
  response.swap(*response_body);

  // The response may start with )]}'. Ignore this.
  if (base::StartsWith(response, kXSSIResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    response = response.substr(strlen(kXSSIResponsePreamble));
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      response, base::BindOnce(&PromoService::OnJsonParsed,
                               weak_ptr_factory_.GetWeakPtr()));
}

void PromoService::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    DVLOG(1) << "Parsing JSON failed: " << result.error();
    PromoDataLoaded(Status::FATAL_ERROR, std::nullopt);
    return;
  }

  std::optional<PromoData> data;
  PromoService::Status status;

  if (JsonToPromoData(*result, &data)) {
    bool is_blocked = IsBlockedAfterClearingExpired(data->promo_id);
    if (is_blocked)
      data = PromoData();
    status = is_blocked ? Status::OK_BUT_BLOCKED : Status::OK_WITH_PROMO;
  } else {
    status = data ? Status::OK_WITHOUT_PROMO : Status::FATAL_ERROR;
  }

  PromoDataLoaded(status, data);
}

void PromoService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnPromoServiceShuttingDown();
  }

  DCHECK(observers_.empty());
}

// static
void PromoService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNtpPromoBlocklist);
}

const std::optional<PromoData>& PromoService::promo_data() const {
  return promo_data_;
}

void PromoService::AddObserver(PromoServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void PromoService::RemoveObserver(PromoServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PromoService::BlocklistPromo(const std::string& promo_id) {
  if (!CanBlockPromos() || promo_id.empty() ||
      IsBlockedAfterClearingExpired(promo_id)) {
    return;
  }

  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kNtpPromoBlocklist);
  double now = base::Time::Now().ToDeltaSinceWindowsEpoch().InSecondsF();
  update->Set(promo_id, now);

  // Check if the promo id to be blocked is the same as the promo id of the
  // current promo being served.
  if (promo_data_ && promo_data_->promo_id == promo_id) {
    promo_data_ = PromoData();
    promo_status_ = Status::OK_BUT_BLOCKED;
    NotifyObservers();
    // TODO(crbug.com/40098612): hide promos on existing, already-opened NTPs.
  }
}

void PromoService::UndoBlocklistPromo(const std::string& promo_id) {
  if (promo_id.empty()) {
    return;
  }

  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kNtpPromoBlocklist);
  update->Remove(promo_id);

  // Refresh promo service since cached promo data was cleared in
  // BlocklistPromo(), which is called before UndoBlocklistPromo().
  Refresh();
}

void PromoService::PromoDataLoaded(Status status,
                                   const std::optional<PromoData>& data) {
  // In case of transient errors, keep our cached data (if any), but still
  // notify observers of the finished load (attempt).
  if (status != Status::TRANSIENT_ERROR) {
    promo_data_ = data;
  }
  promo_status_ = status;
  NotifyObservers();
}

void PromoService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnPromoDataUpdated();
  }
}

bool PromoService::IsBlockedAfterClearingExpired(
    const std::string& promo_id) const {
  if (promo_id.empty() || !CanBlockPromos())
    return false;

  auto expired_delta = base::Days(kDaysThatBlocklistExpiresIn);
  auto expired_time = base::Time::Now() - expired_delta;
  double expired = expired_time.ToDeltaSinceWindowsEpoch().InSecondsF();

  bool found = false;

  std::vector<std::string> expired_ids;

  for (auto blocked :
       profile_->GetPrefs()->GetDict(prefs::kNtpPromoBlocklist)) {
    if (!blocked.second.is_double() || blocked.second.GetDouble() < expired)
      expired_ids.emplace_back(blocked.first);
    else if (!found && blocked.first == promo_id)
      found = true;  // Don't break; keep clearing expired prefs.
  }

  if (!expired_ids.empty()) {
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpPromoBlocklist);
    for (const std::string& key : expired_ids)
      update->Remove(key);
  }

  return found;
}

GURL PromoService::GetLoadURLForTesting() const {
  return GetApiUrl();
}
