// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/built_in_backend_to_android_backend_migrator.h"

#include <optional>
#include <string>

#include "base/barrier_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "components/browser_sync/sync_to_signin_migration.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/pref_names.h"

namespace password_manager {

namespace {

// Threshold for the next migration attempt. This is needed in order to prevent
// clients from spamming GMS Core API.
constexpr base::TimeDelta kMigrationThreshold = base::Days(1);

// The required migration version. If the version saved in
// `prefs::kCurrentMigrationVersionToGoogleMobileServices` is lower than
// 'kRequiredMigrationVersion', passwords will be re-uploaded. Currently set to
// the initial migration version.
constexpr int kRequiredMigrationVersion = 1;

constexpr char kMetricInfix[] =
    "PasswordManager.UnifiedPasswordManager.MigrationForLocalUsers.";

// Returns true if the initial migration to the android backend has happened.
bool HasMigratedToTheAndroidBackend(PrefService* prefs) {
  return prefs->GetInteger(
             prefs::kCurrentMigrationVersionToGoogleMobileServices) >=
         kRequiredMigrationVersion;
}

bool IsBlocklistedFormWithValues(const PasswordForm& form) {
  return form.blocked_by_user &&
         (!form.username_value.empty() || !form.password_value.empty());
}

std::string BackendOperationToString(
    BuiltInBackendToAndroidBackendMigrator::BackendOperationForMigration
        backend_operation) {
  switch (backend_operation) {
    case BuiltInBackendToAndroidBackendMigrator::BackendOperationForMigration::
        kAddLogin:
      return "AddLogin";
    case BuiltInBackendToAndroidBackendMigrator::BackendOperationForMigration::
        kUpdateLogin:
      return "UpdateLogin";
    case BuiltInBackendToAndroidBackendMigrator::BackendOperationForMigration::
        kRemoveLogin:
      return "RemoveLogin";
    case BuiltInBackendToAndroidBackendMigrator::BackendOperationForMigration::
        kGetAllLogins:
      return "GetAllLogins";
  }
}

void ResetUnenrollmentStatus(PrefService* prefs) {
  prefs->ClearPref(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
}

bool IsPasswordSyncEnabled(PrefService* pref_service) {
  switch (browser_sync::GetSyncToSigninMigrationDataTypeDecision(
      pref_service, syncer::PASSWORDS,
      syncer::prefs::internal::kSyncPasswords)) {
    // In particular, in the
    // prefs::UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending
    // state, kDontMigrateTypeNotActive is reported and we wish to return true.
    case browser_sync::SyncToSigninMigrationDataTypeDecision::
        kDontMigrateTypeNotActive:
    case browser_sync::SyncToSigninMigrationDataTypeDecision::kMigrate:
      return true;
    case browser_sync::SyncToSigninMigrationDataTypeDecision::
        kDontMigrateTypeDisabled:
      return false;
  }
}

void SchedulePostMigrationBottomSheet(PrefService* prefs) {
  // There is no need to show the sheet if no passwords were migrated.
  if (prefs->GetBoolean(prefs::kEmptyProfileStoreLoginDatabase)) {
    return;
  }

  // As part of M4 syncing users who were unenrolled migrate their passwords to
  // local GMSCore storage. They shouldn't see the sheet either.
  if (IsPasswordSyncEnabled(prefs) &&
      (prefs->GetBoolean(
           prefs::kUnenrolledFromGoogleMobileServicesDueToErrors) ||
       prefs->GetInteger(
           prefs::kCurrentMigrationVersionToGoogleMobileServices) == 0)) {
    return;
  }

  prefs->SetBoolean(prefs::kShouldShowPostPasswordMigrationSheetAtStartup,
                    true);
}

}  // namespace

struct BuiltInBackendToAndroidBackendMigrator::IsPasswordLess {
  bool operator()(const PasswordForm* lhs, const PasswordForm* rhs) const {
    return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
  }
};

struct BuiltInBackendToAndroidBackendMigrator::BackendAndLoginsResults {
  raw_ptr<PasswordStoreBackend> backend;
  LoginsResultOrError logins_result;

  bool HasError() const {
    return absl::holds_alternative<PasswordStoreBackendError>(logins_result);
  }

  std::optional<int> GetApiError() const {
    if (HasError()) {
      return absl::get<PasswordStoreBackendError>(logins_result)
          .android_backend_api_error;
    }
    return std::nullopt;
  }

  // Converts std::vector<std::unique_ptr<PasswordForms>> into
  // base::flat_set<const PasswordForm*> for quick look up comparing only
  // primary keys.
  base::flat_set<const PasswordForm*, IsPasswordLess> GetLogins() {
    DCHECK(!HasError());

    return base::MakeFlatSet<const PasswordForm*, IsPasswordLess>(
        absl::get<LoginsResult>(logins_result), {},
        [](auto& form) { return &form; });
  }

  BackendAndLoginsResults(PasswordStoreBackend* backend,
                          LoginsResultOrError logins)
      : backend(backend), logins_result(std::move(logins)) {}
  BackendAndLoginsResults(BackendAndLoginsResults&&) = default;
  BackendAndLoginsResults& operator=(BackendAndLoginsResults&&) = default;
  BackendAndLoginsResults(const BackendAndLoginsResults&) = delete;
  BackendAndLoginsResults& operator=(const BackendAndLoginsResults&) = delete;
  ~BackendAndLoginsResults() = default;
};

class BuiltInBackendToAndroidBackendMigrator::MigrationMetricsReporter {
 public:
  MigrationMetricsReporter() = default;
  ~MigrationMetricsReporter() = default;

  void ReportMetrics(bool migration_succeeded) {
    base::TimeDelta duration = base::Time::Now() - start_;
    base::UmaHistogramMediumTimes(base::StrCat({kMetricInfix, "Latency"}),
                                  duration);
    base::UmaHistogramBoolean(base::StrCat({kMetricInfix, "Success"}),
                              migration_succeeded);
    base::UmaHistogramCounts1000(
        base::StrCat({kMetricInfix, "UpdateLoginCount"}), update_logins_count_);
    ReportAdditionalMetricsForLocalPasswordsMigration(migration_succeeded);
    metrics_util::LogLocalPwdMigrationProgressState(
        metrics_util::LocalPwdMigrationProgressState::kFinished);
  }

  void ReportAdditionalMetricsForLocalPasswordsMigration(
      bool migration_succeeded) {
    base::UmaHistogramCounts1000(base::StrCat({kMetricInfix, "AddLoginCount"}),
                                 added_logins_count_);
    base::UmaHistogramCounts1000(
        base::StrCat({kMetricInfix, "MigratedLoginsTotalCount"}),
        added_logins_count_ + update_logins_count_);
    if (migration_succeeded &&
        migration_conflict_won_by_android_count_.has_value()) {
      base::UmaHistogramCounts1000(
          base::StrCat({kMetricInfix, "MergeWhereAndroidHasMostRecent"}),
          migration_conflict_won_by_android_count_.value());
    }
  }

  void HandleBackendOperationResult(
      std::string backend_infix,
      BackendOperationForMigration backend_operation,
      bool is_success,
      std::optional<int> api_error) {
    base::UmaHistogramBoolean(
        base::StrCat({kMetricInfix, backend_infix, ".",
                      BackendOperationToString(backend_operation), ".Success"}),
        is_success);
    if (!is_success) {
      if (api_error.has_value()) {
        base::UmaHistogramSparse(
            base::StrCat({kMetricInfix, backend_infix, ".",
                          BackendOperationToString(backend_operation),
                          ".APIError"}),
            api_error.value());
      }
      return;
    }

    switch (backend_operation) {
      case BackendOperationForMigration::kAddLogin:
        added_logins_count_++;
        break;
      case BackendOperationForMigration::kUpdateLogin:
        update_logins_count_++;
        break;
      case BackendOperationForMigration::kRemoveLogin:
      case BackendOperationForMigration::kGetAllLogins:
        break;
    }
  }

  void SetLocalConflictsWonByAndroidCount(int count) {
    migration_conflict_won_by_android_count_ = count;
  }

 private:
  base::Time start_ = base::Time::Now();
  std::optional<int> migration_conflict_won_by_android_count_;
  int added_logins_count_ = 0;
  int update_logins_count_ = 0;
};

BuiltInBackendToAndroidBackendMigrator::BuiltInBackendToAndroidBackendMigrator(
    PasswordStoreBackend* built_in_backend,
    PasswordStoreBackend* android_backend,
    PrefService* prefs)
    : built_in_backend_(built_in_backend),
      android_backend_(android_backend),
      prefs_(prefs) {
  DCHECK(built_in_backend_);
  DCHECK(android_backend_);
  base::UmaHistogramBoolean(
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone",
      HasMigratedToTheAndroidBackend(prefs_));
}

BuiltInBackendToAndroidBackendMigrator::
    ~BuiltInBackendToAndroidBackendMigrator() = default;

void BuiltInBackendToAndroidBackendMigrator::StartMigrationOfLocalPasswords() {
  CHECK(!migration_in_progress_);

  // Don't try to migrate passwords if there was an attempt earlier today.
  base::TimeDelta time_passed_since_last_migration_attempt =
      base::Time::Now() -
      base::Time::FromTimeT(prefs_->GetDouble(
          password_manager::prefs::kTimeOfLastMigrationAttempt));
  if (time_passed_since_last_migration_attempt < kMigrationThreshold) {
    return;
  }

  migration_in_progress_ = true;

  metrics_reporter_ = std::make_unique<MigrationMetricsReporter>();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords",
                                    "UnifiedPasswordManagerMigration", this);

  prefs_->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt,
                    base::Time::Now().InSecondsFSinceUnixEpoch());

  LogLocalPwdMigrationProgressState(
      metrics_util::LocalPwdMigrationProgressState::kStarted);
  auto barrier_callback = base::BarrierCallback<BackendAndLoginsResults>(
      2,
      base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::
                         MigrateLocalPasswordsBetweenAndroidAndBuiltInBackends,
                     weak_ptr_factory_.GetWeakPtr()));

  auto bind_backend_to_logins = [](PasswordStoreBackend* backend,
                                   LoginsResultOrError result) {
    return BackendAndLoginsResults(backend, std::move(result));
  };

  auto builtin_backend_callback_chain =
      base::BindOnce(bind_backend_to_logins,
                     base::Unretained(built_in_backend_))
          .Then(barrier_callback);

  // Cleanup blacklisted forms in the built in backend before binding.
  builtin_backend_callback_chain = base::BindOnce(
      &BuiltInBackendToAndroidBackendMigrator::RemoveBlocklistedFormsWithValues,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(built_in_backend_),
      std::move(builtin_backend_callback_chain));

  built_in_backend_->GetAllLoginsAsync(
      std::move(builtin_backend_callback_chain));

  auto android_backend_callback_chain =
      base::BindOnce(bind_backend_to_logins, base::Unretained(android_backend_))
          .Then(barrier_callback);

  // Cleanup blacklisted forms in the android backend before binding.
  android_backend_callback_chain = base::BindOnce(
      &BuiltInBackendToAndroidBackendMigrator::RemoveBlocklistedFormsWithValues,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(android_backend_),
      std::move(android_backend_callback_chain));

  android_backend_->GetAllLoginsAsync(
      std::move(android_backend_callback_chain));
}

void BuiltInBackendToAndroidBackendMigrator::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service_ = sync_service;
}

base::WeakPtr<BuiltInBackendToAndroidBackendMigrator>
BuiltInBackendToAndroidBackendMigrator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BuiltInBackendToAndroidBackendMigrator::
    MigrateLocalPasswordsBetweenAndroidAndBuiltInBackends(
        std::vector<BackendAndLoginsResults> results) {
  DCHECK(metrics_reporter_);
  DCHECK_EQ(2u, results.size());

  if (results[0].HasError() || results[1].HasError()) {
    for (const auto& result : results) {
      metrics_reporter_->HandleBackendOperationResult(
          GetMetricInfixFromBackend(result.backend),
          BackendOperationForMigration::kGetAllLogins, !result.HasError(),
          result.GetApiError());
    }

    MigrationFinished(/*is_success=*/false);
    return;
  }

  base::flat_set<const PasswordForm*, IsPasswordLess> built_in_backend_logins =
      (results[0].backend == built_in_backend_) ? results[0].GetLogins()
                                                : results[1].GetLogins();

  base::flat_set<const PasswordForm*, IsPasswordLess> android_logins =
      (results[0].backend == android_backend_) ? results[0].GetLogins()
                                               : results[1].GetLogins();

  MergeBuiltInBackendIntoAndroidBackend(std::move(built_in_backend_logins),
                                        std::move(android_logins));
}

void BuiltInBackendToAndroidBackendMigrator::
    MergeBuiltInBackendIntoAndroidBackend(
        PasswordFormPtrFlatSet built_in_backend_logins,
        PasswordFormPtrFlatSet android_logins) {
  // For a form |F|, there are 2 cases to handle:
  // 1. If |F| exists only in the |built_in_backend_|, then |F| should be added
  //    to the |android_backend_|.
  // 2. If |F| already exists in both |android_backend_|, then
  //    the most recent version of |F| will be kept in |android_backend_|.
  // No changes are made to the |built_in_backend_|.

  // Callbacks are chained like in a stack way by passing 'callback_chain' as a
  // completion for the next operation. At the end, update pref to mark
  // successful completion.
  base::OnceClosure callbacks_chain =
      base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::MigrationFinished,
                     weak_ptr_factory_.GetWeakPtr(), /*is_success=*/true);
  int migration_conflict_won_by_android_count = 0;
  for (auto* const login : built_in_backend_logins) {
    auto android_login_iter = android_logins.find(login);

    if (android_login_iter == android_logins.end()) {
      // Password from the |built_in_backend_| doesn't exist in the
      // |android_backend_|.
      callbacks_chain = base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::AddLoginToBackend,
          weak_ptr_factory_.GetWeakPtr(), android_backend_, *login,
          std::move(callbacks_chain));

      continue;
    }

    // Password from the |built_in_backend_| exists in the |android_backend_|.
    auto* const android_login = (*android_login_iter);

    if (login->password_value == android_login->password_value) {
      // Passwords are identical, nothing else to do.
      continue;
    }

    // Passwords aren't identical. Pick the most recentl one. The most recent is
    // considered the one, which has the newest create, last used or modified
    // date.
    if (std::max({login->date_created, login->date_last_used,
                  login->date_password_modified}) >
        std::max({android_login->date_created, android_login->date_last_used,
                  android_login->date_password_modified})) {
      callbacks_chain = base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::UpdateLoginInBackend,
          weak_ptr_factory_.GetWeakPtr(), android_backend_, *login,
          std::move(callbacks_chain));
    } else {
      migration_conflict_won_by_android_count++;
    }
  }
  metrics_reporter_->SetLocalConflictsWonByAndroidCount(
      migration_conflict_won_by_android_count);
  std::move(callbacks_chain).Run();
}

void BuiltInBackendToAndroidBackendMigrator::AddLoginToBackend(
    PasswordStoreBackend* backend,
    const PasswordForm& form,
    base::OnceClosure callback) {
  backend->AddLoginAsync(
      form,
      base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::RunCallbackOrAbortMigration,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          GetMetricInfixFromBackend(backend),
          BackendOperationForMigration::kAddLogin));
}

void BuiltInBackendToAndroidBackendMigrator::UpdateLoginInBackend(
    PasswordStoreBackend* backend,
    const PasswordForm& form,
    base::OnceClosure callback) {
  backend->UpdateLoginAsync(
      form,
      base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::RunCallbackOrAbortMigration,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          GetMetricInfixFromBackend(backend),
          BackendOperationForMigration::kUpdateLogin));
}

void BuiltInBackendToAndroidBackendMigrator::RemoveLoginFromBackend(
    PasswordStoreBackend* backend,
    const PasswordForm& form,
    base::OnceClosure callback) {
  backend->RemoveLoginAsync(
      FROM_HERE, form,
      base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::RunCallbackOrAbortMigration,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          GetMetricInfixFromBackend(backend),
          BackendOperationForMigration::kRemoveLogin));
}

void BuiltInBackendToAndroidBackendMigrator::RunCallbackOrAbortMigration(
    base::OnceClosure callback,
    const std::string& backend_infix,
    BackendOperationForMigration backend_operation,
    PasswordChangesOrError changes_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(changes_or_error)) {
    const PasswordStoreBackendError& error =
        absl::get<PasswordStoreBackendError>(changes_or_error);
    metrics_reporter_->HandleBackendOperationResult(
        backend_infix, backend_operation, /*is_success=*/false,
        error.android_backend_api_error);
    MigrationFinished(/*is_success=*/false);
    return;
  }

  const PasswordChanges& changes = absl::get<PasswordChanges>(changes_or_error);
  // Nullopt changelist is returned on success by the backends that do not
  // provide exact changelist (e.g. Android). This indicates success operation
  // as well as non-empty changelist.
  if (!changes.has_value() || !changes.value().empty()) {
    metrics_reporter_->HandleBackendOperationResult(backend_infix,
                                                    backend_operation,
                                                    /*is_success=*/true,
                                                    /*api_error=*/std::nullopt);
    // The step was successful, continue the migration.
    std::move(callback).Run();
    return;
  }

  // Migration failed.
  // It is unclear what the reason for this could be, but since there
  // was technically no API error, there is none to record.
  metrics_reporter_->HandleBackendOperationResult(
      backend_infix, backend_operation, /*is_success=*/false,
      /*api_error=*/std::nullopt);
  MigrationFinished(/*is_success=*/false);
}

void BuiltInBackendToAndroidBackendMigrator::MigrationFinished(
    bool is_success) {
  DCHECK(metrics_reporter_);
  metrics_reporter_->ReportMetrics(is_success);
  metrics_reporter_.reset();

  if (is_success && migration_in_progress_) {
    SchedulePostMigrationBottomSheet(prefs_);
    prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                       kRequiredMigrationVersion);
    ResetUnenrollmentStatus(prefs_);
    prefs_->SetInteger(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
  }

  migration_in_progress_ = false;
  TRACE_EVENT_NESTABLE_ASYNC_END0("passwords",
                                  "UnifiedPasswordManagerMigration", this);
}

void BuiltInBackendToAndroidBackendMigrator::RemoveBlocklistedFormsWithValues(
    PasswordStoreBackend* backend,
    LoginsOrErrorReply result_callback,
    LoginsResultOrError logins_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
    std::move(result_callback).Run(std::move(logins_or_error));
    return;
  }

  LoginsResult all_forms = absl::get<LoginsResult>(std::move(logins_or_error));
  LoginsResult clean_forms;
  LoginsResult forms_to_remove;
  clean_forms.reserve(all_forms.size());
  forms_to_remove.reserve(all_forms.size());

  for (auto& form : all_forms) {
    if (IsBlocklistedFormWithValues(form)) {
      forms_to_remove.push_back(std::move(form));
    } else {
      clean_forms.push_back(std::move(form));
    }
  }

  auto callback_chain =
      base::BindOnce(std::move(result_callback), std::move(clean_forms));

  for (auto& form : forms_to_remove) {
    callback_chain = base::BindOnce(
        &BuiltInBackendToAndroidBackendMigrator::RemoveLoginFromBackend,
        weak_ptr_factory_.GetWeakPtr(), backend, form,
        std::move(callback_chain));
  }

  std::move(callback_chain).Run();
}

std::string BuiltInBackendToAndroidBackendMigrator::GetMetricInfixFromBackend(
    PasswordStoreBackend* backend) {
  return backend == built_in_backend_ ? "BuiltInBackend" : "AndroidBackend";
}

}  // namespace password_manager
