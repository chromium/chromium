// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_AUTHENTICATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_AUTHENTICATION_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_login_flow.h"
#include "chromeos/login/auth/user_context.h"

namespace chromeos {

class SupervisedUserManager;

// This is a class that encapsulates all details of password handling for
// supervised users.
// Main property is the schema used to handle password. For now it can be either
// plain password schema, when plain text password is passed to standard
// cryprohome authentication algorithm without modification, or hashed password
// schema, when password is additioUpdateContextToChecknally hashed with
// user-specific salt.
// Second schema is required to allow password syncing across devices for
// supervised users.
class SupervisedUserAuthentication {
 public:
  enum Schema {
    SCHEMA_PLAIN = 1,
    SCHEMA_SALT_HASHED = 2
  };

  enum SupervisedUserPasswordChangeResult {
    PASSWORD_CHANGED_IN_MANAGER_SESSION = 0,
    PASSWORD_CHANGED_IN_USER_SESSION = 1,
    PASSWORD_CHANGE_FAILED_NO_SIGNATURE_KEY = 3,
    PASSWORD_CHANGE_FAILED_NO_PASSWORD_DATA = 4,
    PASSWORD_CHANGE_FAILED_LOADING_DATA = 6,
    PASSWORD_CHANGE_FAILED_INCOMPLETE_DATA = 7,
    PASSWORD_CHANGE_FAILED_AUTHENTICATION_FAILURE = 8,
    PASSWORD_CHANGE_FAILED_STORE_DATA = 9,
    PASSWORD_CHANGE_RESULT_MAX_VALUE = 10
  };

  typedef base::Callback<void(const base::DictionaryValue* password_data)>
      PasswordDataCallback;

  explicit SupervisedUserAuthentication(SupervisedUserManager* owner);
  virtual ~SupervisedUserAuthentication();

  // Returns current schema for whole ChromeOS. It defines if users with older
  // schema should be migrated somehow.
  Schema GetStableSchema();

  // Transforms key according to schema specified in Local State.
  UserContext TransformKey(const UserContext& context);

  // Fills |password_data| with |password|-specific data for |user_id|,
  // depending on target schema. Does not affect Local State.
  bool FillDataForNewUser(const std::string& user_id,
                          const std::string& password,
                          base::DictionaryValue* password_data,
                          base::DictionaryValue* extra_data);

  // Stores |password_data| for |user_id| in Local State. Only public parts
  // of |password_data| will be stored.
  void StorePasswordData(const std::string& user_id,
                         const base::DictionaryValue& password_data);

  bool NeedPasswordChange(const std::string& user_id,
                          const base::DictionaryValue* password_data);

  // Checks if given user should update password upon signin.
  bool HasScheduledPasswordUpdate(const std::string& user_id);
  void ClearScheduledPasswordUpdate(const std::string& user_id);

  // Checks if password was migrated to new schema by supervised user.
  // In this case it does not have encryption key, and should be updated by
  // manager even if password versions match.
  bool HasIncompleteKey(const std::string& user_id);
  void MarkKeyIncomplete(const std::string& user_id, bool incomplete);

  // Loads password data stored by ScheduleSupervisedPasswordChange.
  void LoadPasswordUpdateData(const std::string& user_id,
                              const PasswordDataCallback& success_callback,
                              const base::Closure& failure_callback);

  // Called by supervised user to store password data for migration upon signin.
  void ScheduleSupervisedPasswordChange(
      const std::string& supervised_user_id,
      const base::DictionaryValue* password_data);

  // Utility method that gets schema version for |user_id| from Local State.
  Schema GetPasswordSchema(const std::string& user_id);

  static std::string BuildPasswordSignature(
      const std::string& password,
      int revision,
      const std::string& base64_signature_key);

 private:
  SupervisedUserManager* owner_;

  // Target schema version. Affects migration process and new user creation.
  Schema stable_schema_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserAuthentication);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_AUTHENTICATION_H_
