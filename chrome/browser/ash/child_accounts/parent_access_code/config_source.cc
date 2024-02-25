// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/parent_access_code/config_source.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace parent_access {

namespace {

// Dictionary keys for ParentAccessCodeConfig policy.
constexpr char kFutureConfigDictKey[] = "future_config";
constexpr char kCurrentConfigDictKey[] = "current_config";
constexpr char kOldConfigsDictKey[] = "old_configs";

}  // namespace

ConfigSource::ConfigSource() {
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  for (const user_manager::User* user : users) {
    if (user->IsChild())
      LoadConfigForUser(user);
  }
}

ConfigSource::~ConfigSource() = default;

void ConfigSource::LoadConfigForUser(const user_manager::User* user) {
  DCHECK(user->IsChild());

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const base::Value* dictionary = known_user.FindPath(
      user->GetAccountId(), prefs::kKnownUserParentAccessCodeConfig);
  if (!dictionary)
    return;

  // Clear old authenticators for that user.
  config_map_[user->GetAccountId()].clear();

  const base::Value::Dict* future_config_value =
      dictionary->GetDict().FindDict(kFutureConfigDictKey);
  if (future_config_value) {
    AddAuthenticator(*future_config_value, user);
  } else {
    LOG(WARNING) << "No future config for parent access code in the policy";
  }

  const base::Value::Dict* current_config_value =
      dictionary->GetDict().FindDict(kCurrentConfigDictKey);
  if (current_config_value) {
    AddAuthenticator(*current_config_value, user);
  } else {
    LOG(WARNING) << "No current config for parent access code in the policy";
  }

  const base::Value::List* old_configs_value =
      dictionary->GetDict().FindList(kOldConfigsDictKey);
  if (old_configs_value) {
    for (const auto& config_value : *old_configs_value) {
      AddAuthenticator(config_value.GetDict(), user);
    }
  }
}

void ConfigSource::AddAuthenticator(const base::Value::Dict& dict,
                                    const user_manager::User* user) {
  std::optional<AccessCodeConfig> code_config =
      AccessCodeConfig::FromDictionary(dict);
  if (code_config) {
    config_map_[user->GetAccountId()].push_back(
        std::make_unique<Authenticator>(std::move(code_config.value())));
  }
}

}  // namespace parent_access
}  // namespace ash
