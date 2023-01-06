// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/arc_data_snapshotd_manager.h"

#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/enterprise/arc_data_remove_requested_pref_handler.h"
#include "ash/components/arc/enterprise/arc_data_snapshotd_bridge.h"
#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace arc {
namespace data_snapshotd {

namespace {

// SnapshotInfo related keys.
constexpr char kOsVersion[] = "os_version";
constexpr char kCreationDate[] = "creation_date";
constexpr char kVerified[] = "verified";
constexpr char kUpdated[] = "updated";

// Snapshot related keys.
constexpr char kPrevious[] = "previous";
constexpr char kLast[] = "last";
constexpr char kBlockedUiReboot[] = "blocked_ui_reboot";
constexpr char kStarted[] = "started";

// Snapshot muss automatically expire in 30 days if not updated.
constexpr base::TimeDelta kSnapshotMaxLifetime = base::Days(30);

// Returns true if the Chrome session is restored after crash.
bool IsRestoredSession() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  return command_line->HasSwitch(ash::switches::kLoginUser) &&
         !command_line->HasSwitch(ash::switches::kLoginManager);
}

// Returns true if it is the first Chrome start up after reboot.
bool IsFirstExecAfterBoot() {
  return user_manager::UserManager::Get() &&
         user_manager::UserManager::Get()->IsFirstExecAfterBoot();
}

// Returns true if in ozone platform headless UI mode.
bool IsInHeadlessMode() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->GetSwitchValueASCII(switches::kOzonePlatform) ==
         kHeadless;
}

// Enables ozone platform headless via command line.
void EnableHeadlessMode() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kOzonePlatform, kHeadless);
  command_line->AppendSwitchASCII(switches::kUseGL,
                                  gl::kGLImplementationANGLEName);
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationSwiftShaderName);
}

// Disables D-Bus clients:
// * BIOD
// * CrosDisks
// Should be called while in BlockedUi state.
void DisableDBusClients() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  // Disable BIOD input.
  command_line->AppendSwitch(chromeos::switches::kBiodFake);
  // Disable USB input.
  command_line->AppendSwitch(chromeos::switches::kCrosDisksFake);
}

// Returns non-empty account ID string if a MGS is active.
// Otherwise returns an empty string.
std::string GetMgsCryptohomeAccountId() {
  // Take snapshots only for MGSs.
  if (user_manager::UserManager::Get() &&
      user_manager::UserManager::Get()->IsLoggedInAsPublicAccount() &&
      user_manager::UserManager::Get()->GetActiveUser()) {
    return cryptohome::Identification(user_manager::UserManager::Get()
                                          ->GetActiveUser()
                                          ->GetAccountId())
        .id();
  }
  return std::string();
}

}  // namespace

const char kHeadless[] = "headless";
const char kRestartFreconEnv[] = "RESTART_FRECON=1";

bool ArcDataSnapshotdManager::is_snapshot_enabled_for_testing_ = false;

// This class is owned by `ChromeBrowserMainPartsAsh`.
static ArcDataSnapshotdManager* g_arc_data_snapshotd_manager = nullptr;

ArcDataSnapshotdManager::SnapshotInfo::SnapshotInfo(bool is_last)
    : is_last_(is_last) {
  os_version_ = base::SysInfo::OperatingSystemVersion();
  UpdateCreationDate(base::Time::Now());
}

ArcDataSnapshotdManager::SnapshotInfo::SnapshotInfo(
    const base::Value::Dict& dict,
    bool is_last)
    : is_last_(is_last) {
  {
    auto* found = dict.FindString(kOsVersion);
    if (found)
      os_version_ = *found;
  }
  {
    auto* found = dict.Find(kCreationDate);
    if (found && base::ValueToTime(found).has_value()) {
      auto parsed_time = base::ValueToTime(found).value();
      UpdateCreationDate(parsed_time);
    }
  }
  {
    auto found = dict.FindBool(kVerified);
    if (found.has_value())
      verified_ = found.value();
  }

  {
    auto found = dict.FindBool(kUpdated);
    if (found.has_value())
      updated_ = found.value();
  }
}

ArcDataSnapshotdManager::SnapshotInfo::~SnapshotInfo() = default;

// static
std::unique_ptr<ArcDataSnapshotdManager::SnapshotInfo>
ArcDataSnapshotdManager::SnapshotInfo::CreateForTesting(
    const std::string& os_version,
    const base::Time& creation_date,
    bool verified,
    bool updated,
    bool is_last) {
  return base::WrapUnique(new ArcDataSnapshotdManager::SnapshotInfo(
      os_version, creation_date, verified, updated, is_last));
}

void ArcDataSnapshotdManager::SnapshotInfo::Sync(base::Value::Dict& dict) {
  base::Value::Dict value;
  value.Set(kOsVersion, os_version_);
  value.Set(kCreationDate, base::TimeToValue(creation_date_));
  value.Set(kVerified, verified_);
  value.Set(kUpdated, updated_);

  dict.Set(GetDictPath(), std::move(value));
}

bool ArcDataSnapshotdManager::SnapshotInfo::IsExpired() const {
  if (creation_date_ + kSnapshotMaxLifetime <= base::Time::Now()) {
    VLOG(1) << GetDictPath() << " snapshot is expired. creation_date="
            << base::UTF16ToUTF8(
                   base::TimeFormatShortDateAndTime(creation_date_));
    return true;
  }
  return false;
}

bool ArcDataSnapshotdManager::SnapshotInfo::IsOsVersionUpdated() const {
  return os_version_ != base::SysInfo::OperatingSystemVersion();
}

ArcDataSnapshotdManager::SnapshotInfo::SnapshotInfo(
    const std::string& os_version,
    const base::Time& creation_date,
    bool verified,
    bool updated,
    bool is_last)
    : is_last_(is_last),
      os_version_(os_version),
      verified_(verified),
      updated_(updated) {
  UpdateCreationDate(creation_date);
}

std::string ArcDataSnapshotdManager::SnapshotInfo::GetDictPath() const {
  return is_last_ ? kLast : kPrevious;
}

void ArcDataSnapshotdManager::SnapshotInfo::UpdateCreationDate(
    const base::Time& creation_date) {
  creation_date_ = creation_date;
  if (lifetime_timer_.IsRunning()) {
    LOG(ERROR) << "Updating a snapshot lifetime timer.";
    lifetime_timer_.Stop();
  }
  // If the snapshot is expired on initialization, it is expected to be cleared
  // soon in the flow.
  if (IsExpired())
    return;
  base::TimeDelta delay =
      creation_date_ + kSnapshotMaxLifetime - base::Time::Now();
  lifetime_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&ArcDataSnapshotdManager::SnapshotInfo::OnSnapshotExpired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcDataSnapshotdManager::SnapshotInfo::OnSnapshotExpired() {
  DCHECK(ArcDataSnapshotdManager::Get());
  ArcDataSnapshotdManager::Get()->OnSnapshotExpired();
}

ArcDataSnapshotdManager::Snapshot::Snapshot(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

ArcDataSnapshotdManager::Snapshot::~Snapshot() = default;

// static
std::unique_ptr<ArcDataSnapshotdManager::Snapshot>
ArcDataSnapshotdManager::Snapshot::CreateForTesting(
    PrefService* local_state,
    bool blocked_ui_mode,
    bool started,
    std::unique_ptr<SnapshotInfo> last_snapshot,
    std::unique_ptr<SnapshotInfo> previous_snapshot) {
  return base::WrapUnique(new ArcDataSnapshotdManager::Snapshot(
      local_state, blocked_ui_mode, started, std::move(last_snapshot),
      std::move(previous_snapshot)));
}

void ArcDataSnapshotdManager::Snapshot::Parse() {
  const base::Value::Dict& dict =
      local_state_->GetDict(arc::prefs::kArcSnapshotInfo);
  {
    const auto* found = dict.FindDict(kPrevious);
    if (found)
      previous_snapshot_ = std::make_unique<SnapshotInfo>(*found, false);
  }
  {
    const auto* found = dict.FindDict(kLast);
    if (found)
      last_snapshot_ = std::make_unique<SnapshotInfo>(*found, true);
  }
  {
    auto found = dict.FindBool(kBlockedUiReboot);
    if (found.has_value())
      blocked_ui_mode_ = found.value();
  }
  {
    auto found = dict.FindBool(kStarted);
    if (found.has_value())
      started_ = found.value();
  }
}

void ArcDataSnapshotdManager::Snapshot::Sync() {
  base::Value::Dict dict;
  if (previous_snapshot_)
    previous_snapshot_->Sync(dict);
  if (last_snapshot_)
    last_snapshot_->Sync(dict);
  dict.Set(kBlockedUiReboot, blocked_ui_mode_);
  dict.Set(kStarted, started_);
  local_state_->SetDict(arc::prefs::kArcSnapshotInfo, std::move(dict));
}

void ArcDataSnapshotdManager::Snapshot::Sync(base::OnceClosure callback) {
  Sync();
  local_state_->CommitPendingWrite(std::move(callback), base::DoNothing());
}

void ArcDataSnapshotdManager::Snapshot::ClearSnapshot(bool last) {
  std::unique_ptr<SnapshotInfo>* snapshot =
      (last ? &last_snapshot_ : &previous_snapshot_);
  if (snapshot) {
    snapshot->reset();
    Sync();
  }
}

void ArcDataSnapshotdManager::Snapshot::StartNewSnapshot() {
  // Make the last snapshot a previous one, because the new (last) snapshot is
  // going to be taken now.
  if (last_snapshot_) {
    previous_snapshot_ = std::move(last_snapshot_);
    previous_snapshot_->set_is_last(false);
    last_snapshot_ = nullptr;
  }

  started_ = true;
  Sync();
}

void ArcDataSnapshotdManager::Snapshot::OnSnapshotTaken() {
  if (last_snapshot_) {
    LOG(WARNING) << "Last snapshot exists";
    last_snapshot_.reset();
  }
  last_snapshot_ = std::make_unique<SnapshotInfo>(true /* is_last */);
  // Clear snapshot started pref to highlight that the snapshot creation process
  // is over.
  started_ = false;
}

ArcDataSnapshotdManager::SnapshotInfo*
ArcDataSnapshotdManager::Snapshot::GetCurrentSnapshot() {
  if (last_snapshot_)
    return last_snapshot_.get();

  DCHECK(previous_snapshot_);
  return previous_snapshot_.get();
}

ArcDataSnapshotdManager::Snapshot::Snapshot(
    PrefService* local_state,
    bool blocked_ui_mode,
    bool started,
    std::unique_ptr<SnapshotInfo> last_snapshot,
    std::unique_ptr<SnapshotInfo> previous_snapshot)
    : local_state_(local_state),
      blocked_ui_mode_(blocked_ui_mode),
      started_(started),
      last_snapshot_(std::move(last_snapshot)),
      previous_snapshot_(std::move(previous_snapshot)) {
  DCHECK(local_state_);
}

ArcDataSnapshotdManager::ArcDataSnapshotdManager(
    PrefService* local_state,
    std::unique_ptr<Delegate> delegate,
    base::OnceClosure attempt_user_exit_callback)
    : policy_service_{local_state},
      snapshot_{local_state},
      delegate_(std::move(delegate)),
      attempt_user_exit_callback_(std::move(attempt_user_exit_callback)) {
  DCHECK(!g_arc_data_snapshotd_manager);
  DCHECK(local_state);
  DCHECK(delegate_);

  g_arc_data_snapshotd_manager = this;

  snapshot_.Parse();
  policy_service_.AddObserver(this);

  if (IsRestoredSession()) {
    state_ = State::kRestored;
    DoClearSnapshots();
    return;
  }

  if (local_state->GetAllPrefStoresInitializationStatus() !=
      PrefService::INITIALIZATION_STATUS_SUCCESS) {
    local_state->AddPrefInitObserver(
        base::BindOnce(&ArcDataSnapshotdManager::OnLocalStateInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Ensure the snapshot's info is up-to-date.
    OnLocalStateInitialized(true /* initialized */);
  }
}

ArcDataSnapshotdManager::~ArcDataSnapshotdManager() {
  DCHECK(g_arc_data_snapshotd_manager);
  g_arc_data_snapshotd_manager = nullptr;

  if (session_controller_)
    session_controller_->RemoveObserver(this);
  policy_service_.RemoveObserver(this);

  EnsureDaemonStopped(base::DoNothing());
}

// static
ArcDataSnapshotdManager* ArcDataSnapshotdManager::Get() {
  return g_arc_data_snapshotd_manager;
}

// static
base::TimeDelta ArcDataSnapshotdManager::snapshot_max_lifetime_for_testing() {
  return kSnapshotMaxLifetime;
}

void ArcDataSnapshotdManager::EnsureDaemonStarted(base::OnceClosure callback) {
  if (bridge_) {
    std::move(callback).Run();
    return;
  }
  VLOG(1) << "Starting arc-data-snapshotd";
  daemon_weak_ptr_factory_.InvalidateWeakPtrs();
  ash::UpstartClient::Get()->StartArcDataSnapshotd(
      GetStartEnvVars(),
      base::BindOnce(&ArcDataSnapshotdManager::OnDaemonStarted,
                     daemon_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ArcDataSnapshotdManager::EnsureDaemonStopped(base::OnceClosure callback) {
  if (!bridge_) {
    std::move(callback).Run();
    return;
  }
  StopDaemon(std::move(callback));
}

void ArcDataSnapshotdManager::StartLoadingSnapshot(base::OnceClosure callback) {
  // Do not load a snapshot if it's not a normal MGS setup.
  if (state_ != State::kNone) {
    std::move(callback).Run();
    return;
  }
  std::string account_id = GetMgsCryptohomeAccountId();
  if (!account_id.empty() && IsSnapshotEnabled() &&
      (snapshot_.last_snapshot() || snapshot_.previous_snapshot())) {
    state_ = State::kLoading;
    EnsureDaemonStarted(base::BindOnce(
        &ArcDataSnapshotdManager::LoadSnapshot, weak_ptr_factory_.GetWeakPtr(),
        std::move(account_id), std::move(callback)));
    return;
  }
  std::move(callback).Run();
}

bool ArcDataSnapshotdManager::IsAutoLoginConfigured() {
  switch (state_) {
    case State::kBlockedUi:
    case State::kMgsToLaunch:
    case State::kMgsLaunched:
      return true;
    case State::kLoading:
    case State::kNone:
    case State::kRestored:
    case State::kRunning:
    case State::kStopping:
      return false;
  }
}

bool ArcDataSnapshotdManager::IsAutoLoginAllowed() {
  switch (state_) {
    case State::kBlockedUi:
    case State::kLoading:
    case State::kStopping:
      return false;
    case State::kNone:
    case State::kRestored:
    case State::kRunning:
    case State::kMgsLaunched:
    case State::kMgsToLaunch:
      return true;
  }
}

bool ArcDataSnapshotdManager::IsBlockedUiScreenShown() {
  return IsAutoLoginConfigured() && IsInHeadlessMode();
}

bool ArcDataSnapshotdManager::IsSnapshotInProgress() {
  return state_ == State::kMgsLaunched;
}
void ArcDataSnapshotdManager::OnSnapshotSessionStarted() {
  if (state_ != State::kMgsToLaunch)
    return;
  state_ = State::kMgsLaunched;
}

void ArcDataSnapshotdManager::OnSnapshotSessionStopped() {
  NOTREACHED();
}

void ArcDataSnapshotdManager::OnSnapshotSessionFailed() {
  session_controller_->RemoveObserver(this);
  session_controller_.reset();

  switch (state_) {
    case State::kMgsLaunched:
      state_ = State::kNone;
      OnSnapshotTaken(false /* success */);
      break;
    case State::kRunning:
      state_ = State::kNone;

      if (snapshot_.GetCurrentSnapshot()->is_verified()) {
        snapshot_.GetCurrentSnapshot()->set_updated(true);
      } else {
        snapshot_.ClearSnapshot(snapshot_.GetCurrentSnapshot()->is_last());
      }
      snapshot_.Sync();
      break;
    case State::kBlockedUi:
    case State::kLoading:
    case State::kMgsToLaunch:
    case State::kNone:
    case State::kRestored:
    case State::kStopping:
      NOTREACHED();
  }
}

void ArcDataSnapshotdManager::OnSnapshotAppInstalled(int percent) {
  if (state_ != State::kMgsLaunched)
    return;
  Update(percent);
}

void ArcDataSnapshotdManager::OnSnapshotSessionPolicyCompliant() {
  switch (state_) {
    case State::kMgsLaunched:
      // Stop tracking apps, since ARC is compliant with policy.
      // That means that 100% of required apps got installed and ARC is fully
      // prepared to be snapshotted.
      // If the policy changes or an app gets uninstalled, the compliance with
      // the required apps list will be fixed automatically on the next session
      // startup.
      session_controller_->RemoveObserver(this);
      session_controller_.reset();

      delegate_->RequestStopArcInstance(
          base::BindOnce(&ArcDataSnapshotdManager::OnArcInstanceStopped,
                         weak_ptr_factory_.GetWeakPtr()));

      break;
    case State::kRunning:
      snapshot_.GetCurrentSnapshot()->set_verified(true);
      snapshot_.GetCurrentSnapshot()->set_updated(false);
      snapshot_.Sync();

      session_controller_->RemoveObserver(this);
      session_controller_.reset();
      break;
    case State::kBlockedUi:
    case State::kLoading:
    case State::kMgsToLaunch:
    case State::kNone:
    case State::kRestored:
    case State::kStopping:
      break;
  }
}

void ArcDataSnapshotdManager::OnSnapshotsDisabled() {
  // Stop all ongoing flows.
  daemon_weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_.InvalidateWeakPtrs();
  switch (state_) {
    // If in process of taking or loading a snapshot, stop and restart browser.
    case State::kBlockedUi:
    case State::kLoading:
    case State::kMgsLaunched:
    case State::kMgsToLaunch:
      state_ = State::kStopping;
      snapshot_.set_blocked_ui_mode(false);
      if (session_controller_)
        session_controller_->RemoveObserver(this);
      session_controller_.reset();
      reboot_controller_.reset();
      break;
    // Otherwise, stop all flows, clear snapshots and do not restart browser.
    case State::kNone:
    case State::kRestored:
    case State::kStopping:
    case State::kRunning:
      break;
  }
  DoClearSnapshots();
}

void ArcDataSnapshotdManager::OnSnapshotUpdateEndTimeChanged() {
  if (policy_service_.snapshot_update_end_time().is_null()) {
    // Process the end of the snapshot update interval.
    if (reboot_controller_) {
      // Stop the reboot process if already requested.
      snapshot_.set_blocked_ui_mode(false);
      snapshot_.Sync();
    }
    reboot_controller_.reset();
    return;
  }
  if (!IsSnapshotEnabled())
    return;
  // Snapshot can be updated if necessary. Inside the snapshot update interval.
  // Do not request the reboot of device if already requested.
  if (reboot_controller_)
    return;
  // Do not reboot if last and previous snapshots exist and should not be
  // updated.
  if (snapshot_.last_snapshot() && !snapshot_.last_snapshot()->updated() &&
      snapshot_.previous_snapshot() &&
      !snapshot_.previous_snapshot()->updated()) {
    return;
  }

  switch (state_) {
    case State::kNone:
    case State::kLoading:
    case State::kRestored:
    case State::kRunning:
      snapshot_.set_blocked_ui_mode(true);
      snapshot_.Sync();

      // Request  device to be reboot in a blocked UI mode.
      reboot_controller_ = std::make_unique<SnapshotRebootController>(
          delegate_->CreateRebootNotification());
      return;
    case State::kBlockedUi:
    case State::kMgsToLaunch:
    case State::kMgsLaunched:
    case State::kStopping:
      // Do not reboot the device if in blocked UI mode or in process of
      // disabling the feature.
      return;
  }
}

bool ArcDataSnapshotdManager::IsSnapshotEnabled() {
  if (ArcDataSnapshotdManager::is_snapshot_enabled_for_testing())
    return true;
  return policy_service_.is_snapshot_enabled();
}

void ArcDataSnapshotdManager::OnLocalStateInitialized(bool initialized) {
  if (!initialized)
    LOG(ERROR) << "Local State intiialization failed.";

  if (snapshot_.is_blocked_ui_mode() && IsFirstExecAfterBoot() &&
      IsSnapshotEnabled()) {
    if (!IsInHeadlessMode()) {
      EnableHeadlessMode();
      DisableDBusClients();
      delegate_->RestartChrome(*base::CommandLine::ForCurrentProcess());
      return;
    }
    state_ = State::kBlockedUi;
  }
  DoClearSnapshots();
}

void ArcDataSnapshotdManager::StopDaemon(base::OnceClosure callback) {
  VLOG(1) << "Stopping arc-data-snapshotd";
  daemon_weak_ptr_factory_.InvalidateWeakPtrs();
  ash::UpstartClient::Get()->StopArcDataSnapshotd(base::BindOnce(
      &ArcDataSnapshotdManager::OnDaemonStopped,
      daemon_weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDataSnapshotdManager::DoClearSnapshots() {
  DoClearSnapshot(
      snapshot_.previous_snapshot(),
      base::BindOnce(
          &ArcDataSnapshotdManager::DoClearSnapshot,
          weak_ptr_factory_.GetWeakPtr(), snapshot_.last_snapshot(),
          base::BindOnce(&ArcDataSnapshotdManager::OnSnapshotsCleared,
                         weak_ptr_factory_.GetWeakPtr())),
      true /* success */);
}

void ArcDataSnapshotdManager::DoClearSnapshot(
    SnapshotInfo* snapshot,
    base::OnceCallback<void(bool)> callback,
    bool success) {
  if (!success)
    LOG(ERROR) << "Failed to clear snapshot";
  if (snapshot && (!IsSnapshotEnabled() || snapshot->IsExpired() ||
                   snapshot->IsOsVersionUpdated())) {
    EnsureDaemonStarted(base::BindOnce(
        &ArcDataSnapshotdManager::ClearSnapshot, weak_ptr_factory_.GetWeakPtr(),
        snapshot->is_last(), std::move(callback)));
    snapshot_.ClearSnapshot(snapshot->is_last());
  } else {
    std::move(callback).Run(success);
  }
}

void ArcDataSnapshotdManager::GenerateKeyPair() {
  if (!bridge_) {
    OnKeyPairGenerated(false /* success */);
    return;
  }
  bridge_->GenerateKeyPair(
      base::BindOnce(&ArcDataSnapshotdManager::OnKeyPairGenerated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcDataSnapshotdManager::ClearSnapshot(
    bool last,
    base::OnceCallback<void(bool)> callback) {
  if (!bridge_) {
    std::move(callback).Run(false /* success */);
    return;
  }
  bridge_->ClearSnapshot(last, std::move(callback));
}

void ArcDataSnapshotdManager::TakeSnapshot(const std::string& account_id) {
  if (!bridge_) {
    OnSnapshotTaken(false /* success */);
    return;
  }
  bridge_->TakeSnapshot(
      account_id, base::BindOnce(&ArcDataSnapshotdManager::OnSnapshotTaken,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcDataSnapshotdManager::LoadSnapshot(const std::string& account_id,
                                           base::OnceClosure callback) {
  if (!bridge_) {
    OnSnapshotLoaded(std::move(callback), false /* success */,
                     false /* last */);
    return;
  }
  bridge_->LoadSnapshot(
      account_id,
      base::BindOnce(&ArcDataSnapshotdManager::OnSnapshotLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDataSnapshotdManager::UpdateUi(int percent) {
  if (!bridge_) {
    OnUiUpdated(false /* success */);
    return;
  }
  bridge_->Update(percent, base::BindOnce(&ArcDataSnapshotdManager::OnUiUpdated,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ArcDataSnapshotdManager::OnSnapshotExpired() {
  switch (state_) {
    case State::kBlockedUi:
    case State::kMgsToLaunch:
    case State::kMgsLaunched:
    case State::kStopping:
      LOG(WARNING) << "Expired snapshots are cleared in scope of this process.";
      return;
    case State::kLoading:
      // The expired snapshot may be in the process of being loaded to the
      // running MGS. Postpone the removal until the chrome session restart.
      LOG(WARNING)
          << "The snapshot is expired while might be in use. Postpone exire.";
      return;
    case State::kNone:
    case State::kRestored:
    case State::kRunning:
      DoClearSnapshots();
      return;
  }
}

void ArcDataSnapshotdManager::OnSnapshotsCleared(bool success) {
  switch (state_) {
    case State::kBlockedUi:
      EnsureDaemonStarted(
          base::BindOnce(&ArcDataSnapshotdManager::GenerateKeyPair,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    case State::kNone:
    case State::kRestored:
    case State::kRunning:
      StopDaemon(base::DoNothing());
      return;
    case State::kStopping:
      StopDaemon(std::move(attempt_user_exit_callback_));
      return;
    case State::kLoading:
    case State::kMgsToLaunch:
    case State::kMgsLaunched:
      LOG(WARNING) << "Snapshots are cleared while in incorrect state";
      NOTREACHED();
      return;
  }
}

void ArcDataSnapshotdManager::OnKeyPairGenerated(bool success) {
  if (success) {
    VLOG(1) << "Managed Guest Session is ready to be started with blocked UI.";
    state_ = State::kMgsToLaunch;
    session_controller_ =
        SnapshotSessionController::Create(delegate_->CreateAppsTracker());
    session_controller_->AddObserver(this);

    bridge_->ConnectToUiCancelledSignal(base::BindRepeating(
        &ArcDataSnapshotdManager::OnUiClosed, weak_ptr_factory_.GetWeakPtr()));

    // Move last to previous snapshot:
    snapshot_.StartNewSnapshot();
    snapshot_.Sync();

    if (!reset_autologin_callback_.is_null())
      std::move(reset_autologin_callback_).Run();
  } else {
    LOG(ERROR) << "Key pair generation failed. Abort snapshot creation.";

    snapshot_.set_blocked_ui_mode(false);
    DCHECK(!attempt_user_exit_callback_.is_null());
    snapshot_.Sync(base::BindOnce(&ArcDataSnapshotdManager::EnsureDaemonStopped,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(attempt_user_exit_callback_)));
  }
}

void ArcDataSnapshotdManager::OnDaemonStarted(base::OnceClosure callback,
                                              bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to start arc-data-snapshotd, it might be already "
                << "running";
  } else {
    VLOG(1) << "arc-data-snapshotd started";
  }

  // The bridge has to be created regardless of a |success| value. When
  // arc-data-snapshotd is already running, it responds with an error on
  // attempt to start it.
  if (!bridge_) {
    bridge_ = std::make_unique<ArcDataSnapshotdBridge>(std::move(callback));
    DCHECK(bridge_);
  } else {
    std::move(callback).Run();
  }
}

void ArcDataSnapshotdManager::OnDaemonStopped(base::OnceClosure callback,
                                              bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to stop arc-data-snapshotd, it might be already "
                << "stopped";
  } else {
    VLOG(1) << "arc-data-snapshotd stopped";
  }
  bridge_.reset();
  std::move(callback).Run();
}

void ArcDataSnapshotdManager::Update(int percent) {
  DCHECK_EQ(state_, State::kMgsLaunched);

  EnsureDaemonStarted(base::BindOnce(&ArcDataSnapshotdManager::UpdateUi,
                                     weak_ptr_factory_.GetWeakPtr(), percent));
}

void ArcDataSnapshotdManager::OnArcInstanceStopped(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to stop ARC instance.";
    OnSnapshotTaken(false /* success */);
    return;
  }
  std::string account_id = GetMgsCryptohomeAccountId();
  if (account_id.empty() || state_ != State::kMgsLaunched) {
    LOG(ERROR) << "Cryptohome account ID is empty.";
    OnSnapshotTaken(false /* success */);
    return;
  }
  data_remove_requested_handler_ = ArcDataRemoveRequestedPrefHandler::Create(
      delegate_->GetProfilePrefService(),
      base::BindOnce(
          &ArcDataSnapshotdManager::OnUnexpectedArcDataRemoveRequested,
          weak_ptr_factory_.GetWeakPtr()));
  // OnUnexpectedArcDataRemoveRequested is called already if ARC data removal
  // is in the process (data_remove_requested_handler_ is nullptr).
  if (!data_remove_requested_handler_)
    return;

  // Take a snapshot only if ARC data removal is not requested yet.
  EnsureDaemonStarted(base::BindOnce(&ArcDataSnapshotdManager::TakeSnapshot,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(account_id)));
}

void ArcDataSnapshotdManager::OnSnapshotTaken(bool success) {
  // Stop handling ARC data remove requests.
  data_remove_requested_handler_.reset();

  if (success)
    snapshot_.OnSnapshotTaken();
  else
    LOG(ERROR) << "Failed to take ARC data directory snapshot.";

  snapshot_.set_blocked_ui_mode(false);
  DCHECK(!attempt_user_exit_callback_.is_null());

  snapshot_.Sync(base::BindOnce(&ArcDataSnapshotdManager::EnsureDaemonStopped,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(attempt_user_exit_callback_)));
}

void ArcDataSnapshotdManager::OnSnapshotLoaded(base::OnceClosure callback,
                                               bool success,
                                               bool last) {
  if (!success) {
    LOG(ERROR) << "Failed to load ARC data directory snapshot.";
    state_ = State::kNone;

    snapshot_.ClearSnapshot(false /* last */);
    snapshot_.ClearSnapshot(true /* last */);

    std::move(callback).Run();
    return;
  }
  VLOG(1) << "Successfully loaded " << (last ? "last" : "previous")
          << " snapshot";
  state_ = State::kRunning;
  // Clear last snapshot if the previous one was loaded.
  if (!last) {
    snapshot_.ClearSnapshot(true /* last */);
  }
  EnsureDaemonStopped(base::DoNothing());

  session_controller_ =
      SnapshotSessionController::Create(delegate_->CreateAppsTracker());
  session_controller_->AddObserver(this);

  std::move(callback).Run();
}

void ArcDataSnapshotdManager::OnUnexpectedArcDataRemoveRequested() {
  // Stop TakeSnapshot flow.
  weak_ptr_factory_.InvalidateWeakPtrs();

  OnSnapshotTaken(false /* success */);
}

void ArcDataSnapshotdManager::OnUiUpdated(bool success) {
  if (!success)
    LOG(ERROR) << "Failed to update UI progress bar.";
}

void ArcDataSnapshotdManager::OnUiClosed() {
  // Stop all ongoing flows.
  daemon_weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_.InvalidateWeakPtrs();
  switch (state_) {
    // If in process of taking a snapshot, stop and restart browser.
    case State::kBlockedUi:
    case State::kMgsLaunched:
    case State::kMgsToLaunch:
      state_ = State::kStopping;
      snapshot_.set_blocked_ui_mode(false);
      if (session_controller_)
        session_controller_->RemoveObserver(this);
      session_controller_.reset();
      reboot_controller_.reset();
      break;
    case State::kNone:
    case State::kLoading:
    case State::kRestored:
    case State::kStopping:
    case State::kRunning:
      LOG(ERROR) << "Received a signal from UI when not in blocked UI mode.";
      break;
  }
  snapshot_.Sync(base::BindOnce(&ArcDataSnapshotdManager::StopDaemon,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(attempt_user_exit_callback_)));
}

std::vector<std::string> ArcDataSnapshotdManager::GetStartEnvVars() {
  if (ArcDataSnapshotdManager::IsBlockedUiScreenShown())
    return {kRestartFreconEnv};
  else
    return {};
}

}  // namespace data_snapshotd
}  // namespace arc
