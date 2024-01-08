// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_ARC_KEY_PERMISSIONS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_ARC_KEY_PERMISSIONS_MANAGER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"

class Profile;

namespace base {
class Value;
}

namespace policy {
class PolicyChangeRegistrar;
class PolicyService;
}  // namespace policy

namespace ash {
namespace platform_keys {

// ARC key permissions manager delegate (ArcKpmDelegate) instances observes
// changes that affect ARC usage allowance of corporate keys residing on a
// specific token. If an ArcKpmDelegate observes a change in the state of ARC
// usage allowance, it notifies all observers by calling
// OnArcUsageAllowanceForCorporateKeysChanged. ArcKpmDelegates are used by KPMs
// to keep key permissions updated in chaps.
class ArcKpmDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer();
    explicit Observer(const ArcKpmDelegate&) = delete;
    Observer& operator=(const ArcKpmDelegate&) = delete;
    ~Observer() override;

    virtual void OnArcUsageAllowanceForCorporateKeysChanged(bool allowed) = 0;
  };

  ArcKpmDelegate();
  ArcKpmDelegate(const ArcKpmDelegate&) = delete;
  ArcKpmDelegate& operator=(const ArcKpmDelegate&) = delete;
  virtual ~ArcKpmDelegate();

  virtual void Shutdown();

  // Returns true if corporate keys are allowed for ARC usage for the token in
  // question.
  virtual bool AreCorporateKeysAllowedForArcUsage() const = 0;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

 protected:
  virtual void NotifyArcUsageAllowanceForCorporateKeysChanged(bool allowed);

  bool corporate_keys_allowed_for_arc_usage_ = false;
  base::ObserverList<Observer> observer_list_;
};

// A UserPrivateTokenArcKpmDelegate instance observes changes that affect
// ARC usage allowance of corporate keys residing on a specific user token.
// Corporate keys are allowed for ARC usage on a user token if:
// 1- ARC is enabled for this user and
// 2- there exists an ARC app A installed for the user session and
// 3- app A is mentioned in KeyPermissions user policy.
class UserPrivateTokenArcKpmDelegate : public ArcKpmDelegate,
                                       public arc::ArcSessionManagerObserver,
                                       public ArcAppListPrefs::Observer {
 public:
  explicit UserPrivateTokenArcKpmDelegate(Profile* profile);
  UserPrivateTokenArcKpmDelegate(const UserPrivateTokenArcKpmDelegate&) =
      delete;
  UserPrivateTokenArcKpmDelegate& operator=(
      const UserPrivateTokenArcKpmDelegate&) = delete;
  ~UserPrivateTokenArcKpmDelegate() override;

  // ArcKpmDelegate
  bool AreCorporateKeysAllowedForArcUsage() const override;
  void Shutdown() override;

 private:
  void CheckArcKeyAvailibility();

  void SetArcUsageAllowance(bool allowed);

  void OnKeyPermissionsPolicyChanged(const base::Value* old_value,
                                     const base::Value* new_value);

  // arc::ArcSessionManager
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // ArcAppListPrefs::Observer
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;

  raw_ptr<Profile> profile_;
  const bool is_primary_profile_;
  // True if the delegate was shutdown.
  bool is_shutdown_ = false;
  raw_ptr<policy::PolicyService> policy_service_;
  std::unique_ptr<policy::PolicyChangeRegistrar> policy_change_registrar_;
};

// SystemTokenArcKpmDelegate observes changes that affect ARC usage allowance of
// corporate keys residing on the system token.
// ARC usage is allowed for corporate keys residing on the system token if it is
// allowed for corporate keys residing on the primary user's token.
//
// ** ArcKpmDelegate Chaining **
// As mentioned above, SystemTokenArcKpmDelegate depends on the state reported
// by the UserPrivateTokenArcKpmDelegate instance for the primary user. So
// SystemTokenArcKpmDelegate will forward system token KPM queries about ARC
// usage allowance to the primary user UserPrivateTokenArcKpmDelegate instance
// if exists. It will also notify the system token KPM about ARC usage changes
// whenever the primary user UserPrivateTokenArcKpmDelegate instance observes
// changes.
class SystemTokenArcKpmDelegate : public ArcKpmDelegate,
                                  public ArcKpmDelegate::Observer {
 public:
  // Returns a global instance. May return null if not initialized.
  static SystemTokenArcKpmDelegate* Get();

  static void SetSystemTokenArcKpmDelegateForTesting(
      SystemTokenArcKpmDelegate* system_token_arc_kpm_delegate);

  SystemTokenArcKpmDelegate();
  SystemTokenArcKpmDelegate(const SystemTokenArcKpmDelegate&) = delete;
  SystemTokenArcKpmDelegate& operator=(const SystemTokenArcKpmDelegate&) =
      delete;
  ~SystemTokenArcKpmDelegate() override;

  // ArcKpmDelegate
  bool AreCorporateKeysAllowedForArcUsage() const override;

  // Sets the primary user private token delegate to which the system token
  // delegate is chained (Check ArcKpmDelegate Chaining documentation above).
  // Note: This should be called with nullptr before the primary user delegate
  // is destroyed.
  void SetPrimaryUserArcKpmDelegate(
      UserPrivateTokenArcKpmDelegate* primary_user_arc_usage_manager);

  void ClearPrimaryUserArcKpmDelegate();

 private:
  // ArcKpmDelegate::Observer
  void OnArcUsageAllowanceForCorporateKeysChanged(bool allowed) override;

  raw_ptr<UserPrivateTokenArcKpmDelegate> primary_user_arc_usage_manager_ =
      nullptr;
  base::ScopedObservation<ArcKpmDelegate, ArcKpmDelegate::Observer>
      primary_user_arc_usage_manager_delegate_observation_{this};
};

}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_ARC_KEY_PERMISSIONS_MANAGER_DELEGATE_H_
