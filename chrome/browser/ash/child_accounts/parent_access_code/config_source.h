// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/authenticator.h"

class AccountId;
class PrefService;

namespace ash {
namespace parent_access {

// Base class for parent access code configuration providers.
class ConfigSource {
 public:
  // Map containing a list of Authenticators per child account logged in the
  // device.
  typedef std::map<AccountId, std::vector<std::unique_ptr<Authenticator>>>
      AuthenticatorsMap;

  // `local_state` must be non-null, and must outlive `this`.
  explicit ConfigSource(PrefService* local_state);

  ConfigSource(const ConfigSource&) = delete;
  ConfigSource& operator=(const ConfigSource&) = delete;

  ~ConfigSource();

  const AuthenticatorsMap& config_map() const { return config_map_; }

  // Updates the persisted config for that particular user.
  void UpdateConfigForUser(const AccountId& account_id,
                           base::Value::Dict config);

  // Removes the persisted config for that particular user.
  void RemoveConfigForUser(const AccountId& account_id);

 private:
  // Reloads the Parent Access Code config for that particular user.
  void LoadConfigForUser(const AccountId& account_id,
                         const base::Value::Dict& dictionary);

  // Creates and adds an authenticator to the |config_map_|. |dict| corresponds
  // to an AccessCodeConfig in its serialized format.
  void AddAuthenticator(const base::Value::Dict& dict,
                        const AccountId& account_id);

  const raw_ref<PrefService> local_state_;

  // Holds the Parent Access Code Authenticators for all children signed in this
  // device.
  AuthenticatorsMap config_map_;
};

}  // namespace parent_access
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_
