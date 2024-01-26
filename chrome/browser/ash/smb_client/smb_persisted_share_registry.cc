// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_persisted_share_registry.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::smb_client {
namespace {

constexpr char kShareUrlKey[] = "share_url";
constexpr char kDisplayNameKey[] = "display_name";
constexpr char kUsernameKey[] = "username";
constexpr char kWorkgroupKey[] = "workgroup";
constexpr char kUseKerberosKey[] = "use_kerberos";
constexpr char kPasswordSaltKey[] = "password_salt";

base::Value ShareToDict(const SmbShareInfo& share) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set(kShareUrlKey, share.share_url().ToString())
                               .Set(kDisplayNameKey, share.display_name())
                               .Set(kUseKerberosKey, share.use_kerberos());
  if (!share.username().empty()) {
    dict.Set(kUsernameKey, share.username());
  }
  if (!share.workgroup().empty()) {
    dict.Set(kWorkgroupKey, share.workgroup());
  }
  if (!share.password_salt().empty()) {
    // Blob base::Values can't be stored in prefs, so store as a base64 encoded
    // string.
    dict.Set(kPasswordSaltKey, base::Base64Encode(share.password_salt()));
  }
  return base::Value(std::move(dict));
}

std::string GetStringValue(const base::Value::Dict& dict,
                           const std::string& key) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    return {};
  }
  return *value;
}

std::vector<uint8_t> GetEncodedBinaryValue(const base::Value::Dict& dict,
                                           const std::string& key) {
  const std::string* encoded_value = dict.FindString(key);
  if (!encoded_value) {
    return {};
  }
  std::string decoded_value;
  if (!base::Base64Decode(*encoded_value, &decoded_value)) {
    LOG(ERROR) << "Unable to decode base64-encoded binary pref from key: "
               << key;
    return {};
  }
  return {decoded_value.begin(), decoded_value.end()};
}

std::optional<SmbShareInfo> DictToShare(const base::Value::Dict& dict) {
  std::string share_url = GetStringValue(dict, kShareUrlKey);
  if (share_url.empty()) {
    return {};
  }

  SmbUrl url(share_url);
  DCHECK(url.IsValid());
  SmbShareInfo info(url, GetStringValue(dict, kDisplayNameKey),
                    GetStringValue(dict, kUsernameKey),
                    GetStringValue(dict, kWorkgroupKey),
                    dict.FindBool(kUseKerberosKey).value_or(false),
                    GetEncodedBinaryValue(dict, kPasswordSaltKey));
  return std::make_optional(std::move(info));
}

}  // namespace

// static
void SmbPersistedShareRegistry::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kNetworkFileSharesSavedShares);
}

SmbPersistedShareRegistry::SmbPersistedShareRegistry(Profile* profile)
    : profile_(profile) {}

void SmbPersistedShareRegistry::Save(const SmbShareInfo& share) {
  ScopedListPrefUpdate pref(profile_->GetPrefs(),
                            prefs::kNetworkFileSharesSavedShares);

  base::Value::List& share_list = pref.Get();
  for (base::Value& item : share_list) {
    if (GetStringValue(item.GetDict(), kShareUrlKey) ==
        share.share_url().ToString()) {
      item = ShareToDict(share);
      return;
    }
  }

  pref->Append(ShareToDict(share));
  return;
}

void SmbPersistedShareRegistry::Delete(const SmbUrl& share_url) {
  ScopedListPrefUpdate pref(profile_->GetPrefs(),
                            prefs::kNetworkFileSharesSavedShares);

  base::Value::List& list_update = pref.Get();
  for (auto it = list_update.begin(); it != list_update.end(); ++it) {
    if (GetStringValue(it->GetDict(), kShareUrlKey) == share_url.ToString()) {
      list_update.erase(it);
      return;
    }
  }
}

std::optional<SmbShareInfo> SmbPersistedShareRegistry::Get(
    const SmbUrl& share_url) const {
  const base::Value& pref =
      profile_->GetPrefs()->GetValue(prefs::kNetworkFileSharesSavedShares);

  for (const base::Value& entry : pref.GetList()) {
    if (GetStringValue(entry.GetDict(), kShareUrlKey) == share_url.ToString()) {
      return DictToShare(entry.GetDict());
    }
  }
  return {};
}

std::vector<SmbShareInfo> SmbPersistedShareRegistry::GetAll() const {
  const base::Value& pref =
      profile_->GetPrefs()->GetValue(prefs::kNetworkFileSharesSavedShares);

  std::vector<SmbShareInfo> shares;
  for (const auto& entry : pref.GetList()) {
    std::optional<SmbShareInfo> info = DictToShare(entry.GetDict());
    if (info) {
      shares.push_back(std::move(*info));
    }
  }
  return shares;
}

}  // namespace ash::smb_client
