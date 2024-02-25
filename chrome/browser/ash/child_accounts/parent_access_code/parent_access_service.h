// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_SERVICE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/config_source.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;

namespace ash {

enum class ParentCodeValidationResult;
enum class SupervisedAction;

namespace parent_access {

// Parent access code validation service.
class ParentAccessService {
 public:
  // Observer that gets notified about attempts to validate parent access code.
  class Observer : public base::CheckedObserver {
   public:
    // Called when access code validation was performed. |result| is true if
    // validation finished with a success and false otherwise. |account_id| is
    // forwarded from the ValidateParentAccessCode method that triggered this
    // event, when it is filled it means that the validation happened
    // specifically to the account identified by the parameter.
    virtual void OnAccessCodeValidation(
        ParentCodeValidationResult result,
        std::optional<AccountId> account_id) = 0;
  };

  // Registers preferences.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Gets the service singleton.
  static ParentAccessService& Get();

  ParentAccessService(const ParentAccessService&) = delete;
  ParentAccessService& operator=(const ParentAccessService&) = delete;

  // Checks if the provided |action| requires parental approval to be performed.
  // Requires owner_account_id to be available in the UserManager, so if calling
  // close to startup, ensure owner account is set before calling.
  static bool IsApprovalRequired(SupervisedAction action);

  // Checks if |access_code| is valid for the user identified by |account_id|.
  // When account_id is empty, this method checks if the |access_code| is valid
  // for any child that was added to this device. |validation_time| is the time
  // that will be used to validate the code, it will succeed if the code was
  // valid this given time.
  ParentCodeValidationResult ValidateParentAccessCode(
      const AccountId& account_id,
      const std::string& access_code,
      base::Time validation_time);

  // Reloads config for the provided user.
  void LoadConfigForUser(const user_manager::User* user);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class base::NoDestructor<ParentAccessService>;

  ParentAccessService();
  ~ParentAccessService();

  void NotifyObservers(ParentCodeValidationResult validation_result,
                       const AccountId& account_id);

  // Provides configurations to be used for validation of access codes.
  ConfigSource config_source_;

  base::ObserverList<Observer> observers_;
};

}  // namespace parent_access
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_SERVICE_H_
