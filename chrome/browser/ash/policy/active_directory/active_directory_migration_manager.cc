// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/active_directory/active_directory_migration_manager.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// The amount of time we wait before actively checking the preconditions to
// start the migration again.
constexpr base::TimeDelta kRetryDelay = base::Hours(1);

// The amount of time we wait before triggering a new powerwash, in case the
// last request has failed for any reason.
constexpr base::TimeDelta kPowerwashBackoffTime = base::Days(1);

// Returns true if any user is logged in (session is started).
bool IsUserLoggedIn() {
  auto* session_manager = session_manager::SessionManager::Get();
  return session_manager && session_manager->IsSessionStarted();
}

}  // namespace

ActiveDirectoryMigrationManager::ActiveDirectoryMigrationManager(
    PrefService* local_state)
    : local_state_(local_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(local_state_);

  // Listen to user session state changes.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->AddObserver(this);
  }
}

ActiveDirectoryMigrationManager::~ActiveDirectoryMigrationManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop listening to user session state changes.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
  }
}

// static
void ActiveDirectoryMigrationManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kLastChromadMigrationAttemptTime,
                             /*default_value=*/base::Time());
}

void ActiveDirectoryMigrationManager::Init() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Listen to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(local_state_);
  pref_change_registrar_->Add(
      prefs::kEnrollmentIdUploadedOnChromad,
      base::BindRepeating(
          &ActiveDirectoryMigrationManager::OnEnrollmentIdUploadedPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_->Add(
      ash::prefs::kChromadToCloudMigrationEnabled,
      base::BindRepeating(&ActiveDirectoryMigrationManager::
                              OnChromadMigrationEnabledPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // Check the conditions here as well, because this manager might be
  // initialized while the pre-conditions are already satisfied.
  TryToStartMigration();
}

void ActiveDirectoryMigrationManager::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pref_change_registrar_->RemoveAll();
}

void ActiveDirectoryMigrationManager::SetStatusCallbackForTesting(
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  status_callback_for_testing_ = std::move(callback);
}

bool ActiveDirectoryMigrationManager::HasUploadedEnrollmentId() const {
  return local_state_->GetBoolean(prefs::kEnrollmentIdUploadedOnChromad);
}

bool ActiveDirectoryMigrationManager::IsChromadMigrationEnabled() const {
  return local_state_->GetBoolean(ash::prefs::kChromadToCloudMigrationEnabled);
}

bool ActiveDirectoryMigrationManager::HasBackoffTimePassed() const {
  base::Time last_migration_attempt_time =
      local_state_->GetTime(prefs::kLastChromadMigrationAttemptTime);
  base::Time now = base::Time::Now();

  return now - last_migration_attempt_time > kPowerwashBackoffTime;
}

void ActiveDirectoryMigrationManager::OnEnrollmentIdUploadedPrefChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  TryToStartMigration();
}

void ActiveDirectoryMigrationManager::OnChromadMigrationEnabledPrefChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  TryToStartMigration();
}

void ActiveDirectoryMigrationManager::OnLoginOrLockScreenVisible() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  TryToStartMigration();
}

void ActiveDirectoryMigrationManager::TryToStartMigration() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool is_on_login_screen = !IsUserLoggedIn();

  if (is_on_login_screen && HasUploadedEnrollmentId() &&
      IsChromadMigrationEnabled() && HasBackoffTimePassed()) {
    StartPowerwash();
    MaybeRunStatusCallback(/*started=*/true, /*rescheduled=*/false);
    return;
  }

  // Theoretically, the following reschedule logic is not necessary. However, it
  // was added as a fallback, in case any of the signals this class listens is
  // not triggered as expected. Ultimately, we want to avoid inactive devices
  // getting stuck and not migrating.
  if (is_on_login_screen && !retry_already_scheduled_) {
    retry_already_scheduled_ = true;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ActiveDirectoryMigrationManager::RetryToStartMigration,
                       weak_ptr_factory_.GetWeakPtr()),
        kRetryDelay);

    MaybeRunStatusCallback(/*started=*/false, /*rescheduled=*/true);
    return;
  }

  MaybeRunStatusCallback(/*started=*/false, /*rescheduled=*/false);
}

void ActiveDirectoryMigrationManager::RetryToStartMigration() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  retry_already_scheduled_ = false;
  TryToStartMigration();
}

void ActiveDirectoryMigrationManager::StartPowerwash() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  local_state_->SetTime(prefs::kLastChromadMigrationAttemptTime,
                        base::Time::Now());

  // Unsigned remote powerwash requests are allowed in AD mode.
  ash::SessionManagerClient::Get()->StartRemoteDeviceWipe(em::SignedData());
}

void ActiveDirectoryMigrationManager::MaybeRunStatusCallback(bool started,
                                                             bool rescheduled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status_callback_for_testing_) {
    std::move(status_callback_for_testing_).Run(started, rescheduled);
  }
}

}  // namespace policy
