// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_H_
#define CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/kerberos/kerberos_files_handler.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "net/base/backoff_entry.h"

class PrefRegistrySimple;
class PrefService;
class PrefChangeRegistrar;
class Profile;

namespace chromeos {
class VariableExpander;
}

namespace policy {
class PolicyMap;
}  // namespace policy

namespace ash {

class KerberosAddAccountRunner;

class KerberosCredentialsManager : public KeyedService,
                                   public policy::PolicyService::Observer {
 public:
  using ResultCallback = base::OnceCallback<void(kerberos::ErrorType)>;
  using ListAccountsCallback =
      base::OnceCallback<void(const kerberos::ListAccountsResponse&)>;
  using ValidateConfigCallback =
      base::OnceCallback<void(const kerberos::ValidateConfigResponse&)>;

  class Observer : public base::CheckedObserver {
   public:
    Observer();

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    ~Observer() override;

    // Called when the set of accounts was changed through Kerberos credentials
    // manager.
    virtual void OnAccountsChanged() {}

    // Called when Kerberos enabled/disabled state changes. The new state is
    // available via IsKerberosEnabled().
    virtual void OnKerberosEnabledStateChanged() {}
  };

  // Maximum number of managed accounts addition retries per prefs change.
  static constexpr int kMaxFailureCountForManagedAccounts = 10;

  KerberosCredentialsManager(PrefService* local_state,
                             Profile* primary_profile);

  KerberosCredentialsManager(const KerberosCredentialsManager&) = delete;
  KerberosCredentialsManager& operator=(const KerberosCredentialsManager&) =
      delete;

  ~KerberosCredentialsManager() override;

  // Registers prefs stored in local state.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers prefs stored in user profiles.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Helper method for ignoring the results of method calls.
  static ResultCallback EmptyResultCallback();

  // Returns the default Kerberos configuration (krb5.conf).
  static const char* GetDefaultKerberosConfig();

  // Returns true if the Kerberos feature is enabled.
  bool IsKerberosEnabled() const;

  // PolicyService:
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // PolicyService:
  void OnPolicyServiceInitialized(policy::PolicyDomain domain) override;

  // Start observing this object. |observer| is not owned.
  void AddObserver(Observer* observer);

  // Stop observing this object. |observer| is not owned.
  void RemoveObserver(Observer* observer);

  // Adds an account for the given |principal_name| (user@example.com). If
  // |is_managed| is true, the account is assumed to be managed by an admin and
  // it overwrites any existing unmanaged account. If |password| is given,
  // authenticates the account. If |remember_password| is true and a |password|
  // is given, the Kerberos daemon remembers the password. |krb5_conf| is set as
  // configuration. On success, sets |principal_name| as active principal and
  // calls OnAccountsChanged() for observers. If |allow_existing| is false and
  // an account for |principal_name| already exists, no action is performed and
  // the method returns with ERROR_DUPLICATE_PRINCIPAL_NAME. If true, the
  // existing account is updated.
  void AddAccountAndAuthenticate(std::string principal_name,
                                 bool is_managed,
                                 const std::optional<std::string>& password,
                                 bool remember_password,
                                 const std::string& krb5_conf,
                                 bool allow_existing,
                                 ResultCallback callback);

  // Removes the Kerberos account for the account with given |principal_name|.
  // On success, calls OnAccountsChanged() for observers.
  void RemoveAccount(std::string principal_name, ResultCallback callback);

  // Removes all Kerberos accounts.
  // On success, calls OnAccountsChanged() for observers.
  void ClearAccounts(ResultCallback callback);

  // Returns a list of all existing accounts, including current status like
  // remaining Kerberos ticket lifetime.
  void ListAccounts(ListAccountsCallback callback);

  // Sets the contents of the Kerberos configuration (krb5.conf) to |krb5_conf|
  // for the account with given |principal_name|.
  void SetConfig(std::string principal_name,
                 const std::string& krb5_conf,
                 ResultCallback callback);

  // Verifies that only allowlisted configuration options are used in the
  // Kerberos configuration |krb5_conf|. The Kerberos daemon does not allow all
  // options for security reasons. Also performs basic syntax checks. Returns
  // useful error information.
  void ValidateConfig(const std::string& krb5_conf,
                      ValidateConfigCallback callback);

  // Sets the currently active account.
  kerberos::ErrorType SetActiveAccount(std::string principal_name);

  // Returns the currently active account or an empty string if there is none.
  const std::string& GetActiveAccount() const {
    return GetActivePrincipalName();
  }

  // Getter for the GetKerberosFiles() callback, used on tests to build a mock
  // KerberosFilesHandler.
  base::RepeatingClosure GetGetKerberosFilesCallbackForTesting();

  // Used on tests to replace the KerberosFilesHandler created on the
  // constructor with a mock KerberosFilesHandler.
  void SetKerberosFilesHandlerForTesting(
      std::unique_ptr<KerberosFilesHandler> kerberos_files_handler);

  // Used on tests to optionally set a callback that will be called after adding
  // a managed account.
  void SetAddManagedAccountCallbackForTesting(
      base::RepeatingCallback<void(kerberos::ErrorType)> callback);

 private:
  friend class KerberosAddAccountRunner;
  using RepeatedAccountField =
      google::protobuf::RepeatedPtrField<kerberos::Account>;

  // Callback on KerberosAddAccountRunner::Done.
  void OnAddAccountRunnerDone(KerberosAddAccountRunner* runner,
                              std::string principal_name,
                              bool is_managed,
                              ResultCallback callback,
                              kerberos::ErrorType error);

  // Callback for KerberosAddAccountRunner when adding a managed account.
  void OnAddManagedAccountRunnerDone(kerberos::ErrorType error);

  // Callback for RemoveAccount().
  void OnRemoveAccount(const std::string& principal_name,
                       ResultCallback callback,
                       const kerberos::RemoveAccountResponse& response);

  // Callback for ClearAccounts().
  void OnClearAccounts(kerberos::ClearMode mode,
                       ResultCallback callback,
                       const kerberos::ClearAccountsResponse& response);

  // Callback for RemoveAccount().
  void OnListAccounts(ListAccountsCallback callback,
                      const kerberos::ListAccountsResponse& response);

  // Callback for SetConfig().
  void OnSetConfig(ResultCallback callback,
                   const kerberos::SetConfigResponse& response);

  // Callback for ValidateConfig().
  void OnValidateConfig(ValidateConfigCallback callback,
                        const kerberos::ValidateConfigResponse& response);

  // Calls KerberosClient::GetKerberosFiles().
  void GetKerberosFiles();

  // Callback for GetKerberosFiles().
  void OnGetKerberosFiles(const std::string& principal_name,
                          const kerberos::GetKerberosFilesResponse& response);

  // Callback for 'KerberosFilesChanged' D-Bus signal sent by kerberosd.
  void OnKerberosFilesChanged(const std::string& principal_name);

  // Callback for 'KerberosTicketExpiring' D-Bus signal sent by kerberosd.
  void OnKerberosTicketExpiring(const std::string& principal_name);

  // Calls OnAccountsChanged() on all observers.
  void NotifyAccountsChanged();

  // Calls OnKerberosEnabledStateChanged() on all observers.
  void NotifyEnabledStateChanged();

  // Accessors for active principal (stored in user pref).
  const std::string& GetActivePrincipalName() const;
  void SetActivePrincipalName(const std::string& principal_name);
  void ClearActivePrincipalName();

  // Checks whether the active principal is contained in the given |accounts|.
  // If not, resets it to the first principal or clears it if the list is empty.
  // It's expected to trigger if the active account is removed by
  // |RemoveAccount()| or |ClearAccounts()|.
  void ValidateActivePrincipal(const RepeatedAccountField& accounts);

  // Notification shown when the Kerberos ticket is about to expire.
  void ShowTicketExpiryNotification();

  // Pref change handlers.
  void UpdateEnabledFromPref();
  void UpdateRememberPasswordEnabledFromPref();
  void UpdateAddAccountsAllowedFromPref();
  void UpdateAccountsFromPref(bool is_retry);

  // Does the main work for UpdateAccountsFromPref(). To clean up stale managed
  // accounts, an up-to-date accounts list is needed. UpdateAccountsFromPref()
  // first gets a list of accounts (asynchronously) and calls into this method
  // to set new accounts and clean up old ones.
  void RemoveAllManagedAccountsExcept(std::vector<std::string> keep_list);

  // Informs session manager whether it needs to store the login password in the
  // kernel keyring. That's the case when '${PASSWORD}' is used as password in
  // the KerberosAccounts policy. The Kerberos daemon expands that to the login
  // password.
  void NotifyRequiresLoginPassword(bool requires_login_password);

  // Called when the user clicks on the ticket expiry notification.
  void OnTicketExpiryNotificationClick(const std::string& principal_name);

  // Local state prefs, not owned.
  raw_ptr<PrefService> local_state_ = nullptr;

  // Primary profile, not owned.
  raw_ptr<Profile> primary_profile_ = nullptr;

  // Policy service of the primary profile, not owned.
  raw_ptr<policy::PolicyService> policy_service_ = nullptr;

  // Called by OnSignalConnected(), puts Kerberos files where GSSAPI finds them.
  std::unique_ptr<KerberosFilesHandler> kerberos_files_handler_;

  // Observer for Kerberos-related prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Subscriptions whose destruction will cancel the corresponding callbacks.
  // The callbacks are used to listen to signals from KerberosClient.
  base::CallbackListSubscription kerberos_file_changed_signal_subscription_;
  base::CallbackListSubscription kerberos_ticket_expiring_signal_subscription_;

  // Keeps track of accounts currently being added.
  std::vector<std::unique_ptr<KerberosAddAccountRunner>> add_account_runners_;

  // Variable expander for the principal name (replaces ${LOGIN_ID} etc.).
  std::unique_ptr<chromeos::VariableExpander> principal_expander_;

  // List of objects that observe this instance.
  base::ObserverList<Observer, true /* check_empty */> observers_;

  // Backoff entry used to control managed accounts addition retries.
  net::BackoffEntry backoff_entry_for_managed_accounts_;

  // Timer for keeping track of managed accounts addition retries.
  base::OneShotTimer managed_accounts_retry_timer_;

  // Callback optionally used for testing.
  base::RepeatingCallback<void(kerberos::ErrorType)>
      add_managed_account_callback_for_testing_;

  base::WeakPtrFactory<KerberosCredentialsManager> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_H_
