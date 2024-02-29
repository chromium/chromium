// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_pref_service_helper.h"

#include <optional>

#include "base/base64.h"
#include "base/containers/to_value_list.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
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

// Converts a base::Value::List of Time to std::vector<base::Time>
std::vector<base::Time> ListToTimes(const base::Value::List& time_list) {
  std::vector<base::Time> times;
  for (const base::Value& time_value : time_list) {
    auto time = base::ValueToTime(time_value);
    if (time) {
      times.push_back(time.value());
    } else {
      DVLOG(ERROR) << "Could not convert time_value=" << time_value
                   << " to time.";
    }
  }
  return times;
}

// Data stored in the kMediaCdmOriginData Pref dictionary.
// {
//     $origin_string: {
//         # A unique random string for the real "origin_string".
//         "origin_id": $origin_id
//         "origin_id_creation_time": $origin_id_creation_time
//         "hardware_secure_decryption_disable_times":
//         $hw_secure_decryption_disable_times
//         "client_token": $client_token (optional)
//         "client_token_creation_time": $client_token_creation_time (optional)
//     },
//     more origin_string map...
// }
base::Value::Dict ToDictValue(const CdmPrefData& pref_data) {
  // Origin ID
  auto dict =
      base::Value::Dict()
          .Set(kOriginId, base::UnguessableTokenToValue(pref_data.origin_id()))
          .Set(kOriginIdCreationTime,
               base::TimeToValue(pref_data.origin_id_creation_time()));

  // Optional Client Token
  const std::optional<std::vector<uint8_t>> client_token =
      pref_data.client_token();
  if (client_token.has_value() && !client_token->empty()) {
    std::string encoded_client_token = base::Base64Encode(client_token.value());
    dict.Set(kClientToken, encoded_client_token);
    dict.Set(kClientTokenCreationTime,
             base::TimeToValue(pref_data.client_token_creation_time()));
  }
  dict.Set(prefs::kHardwareSecureDecryptionDisabledTimes,
           base::ToValueList(pref_data.hw_secure_decryption_disable_times(),
                             &base::TimeToValue));
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
  if (!origin_id_value) {
    return nullptr;
  }

  std::optional<base::UnguessableToken> origin_id =
      base::ValueToUnguessableToken(*origin_id_value);
  if (!origin_id) {
    return nullptr;
  }

  const base::Value* time_value = cdm_data_dict.Find(kOriginIdCreationTime);
  if (!time_value) {
    return nullptr;
  }

  std::optional<base::Time> origin_id_time = base::ValueToTime(time_value);
  if (!origin_id_time || origin_id_time.value().is_null()) {
    return nullptr;
  }

#if BUILDFLAG(IS_WIN)
  std::vector<base::Time> hw_secure_disabled_times;
  const base::Value::List* hw_secure_disabled_time_values =
      cdm_data_dict.FindList(prefs::kHardwareSecureDecryptionDisabledTimes);
  if (!hw_secure_disabled_time_values) {
    return nullptr;
  }
  hw_secure_disabled_times = ListToTimes(*hw_secure_disabled_time_values);

  auto cdm_pref_data = std::make_unique<CdmPrefData>(
      origin_id.value(), origin_id_time.value(), hw_secure_disabled_times);
#else
  auto cdm_pref_data =
      std::make_unique<CdmPrefData>(origin_id.value(), origin_id_time.value());
#endif  // BUILDFLAG(IS_WIN)

  // Client Token
  const std::string* encoded_client_token =
      cdm_data_dict.FindString(kClientToken);
  if (encoded_client_token) {
    std::string decoded_client_token;
    if (!base::Base64Decode(*encoded_client_token, &decoded_client_token)) {
      return nullptr;
    }

    std::vector<uint8_t> client_token(decoded_client_token.begin(),
                                      decoded_client_token.end());

    time_value = cdm_data_dict.Find(kClientTokenCreationTime);

    // If we have a client token but no creation time, this is an error.
    if (!time_value) {
      return nullptr;
    }

    std::optional<base::Time> client_token_time = base::ValueToTime(time_value);
    if (!client_token_time) {
      return nullptr;
    }

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

CdmPrefData::CdmPrefData(
    const base::UnguessableToken& origin_id,
    base::Time origin_id_time,
    std::vector<base::Time> hw_secure_decryption_disable_times)
    : origin_id_(origin_id),
      origin_id_creation_time_(origin_id_time),
      hw_secure_decryption_disable_times_(hw_secure_decryption_disable_times) {
  CHECK(origin_id_);
}

CdmPrefData::~CdmPrefData() = default;

const base::UnguessableToken& CdmPrefData::origin_id() const {
  return origin_id_;
}

base::Time CdmPrefData::origin_id_creation_time() const {
  return origin_id_creation_time_;
}

const std::optional<std::vector<uint8_t>> CdmPrefData::client_token() const {
  return client_token_;
}

base::Time CdmPrefData::client_token_creation_time() const {
  return client_token_creation_time_;
}

std::vector<base::Time> CdmPrefData::hw_secure_decryption_disable_times()
    const {
  return hw_secure_decryption_disable_times_;
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
  for (auto [origin, origin_value] : *update) {
    // Null filter indicates that we should delete everything.
    if (filter && !filter.Run(GURL(origin))) {
      continue;
    }

    auto* origin_dict = origin_value.GetIfDict();
    if (!origin_dict) {
      DVLOG(ERROR) << "Could not parse the preference data. Removing entry.";
      origins_to_delete.push_back(origin);
      continue;
    }

    std::unique_ptr<CdmPrefData> cdm_pref_data = FromDictValue(*origin_dict);

    if (!cdm_pref_data) {
      origins_to_delete.push_back(origin);
      continue;
    }

    if (TimeIsBetween(cdm_pref_data->origin_id_creation_time(), start, end)) {
      DVLOG(1) << "Clearing cdm pref data for " << origin;
      origins_to_delete.push_back(origin);
    } else if (TimeIsBetween(cdm_pref_data->client_token_creation_time(), start,
                             end)) {
      origin_dict->Remove(kClientToken);
      origin_dict->Remove(kClientTokenCreationTime);
    }
  }

  // Remove CDM preference data.
  for (const auto& origin_str : origins_to_delete) {
    update->Remove(origin_str);
  }

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
  if (cdm_data_dict) {
    cdm_pref_data = FromDictValue(*cdm_data_dict);
  }

  // Create an new entry or overwrite the existing one in case we weren't able
  // to get a valid origin ID from `FromDictValue()`.
  if (!cdm_pref_data) {
    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);

#if BUILDFLAG(IS_WIN)
    // Initialize hardware secure decryption disabled times to match local
    // state's hardware secure decryption disabled times. This prevents sites
    // with no prior hardware secure playback from re-experiecing errors/crashes
    // if there were previous errors that are recorded globally. See
    // go/hardware-secure-per-site-fallback for details.
    cdm_pref_data = std::make_unique<CdmPrefData>(
        base::UnguessableToken::Create(), base::Time::Now(),
        ListToTimes(g_browser_process->local_state()->GetList(
            prefs::kGlobalHardwareSecureDecryptionDisabledTimes)));
#else
    cdm_pref_data = std::make_unique<CdmPrefData>(
        base::UnguessableToken::Create(), base::Time::Now());
#endif  // BUILDFLAG(IS_WIN)
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
    if (!origin_id_value) {
      continue;
    }

    std::optional<base::UnguessableToken> origin_id =
        base::ValueToUnguessableToken(*origin_id_value);
    if (!origin_id) {
      continue;
    }

    const url::Origin origin = url::Origin::Create(GURL(key_value.first));

    mapping[origin_id->ToString()] = origin;
  }

  return mapping;
}
