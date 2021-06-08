// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_pref_service_helper.h"

#include "base/logging.h"

#include "base/memory/ptr_util.h"
#include "base/util/values/values_util.h"
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
const char kCreationTime[] = "creation_time";

// Data stored in the kMediaCdmOrigin Pref dictionary.
// {
//     $origin_string: {
//         # A unique random string for the real "origin_string".
//         "origin_id": $origin_id
//         "creation_time": $creation_time
//     },
//     more origin_string map...
// }
class OriginData {
 public:
  OriginData(const base::UnguessableToken& origin_id, base::Time time)
      : origin_id_(origin_id), creation_time_(time) {
    DCHECK(origin_id_);
  }

  const base::UnguessableToken& origin_id() const { return origin_id_; }

  base::Time creation_time() const { return creation_time_; }

  base::Value ToDictValue() const {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetKey(kOriginId, util::UnguessableTokenToValue(origin_id_));
    dict.SetKey(kCreationTime, util::TimeToValue(creation_time_));

    return dict;
  }

  // Convert `origin_dict` to OriginData. `origin_dict` contains the origin id
  // and the time it was first created. Return nullptr if `origin_dict` has any
  // corruption, e.g. format error, missing fields, invalid value.
  static std::unique_ptr<OriginData> FromDictValue(
      const base::Value& origin_dict) {
    DCHECK(origin_dict.is_dict());

    const base::Value* origin_id_value = origin_dict.FindKey(kOriginId);
    if (!origin_id_value)
      return nullptr;

    absl::optional<base::UnguessableToken> origin_id =
        util::ValueToUnguessableToken(*origin_id_value);
    if (!origin_id)
      return nullptr;

    const base::Value* time_value = origin_dict.FindKey(kCreationTime);
    if (!time_value)
      return nullptr;

    absl::optional<base::Time> time = util::ValueToTime(time_value);
    if (!time || time.value().is_null())
      return nullptr;

    return std::make_unique<OriginData>(origin_id.value(), time.value());
  }

 private:
  base::UnguessableToken origin_id_;
  base::Time creation_time_;
};

}  // namespace

CdmPrefServiceHelper::CdmPrefServiceHelper() = default;
CdmPrefServiceHelper::~CdmPrefServiceHelper() = default;

void CdmPrefServiceHelper::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kMediaCdmOrigin);
}

base::UnguessableToken CdmPrefServiceHelper::GetCdmOriginId(
    PrefService* user_prefs,
    const url::Origin& cdm_origin) {
  // Access to the PrefService must be made from the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::DictionaryValue* dict =
      user_prefs->GetDictionary(prefs::kMediaCdmOrigin);

  DCHECK(!cdm_origin.opaque());
  if (cdm_origin.opaque()) {
    mojo::ReportBadMessage("EME use is not allowed on opaque origin");
    return base::UnguessableToken::Null();
  }

  const std::string serialized_cdm_origin = cdm_origin.Serialize();
  DCHECK(!serialized_cdm_origin.empty());

  const base::Value* origin_dict =
      dict->FindKeyOfType(serialized_cdm_origin, base::Value::Type::DICTIONARY);

  std::unique_ptr<OriginData> origin_data;
  if (origin_dict) {
    origin_data = OriginData::FromDictValue(*origin_dict);
  }

  // Create an new entry or overwrite the existing one in case we weren't able
  // to get a valid origin ID from `FromDictValue()`.
  if (!origin_data) {
    DictionaryPrefUpdate update(user_prefs, prefs::kMediaCdmOrigin);
    base::DictionaryValue* dict = update.Get();

    origin_data = std::make_unique<OriginData>(base::UnguessableToken::Create(),
                                               base::Time::Now());
    dict->SetKey(serialized_cdm_origin, origin_data->ToDictValue());
  }

  return origin_data->origin_id();
}
