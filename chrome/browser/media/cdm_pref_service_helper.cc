// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_pref_service_helper.h"

#include "base/logging.h"

#include "base/base64.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char kOriginId[] = "origin_id";
const char kOriginIdCreationTime[] = "origin_id_creation_time";

const char kClientToken[] = "client_token";
const char kClientTokenCreationTime[] = "client_token_creation_time";

bool TimeIsBetween(const base::Time& time,
                   const base::Time& start,
                   const base::Time& end) {
  return time >= start && (end.is_null() || time <= end);
}

// Data stored in the kMediaCdmOriginData Pref dictionary.
// {
//     $origin_string: {
//         # A unique random string for the real "origin_string".
//         "origin_id": $origin_id
//         "origin_id_creation_time": $origin_id_creation_time
//         "client_token": $client_token (optional)
//         "client_token_creation_time": $client_token_creation_time (optional)
//     },
//     more origin_string map...
// }
base::Value ToDictValue(const CdmPrefData& pref_data) {
  base::Value dict(base::Value::Type::DICT);

  // Origin ID
  dict.SetKey(kOriginId, base::UnguessableTokenToValue(pref_data.origin_id()));
  dict.SetKey(kOriginIdCreationTime,
              base::TimeToValue(pref_data.origin_id_creation_time()));

  // Optional Client Token
  const absl::optional<std::vector<uint8_t>> client_token =
      pref_data.client_token();
  if (client_token.has_value() && !client_token->empty()) {
    std::string encoded_client_token = base::Base64Encode(client_token.value());
    dict.SetStringKey(kClientToken, encoded_client_token);
    dict.SetKey(kClientTokenCreationTime,
                base::TimeToValue(pref_data.client_token_creation_time()));
  }

  return dict;
}

// Convert `cdm_data_dict` to CdmPrefData. `cdm_data_dict` contains the origin
// id and the time it was first created as well as the client token and the time
// it was set/updated. Return nullptr if `cdm_data_dict` has any corruption,
// e.g. format error, missing fields, invalid value.
std::unique_ptr<CdmPrefData> FromDictValue(
    const base::Value::Dict& cdm_data_dict) {
  // Origin ID
  const base::Value* origin_id_value = cdm_data_dict.Find(kOriginId);
  if (!origin_id_value)
    return nullptr;

  absl::optional<base::UnguessableToken> origin_id =
      base::ValueToUnguessableToken(*origin_id_value);
  if (!origin_id)
    return nullptr;

  const base::Value* time_value = cdm_data_dict.Find(kOriginIdCreationTime);
  if (!time_value)
    return nullptr;

  absl::optional<base::Time> origin_id_time = base::ValueToTime(time_value);
  if (!origin_id_time || origin_id_time.value().is_null())
    return nullptr;

  auto cdm_pref_data =
      std::make_unique<CdmPrefData>(origin_id.value(), origin_id_time.value());

  // Client Token
  const std::string* encoded_client_token =
      cdm_data_dict.FindString(kClientToken);
  if (encoded_client_token) {
    std::string decoded_client_token;
    if (!base::Base64Decode(*encoded_client_token, &decoded_client_token))
      return nullptr;

    std::vector<uint8_t> client_token(decoded_client_token.begin(),
                                      decoded_client_token.end());

    time_value = cdm_data_dict.Find(kClientTokenCreationTime);

    // If we have a client token but no creation time, this is an error.
    if (!time_value)
      return nullptr;

    absl::optional<base::Time> client_token_time =
        base::ValueToTime(time_value);
    if (!client_token_time)
      return nullptr;

    cdm_pref_data->SetClientToken(client_token, client_token_time.value());
  }

  return cdm_pref_data;
}
}  // namespace

CdmPrefData::CdmPrefData(const base::UnguessableToken& origin_id,
                         base::Time origin_id_time)
    : origin_id_(origin_id), origin_id_creation_time_(origin_id_time) {
  DCHECK(origin_id_);
}

CdmPrefData::~CdmPrefData() = default;

const base::UnguessableToken& CdmPrefData::origin_id() const {
  return origin_id_;
}

base::Time CdmPrefData::origin_id_creation_time() const {
  return origin_id_creation_time_;
}

const absl::optional<std::vector<uint8_t>> CdmPrefData::client_token() const {
  return client_token_;
}

base::Time CdmPrefData::client_token_creation_time() const {
  return client_token_creation_time_;
}

void CdmPrefData::SetClientToken(const std::vector<uint8_t>& client_token,
                                 const base::Time creation_time) {
  VLOG(1) << __func__;
  client_token_ = client_token;
  client_token_creation_time_ = creation_time;
}

CdmPrefServiceHelper::CdmPrefServiceHelper() = default;
CdmPrefServiceHelper::~CdmPrefServiceHelper() = default;

void CdmPrefServiceHelper::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kMediaCdmOriginData);
}

// Removes the CDM preference data from origin dict if the session's creation
// time falls in [`start`, `end`] and `filter` returns true on its origin.
// `start` can be null, which would indicate that we should delete everything
// since the beginning of time. `end` can also be null, in which case we can
// just ignore it. If only `client_token_creation_time` falls between `start`
// and `end`, we only clear that field. If `origin_id_creation_time` falls
// between `start` and `end`, we clear the whole entry.
void CdmPrefServiceHelper::ClearCdmPreferenceData(
    PrefService* user_prefs,
    base::Time start,
    base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& filter) {
  DVLOG(1) << __func__ << " From [" << start << ", " << end << "]";

  ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);

  std::vector<std::string> origins_to_delete;
  for (auto key_value : *update) {
    const std::string& origin = key_value.first;

    // Null filter indicates that we should delete everything.
    if (filter && !filter.Run(GURL(origin)))
      continue;

    const base::Value& origin_dict = key_value.second;
    if (!origin_dict.is_dict()) {
      DVLOG(ERROR) << "Could not parse the preference data. Removing entry.";
      origins_to_delete.push_back(origin);
      continue;
    }

    std::unique_ptr<CdmPrefData> cdm_pref_data =
        FromDictValue(origin_dict.GetDict());

    if (!cdm_pref_data) {
      origins_to_delete.push_back(origin);
      continue;
    }

    if (TimeIsBetween(cdm_pref_data->origin_id_creation_time(), start, end)) {
      DVLOG(1) << "Clearing cdm pref data for " << origin;
      origins_to_delete.push_back(origin);
    } else if (TimeIsBetween(cdm_pref_data->client_token_creation_time(), start,
                             end)) {
      key_value.second.RemoveKey(kClientToken);
      key_value.second.RemoveKey(kClientTokenCreationTime);
    }
  }

  // Remove CDM preference data.
  for (const auto& origin_str : origins_to_delete)
    update->Remove(origin_str);

  DVLOG(1) << __func__ << "Done removing CDM preference data";
}

std::unique_ptr<CdmPrefData> CdmPrefServiceHelper::GetCdmPrefData(
    PrefService* user_prefs,
    const url::Origin& cdm_origin) {
  VLOG(1) << __func__;
  // Access to the PrefService must be made from the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::Dict& dict =
      user_prefs->GetDict(prefs::kMediaCdmOriginData);

  DCHECK(!cdm_origin.opaque());
  if (cdm_origin.opaque()) {
    mojo::ReportBadMessage("EME use is not allowed on opaque origin");
    return nullptr;
  }

  const std::string serialized_cdm_origin = cdm_origin.Serialize();
  DCHECK(!serialized_cdm_origin.empty());

  const base::Value::Dict* cdm_data_dict = dict.FindDict(serialized_cdm_origin);

  std::unique_ptr<CdmPrefData> cdm_pref_data;
  if (cdm_data_dict)
    cdm_pref_data = FromDictValue(*cdm_data_dict);

  // Create an new entry or overwrite the existing one in case we weren't able
  // to get a valid origin ID from `FromDictValue()`.
  if (!cdm_pref_data) {
    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);

    cdm_pref_data = std::make_unique<CdmPrefData>(
        base::UnguessableToken::Create(), base::Time::Now());
    update->Set(serialized_cdm_origin, ToDictValue(*cdm_pref_data));
  }

  return cdm_pref_data;
}

void CdmPrefServiceHelper::SetCdmClientToken(
    PrefService* user_prefs,
    const url::Origin& cdm_origin,
    const std::vector<uint8_t>& client_token) {
  VLOG(1) << __func__;
  // Access to the PrefService must be made from the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(!cdm_origin.opaque());

  const std::string serialized_cdm_origin = cdm_origin.Serialize();
  DCHECK(!serialized_cdm_origin.empty());

  ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
  base::Value::Dict& dict = update.Get();

  base::Value::Dict* dict_value = dict.FindDict(serialized_cdm_origin);
  if (!dict_value) {
    // If there is no preference associated with the origin at this point, this
    // means that the preference data was deleted by the user recently. No need
    // to save the client token in that case.
    return;
  }

  std::unique_ptr<CdmPrefData> cdm_pref_data = FromDictValue(*dict_value);
  if (!cdm_pref_data) {
    DVLOG(ERROR) << "The CDM preference data for origin \""
                 << serialized_cdm_origin
                 << "\" could not be parsed. Removing entry from preferences.";
    dict.Remove(serialized_cdm_origin);
    return;
  }

  cdm_pref_data->SetClientToken(client_token, base::Time::Now());
  dict.Set(serialized_cdm_origin, ToDictValue(*cdm_pref_data));
}

std::map<std::string, url::Origin> CdmPrefServiceHelper::GetOriginIdMapping(
    PrefService* user_prefs) {
  std::map<std::string, url::Origin> mapping;
  const base::Value::Dict& dict =
      user_prefs->GetDict(prefs::kMediaCdmOriginData);

  for (auto key_value : dict) {
    const base::Value* origin_id_value =
        key_value.second.GetDict().Find(kOriginId);
    if (!origin_id_value)
      continue;

    absl::optional<base::UnguessableToken> origin_id =
        base::ValueToUnguessableToken(*origin_id_value);
    if (!origin_id)
      continue;

    const url::Origin origin = url::Origin::Create(GURL(key_value.first));

    mapping[origin_id->ToString()] = origin;
  }

  return mapping;
}
