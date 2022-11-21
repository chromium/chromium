// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/supervised_user_manager_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using content::BrowserThread;

namespace {

// Names for pref keys in Local State.
// A map from supervised user local user id to sync user id.
const char kSupervisedUserSyncId[] = "ManagedUserSyncId";

// A map from supervised user id to manager user id.
const char kSupervisedUserManagers[] = "ManagedUserManagers";

// A map from supervised user id to manager display name.
const char kSupervisedUserManagerNames[] = "ManagedUserManagerNames";

// A map from supervised user id to manager display e-mail.
const char kSupervisedUserManagerDisplayEmails[] =
    "ManagedUserManagerDisplayEmails";

// A vector pref of the supervised accounts defined on this device, that had
// not logged in yet.
const char kSupervisedUsersFirstRun[] = "LocallyManagedUsersFirstRun";

// A map from user id to password schema id.
const char kSupervisedUserPasswordSchema[] = "SupervisedUserPasswordSchema";

// A map from user id to password salt.
const char kSupervisedUserPasswordSalt[] = "SupervisedUserPasswordSalt";

// A map from user id to password revision.
const char kSupervisedUserPasswordRevision[] = "SupervisedUserPasswordRevision";

// A map from user id to flag indicating if password should be updated upon
// signin.
const char kSupervisedUserNeedPasswordUpdate[] =
    "SupervisedUserNeedPasswordUpdate";

// A map from user id to flag indicating if cryptohome does not have signature
// key.
const char kSupervisedUserIncompleteKey[] = "SupervisedUserHasIncompleteKey";

}  // namespace

namespace ash {

const char kSchemaVersion[] = "SchemaVersion";
const char kPasswordRevision[] = "PasswordRevision";
const char kSalt[] = "PasswordSalt";
const char kRequirePasswordUpdate[] = "RequirePasswordUpdate";
const char kHasIncompleteKey[] = "HasIncompleteKey";

// static
void SupervisedUserManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(kSupervisedUsersFirstRun);
  registry->RegisterDictionaryPref(kSupervisedUserSyncId);
  registry->RegisterDictionaryPref(kSupervisedUserManagers);
  registry->RegisterDictionaryPref(kSupervisedUserManagerNames);
  registry->RegisterDictionaryPref(kSupervisedUserManagerDisplayEmails);

  registry->RegisterDictionaryPref(kSupervisedUserPasswordSchema);
  registry->RegisterDictionaryPref(kSupervisedUserPasswordSalt);
  registry->RegisterDictionaryPref(kSupervisedUserPasswordRevision);

  registry->RegisterDictionaryPref(kSupervisedUserNeedPasswordUpdate);
  registry->RegisterDictionaryPref(kSupervisedUserIncompleteKey);
}

SupervisedUserManagerImpl::SupervisedUserManagerImpl(
    ChromeUserManagerImpl* owner)
    : owner_(owner), cros_settings_(CrosSettings::Get()) {
  // SupervisedUserManager instance should be used only on UI thread.
  // (or in unit_tests)
  if (base::SingleThreadTaskRunner::HasCurrentDefault())
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SupervisedUserManagerImpl::~SupervisedUserManagerImpl() {}

std::string SupervisedUserManagerImpl::GetUserSyncId(
    const std::string& user_id) const {
  std::string result;
  GetUserStringValue(user_id, kSupervisedUserSyncId, &result);
  return result;
}

std::u16string SupervisedUserManagerImpl::GetManagerDisplayName(
    const std::string& user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& manager_names =
      local_state->GetDict(kSupervisedUserManagerNames);
  const std::string* result = manager_names.FindString(user_id);
  if (result && !result->empty())
    return base::UTF8ToUTF16(*result);
  return base::UTF8ToUTF16(GetManagerDisplayEmail(user_id));
}

std::string SupervisedUserManagerImpl::GetManagerUserId(
    const std::string& user_id) const {
  std::string result;
  GetUserStringValue(user_id, kSupervisedUserManagers, &result);
  return result;
}

std::string SupervisedUserManagerImpl::GetManagerDisplayEmail(
    const std::string& user_id) const {
  std::string result;
  if (GetUserStringValue(user_id, kSupervisedUserManagerDisplayEmails,
                         &result) &&
      !result.empty())
    return result;
  return GetManagerUserId(user_id);
}

void SupervisedUserManagerImpl::GetPasswordInformation(
    const std::string& user_id,
    base::Value::Dict* result) {
  int value;
  if (GetUserIntegerValue(user_id, kSupervisedUserPasswordSchema, &value))
    result->Set(kSchemaVersion, base::Value(value));
  if (GetUserIntegerValue(user_id, kSupervisedUserPasswordRevision, &value))
    result->Set(kPasswordRevision, base::Value(value));

  bool flag;
  if (GetUserBooleanValue(user_id, kSupervisedUserNeedPasswordUpdate, &flag))
    result->Set(kRequirePasswordUpdate, base::Value(flag));
  if (GetUserBooleanValue(user_id, kSupervisedUserIncompleteKey, &flag))
    result->Set(kHasIncompleteKey, base::Value(flag));

  std::string salt;
  if (GetUserStringValue(user_id, kSupervisedUserPasswordSalt, &salt))
    result->Set(kSalt, base::Value(salt));
}

void SupervisedUserManagerImpl::SetPasswordInformation(
    const std::string& user_id,
    const base::Value::Dict* password_info) {
  absl::optional<int> schema_version = password_info->FindInt(kSchemaVersion);
  if (schema_version.has_value())
    SetUserIntegerValue(user_id, kSupervisedUserPasswordSchema,
                        schema_version.value());
  absl::optional<int> password_revision =
      password_info->FindInt(kPasswordRevision);
  if (password_revision.has_value())
    SetUserIntegerValue(user_id, kSupervisedUserPasswordRevision,
                        password_revision.value());

  absl::optional<bool> flag = password_info->FindBool(kRequirePasswordUpdate);
  if (flag.has_value()) {
    SetUserBooleanValue(user_id, kSupervisedUserNeedPasswordUpdate,
                        flag.value());
  }
  flag = password_info->FindBool(kHasIncompleteKey);
  if (flag.has_value())
    SetUserBooleanValue(user_id, kSupervisedUserIncompleteKey, flag.value());

  const std::string* salt = password_info->FindString(kSalt);
  if (salt)
    SetUserStringValue(user_id, kSupervisedUserPasswordSalt, *salt);
  g_browser_process->local_state()->CommitPendingWrite();
}

bool SupervisedUserManagerImpl::GetUserStringValue(
    const std::string& user_id,
    const char* key,
    std::string* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& dictionary = local_state->GetDict(key);
  const std::string* value = dictionary.FindString(user_id);
  if (!value)
    return false;

  *out_value = *value;
  return true;
}

bool SupervisedUserManagerImpl::GetUserIntegerValue(const std::string& user_id,
                                                    const char* key,
                                                    int* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& dictionary = local_state->GetDict(key);
  absl::optional<int> value = dictionary.FindInt(user_id);
  if (!value)
    return false;

  *out_value = value.value();
  return true;
}

bool SupervisedUserManagerImpl::GetUserBooleanValue(const std::string& user_id,
                                                    const char* key,
                                                    bool* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& dictionary = local_state->GetDict(key);
  absl::optional<bool> flag = dictionary.FindBool(user_id);
  if (!flag)
    return false;

  *out_value = flag.value();
  return true;
}

void SupervisedUserManagerImpl::SetUserStringValue(const std::string& user_id,
                                                   const char* key,
                                                   const std::string& value) {
  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate update(local_state, key);
  update->Set(user_id, value);
}

void SupervisedUserManagerImpl::SetUserIntegerValue(const std::string& user_id,
                                                    const char* key,
                                                    const int value) {
  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate update(local_state, key);
  update->Set(user_id, value);
}

void SupervisedUserManagerImpl::SetUserBooleanValue(const std::string& user_id,
                                                    const char* key,
                                                    const bool value) {
  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate update(local_state, key);
  update->Set(user_id, value);
}

void SupervisedUserManagerImpl::RemoveNonCryptohomeData(
    const std::string& user_id) {
  PrefService* prefs = g_browser_process->local_state();
  ScopedListPrefUpdate prefs_new_users_update(prefs, kSupervisedUsersFirstRun);
  prefs_new_users_update->EraseValue(base::Value(user_id));

  CleanPref(user_id, kSupervisedUserSyncId);
  CleanPref(user_id, kSupervisedUserManagers);
  CleanPref(user_id, kSupervisedUserManagerNames);
  CleanPref(user_id, kSupervisedUserManagerDisplayEmails);
  CleanPref(user_id, kSupervisedUserPasswordSalt);
  CleanPref(user_id, kSupervisedUserPasswordSchema);
  CleanPref(user_id, kSupervisedUserPasswordRevision);
  CleanPref(user_id, kSupervisedUserNeedPasswordUpdate);
  CleanPref(user_id, kSupervisedUserIncompleteKey);
}

void SupervisedUserManagerImpl::CleanPref(const std::string& user_id,
                                          const char* key) {
  PrefService* prefs = g_browser_process->local_state();
  ScopedDictPrefUpdate dict_update(prefs, key);
  dict_update->Remove(user_id);
}

bool SupervisedUserManagerImpl::CheckForFirstRun(const std::string& user_id) {
  ScopedListPrefUpdate prefs_new_users_update(g_browser_process->local_state(),
                                              kSupervisedUsersFirstRun);
  return prefs_new_users_update->EraseValue(base::Value(user_id));
}

}  // namespace ash
