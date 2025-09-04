// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/parent_access_code/config_source.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace parent_access {

namespace {

// Dictionary keys for ParentAccessCodeConfig policy.
constexpr char kFutureConfigDictKey[] = "future_config";
constexpr char kCurrentConfigDictKey[] = "current_config";
constexpr char kOldConfigsDictKey[] = "old_configs";

}  // namespace

ConfigSource::ConfigSource(PrefService* local_state)
    : local_state_(CHECK_DEREF(local_state)) {
  user_manager::KnownUser known_user(&local_state_.get());

  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetPersistedUsers();
  for (const user_manager::User* user : users) {
    if (!user->IsChild()) {
      continue;
    }

    const base::Value* dictionary = known_user.FindPath(
        user->GetAccountId(), prefs::kKnownUserParentAccessCodeConfig);
    if (dictionary) {
      LoadConfigForUser(user->GetAccountId(), dictionary->GetDict());
    }
  }
}

ConfigSource::~ConfigSource() = default;

void ConfigSource::UpdateConfigForUser(const AccountId& account_id,
                                       base::Value::Dict config) {
#if DCHECK_IS_ON()
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  DCHECK(user->IsChild());
#endif  // DCHECK_IS_ON()

  user_manager::KnownUser known_user(&local_state_.get());
  known_user.SetPath(account_id, ::prefs::kKnownUserParentAccessCodeConfig,
                     base::Value(std::move(config)));

  const base::Value* dictionary =
      known_user.FindPath(account_id, prefs::kKnownUserParentAccessCodeConfig);
  CHECK(dictionary);
  LoadConfigForUser(account_id, dictionary->GetDict());
}

void ConfigSource::RemoveConfigForUser(const AccountId& account_id) {
  user_manager::KnownUser(&local_state_.get())
      .RemovePref(account_id, ::prefs::kKnownUserParentAccessCodeConfig);
}

void ConfigSource::LoadConfigForUser(const AccountId& account_id,
                                     const base::Value::Dict& dictionary) {
  // Clear old authenticators for that user.
  config_map_[account_id].clear();

  const base::Value::Dict* future_config_value =
      dictionary.FindDict(kFutureConfigDictKey);
  if (future_config_value) {
    AddAuthenticator(*future_config_value, account_id);
  } else {
    LOG(WARNING) << "No future config for parent access code in the policy";
  }

  const base::Value::Dict* current_config_value =
      dictionary.FindDict(kCurrentConfigDictKey);
  if (current_config_value) {
    AddAuthenticator(*current_config_value, account_id);
  } else {
    LOG(WARNING) << "No current config for parent access code in the policy";
  }

  const base::Value::List* old_configs_value =
      dictionary.FindList(kOldConfigsDictKey);
  if (old_configs_value) {
    for (const auto& config_value : *old_configs_value) {
      AddAuthenticator(config_value.GetDict(), account_id);
    }
  }
}

void ConfigSource::AddAuthenticator(const base::Value::Dict& dict,
                                    const AccountId& account_id) {
  std::optional<AccessCodeConfig> code_config =
      AccessCodeConfig::FromDictionary(dict);
  if (code_config) {
    config_map_[account_id].push_back(
        std::make_unique<Authenticator>(std::move(code_config.value())));
  }
}

}  // namespace parent_access
}  // namespace ash
