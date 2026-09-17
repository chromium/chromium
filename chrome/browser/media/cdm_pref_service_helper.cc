// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_pref_service_helper.h"

#include <optional>

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

bool TimeIsBetween(const base::Time& time,
                   const base::Time& start,
                   const base::Time& end) {
  return time >= start && (end.is_null() || time <= end);
}

// Converts a base::ListValue of Time to std::vector<base::Time>
std::vector<base::Time> ListToTimes(const base::ListValue& time_list) {
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
//     },
//     more origin_string map...
// }
base::DictValue ToDictValue(const CdmPrefData& pref_data) {
  // Origin ID
  auto dict =
      base::DictValue()
          .Set(kOriginId, base::UnguessableTokenToValue(pref_data.origin_id()))
          .Set(kOriginIdCreationTime,
               base::TimeToValue(pref_data.origin_id_creation_time()));

  dict.Set(prefs::kHardwareSecureDecryptionDisabledTimes,
           base::ToValueList(pref_data.hw_secure_decryption_disable_times(),
                             &base::TimeToValue));
  return dict;
}

// Convert `cdm_data_dict` to CdmPrefData. `cdm_data_dict` contains the origin
// id and the time it was first created. Return nullptr if `cdm_data_dict` has
// any corruption, e.g. format error, missing fields, invalid value.
std::unique_ptr<CdmPrefData> FromDictValue(
    const base::DictValue& cdm_data_dict) {
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
  const base::ListValue* hw_secure_disabled_time_values =
      cdm_data_dict.FindList(prefs::kHardwareSecureDecryptionDisabledTimes);
  if (!hw_secure_disabled_time_values) {
    return nullptr;
  }
  hw_secure_disabled_times = ListToTimes(*hw_secure_disabled_time_values);

  return std::make_unique<CdmPrefData>(
      origin_id.value(), origin_id_time.value(), hw_secure_disabled_times);
#else
  return std::make_unique<CdmPrefData>(origin_id.value(),
                                       origin_id_time.value());
#endif  // BUILDFLAG(IS_WIN)
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

std::vector<base::Time> CdmPrefData::hw_secure_decryption_disable_times()
    const {
  return hw_secure_decryption_disable_times_;
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
// just ignore it.
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

  const base::DictValue& dict = user_prefs->GetDict(prefs::kMediaCdmOriginData);

  DCHECK(!cdm_origin.opaque());
  if (cdm_origin.opaque()) {
    mojo::ReportBadMessage("EME use is not allowed on opaque origin");
    return nullptr;
  }

  const std::string serialized_cdm_origin = cdm_origin.Serialize();
  DCHECK(!serialized_cdm_origin.empty());

  const base::DictValue* cdm_data_dict = dict.FindDict(serialized_cdm_origin);

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

// static
void CdmPrefServiceHelper::MigrateObsoleteProfilePrefs(
    PrefService* profile_prefs) {
  if (!profile_prefs->HasPrefPath(prefs::kMediaCdmOriginData)) {
    return;
  }

  // Check if any obsolete keys exist before creating a ScopedDictPrefUpdate.
  // Creating a ScopedDictPrefUpdate unconditionally marks the pref as dirty and
  // notifies observers, which would trigger unnecessary disk writes on every
  // browser startup once migrated.
  const base::DictValue& dict =
      profile_prefs->GetDict(prefs::kMediaCdmOriginData);
  bool needs_migration = false;
  for (auto [origin, origin_value] : dict) {
    if (const auto* origin_dict = origin_value.GetIfDict()) {
      if (origin_dict->Find("client_token") ||
          origin_dict->Find("client_token_creation_time")) {
        needs_migration = true;
        break;
      }
    }
  }

  if (!needs_migration) {
    return;
  }

  ScopedDictPrefUpdate update(profile_prefs, prefs::kMediaCdmOriginData);
  for (auto [origin, origin_value] : *update) {
    if (auto* origin_dict = origin_value.GetIfDict()) {
      // Added 09/2026.
      origin_dict->Remove("client_token");
      origin_dict->Remove("client_token_creation_time");
    }
  }
}

std::map<std::string, url::Origin> CdmPrefServiceHelper::GetOriginIdMapping(
    PrefService* user_prefs) {
  std::map<std::string, url::Origin> mapping;
  const base::DictValue& dict = user_prefs->GetDict(prefs::kMediaCdmOriginData);

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
