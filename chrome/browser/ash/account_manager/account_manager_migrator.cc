// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_migrator.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_factory.h"
#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/account_manager_facade_factory.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/auth/arc_auth_service.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/user_manager/user.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

namespace {

// These names are used in histograms. Values should never be changed.
constexpr char kDeviceAccountMigration[] = "DeviceAccountMigration";
constexpr char kContentAreaAccountsMigration[] = "ContentAreaAccountsMigration";
constexpr char kSuccessStorage[] = "SuccessStorage";
constexpr char kMigrationResultMetricName[] =
    "AccountManager.Migrations.Result";

// Maximum number of times migrations should be run.
constexpr int kMaxMigrationRuns = 1;

::account_manager::AccountKey GetDeviceAccount(const Profile* profile) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  const AccountId& account_id = user->GetAccountId();
  switch (account_id.GetAccountType()) {
    case AccountType::ACTIVE_DIRECTORY:
      return ::account_manager::AccountKey{
          account_id.GetObjGuid(),
          account_manager::AccountType::kActiveDirectory};
    case AccountType::GOOGLE:
      return ::account_manager::AccountKey{account_id.GetGaiaId(),
                                           account_manager::AccountType::kGaia};
    case AccountType::UNKNOWN:
      return ::account_manager::AccountKey{std::string(),
                                           account_manager::AccountType::kGaia};
  }
}

std::string RemoveAccountIdPrefix(const std::string& prefixed_account_id) {
  return prefixed_account_id.substr(10 /* length of "AccountId-" */);
}

// A helper base class for account migration steps that need to read and write
// to Account Manager.
class AccountMigrationBaseStep : public AccountMigrationRunner::Step {
 public:
  AccountMigrationBaseStep(
      const std::string& id,
      AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      signin::IdentityManager* identity_manager)
      : AccountMigrationRunner::Step(id),
        account_manager_(account_manager),
        account_manager_facade_(account_manager_facade),
        identity_manager_(identity_manager) {}
  ~AccountMigrationBaseStep() override = default;

 protected:
  bool IsAccountPresentInAccountManager(
      const ::account_manager::AccountKey& account) const {
    return base::Contains(account_manager_accounts_, account);
  }

  bool IsAccountManagerEmpty() const {
    return account_manager_accounts_.empty();
  }

  void MigrateSecondaryAccount(const std::string& gaia_id,
                               const std::string& email) {
    if (base::Contains(account_manager_accounts_,
                       ::account_manager::AccountKey{
                           gaia_id, account_manager::AccountType::kGaia})) {
      // Do not overwrite any existing account in |AccountManager|.
      VLOG(1) << "Ignoring migration of existing account: " << email;
      return;
    }

    account_manager_->UpsertAccount(
        ::account_manager::AccountKey{gaia_id,
                                      account_manager::AccountType::kGaia},
        email, AccountManager::kInvalidToken);
    VLOG(1) << "Successfully migrated: " << email;
  }

  AccountManager* account_manager() { return account_manager_; }

  account_manager::AccountManagerFacade* account_manager_facade() {
    return account_manager_facade_;
  }

  signin::IdentityManager* identity_manager() { return identity_manager_; }

 private:
  // Implementations should use this to start their migration flow, instead of
  // overriding |Run|.
  virtual void StartMigration() = 0;

  // Overrides |AccountMigrationRunner::Step| and stops further overrides.
  // Subclasses should use |StartMigration| to begin their migration flow and
  // must call either of |Step::FinishWithSuccess| or |Step::FinishWithFailure|
  // when done.
  void Run() final {
    account_manager_facade_->GetAccounts(base::BindOnce(
        &AccountMigrationBaseStep::OnGetAccounts, weak_factory_.GetWeakPtr()));
  }

  void OnGetAccounts(const std::vector<::account_manager::Account>& accounts) {
    account_manager_accounts_.clear();
    account_manager_accounts_.reserve(accounts.size());
    for (const ::account_manager::Account& account : accounts) {
      account_manager_accounts_.emplace_back(account.key);
    }
    StartMigration();
  }

  // A non-owning pointer to Account Manager.
  AccountManager* const account_manager_;
  // A non-owning pointer.
  account_manager::AccountManagerFacade* const account_manager_facade_;

  // Non-owning pointer.
  signin::IdentityManager* const identity_manager_;

  // A temporary cache of accounts in |AccountManager|, guaranteed to be
  // up-to-date when |StartMigration| is called.
  std::vector<::account_manager::AccountKey> account_manager_accounts_;

  base::WeakPtrFactory<AccountMigrationBaseStep> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AccountMigrationBaseStep);
};

// An |AccountMigrationRunner::Step| to migrate the Chrome OS Device Account's
// LST to |AccountManager|.
class DeviceAccountMigration : public AccountMigrationBaseStep,
                               public WebDataServiceConsumer {
 public:
  DeviceAccountMigration(
      const ::account_manager::AccountKey& device_account,
      const std::string& device_account_raw_email,
      AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      signin::IdentityManager* identity_manager,
      scoped_refptr<TokenWebData> token_web_data)
      : AccountMigrationBaseStep(kDeviceAccountMigration,
                                 account_manager,
                                 account_manager_facade,
                                 identity_manager),
        token_web_data_(token_web_data),
        device_account_(device_account),
        device_account_raw_email_(device_account_raw_email) {}
  ~DeviceAccountMigration() override = default;

 private:
  void StartMigration() override {
    if (!IsAccountPresentInAccountManager(device_account_)) {
      MigrateDeviceAccount();
      return;
    }

    account_manager_facade()->GetPersistentErrorForAccount(
        device_account_,
        base::BindOnce(&DeviceAccountMigration::OnGetPersistentErrorForAccount,
                       weak_factory_.GetWeakPtr()));
  }

  void OnGetPersistentErrorForAccount(const GoogleServiceAuthError& error) {
    if (!error.IsPersistentError()) {
      FinishWithSuccess();
      return;
    }

    MigrateDeviceAccount();
  }

  void MigrateDeviceAccount() {
    switch (device_account_.account_type) {
      case account_manager::AccountType::kActiveDirectory:
        MigrateActiveDirectoryAccount();
        break;
      case account_manager::AccountType::kGaia:
        MigrateGaiaAccount();
        break;
    }
  }

  void MigrateActiveDirectoryAccount() {
    // TODO(sinhak): Migrate Active Directory account.
    FinishWithSuccess();
  }

  void MigrateGaiaAccount() { token_web_data_->GetAllTokens(this); }

  // WebDataServiceConsumer override.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!result) {
      LOG(ERROR) << "Could not load the token database. Aborting.";
      FinishWithFailure();
      return;
    }

    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<TokenResult>* token_result =
        static_cast<const WDResult<TokenResult>*>(result.get());

    const std::map<std::string, std::string>& token_map =
        token_result->GetValue().tokens;

    bool is_success = false;
    for (auto it = token_map.begin(); it != token_map.end(); ++it) {
      const std::string token_account_id = RemoveAccountIdPrefix(it->first);
      if (token_account_id.empty()) {
        LOG(ERROR) << "Empty account id loaded from token DB.";
        continue;
      }
      DCHECK(token_account_id.find('@') != std::string::npos)
          << "Expecting an email as the account ID of a token [actual = "
          << token_account_id << "]";

      if (!gaia::AreEmailsSame(device_account_raw_email_, token_account_id))
        continue;

      account_manager()->UpsertAccount(
          device_account_, device_account_raw_email_ /* raw_email */,
          it->second /* token */);
      is_success = true;
      break;
    }

    if (is_success) {
      DVLOG(1) << "Device Account migrated.";
      FinishWithSuccess();
    } else {
      LOG(ERROR) << "Could not find a refresh token for the Device Account.";
      FinishWithFailure();
    }
  }

  // Current storage of LSTs.
  scoped_refptr<TokenWebData> token_web_data_;

  // Device Account on Chrome OS.
  const ::account_manager::AccountKey device_account_;

  // Raw, un-canonicalized email for the device account.
  const std::string device_account_raw_email_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DeviceAccountMigration> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceAccountMigration);
};

// An |AccountMigrationRunner::Step| to migrate the Chrome content area accounts
// to |AccountManager|. The objective is to migrate the account names only. We
// cannot migrate any credentials (cookies).
class ContentAreaAccountsMigration : public AccountMigrationBaseStep,
                                     signin::IdentityManager::Observer {
 public:
  ContentAreaAccountsMigration(
      AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,

      signin::IdentityManager* identity_manager)
      : AccountMigrationBaseStep(kContentAreaAccountsMigration,
                                 account_manager,
                                 account_manager_facade,
                                 identity_manager),
        identity_manager_(identity_manager) {}
  ~ContentAreaAccountsMigration() override {
    identity_manager_->RemoveObserver(this);
  }

 private:
  void StartMigration() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    identity_manager_->AddObserver(this);
    signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
        identity_manager_->GetAccountsInCookieJar();
    if (accounts_in_cookie_jar_info.accounts_are_fresh) {
      OnAccountsInCookieUpdated(
          accounts_in_cookie_jar_info,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    }
  }

  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // We should not have reached here without |OnGetAccounts| having been
    // called and |account_manager_accounts_| empty.
    // Furthermore, Account Manager must have been populated with the Device
    // Account before this |Step| is run.
    DCHECK(!IsAccountManagerEmpty());
    identity_manager_->RemoveObserver(this);

    MigrateAccounts(accounts_in_cookie_jar_info.signed_in_accounts,
                    accounts_in_cookie_jar_info.signed_out_accounts);

    FinishWithSuccess();
  }

  void MigrateAccounts(
      const std::vector<gaia::ListedAccount>& signed_in_content_area_accounts,
      const std::vector<gaia::ListedAccount>&
          signed_out_content_area_accounts) {
    for (const gaia::ListedAccount& account : signed_in_content_area_accounts) {
      MigrateSecondaryAccount(account.gaia_id, account.raw_email);
    }
    for (const gaia::ListedAccount& account :
         signed_out_content_area_accounts) {
      MigrateSecondaryAccount(account.gaia_id, account.raw_email);
    }
  }

  // A non-owning pointer to |IdentityManager|.
  signin::IdentityManager* const identity_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ContentAreaAccountsMigration);
};

// An |AccountMigrationRunner::Step| to migrate ARC accounts to
// |AccountManager|. The objective is to migrate the account names and Gaia ids
// only. We cannot migrate any credentials.
// This is a timed |Step|. Since ARC can fail independently of Chrome, we can be
// potentially waiting forever to get a callback from ARC. If we do not have a
// timeout, this |Step| can make the rest of migration |Step|s wait forever.
class ArcAccountsMigration : public AccountMigrationBaseStep,
                             public arc::ArcSessionManagerObserver {
 public:
  ArcAccountsMigration(
      AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      signin::IdentityManager* identity_manager,
      arc::ArcAuthService* arc_auth_service)
      : AccountMigrationBaseStep(
            AccountManagerMigrator::kArcAccountsMigrationId,
            account_manager,
            account_manager_facade,
            identity_manager),
        arc_auth_service_(arc_auth_service) {}
  ~ArcAccountsMigration() override { Reset(); }

 private:
  void StartMigration() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!arc_auth_service_) {
      // ArcAuthService is not available for Secondary Profiles participating in
      // Multi-Signin. It is started only for the Primary Profile.
      VLOG(1) << "ArcAuthService unavailable. Aborting ARC accounts migration.";

      // It is important to mark this step as a failure so that if/when this
      // Profile signs in as a Primary Profile, this step can be retried.
      // However, skip emitting a failure UMA stat because otherwise we will get
      // false failure stats for all Secondary Profiles using Multi-Signin.
      FinishWithFailure(false /* emit_uma_stats */);
      return;
    }

    timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kStepTimeoutInSeconds),
        base::BindOnce(&ArcAccountsMigration::FinishWithFailure,
                       weak_factory_.GetWeakPtr(), true /* emit_uma_stats */));
    arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
    arc_session_manager->AddObserver(this);
    if (arc_session_manager->state() == arc::ArcSessionManager::State::ACTIVE) {
      OnArcStarted();
    }
  }

  void OnArcStarted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    arc::ArcSessionManager::Get()->RemoveObserver(this);

    DCHECK(arc_auth_service_);
    arc_auth_service_->GetGoogleAccountsInArc(
        base::BindOnce(&ArcAccountsMigration::OnGetGoogleAccountsInArc,
                       weak_factory_.GetWeakPtr()));
  }

  void OnGetGoogleAccountsInArc(
      std::vector<arc::mojom::ArcAccountInfoPtr> arc_accounts) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    timer_.Stop();

    for (const arc::mojom::ArcAccountInfoPtr& arc_account : arc_accounts) {
      MigrateSecondaryAccount(arc_account->gaia_id, arc_account->email);
    }

    FinishWithSuccess();
  }

  void FinishWithSuccess() {
    Reset();
    Step::FinishWithSuccess();
  }

  void FinishWithFailure(bool emit_uma_stats) {
    Reset();
    Step::FinishWithFailure(emit_uma_stats);
  }

  void Reset() {
    timer_.Stop();
    arc::ArcSessionManager::Get()->RemoveObserver(this);
    weak_factory_.InvalidateWeakPtrs();
  }

  // A non-owning pointer to |ArcAuthService|.
  arc::ArcAuthService* const arc_auth_service_;

  base::OneShotTimer timer_;

  // Timeout duration for this |Step|.
  const int64_t kStepTimeoutInSeconds = 60;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ArcAccountsMigration> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcAccountsMigration);
};

// Stores a successful migration run in Prefs.
// This MUST be the last |Step| of the migration run.
// |AccountMigrationRunner::Step|s are run only if the previous
// |AccountMigrationRunner::Step| was successful. The fact that this step is
// being run implies that the "real" migration steps ran successfully.
class SuccessStorage : public AccountMigrationRunner::Step {
 public:
  explicit SuccessStorage(PrefService* pref_service)
      : AccountMigrationRunner::Step(kSuccessStorage),
        pref_service_(pref_service) {}
  ~SuccessStorage() override = default;

  void Run() override {
    const int num_times_ran_successfully = pref_service_->GetInteger(
        ::prefs::kAccountManagerNumTimesMigrationRanSuccessfully);
    pref_service_->SetInteger(
        ::prefs::kAccountManagerNumTimesMigrationRanSuccessfully,
        num_times_ran_successfully + 1);
    FinishWithSuccess();
  }

 private:
  // A non-owning pointer.
  PrefService* const pref_service_;

  DISALLOW_COPY_AND_ASSIGN(SuccessStorage);
};

}  // namespace

// Used in histograms and elsewhere. Never change this value.
// static
const char AccountManagerMigrator::kArcAccountsMigrationId[] =
    "ArcAccountsMigration";

AccountManagerMigrator::AccountManagerMigrator(Profile* profile)
    : profile_(profile) {}

AccountManagerMigrator::~AccountManagerMigrator() = default;

void AccountManagerMigrator::Start() {
  DVLOG(1) << "AccountManagerMigrator::Start";

  if (!IsAccountManagerAvailable(profile_))
    return;

  if (migration_runner_ && (migration_runner_->GetStatus() ==
                            AccountMigrationRunner::Status::kRunning)) {
    return;
  }
  migration_runner_ = std::make_unique<AccountMigrationRunner>();

  ran_migration_steps_ = false;
  if (ShouldRunMigrations()) {
    ran_migration_steps_ = true;
    AddMigrationSteps();
  }

  // Cleanup tasks (like re-enabling Chrome account reconciliation) rely on the
  // migration being run, even if they were no-op. Check
  // |OnMigrationRunComplete| and |RunCleanupTasks|.
  migration_runner_->Run(
      base::BindOnce(&AccountManagerMigrator::OnMigrationRunComplete,
                     weak_factory_.GetWeakPtr()));
}

bool AccountManagerMigrator::ShouldRunMigrations() const {
  // Account migration does not make sense for ephemeral (Guest, Managed
  // Session, Kiosk, Demo etc.) sessions.
  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral()) {
    VLOG(1) << "Skipping migrations for ephemeral session";
    return false;
  }

  // Do not unnecessarily run migrations if they have been successfully run
  // before.
  if (profile_->GetPrefs()->GetInteger(
          ::prefs::kAccountManagerNumTimesMigrationRanSuccessfully) >=
      kMaxMigrationRuns) {
    VLOG(1) << "Skipping migrations because of previous successful runs";
    return false;
  }

  // Do not run migrations if the Device Account is invalid.
  if (GetDeviceAccount(profile_).id.empty()) {
    // Unfortunately this is a valid case for tests. See
    // https://crbug.com/915628. Early exit here because if the Device Account
    // itself is invalid, we should not attempt any migration.
    return false;
  }

  return true;
}

void AccountManagerMigrator::AddMigrationSteps() {
  auto* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  auto* account_manager =
      factory->GetAccountManager(profile_->GetPath().value());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  auto* account_manager_facade =
      ::GetAccountManagerFacade(profile_->GetPath().value());

  migration_runner_->AddStep(std::make_unique<DeviceAccountMigration>(
      GetDeviceAccount(profile_),
      ProfileHelper::Get()
          ->GetUserByProfile(profile_)
          ->display_email() /* device_account_raw_email */,
      account_manager, account_manager_facade, identity_manager,
      WebDataServiceFactory::GetTokenWebDataForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS) /* token_web_data */));

  const bool is_secondary_google_account_signin_allowed =
      profile_->GetPrefs()->GetBoolean(
          chromeos::prefs::kSecondaryGoogleAccountSigninAllowed);

  if (is_secondary_google_account_signin_allowed) {
    migration_runner_->AddStep(std::make_unique<ContentAreaAccountsMigration>(
        account_manager, account_manager_facade, identity_manager));

    if (arc::IsArcProvisioned(profile_)) {
      // Add a migration step for ARC only if ARC has been provisioned. If ARC
      // has not been provisioned yet, there cannot be any accounts that need to
      // be migrated.
      migration_runner_->AddStep(std::make_unique<ArcAccountsMigration>(
          account_manager, account_manager_facade, identity_manager,
          arc::ArcAuthService::GetForBrowserContext(
              profile_) /* arc_auth_service */));
    } else {
      VLOG(1) << "Skipping migration of ARC accounts. ARC has not been "
                 "provisioned yet";
    }
  }

  // This MUST be the last step. Check the class level documentation of
  // |SuccessStorage| for the reason.
  migration_runner_->AddStep(
      std::make_unique<SuccessStorage>(profile_->GetPrefs()));

  // TODO(sinhak): Verify Device Account LST state.
}

AccountMigrationRunner::Status AccountManagerMigrator::GetStatus() const {
  if (!migration_runner_)
    return AccountMigrationRunner::Status::kNotStarted;

  return migration_runner_->GetStatus();
}

base::Optional<AccountMigrationRunner::MigrationResult>
AccountManagerMigrator::GetLastMigrationRunResult() const {
  return last_migration_run_result_;
}

void AccountManagerMigrator::OnMigrationRunComplete(
    const AccountMigrationRunner::MigrationResult& result) {
  DCHECK_NE(AccountMigrationRunner::Status::kNotStarted,
            migration_runner_->GetStatus());
  DCHECK_NE(AccountMigrationRunner::Status::kRunning,
            migration_runner_->GetStatus());

  last_migration_run_result_ = base::make_optional(result);

  VLOG(1) << "Account migrations completed with result: "
          << static_cast<int>(result.final_status);
  if (result.final_status == AccountMigrationRunner::Status::kFailure)
    VLOG(1) << "Failed step: " << result.failed_step_id;

  if (ran_migration_steps_) {
    // Update the UMA stats only for migrations that actually ran some steps.
    base::UmaHistogramBoolean(
        kMigrationResultMetricName,
        result.final_status == AccountMigrationRunner::Status::kSuccess);
  }

  RunCleanupTasks();
}

void AccountManagerMigrator::RunCleanupTasks() {
  // Migration could have finished with a failure but we need to start account
  // reconciliation anyways. This may cause us to lose Chrome content area
  // Secondary Accounts but if we do not enable reconciliation, users will not
  // be able to add any account to Chrome content area which is a much worse
  // experience than losing Chrome content area Secondary Accounts.
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile_);
  account_reconcilor->EnableReconcile();
}

// static
AccountManagerMigrator* AccountManagerMigratorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccountManagerMigrator*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AccountManagerMigratorFactory* AccountManagerMigratorFactory::GetInstance() {
  static base::NoDestructor<AccountManagerMigratorFactory> instance;
  return instance.get();
}

AccountManagerMigratorFactory::AccountManagerMigratorFactory()
    : BrowserContextKeyedServiceFactory(
          "AccountManagerMigrator",
          BrowserContextDependencyManager::GetInstance()) {
  // Stores the LSTs, that need to be copied over to |AccountManager|.
  DependsOn(WebDataServiceFactory::GetInstance());
  // Account reconciliation is paused for the duration of migration and needs to
  // be re-enabled once migration is done.
  DependsOn(AccountReconcilorFactory::GetInstance());
  // For getting Chrome content area accounts.
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountManagerMigratorFactory::~AccountManagerMigratorFactory() = default;

KeyedService* AccountManagerMigratorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AccountManagerMigrator(Profile::FromBrowserContext(context));
}

}  // namespace ash
