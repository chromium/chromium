// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"

#include <vector>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/kerberos/data_pipe_utils.h"
#include "chrome/browser/ash/kerberos/kerberos_ticket_expiry_notification.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_client.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_service.pb.h"
#include "chromeos/components/onc/variable_expander.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/kerberos/dbus-constants.h"

namespace ash {

namespace {

// Account keys for the kerberos.accounts pref.
constexpr char kPrincipal[] = "principal";
constexpr char kPassword[] = "password";
constexpr char kRememberPasswordFromPolicy[] = "remember_password_from_policy";
constexpr char kKrb5Conf[] = "krb5conf";

// Principal placeholders for the KerberosAccounts policy.
constexpr char kLoginId[] = "LOGIN_ID";
constexpr char kLoginEmail[] = "LOGIN_EMAIL";

// Password placeholder.
constexpr char kLoginPasswordPlaceholder[] = "${PASSWORD}";

// Default config with strong encryption.
constexpr char kDefaultKerberosConfig[] = R"([libdefaults]
  default_tgs_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
  default_tkt_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
  permitted_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
  forwardable = true
)";

// Backoff policy used to control managed accounts addition retries.
const net::BackoffEntry::Policy kBackoffPolicyForManagedAccounts = {
    0,               // Number of initial errors to ignore without backoff.
    1 * 1000,        // Initial delay for backoff in ms: 1 second.
    2,               // Factor to multiply for exponential backoff.
    0,               // Fuzzing percentage.
    10 * 60 * 1000,  // Maximum time to delay requests in ms: 10 minutes.
    -1,              // Don't discard entry even if unused.
    false            // Don't use initial delay unless the last was an error.
};

// If |principal_name| is "UsEr@realm.com", sets |principal_name| to
// "user@REALM.COM". Returns false if the given name has no @ or one of the
// parts is empty.
bool NormalizePrincipal(std::string* principal_name) {
  std::vector<std::string> parts = base::SplitString(
      *principal_name, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2 || parts.at(0).empty() || parts.at(1).empty())
    return false;
  *principal_name =
      base::ToLowerASCII(parts[0]) + "@" + base::ToUpperASCII(parts[1]);
  return true;
}

// Tries to normalize |principal_name|. Runs |callback| with
// |ERROR_PARSE_PRINCIPAL_FAILED| if not possible.
bool NormalizePrincipalOrPostCallback(
    std::string* principal_name,
    KerberosCredentialsManager::ResultCallback* callback) {
  if (NormalizePrincipal(principal_name))
    return true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*callback),
                                kerberos::ERROR_PARSE_PRINCIPAL_FAILED));
  return false;
}

// Logs an error if |error| is not |ERROR_NONE|.
void LogError(const char* function_name, kerberos::ErrorType error) {
  LOG_IF(ERROR, error != kerberos::ERROR_NONE)
      << function_name << " failed with error code " << error;
}

// Returns true if |error| is |ERROR_NONE|.
bool Succeeded(kerberos::ErrorType error) {
  return error == kerberos::ERROR_NONE;
}

bool ShouldRetry(kerberos::ErrorType error) {
  // The error types that should trigger a managed accounts addition retry.
  // ERROR_UNKNOWN_KRB5_ERROR will cover unknown temporary issues preventing the
  // device from adding a ticket, such as "resource temporarily unavailable".
  return error == kerberos::ERROR_NETWORK_PROBLEM ||
         error == kerberos::ERROR_CONTACTING_KDC_FAILED ||
         error == kerberos::ERROR_IN_PROGRESS ||
         error == kerberos::ERROR_UNKNOWN_KRB5_ERROR;
}

}  // namespace

// Encapsulates the steps to add a Kerberos account. Overview of the flow:
// - Call the daemon's AddAccount. Ignores duplicate account errors if
//   |allow_existing| is true.
// - Call daemon's SetConfig.
// - If |password| is set, call daemon's AcquireKerberosTgt.
// - Call manager's OnAddAccountRunnerDone.
// If an error happens on any step, removes account if it was newly added and
// not managed by policy and calls OnAddAccountRunnerDone with the error.
class KerberosAddAccountRunner {
 public:
  // Kicks off the flow to add (or re-authenticate) a Kerberos account.
  // |manager| is a non-owned pointer to the owning manager.
  // |normalized_principal| is the normalized user principal name, e.g.
  //   user@REALM.COM.
  // |is_managed| is true for accounts set by admins via policy.
  // |password| is the password of the account. If it matches "${PASSWORD}" and
  //   the account is managed, the login password is used.
  // If |remember_password| is true, the password is remembered by the daemon.
  //   The flag has effect when the login password is used.
  // |krb5_conf| is set as configuration.
  // If |allow_existing| is false and an account for |principal_name| already
  //   exists, no action is performed and the method returns with
  //   ERROR_DUPLICATE_PRINCIPAL_NAME. If true, the existing account is updated.
  // |callback| is called by OnAddAccountRunnerDone() at the end of the flow,
  //   see class description.
  KerberosAddAccountRunner(KerberosCredentialsManager* manager,
                           std::string normalized_principal,
                           bool is_managed,
                           const std::optional<std::string>& password,
                           bool remember_password,
                           const std::string& krb5_conf,
                           bool allow_existing,
                           KerberosCredentialsManager::ResultCallback callback)
      : manager_(manager),
        normalized_principal_(normalized_principal),
        is_managed_(is_managed),
        password_(password),
        remember_password_(remember_password),
        krb5_conf_(krb5_conf),
        allow_existing_(allow_existing),
        callback_(std::move(callback)) {
    AddAccount();
  }

  KerberosAddAccountRunner(const KerberosAddAccountRunner&) = delete;
  KerberosAddAccountRunner& operator=(const KerberosAddAccountRunner&) = delete;

 private:
  // Adds the |normalized_principal_| account to the Kerberos daemon.
  void AddAccount() {
    kerberos::AddAccountRequest request;
    request.set_principal_name(normalized_principal_);
    request.set_is_managed(is_managed_);
    KerberosClient::Get()->AddAccount(
        request, base::BindOnce(&KerberosAddAccountRunner::OnAddAccount,
                                weak_factory_.GetWeakPtr()));
  }

  // Forwards to SetConfig() if there was no error (other than a managed account
  // overwriting an existing one, which is handled transparently). Calls Done()
  // on error.
  void OnAddAccount(const kerberos::AddAccountResponse& response) {
    is_new_account_ = response.error() == kerberos::ERROR_NONE;
    const bool is_existing_account =
        response.error() == kerberos::ERROR_DUPLICATE_PRINCIPAL_NAME;

    if (is_new_account_ || (is_existing_account && allow_existing_)) {
      SetConfig();
      return;
    }

    // Error.
    Done(response.error());
  }

  // Set the Kerberos configuration.
  void SetConfig() {
    kerberos::SetConfigRequest request;
    request.set_principal_name(normalized_principal_);
    request.set_krb5conf(krb5_conf_);
    KerberosClient::Get()->SetConfig(
        request, base::BindOnce(&KerberosAddAccountRunner::OnSetConfig,
                                weak_factory_.GetWeakPtr()));
  }

  // Calls MaybeAcquireKerberosTgt() if no error occurred or Done() otherwise.
  void OnSetConfig(const kerberos::SetConfigResponse& response) {
    if (response.error() == kerberos::ERROR_NONE) {
      MaybeAcquireKerberosTgt();
      return;
    }

    // Error.
    Done(response.error());
  }

  // Authenticates |normalized_principal_| using |password_| if |password_| is
  // set. Otherwise, continues with Done(). If |password_| is "${PASSWORD}" and
  // the account is managed, the login password is used.
  void MaybeAcquireKerberosTgt() {
    if (!password_) {
      Done(kerberos::ERROR_NONE);
      return;
    }

    kerberos::AcquireKerberosTgtRequest request;
    request.set_principal_name(normalized_principal_);
    request.set_remember_password(remember_password_);
    request.set_use_login_password(is_managed_ &&
                                   *password_ == kLoginPasswordPlaceholder);
    KerberosClient::Get()->AcquireKerberosTgt(
        request, data_pipe_utils::GetDataReadPipe(*password_).get(),
        base::BindOnce(&KerberosAddAccountRunner::OnAcquireKerberosTgt,
                       weak_factory_.GetWeakPtr()));
    password_.reset();
  }

  // Forwards to Done().
  void OnAcquireKerberosTgt(
      const kerberos::AcquireKerberosTgtResponse& response) {
    // We're ready.
    Done(response.error());
  }

  // Calls back into |manager_|'s OnAddAccountRunnerDone().
  void Done(kerberos::ErrorType error) {
    // Remove new, unmanaged accounts on error. Keep new, managed accounts on
    // error for admin visibility.
    if (error != kerberos::ERROR_NONE && is_new_account_ && !is_managed_) {
      // Do a best effort cleaning up the account we added before.
      kerberos::RemoveAccountRequest request;
      request.set_principal_name(normalized_principal_);
      KerberosClient::Get()->RemoveAccount(
          request, base::BindOnce(&KerberosAddAccountRunner::OnRemoveAccount,
                                  weak_factory_.GetWeakPtr(), error));
    } else {
      // We're done. This call will delete us!
      manager_->OnAddAccountRunnerDone(this, std::move(normalized_principal_),
                                       is_managed_, std::move(callback_),
                                       error);
    }
  }

  // Prints out a warning if |removal_error| is an error case and forwards
  // |original_error| to Done().
  void OnRemoveAccount(kerberos::ErrorType original_error,
                       const kerberos::RemoveAccountResponse& response) {
    if (response.error() != kerberos::ERROR_NONE) {
      LOG(WARNING) << "Failed to remove Kerberos account for "
                   << normalized_principal_;
    }

    // We're done. This call will delete us! Note that we're passing the
    // |original_error| here, not the |response.error()|.
    manager_->OnAddAccountRunnerDone(this, std::move(normalized_principal_),
                                     is_managed_, std::move(callback_),
                                     original_error);
  }

  // Pointer to the owning manager, not owned.
  const raw_ptr<KerberosCredentialsManager> manager_ = nullptr;
  std::string normalized_principal_;
  bool is_managed_ = false;
  std::optional<std::string> password_;
  bool remember_password_ = false;
  std::string krb5_conf_;
  bool allow_existing_ = false;
  KerberosCredentialsManager::ResultCallback callback_;

  // Whether the account was newly added.
  bool is_new_account_ = false;

  base::WeakPtrFactory<KerberosAddAccountRunner> weak_factory_{this};
};

KerberosCredentialsManager::Observer::Observer() = default;

KerberosCredentialsManager::Observer::~Observer() = default;

KerberosCredentialsManager::KerberosCredentialsManager(PrefService* local_state,
                                                       Profile* primary_profile)
    : local_state_(local_state),
      primary_profile_(primary_profile),
      kerberos_files_handler_(std::make_unique<KerberosFilesHandler>(
          base::BindRepeating(&KerberosCredentialsManager::GetKerberosFiles,
                              base::Unretained(this)))),
      backoff_entry_for_managed_accounts_(&kBackoffPolicyForManagedAccounts) {
  DCHECK(primary_profile_);
  const user_manager::User* primary_user =
      ProfileHelper::Get()->GetUserByProfile(primary_profile);
  DCHECK(primary_user);

  // Set up expansions:
  //   '${LOGIN_ID}'    -> 'user'
  //   '${LOGIN_EMAIL}' -> 'user@EXAMPLE.COM'
  base::flat_map<std::string, std::string> substitutions;
  substitutions[kLoginId] =
      primary_user->GetAccountName(false /* use_display_email */);
  substitutions[kLoginEmail] = primary_user->GetAccountId().GetUserEmail();
  principal_expander_ =
      std::make_unique<chromeos::VariableExpander>(substitutions);

  // Connect to a signal that indicates when Kerberos files change.
  kerberos_file_changed_signal_subscription_ =
      KerberosClient::Get()->SubscribeToKerberosFileChangedSignal(
          base::BindRepeating(
              &KerberosCredentialsManager::OnKerberosFilesChanged,
              weak_factory_.GetWeakPtr()));

  // Connect to a signal that indicates when a Kerberos ticket is about to
  // expire.
  kerberos_ticket_expiring_signal_subscription_ =
      KerberosClient::Get()->SubscribeToKerberosTicketExpiringSignal(
          base::BindRepeating(
              &KerberosCredentialsManager::OnKerberosTicketExpiring,
              weak_factory_.GetWeakPtr()));

  // Listen to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(local_state);
  pref_change_registrar_->Add(
      prefs::kKerberosEnabled,
      base::BindRepeating(&KerberosCredentialsManager::UpdateEnabledFromPref,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_->Add(
      prefs::kKerberosRememberPasswordEnabled,
      base::BindRepeating(
          &KerberosCredentialsManager::UpdateRememberPasswordEnabledFromPref,
          weak_factory_.GetWeakPtr()));
  pref_change_registrar_->Add(
      prefs::kKerberosAddAccountsAllowed,
      base::BindRepeating(
          &KerberosCredentialsManager::UpdateAddAccountsAllowedFromPref,
          weak_factory_.GetWeakPtr()));
  pref_change_registrar_->Add(
      prefs::kKerberosAccounts,
      base::BindRepeating(&KerberosCredentialsManager::UpdateAccountsFromPref,
                          weak_factory_.GetWeakPtr(), false /* is_retry */));

  // Update accounts if policy is already available or start observing.
  policy_service_ =
      primary_profile->GetProfilePolicyConnector()->policy_service();
  const bool policy_initialized =
      policy_service_->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME);
  VLOG(1) << "Policy service initialized at startup: " << policy_initialized;
  if (policy_initialized)
    UpdateAccountsFromPref(false /* is_retry */);
  else
    policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  // Get Kerberos files if there is an active principal. This also wakes up the
  // daemon, which is important as it starts background renewal processes.
  if (!GetActivePrincipalName().empty()) {
    VLOG(1) << "Waking up Kerberos (the daemon, not the 3-headed dog) and "
               "refreshing credentials.";
    GetKerberosFiles();
  }
}

KerberosCredentialsManager::~KerberosCredentialsManager() {
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
}

// static
void KerberosCredentialsManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kKerberosEnabled, false);
  registry->RegisterBooleanPref(prefs::kKerberosRememberPasswordEnabled, true);
  registry->RegisterBooleanPref(prefs::kKerberosAddAccountsAllowed, true);
  registry->RegisterListPref(prefs::kKerberosAccounts);
  registry->RegisterStringPref(prefs::kKerberosDomainAutocomplete, "");
  registry->RegisterBooleanPref(prefs::kKerberosUseCustomPrefilledConfig,
                                false);
  registry->RegisterStringPref(prefs::kKerberosCustomPrefilledConfig, "");
}

void KerberosCredentialsManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kKerberosActivePrincipalName,
                               std::string());
}

// static
KerberosCredentialsManager::ResultCallback
KerberosCredentialsManager::EmptyResultCallback() {
  return base::BindOnce([](kerberos::ErrorType error) {
    // Do nothing.
  });
}

// static
const char* KerberosCredentialsManager::GetDefaultKerberosConfig() {
  return kDefaultKerberosConfig;
}

bool KerberosCredentialsManager::IsKerberosEnabled() const {
  return local_state_->GetBoolean(prefs::kKerberosEnabled);
}

void KerberosCredentialsManager::OnPolicyUpdated(
    const policy::PolicyNamespace& ns,
    const policy::PolicyMap& previous,
    const policy::PolicyMap& current) {
  // Ignore this call. Policy changes are already observed by the registrar.
}

void KerberosCredentialsManager::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  DCHECK(domain == policy::POLICY_DOMAIN_CHROME);

  if (policy_service_->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME)) {
    VLOG(1) << "Policy service initialized";
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
    UpdateAccountsFromPref(false /* is_retry */);
  }
}

void KerberosCredentialsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void KerberosCredentialsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void KerberosCredentialsManager::AddAccountAndAuthenticate(
    std::string principal_name,
    bool is_managed,
    const std::optional<std::string>& password,
    bool remember_password,
    const std::string& krb5_conf,
    bool allow_existing,
    ResultCallback callback) {
  if (!NormalizePrincipalOrPostCallback(&principal_name, &callback))
    return;

  add_account_runners_.push_back(std::make_unique<KerberosAddAccountRunner>(
      this, principal_name, is_managed, password, remember_password, krb5_conf,
      allow_existing, std::move(callback)));
  // The runner starts automatically and calls OnAddAccountRunnerDone when it's
  // done.
}

void KerberosCredentialsManager::OnAddAccountRunnerDone(
    KerberosAddAccountRunner* runner,
    std::string updated_principal,
    bool is_managed,
    ResultCallback callback,
    kerberos::ErrorType error) {
  // Reset the |runner|. Note that |updated_principal| is passed by value,
  // not by reference, since |runner| owns the reference.
  auto it = base::ranges::find(add_account_runners_, runner,
                               &std::unique_ptr<KerberosAddAccountRunner>::get);

  // Semantically, this `CHECK()` should never trigger. However, it protects
  // the `erase()` call from semantically incorrect changes to this class.
  CHECK(it != add_account_runners_.end());
  add_account_runners_.erase(it);

  LogError("AddAccountAndAuthenticate", error);

  if (Succeeded(error)) {
    // Set active account. Be sure not to wipe user selection if the account was
    // added automatically by policy.
    // TODO(b/259178114): Wait until the files have been saved. This is
    // important when this code is triggered directly through a page that
    // requires Kerberos auth.
    if (!is_managed || GetActivePrincipalName().empty()) {
      SetActivePrincipalName(updated_principal);
    } else if (GetActivePrincipalName() == updated_principal) {
      GetKerberosFiles();
    }
  }

  // Bring the merry news to the observers, but only if there is no outstanding
  // query, so we don't spam observers. We want to notify observers even if the
  // additions result in error, because the account might actually have been
  // added, in case of a managed account.
  if (add_account_runners_.empty())
    NotifyAccountsChanged();

  if (is_managed) {
    OnAddManagedAccountRunnerDone(error);
  }

  std::move(callback).Run(error);
}

void KerberosCredentialsManager::OnAddManagedAccountRunnerDone(
    kerberos::ErrorType error) {
  if (!managed_accounts_retry_timer_.IsRunning() && ShouldRetry(error)) {
    backoff_entry_for_managed_accounts_.InformOfRequest(false);

    if (backoff_entry_for_managed_accounts_.failure_count() <
        kMaxFailureCountForManagedAccounts) {
      managed_accounts_retry_timer_.Start(
          FROM_HERE, backoff_entry_for_managed_accounts_.GetTimeUntilRelease(),
          base::BindOnce(&KerberosCredentialsManager::UpdateAccountsFromPref,
                         weak_factory_.GetWeakPtr(), true /* is_retry */));
    }
  }

  if (add_managed_account_callback_for_testing_) {
    add_managed_account_callback_for_testing_.Run(error);
  }
}

void KerberosCredentialsManager::RemoveAccount(std::string principal_name,
                                               ResultCallback callback) {
  if (!NormalizePrincipalOrPostCallback(&principal_name, &callback))
    return;

  kerberos::RemoveAccountRequest request;
  request.set_principal_name(principal_name);
  KerberosClient::Get()->RemoveAccount(
      request, base::BindOnce(&KerberosCredentialsManager::OnRemoveAccount,
                              weak_factory_.GetWeakPtr(), principal_name,
                              std::move(callback)));
}

void KerberosCredentialsManager::OnRemoveAccount(
    const std::string& principal_name,
    ResultCallback callback,
    const kerberos::RemoveAccountResponse& response) {
  LogError("RemoveAccount", response.error());
  if (Succeeded(response.error())) {
    // Reassign active principal if it got deleted.
    if (GetActivePrincipalName() == principal_name)
      ValidateActivePrincipal(response.accounts());

    // Express our condolence to the observers.
    NotifyAccountsChanged();
  }

  std::move(callback).Run(response.error());
}

void KerberosCredentialsManager::ClearAccounts(ResultCallback callback) {
  kerberos::ClearAccountsRequest request;
  request.set_mode(kerberos::CLEAR_ALL);
  KerberosClient::Get()->ClearAccounts(
      request, base::BindOnce(&KerberosCredentialsManager::OnClearAccounts,
                              weak_factory_.GetWeakPtr(), request.mode(),
                              std::move(callback)));
}

void KerberosCredentialsManager::OnClearAccounts(
    kerberos::ClearMode mode,
    ResultCallback callback,
    const kerberos::ClearAccountsResponse& response) {
  LogError("ClearAccounts", response.error());
  if (Succeeded(response.error())) {
    // Depending on the mode, we might have to check if the active principal is
    // still valid.
    if (!GetActivePrincipalName().empty()) {
      switch (mode) {
        case kerberos::CLEAR_ALL:
        case kerberos::CLEAR_ONLY_MANAGED_ACCOUNTS:
        case kerberos::CLEAR_ONLY_UNMANAGED_ACCOUNTS:
          // Check if the active account was wiped and if so, replace it.
          ValidateActivePrincipal(response.accounts());
          break;

        case kerberos::CLEAR_ONLY_UNMANAGED_REMEMBERED_PASSWORDS:
          // We're good, only passwords got wiped, not accounts.
          break;
      }
    }

    // Tattle on the lost accounts to the observers.
    NotifyAccountsChanged();
  }

  std::move(callback).Run(response.error());
}

void KerberosCredentialsManager::ListAccounts(ListAccountsCallback callback) {
  kerberos::ListAccountsRequest request;
  KerberosClient::Get()->ListAccounts(
      request, base::BindOnce(&KerberosCredentialsManager::OnListAccounts,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KerberosCredentialsManager::OnListAccounts(
    ListAccountsCallback callback,
    const kerberos::ListAccountsResponse& response) {
  LogError("ListAccounts", response.error());
  // Lazily validate principal here while we're at it.
  ValidateActivePrincipal(response.accounts());
  std::move(callback).Run(response);
}

kerberos::ErrorType KerberosCredentialsManager::SetActiveAccount(
    std::string principal_name) {
  if (!NormalizePrincipal(&principal_name))
    return kerberos::ERROR_PARSE_PRINCIPAL_FAILED;

  SetActivePrincipalName(principal_name);
  NotifyAccountsChanged();
  return kerberos::ERROR_NONE;
}

void KerberosCredentialsManager::SetConfig(std::string principal_name,
                                           const std::string& krb5_conf,
                                           ResultCallback callback) {
  if (!NormalizePrincipalOrPostCallback(&principal_name, &callback))
    return;

  kerberos::SetConfigRequest request;
  request.set_principal_name(principal_name);
  request.set_krb5conf(krb5_conf);
  KerberosClient::Get()->SetConfig(
      request, base::BindOnce(&KerberosCredentialsManager::OnSetConfig,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KerberosCredentialsManager::OnSetConfig(
    ResultCallback callback,
    const kerberos::SetConfigResponse& response) {
  LogError("SetConfig", response.error());

  if (Succeeded(response.error())) {
    // Yell out to the world that the config changed.
    NotifyAccountsChanged();
  }

  std::move(callback).Run(response.error());
}

void KerberosCredentialsManager::ValidateConfig(
    const std::string& krb5_conf,
    ValidateConfigCallback callback) {
  kerberos::ValidateConfigRequest request;
  request.set_krb5conf(krb5_conf);
  KerberosClient::Get()->ValidateConfig(
      request, base::BindOnce(&KerberosCredentialsManager::OnValidateConfig,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KerberosCredentialsManager::OnValidateConfig(
    ValidateConfigCallback callback,
    const kerberos::ValidateConfigResponse& response) {
  LogError("ValidateConfig", response.error());
  std::move(callback).Run(std::move(response));
}

void KerberosCredentialsManager::GetKerberosFiles() {
  if (GetActivePrincipalName().empty())
    return;

  VLOG(1) << "Refreshing credentials for " << GetActivePrincipalName();

  kerberos::GetKerberosFilesRequest request;
  request.set_principal_name(GetActivePrincipalName());
  KerberosClient::Get()->GetKerberosFiles(
      request,
      base::BindOnce(&KerberosCredentialsManager::OnGetKerberosFiles,
                     weak_factory_.GetWeakPtr(), request.principal_name()));
}

void KerberosCredentialsManager::OnGetKerberosFiles(
    const std::string& principal_name,
    const kerberos::GetKerberosFilesResponse& response) {
  LogError("GetKerberosFiles", response.error());
  if (!Succeeded(response.error()))
    return;

  // Ignore if the principal changed in the meantime.
  if (GetActivePrincipalName() != principal_name) {
    VLOG(1) << "Ignoring Kerberos files. Active principal changed from "
            << principal_name << " to " << GetActivePrincipalName();
    return;
  }

  // In case the credential cache is missing, remove the files. This could
  // happen when switching from an account with ticket to an account without
  // ticket. In that case, the files must go.
  if (response.files().has_krb5cc()) {
    DCHECK(response.files().has_krb5conf());
    kerberos_files_handler_->SetFiles(response.files().krb5cc(),
                                      response.files().krb5conf());
  } else {
    kerberos_files_handler_->DeleteFiles();
  }
}

void KerberosCredentialsManager::OnKerberosFilesChanged(
    const std::string& principal_name) {
  // Only listen to the active account.
  VLOG(1) << "Got KerberosFilesChanged for " << principal_name;
  if (principal_name == GetActivePrincipalName())
    GetKerberosFiles();
}

void KerberosCredentialsManager::OnKerberosTicketExpiring(
    const std::string& principal_name) {
  // Only listen to the active account.
  VLOG(1) << "Got KerberosTicketExpiring for " << principal_name;
  if (principal_name == GetActivePrincipalName()) {
    kerberos_ticket_expiry_notification::Show(
        primary_profile_, GetActivePrincipalName(),
        base::BindRepeating(
            &KerberosCredentialsManager::OnTicketExpiryNotificationClick,
            weak_factory_.GetWeakPtr()));
  }
}

void KerberosCredentialsManager::NotifyAccountsChanged() {
  for (auto& observer : observers_)
    observer.OnAccountsChanged();
}

void KerberosCredentialsManager::NotifyEnabledStateChanged() {
  for (auto& observer : observers_)
    observer.OnKerberosEnabledStateChanged();
}

const std::string& KerberosCredentialsManager::GetActivePrincipalName() const {
  // Using GetValue().GetString() instead of GetString() directly to prevent a
  // string copy.
  return primary_profile_->GetPrefs()
      ->GetValue(prefs::kKerberosActivePrincipalName)
      .GetString();
}

void KerberosCredentialsManager::SetActivePrincipalName(
    const std::string& principal_name) {
  // Don't early out if names are equal, this might be required to bootstrap
  // Kerberos credentials.
  primary_profile_->GetPrefs()->SetString(prefs::kKerberosActivePrincipalName,
                                          principal_name);
  GetKerberosFiles();
}

void KerberosCredentialsManager::ClearActivePrincipalName() {
  primary_profile_->GetPrefs()->ClearPref(prefs::kKerberosActivePrincipalName);
  kerberos_files_handler_->DeleteFiles();
}

void KerberosCredentialsManager::ValidateActivePrincipal(
    const RepeatedAccountField& accounts) {
  const std::string& active_principal = GetActivePrincipalName();
  bool found = false;

  for (const kerberos::Account& account : accounts)
    found |= account.principal_name() == active_principal;

  if (!found) {
    VLOG(1) << "Active principal got removed. Restoring.";
    if (accounts.size() > 0)
      SetActivePrincipalName(accounts.Get(0).principal_name());
    else
      ClearActivePrincipalName();
  }
}

void KerberosCredentialsManager::UpdateEnabledFromPref() {
  if (IsKerberosEnabled()) {
    // Kerberos got enabled, re-populate managed accounts.
    VLOG(1) << "Kerberos got enabled, populating managed accounts";
    UpdateAccountsFromPref(false /* is_retry */);
  } else {
    // Note that ClearAccounts logs an error if the operation fails.
    VLOG(1) << "Kerberos got disabled, clearing accounts";
    ClearAccounts(EmptyResultCallback());
  }

  NotifyEnabledStateChanged();
}

void KerberosCredentialsManager::UpdateRememberPasswordEnabledFromPref() {
  if (local_state_->GetBoolean(prefs::kKerberosRememberPasswordEnabled))
    return;

  VLOG(1) << "'Remember password' got disabled, clearing remembered passwords";
  kerberos::ClearAccountsRequest request;
  request.set_mode(kerberos::CLEAR_ONLY_UNMANAGED_REMEMBERED_PASSWORDS);
  KerberosClient::Get()->ClearAccounts(
      request, base::BindOnce(&KerberosCredentialsManager::OnClearAccounts,
                              weak_factory_.GetWeakPtr(), request.mode(),
                              EmptyResultCallback()));
}

void KerberosCredentialsManager::UpdateAddAccountsAllowedFromPref() {
  if (local_state_->GetBoolean(prefs::kKerberosAddAccountsAllowed))
    return;

  VLOG(1) << "'Add accounts allowed' got disabled, clearing unmanaged accounts";
  kerberos::ClearAccountsRequest request;
  request.set_mode(kerberos::CLEAR_ONLY_UNMANAGED_ACCOUNTS);
  KerberosClient::Get()->ClearAccounts(
      request, base::BindOnce(&KerberosCredentialsManager::OnClearAccounts,
                              weak_factory_.GetWeakPtr(), request.mode(),
                              EmptyResultCallback()));
}

void KerberosCredentialsManager::UpdateAccountsFromPref(bool is_retry) {
  if (is_retry) {
    VLOG(1) << "Retrying to update KerberosAccounts from Prefs";
  } else {
    // Refreshing backoff entry, since this call was triggered by prefs change.
    backoff_entry_for_managed_accounts_.Reset();
  }

  if (!IsKerberosEnabled()) {
    VLOG(1) << "Kerberos disabled";
    NotifyRequiresLoginPassword(false);
    // All managed accounts have already been removed here.
    // No need to call RemoveAllManagedAccountsExcept().
    return;
  }

  const base::Value::List& accounts =
      local_state_->GetList(prefs::kKerberosAccounts);
  if (accounts.empty()) {
    VLOG(1) << "Empty KerberosAccounts policy";
    NotifyRequiresLoginPassword(false);

    // The active principal is empty if there are no accounts, so no need to
    // remove accounts. It would just start up the daemon unnecessarily.
    if (!GetActivePrincipalName().empty())
      RemoveAllManagedAccountsExcept({});
    return;
  }

  VLOG(1) << accounts.size() << " accounts in KerberosAccounts";
  bool requires_login_password = false;
  std::vector<std::string> managed_accounts_added;
  for (const auto& account : accounts) {
    const base::Value::Dict& account_dict = account.GetDict();

    // Get the principal. Should always be set.
    const std::string* principal_string = account_dict.FindString(kPrincipal);
    DCHECK(principal_string);
    std::string principal = *principal_string;
    if (!principal_expander_->ExpandString(&principal)) {
      VLOG(1) << "Failed to expand principal '" << principal << "'";
      continue;
    }
    if (!NormalizePrincipal(&principal)) {
      VLOG(1) << "Ignoring bad principal '" << principal << "'";
      continue;
    }

    // Get the password, defaults to not set.
    const std::string* password_str = account_dict.FindString(kPassword);
    std::optional<std::string> password;
    if (password_str) {
      password = std::move(*password_str);
    }

    // Keep track of whether any account has the '${PASSWORD}' placeholder.
    if (password == kLoginPasswordPlaceholder) {
      requires_login_password = true;
    }

    // Get the "remember password from policy" flag, defaults to true.
    bool remember_password =
        account.GetDict().FindBool(kRememberPasswordFromPolicy).value_or(true);

    // Get Kerberos configuration if given. Otherwise, use default to make sure
    // it overwrites an existing unmanaged account.
    std::string krb5_conf;
    const base::Value* krb5_conf_value = account_dict.Find(kKrb5Conf);
    if (krb5_conf_value) {
      // Note: The config is encoded as a list of lines.
      for (const auto& config_line : krb5_conf_value->GetList()) {
        krb5_conf += config_line.GetString();
        krb5_conf += "\n";
      }
    } else {
      krb5_conf = kDefaultKerberosConfig;
    }

    // By setting allow_existing == true, existing managed accounts are updated
    // and existing unmanaged accounts are overwritten.
    add_account_runners_.push_back(std::make_unique<KerberosAddAccountRunner>(
        this, principal, true /* is_managed */, password, remember_password,
        krb5_conf, true /* allow_existing */, EmptyResultCallback()));
    managed_accounts_added.push_back(principal);
  }

  // Let UserSessionManager know whether it should keep the login password.
  NotifyRequiresLoginPassword(requires_login_password);
  RemoveAllManagedAccountsExcept(std::move(managed_accounts_added));
}

void KerberosCredentialsManager::RemoveAllManagedAccountsExcept(
    std::vector<std::string> keep_list) {
  VLOG(1) << "Clearing out managed accounts except for " << keep_list.size();

  kerberos::ClearAccountsRequest request;
  request.set_mode(kerberos::CLEAR_ONLY_MANAGED_ACCOUNTS);
  for (const std::string& principal_name : keep_list)
    *request.add_principal_names_to_ignore() = principal_name;

  KerberosClient::Get()->ClearAccounts(
      request, base::BindOnce(&KerberosCredentialsManager::OnClearAccounts,
                              weak_factory_.GetWeakPtr(), request.mode(),
                              EmptyResultCallback()));
}

void KerberosCredentialsManager::NotifyRequiresLoginPassword(
    bool requires_login_password) {
  VLOG(1) << "Requires login password: " << requires_login_password;
  UserSessionManager::GetInstance()->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kKerberos,
      requires_login_password);
}

void KerberosCredentialsManager::OnTicketExpiryNotificationClick(
    const std::string& principal_name) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      primary_profile_,
      chromeos::settings::mojom::kKerberosAccountsV2SubpagePath +
          std::string("?kerberos_reauth=") +
          base::EscapeQueryParamValue(principal_name, false /* use_plus */));

  // Close last! |principal_name| is owned by the notification.
  kerberos_ticket_expiry_notification::Close(primary_profile_);
}

base::RepeatingClosure
KerberosCredentialsManager::GetGetKerberosFilesCallbackForTesting() {
  return base::BindRepeating(&KerberosCredentialsManager::GetKerberosFiles,
                             base::Unretained(this));
}

void KerberosCredentialsManager::SetKerberosFilesHandlerForTesting(
    std::unique_ptr<KerberosFilesHandler> kerberos_files_handler) {
  kerberos_files_handler_ = std::move(kerberos_files_handler);
}

void KerberosCredentialsManager::SetAddManagedAccountCallbackForTesting(
    base::RepeatingCallback<void(kerberos::ErrorType)> callback) {
  add_managed_account_callback_for_testing_ = std::move(callback);
}

}  // namespace ash
