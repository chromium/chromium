// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/config_source.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
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

  const base::Value* dictionary = nullptr;
  if (!user_manager::known_user::GetPref(
          user->GetAccountId(), prefs::kKnownUserParentAccessCodeConfig,
          &dictionary)) {
    return;
  }

  // Clear old authenticators for that user.
  config_map_[user->GetAccountId()].clear();

  const base::Value* future_config_value = dictionary->FindKeyOfType(
      kFutureConfigDictKey, base::Value::Type::DICTIONARY);
  if (future_config_value) {
    AddAuthenticator(*future_config_value, user);
  } else {
    LOG(WARNING) << "No future config for parent access code in the policy";
  }

  const base::Value* current_config_value = dictionary->FindKeyOfType(
      kCurrentConfigDictKey, base::Value::Type::DICTIONARY);
  if (current_config_value) {
    AddAuthenticator(*current_config_value, user);
  } else {
    LOG(WARNING) << "No current config for parent access code in the policy";
  }

  const base::Value* old_configs_value =
      dictionary->FindKeyOfType(kOldConfigsDictKey, base::Value::Type::LIST);
  if (old_configs_value) {
    for (const auto& config_value : old_configs_value->GetList())
      AddAuthenticator(config_value, user);
  }
}

void ConfigSource::AddAuthenticator(const base::Value& dict,
                                    const user_manager::User* user) {
  if (!dict.is_dict())
    return;

  base::Optional<AccessCodeConfig> code_config =
      AccessCodeConfig::FromDictionary(
          static_cast<const base::DictionaryValue&>(dict));
  if (code_config) {
    config_map_[user->GetAccountId()].push_back(
        std::make_unique<Authenticator>(std::move(code_config.value())));
  }
}

}  // namespace parent_access
}  // namespace chromeos
