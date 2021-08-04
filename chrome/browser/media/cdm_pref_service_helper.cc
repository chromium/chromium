// Copyright 2021 The Chromium Authors. All rights reserved.
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

namespace {

const char kOriginId[] = "origin_id";
const char kOriginIdCreationTime[] = "origin_id_creation_time";

const char kClientToken[] = "client_token";
const char kClientTokenCreationTime[] = "client_token_creation_time";

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
class CdmData {
 public:
  CdmData(const base::UnguessableToken& origin_id, base::Time origin_id_time)
      : origin_id_(origin_id), origin_id_creation_time_(origin_id_time) {
    DCHECK(origin_id_);
  }

  const base::UnguessableToken& origin_id() const { return origin_id_; }

  base::Time origin_id_creation_time() const {
    return origin_id_creation_time_;
  }

  const absl::optional<std::vector<uint8_t>> client_token() const {
    return client_token_;
  }

  base::Time client_token_creation_time() const {
    return client_token_creation_time_;
  }

  void SetClientToken(const std::vector<uint8_t>& client_token,
                      const base::Time creation_time) {
    VLOG(1) << __func__;
    client_token_ = client_token;
    client_token_creation_time_ = creation_time;
  }

  base::Value ToDictValue() const {
    base::Value dict(base::Value::Type::DICTIONARY);

    // Origin ID
    dict.SetKey(kOriginId, base::UnguessableTokenToValue(origin_id_));
    dict.SetKey(kOriginIdCreationTime,
                base::TimeToValue(origin_id_creation_time_));

    // Optional Client Token
    if (client_token_.has_value() && !client_token_->empty()) {
      std::string encoded_client_token =
          base::Base64Encode(client_token_.value());
      dict.SetStringKey(kClientToken, encoded_client_token);
      dict.SetKey(kClientTokenCreationTime,
                  base::TimeToValue(client_token_creation_time_));
    }

    return dict;
  }

  // Convert `cdm_data_dict` to CdmData. `cdm_data_dict` contains the origin id
  // and the time it was first created as well as the client token and the time
  // it was set/updated. Return nullptr if `cdm_data_dict` has any corruption,
  // e.g. format error, missing fields, invalid value.
  static std::unique_ptr<CdmData> FromDictValue(
      const base::Value& cdm_data_dict) {
    DCHECK(cdm_data_dict.is_dict());
    // Origin ID
    const base::Value* origin_id_value = cdm_data_dict.FindKey(kOriginId);
    if (!origin_id_value)
      return nullptr;

    absl::optional<base::UnguessableToken> origin_id =
        base::ValueToUnguessableToken(*origin_id_value);
    if (!origin_id)
      return nullptr;

    const base::Value* time_value =
        cdm_data_dict.FindKey(kOriginIdCreationTime);
    if (!time_value)
      return nullptr;

    absl::optional<base::Time> origin_id_time = base::ValueToTime(time_value);
    if (!origin_id_time || origin_id_time.value().is_null())
      return nullptr;

    auto cdm_data =
        std::make_unique<CdmData>(origin_id.value(), origin_id_time.value());

    // Client Token
    const std::string* encoded_client_token =
        cdm_data_dict.FindStringKey(kClientToken);
    if (encoded_client_token) {
      std::string decoded_client_token;
      if (!base::Base64Decode(*encoded_client_token, &decoded_client_token))
        return nullptr;

      std::vector<uint8_t> client_token(decoded_client_token.begin(),
                                        decoded_client_token.end());

      time_value = cdm_data_dict.FindKey(kClientTokenCreationTime);

      // If we have a client token but no creation time, this is an error.
      if (!time_value)
        return nullptr;

      absl::optional<base::Time> client_token_time =
          base::ValueToTime(time_value);
      if (!client_token_time)
        return nullptr;

      cdm_data->SetClientToken(client_token, client_token_time.value());
    }

    return cdm_data;
  }

 private:
  base::UnguessableToken origin_id_;
  base::Time origin_id_creation_time_;

  absl::optional<std::vector<uint8_t>> client_token_;
  base::Time client_token_creation_time_;
};

}  // namespace

CdmPrefServiceHelper::CdmPrefServiceHelper() = default;
CdmPrefServiceHelper::~CdmPrefServiceHelper() = default;

void CdmPrefServiceHelper::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kMediaCdmOriginData);
}

std::unique_ptr<media::CdmPreferenceData>
CdmPrefServiceHelper::GetCdmPreferenceData(PrefService* user_prefs,
                                           const url::Origin& cdm_origin) {
  VLOG(1) << __func__;
  // Access to the PrefService must be made from the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::DictionaryValue* dict =
      user_prefs->GetDictionary(prefs::kMediaCdmOriginData);

  DCHECK(!cdm_origin.opaque());
  if (cdm_origin.opaque()) {
    mojo::ReportBadMessage("EME use is not allowed on opaque origin");
    return nullptr;
  }

  const std::string serialized_cdm_origin = cdm_origin.Serialize();
  DCHECK(!serialized_cdm_origin.empty());

  const base::Value* cdm_data_dict =
      dict->FindKeyOfType(serialized_cdm_origin, base::Value::Type::DICTIONARY);

  std::unique_ptr<CdmData> cdm_data;
  if (cdm_data_dict)
    cdm_data = CdmData::FromDictValue(*cdm_data_dict);

  // Create an new entry or overwrite the existing one in case we weren't able
  // to get a valid origin ID from `FromDictValue()`.
  if (!cdm_data) {
    DictionaryPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
    base::DictionaryValue* dict = update.Get();

    cdm_data = std::make_unique<CdmData>(base::UnguessableToken::Create(),
                                         base::Time::Now());
    dict->SetKey(serialized_cdm_origin, cdm_data->ToDictValue());
  }

  return std::make_unique<media::CdmPreferenceData>(cdm_data->origin_id(),
                                                    cdm_data->client_token());
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

  DictionaryPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
  base::DictionaryValue* dict = update.Get();

  base::Value* dict_value =
      dict->FindKeyOfType(serialized_cdm_origin, base::Value::Type::DICTIONARY);
  std::unique_ptr<CdmData> cdm_data;
  if (!dict_value) {
    // If there is no preference associated with the origin at this point, this
    // means that the preference data was deleted by the user recently. No need
    // to save the client token in that case.
    return;
  }

  cdm_data = CdmData::FromDictValue(*dict_value);
  if (!cdm_data) {
    DVLOG(ERROR) << "The CDM preference data for origin \""
                 << serialized_cdm_origin
                 << "\" could not be parsed. Removing entry from preferences.";
    dict->RemoveKey(serialized_cdm_origin);
    return;
  }

  cdm_data->SetClientToken(client_token, base::Time::Now());
  dict->SetKey(serialized_cdm_origin, cdm_data->ToDictValue());
}
