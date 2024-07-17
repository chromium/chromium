// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/per_user_state_manager_chromeos.h"

#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/uuid.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace metrics {

namespace {

constexpr char kDaemonStorePath[] = "/run/daemon-store/uma-consent/";
constexpr char kDaemonStoreFileName[] = "consent-enabled";
// We want to collect crashes that cause the OS to reboot based on the consent
// of the user logged in at the time the OS rebooted. These "boot collectors"
// can't access the cryptohome of that user, so we track the current consent
// state outside of cryptohome, solely for the purpose of respecting user
// consent while collecting these crashes.
constexpr char kOutOfCryptohomeConsent[] = "/home/chronos/boot-collect-consent";
constexpr char kWriteFileFailMetric[] =
    "UMA.CrosPerUser.DaemonStoreWriteFailed";

std::optional<bool> g_is_managed_for_testing;

std::string GenerateUserId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

// Keep in sync with PerUserDaemonStoreFail enum.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused
enum class DaemonStoreFailType {
  kFailedDisabling = 0,
  kFailedEnabling = 1,
  kFailedWritingBoot = 2,
  kFailedDeletingBoot = 3,
  kWritesAttempted = 4,
  kRemoveSucceeded = 5,
  kMaxValue = kRemoveSucceeded,
};

// Keep in sync with PerUserIdType enum.
enum class IdResetType {
  kClientId = 0,
  kUserId = 1,
  kMaxValue = kUserId,
};

// Keep in sync with enum PerUserIdType.
void RecordIdReset(IdResetType id_type) {
  base::UmaHistogramEnumeration("UMA.CrosPerUser.IdReset", id_type);
}

void WriteDaemonStore(base::FilePath path, bool metrics_consent) {
  const std::string file_contents = metrics_consent ? "1" : "0";
  if (!base::WriteFile(path, file_contents)) {
    LOG(ERROR) << "Failed to persist consent change " << file_contents
               << " to daemon-store. State on disk will be inaccurate!";
    base::UmaHistogramEnumeration(kWriteFileFailMetric,
                                  metrics_consent
                                      ? DaemonStoreFailType::kFailedEnabling
                                      : DaemonStoreFailType::kFailedDisabling);
  }
  if (!base::WriteFile(base::FilePath(kOutOfCryptohomeConsent),
                       file_contents)) {
    LOG(ERROR) << "Failed to write out-of-cryptohome consent change: "
               << file_contents;
    base::UmaHistogramEnumeration(kWriteFileFailMetric,
                                  DaemonStoreFailType::kFailedWritingBoot);
  }
  base::UmaHistogramEnumeration(kWriteFileFailMetric,
                                DaemonStoreFailType::kWritesAttempted);
}

void RemoveGlobalMetricsConsent() {
  if (!base::DeleteFile(base::FilePath(kOutOfCryptohomeConsent))) {
    LOG(ERROR) << "failed to remove out-of-cryptohome consent file";
    base::UmaHistogramEnumeration(kWriteFileFailMetric,
                                  DaemonStoreFailType::kFailedDeletingBoot);
  } else {
    base::UmaHistogramEnumeration(kWriteFileFailMetric,
                                  DaemonStoreFailType::kRemoveSucceeded);
  }
}

}  // namespace

PerUserStateManagerChromeOS::PerUserStateManagerChromeOS(
    MetricsServiceClient* metrics_service_client,
    user_manager::UserManager* user_manager,
    PrefService* local_state,
    const MetricsLogStore::StorageLimits& storage_limits,
    const std::string& signing_key)
    : metrics_service_client_(metrics_service_client),
      user_manager_(user_manager),
      local_state_(local_state),
      storage_limits_(storage_limits),
      signing_key_(signing_key) {
  user_manager_->AddSessionStateObserver(this);
  user_manager_->AddObserver(this);
  // This could be null in very narrow cases during early browser startup
  // (PreMainMessageLoopRun) or shutdown (destruction of
  // ChromeMainBrowserPartsAsh).
  // It's unlikely that we'd be running this constructor that early or that
  // late, but just in case, avoid crashing. We won't behave correctly on
  // logout, but if there's no termination manager there's not much we can do.
  if (ash::SessionTerminationManager::Get()) {
    ash::SessionTerminationManager::Get()->AddObserver(this);
  }

  task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
}

PerUserStateManagerChromeOS::PerUserStateManagerChromeOS(
    MetricsServiceClient* metrics_service_client,
    PrefService* local_state)
    : PerUserStateManagerChromeOS(
          metrics_service_client,
          user_manager::UserManager::Get(),
          local_state,
          metrics_service_client->GetStorageLimits(),
          metrics_service_client->GetUploadSigningKey()) {
  // Ensure that user_manager has been initialized.
  DCHECK(user_manager::UserManager::IsInitialized());
}

PerUserStateManagerChromeOS::~PerUserStateManagerChromeOS() {
  user_manager_->RemoveObserver(this);
  user_manager_->RemoveSessionStateObserver(this);
}

// static
void PerUserStateManagerChromeOS::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kMetricsCurrentUserId, "");
}

// static
void PerUserStateManagerChromeOS::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kMetricsUserMetricLogs);
  registry->RegisterDictionaryPref(prefs::kMetricsUserMetricLogsMetadata);
  registry->RegisterStringPref(prefs::kMetricsUserId, "");
  registry->RegisterBooleanPref(prefs::kMetricsUserConsent, false);
  registry->RegisterBooleanPref(prefs::kMetricsRequiresClientIdResetOnConsent,
                                false);
}

std::optional<std::string> PerUserStateManagerChromeOS::GetCurrentUserId()
    const {
  if (state_ != State::USER_LOG_STORE_HANDLED)
    return std::nullopt;
  auto user_id = GetCurrentUserPrefs()->GetString(prefs::kMetricsUserId);
  if (user_id.empty())
    return std::nullopt;
  return user_id;
}

std::optional<bool>
PerUserStateManagerChromeOS::GetCurrentUserReportingConsentIfApplicable()
    const {
  if (state_ != State::USER_LOG_STORE_HANDLED) {
    return std::nullopt;
  }

  // Cases in which user permissions should be applied to metrics reporting.
  if (IsUserAllowedToChangeConsent(current_user_)) {
    return GetCurrentUserPrefs()->GetBoolean(prefs::kMetricsUserConsent);
  }

  return std::nullopt;
}

void PerUserStateManagerChromeOS::SetCurrentUserMetricsConsent(
    bool metrics_consent) {
  DCHECK_EQ(state_, State::USER_LOG_STORE_HANDLED);

  DCHECK(current_user_);

  // No-op if user should not be able to change metrics consent.
  if (!IsUserAllowedToChangeConsent(current_user_))
    return;

  auto* user_prefs = GetCurrentUserPrefs();

  // Value of |metrics_consent| is the same as the current consent. Terminate
  // early.
  if (user_prefs->GetBoolean(prefs::kMetricsUserConsent) == metrics_consent)
    return;

  // |new_user_id| = "" for on->off.
  std::string new_user_id;

  // off -> on state.
  if (metrics_consent) {
    new_user_id = GenerateUserId();
  }

  UpdateCurrentUserId(new_user_id);
  SetReportingState(metrics_consent);
  UpdateLocalStatePrefs(metrics_consent);
}

bool PerUserStateManagerChromeOS::IsUserAllowedToChangeConsent(
    user_manager::User* user) const {
  // Devices with managed policy should not have control to toggle metrics
  // consent.
  if (IsReportingPolicyManaged())
    return false;

  auto user_type = user->GetType();

  // Unmanaged guest users are allowed to update their consent during OOBE.
  // Unmanaged guests can be created without a device owner, which is why this
  // logic is explicitly before the device ownership logic below.
  if (user_type == user_manager::UserType::kGuest) {
    return true;
  }

  // If the device isn't owned yet, we consider this user as the owner.
  // During OOBE by the first user, the device remains unowned.
  // Guest sessions can be started before there is a device owner, which
  // is handled above.
  if (!IsDeviceOwned()) {
    return false;
  }

  // Check if the current user is the owner user.
  if (user->GetAccountId() == user_manager_->GetOwnerAccountId()) {
    return false;
  }

  // User types that are not the owner can update their consent during OOBE.
  return true;
}

base::CallbackListSubscription PerUserStateManagerChromeOS::AddObserver(
    const MetricsConsentHandler& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callback_list_.Add(callback);
}

// static
void PerUserStateManagerChromeOS::SetIsManagedForTesting(bool is_managed) {
  g_is_managed_for_testing = is_managed;
}

void PerUserStateManagerChromeOS::ResetStateForTesting() {
  ResetState();
}

void PerUserStateManagerChromeOS::SetUserLogStore(
    std::unique_ptr<UnsentLogStore> log_store) {
  DCHECK(state_ == State::USER_PROFILE_READY);

  metrics_service_client_->GetMetricsService()->SetUserLogStore(
      std::move(log_store));
}

void PerUserStateManagerChromeOS::UnsetUserLogStore() {
  DCHECK_EQ(state_, State::USER_LOG_STORE_HANDLED);
  metrics_service_client_->GetMetricsService()->UnsetUserLogStore();
}

void PerUserStateManagerChromeOS::ForceClientIdReset() {
  metrics_service_client_->GetMetricsService()->ResetClientId();
}

void PerUserStateManagerChromeOS::SetReportingState(bool metrics_consent) {
  DCHECK_EQ(state_, State::USER_LOG_STORE_HANDLED);
  DCHECK(IsUserAllowedToChangeConsent(current_user_));

  GetCurrentUserPrefs()->SetBoolean(prefs::kMetricsUserConsent,
                                    metrics_consent);

  std::string hash = user_manager_->GetActiveUser()->username_hash();
  base::FilePath daemon_store_consent = base::FilePath(kDaemonStorePath)
                                            .Append(hash)
                                            .Append(kDaemonStoreFileName);
  // Do this asynchronously so we don't block in a potentially non-blocking
  // context.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(WriteDaemonStore, daemon_store_consent, metrics_consent));
  NotifyObservers(metrics_consent);
}

bool PerUserStateManagerChromeOS::IsReportingPolicyManaged() const {
  if (g_is_managed_for_testing.has_value())
    return g_is_managed_for_testing.value();

  return metrics_service_client_->IsReportingPolicyManaged();
}

bool PerUserStateManagerChromeOS::HasUserLogStore() const {
  return metrics_service_client_->GetMetricsService()->HasUserLogStore();
}

bool PerUserStateManagerChromeOS::IsDeviceOwned() const {
  DCHECK(IsDeviceStatusKnown());

  return ash::DeviceSettingsService::Get()->GetOwnershipStatus() ==
         ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken;
}

bool PerUserStateManagerChromeOS::IsDeviceStatusKnown() const {
  return ash::DeviceSettingsService::Get()->GetOwnershipStatus() !=
         ash::DeviceSettingsService::OwnershipStatus::kOwnershipUnknown;
}

void PerUserStateManagerChromeOS::ActiveUserChanged(user_manager::User* user) {
  // Logged-in user is already detected. Do nothing since multi-user is
  // deprecated and since the first user is the primary user.
  if (state_ > State::CONSTRUCTED) {
    return;
  }

  state_ = State::USER_LOGIN;

  current_user_ = user;
  user->AddProfileCreatedObserver(
      base::BindOnce(&PerUserStateManagerChromeOS::WaitForOwnershipStatus,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PerUserStateManagerChromeOS::OnUserToBeRemoved(
    const AccountId& account_id) {
  auto* user = user_manager_->FindUser(account_id);

  // Resets the state of |this| if user to be removed is the current logged in
  // user.
  if (user && current_user_ == user) {
    ResetState();
  }
}

void PerUserStateManagerChromeOS::OnSessionWillBeTerminated() {
  // A logout will log out *all* signed-in users, so remove the user-specific
  // consent file.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(RemoveGlobalMetricsConsent));
}

void PerUserStateManagerChromeOS::WaitForOwnershipStatus() {
  DCHECK_EQ(state_, State::USER_LOGIN);

  // Device ownership determination happens asynchronously in parallel with
  // profile loading, so there is a chance that status is not known yet.
  // The device status will either be None or Taken, it cannot be Unknown.
  ash::DeviceSettingsService::Get()->GetOwnershipStatusAsync(base::BindOnce(
      &PerUserStateManagerChromeOS::InitializeProfileMetricsState,
      weak_ptr_factory_.GetWeakPtr()));
}

bool PerUserStateManagerChromeOS::ShouldUseUserLogStore() const {
  CHECK_GT(state_, State::CONSTRUCTED);
  return !ash::DemoSession::IsDeviceInDemoMode();
}

void PerUserStateManagerChromeOS::InitializeProfileMetricsState(
    ash::DeviceSettingsService::OwnershipStatus status) {
  DCHECK_NE(status,
            ash::DeviceSettingsService::OwnershipStatus::kOwnershipUnknown);
  DCHECK_EQ(state_, State::USER_LOGIN);

  state_ = State::USER_PROFILE_READY;

  if (ShouldUseUserLogStore()) {
    // Sets the metrics log store to the user cryptohome, flushing pending logs
    // recorded before user login to local state. Non-Demo-mode Ephemeral state
    // users use a temporary log store, doing a best effort upload of metrics
    // when consented. Persistent cryptohome users store logs in their
    // cryptohome. Demo mode user uses Local State store instead of cryptohome.
    AssignUserLogStore();
  }

  state_ = State::USER_LOG_STORE_HANDLED;

  // Handle unmanaged guest sessions below.
  // Managed guest sessions like other managed devices, are handled separately
  // by the enterprise policy.
  bool is_managed = IsReportingPolicyManaged();
  const bool is_guest =
      current_user_->GetType() == user_manager::UserType::kGuest;

  // If a guest session is about to be started, the metrics reporting will
  // set metrics reporting during ToS.
  // kOobeGuestMetricsEnabled saves the guest consent since the browser is
  // restarted after guest login.
  // Managed users, such as managed guest sessions do not use per user consent.
  if (is_guest && !is_managed) {
    // Guest sessions delete the profile and all associated prefs on restart.
    // Calling this method to set the user metrics consent ensures that the
    // guest session will have an associated user id. Method will also set the
    // current logged-in user ID to handle crashes by assigning to the local
    // pref.
    SetCurrentUserMetricsConsent(
        local_state_->GetBoolean(ash::prefs::kOobeGuestMetricsEnabled));

    // Reset state set during guest OOBE. This should be fine as if the guest
    // session crashes, the consent requested again during guest OOBE.
    local_state_->ClearPref(ash::prefs::kOobeGuestMetricsEnabled);

    return;
  }

  // Sets the current logged-in user ID to handle crashes. Assigns a user ID if
  // current user has one. Otherwise, clear the pref to reflect that current
  // user does not have a user id.
  auto user_id = GetCurrentUserId();
  if (user_id.has_value()) {
    local_state_->SetString(prefs::kMetricsCurrentUserId, user_id.value());
  } else {
    RecordIdReset(IdResetType::kUserId);
    local_state_->ClearPref(prefs::kMetricsCurrentUserId);
  }

  // New users will not be consented until they consent in OOBE.
  // Only initialize metrics consent to user metrics consent if user is
  // allowed to change the metrics consent.
  if (IsUserAllowedToChangeConsent(current_user_)) {
    SetReportingState(
        GetCurrentUserPrefs()->GetBoolean(prefs::kMetricsUserConsent));
  } else {
    // Clear out the non-cryptohome consent file, as we shouldn't allow this
    // user to set consent. (Either we're the owner, or per-user consent is
    // disabled. In either case, this user doesn't have any say over consent
    // beyond device policy.)
    // In general, the non-cryptohome consent file should be cleared on logout,
    // but on an abnormal logout -- e.g. a session-manager crash -- it wouldn't
    // be.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(RemoveGlobalMetricsConsent));
  }
}

void PerUserStateManagerChromeOS::UpdateCurrentUserId(
    const std::string& new_user_id) {
  DCHECK_EQ(state_, State::USER_LOG_STORE_HANDLED);

  // Updates both the user profile as well as the user ID stored in local
  // state to handle crashes appropriately.
  GetCurrentUserPrefs()->SetString(prefs::kMetricsUserId, new_user_id);
  local_state_->SetString(prefs::kMetricsCurrentUserId, new_user_id);
}

void PerUserStateManagerChromeOS::ResetState() {
  UnsetUserLogStore();

  current_user_ = nullptr;
  local_state_->ClearPref(prefs::kMetricsCurrentUserId);
  state_ = State::CONSTRUCTED;
}

PrefService* PerUserStateManagerChromeOS::GetCurrentUserPrefs() const {
  DCHECK(state_ >= State::USER_PROFILE_READY);
  return ash::ProfileHelper::Get()->GetProfileByUser(current_user_)->GetPrefs();
}

void PerUserStateManagerChromeOS::AssignUserLogStore() {
  SetUserLogStore(std::make_unique<UnsentLogStore>(
      std::make_unique<UnsentLogStoreMetricsImpl>(), GetCurrentUserPrefs(),
      prefs::kMetricsUserMetricLogs, prefs::kMetricsUserMetricLogsMetadata,
      storage_limits_.ongoing_log_queue_limits, signing_key_,
      // |logs_event_manager| will be set by the metrics service directly in
      // MetricsLogStore::SetAlternateOngoingLogStore().
      /*logs_event_manager=*/nullptr));
}

void PerUserStateManagerChromeOS::NotifyObservers(bool metrics_consent) {
  callback_list_.Notify(metrics_consent);
}

void PerUserStateManagerChromeOS::UpdateLocalStatePrefs(bool metrics_enabled) {
  // If user consent is turned off, client ID does not need to be reset. Only
  // profile prefs (ie user_id) should be reset.
  if (!metrics_enabled)
    return;

  auto* user_prefs = GetCurrentUserPrefs();

  // TODO(crbug.com/40203990): In the case that multiple users toggle consent
  // off to on, this will cause the client ID to be reset each time, which is
  // not necessary. Look for a way to allow resetting client id less eager.
  if (user_prefs->GetBoolean(prefs::kMetricsRequiresClientIdResetOnConsent)) {
    RecordIdReset(IdResetType::kClientId);
    ForceClientIdReset();
  } else {
    user_prefs->SetBoolean(prefs::kMetricsRequiresClientIdResetOnConsent, true);
  }
}

}  // namespace metrics
