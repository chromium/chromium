// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/ash/login/lacros_data_migration_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace ash {
namespace {
constexpr char kUserActionSkip[] = "skip";
constexpr char kUserActionCancel[] = "cancel";
constexpr char kUserActionGotoFiles[] = "gotoFiles";
constexpr base::TimeDelta kShowSkipButtonDuration = base::Seconds(10);

// If the battery percent is lower than this ratio, and the charger is not
// connected, then the low-battery warning will be displayed.
constexpr double kInsufficientBatteryPercent = 50;
}  // namespace

LacrosDataMigrationScreen::LacrosDataMigrationScreen(
    base::WeakPtr<LacrosDataMigrationScreenView> view)
    : BaseScreen(LacrosDataMigrationScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(std::move(view)),
      attempt_restart_(base::BindRepeating(&chrome::AttemptRestart)) {
  DCHECK(view_);
}

LacrosDataMigrationScreen::~LacrosDataMigrationScreen() = default;

void LacrosDataMigrationScreen::OnViewVisible() {
  // If set, do not post `ShowSkipButton()`.
  if (skip_post_show_button_for_testing_)
    return;

  // Post a delayed task to show the skip button after
  // `kShowSkipButtonDuration`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LacrosDataMigrationScreen::ShowSkipButton,
                     weak_factory_.GetWeakPtr()),
      kShowSkipButtonDuration);
}

void LacrosDataMigrationScreen::ShowImpl() {
  if (!view_)
    return;

  if (!power_manager_subscription_.IsObserving())
    power_manager_subscription_.Observe(chromeos::PowerManagerClient::Get());
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();

  if (!migrator_) {
    const std::string user_id_hash =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kBrowserDataMigrationForUser);

    DCHECK(!user_id_hash.empty()) << "user_id_hash should not be empty.";
    if (user_id_hash.empty()) {
      LOG(ERROR) << "Could not retrieve user_id_hash from switch "
                 << switches::kBrowserDataMigrationForUser
                 << ". Aborting migration.";

      attempt_restart_.Run();
      return;
    }

    base::FilePath user_data_dir;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
      LOG(ERROR) << "Could not get the original user data dir path. Aborting "
                    "migration.";
      attempt_restart_.Run();
      return;
    }

    const base::FilePath profile_data_dir =
        user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));

    base::RepeatingCallback<void(int)> progress_callback = base::BindPostTask(
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindRepeating(&LacrosDataMigrationScreen::OnProgressUpdate,
                            weak_factory_.GetWeakPtr()),
        FROM_HERE);

    migrator_ = std::make_unique<BrowserDataMigratorImpl>(
        profile_data_dir, user_id_hash, progress_callback,
        g_browser_process->local_state());
  }

  auto mode_str = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kBrowserDataMigrationMode);

  DCHECK(!mode_str.empty()) << "mode should not be empty.";
  if (mode_str.empty()) {
    LOG(ERROR) << "Could not retrieve migration mode from switch "
               << switches::kBrowserDataMigrationMode
               << ". Aborting migration.";

    attempt_restart_.Run();
    return;
  }

  crosapi::browser_util::MigrationMode mode;
  if (mode_str == browser_data_migrator_util::kCopySwitchValue) {
    mode = crosapi::browser_util::MigrationMode::kCopy;
  } else if (mode_str == browser_data_migrator_util::kMoveSwitchValue) {
    mode = crosapi::browser_util::MigrationMode::kMove;
  } else {
    NOTREACHED() << "Unsupported mode";

    LOG(ERROR) << "Unsupported mode " << switches::kBrowserDataMigrationMode
               << " = " << mode_str << ". Aborting migration.";

    attempt_restart_.Run();
    return;
  }

  migrator_->Migrate(mode,
                     base::BindOnce(&LacrosDataMigrationScreen::OnMigrated,
                                    weak_factory_.GetWeakPtr()));

  if (LoginDisplayHost::default_host() &&
      LoginDisplayHost::default_host()->GetOobeUI()) {
    observation_.Observe(LoginDisplayHost::default_host()->GetOobeUI());
  }

  // Show the screen.
  view_->Show();

  GetWakeLock()->RequestWakeLock();
  UpdateLowBatteryStatus();
}

void LacrosDataMigrationScreen::OnProgressUpdate(int progress) {
  if (view_) {
    view_->SetProgressValue(progress);
  }
}

void LacrosDataMigrationScreen::ShowSkipButton() {
  if (view_) {
    view_->ShowSkipButton();
  }
}
void LacrosDataMigrationScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionSkip) {
    LOG(WARNING) << "User has skipped the migration.";
    if (migrator_) {
      // Here migrator should be running. Trigger to cancel, then the migrator
      // will report completion (actual completion or cancel) some time soon,
      // which triggers Chrome to restart.
      migrator_->Cancel();
    }
  } else if (action_id == kUserActionCancel) {
    attempt_restart_.Run();
  } else if (action_id == kUserActionGotoFiles) {
    // Persistent that "Go to files" button is clicked.
    // BrowserManager will take a look at the value on the next session
    // starting (i.e. after Chrome's restart), and if necessary launches
    // Files.app.
    const std::string user_id_hash =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kBrowserDataMigrationForUser);
    auto* local_state = g_browser_process->local_state();
    crosapi::browser_util::SetGotoFilesClicked(local_state, user_id_hash);
    local_state->CommitPendingWrite(
        base::BindOnce(&LacrosDataMigrationScreen::OnLocalStateCommited,
                       weak_factory_.GetWeakPtr()));
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void LacrosDataMigrationScreen::OnCurrentScreenChanged(
    OobeScreenId current_screen,
    OobeScreenId new_screen) {
  if (new_screen == LacrosDataMigrationScreenView::kScreenId) {
    OnViewVisible();
    observation_.Reset();
  }
}

void LacrosDataMigrationScreen::OnDestroyingOobeUI() {
  observation_.Reset();
}

void LacrosDataMigrationScreen::OnMigrated(BrowserDataMigrator::Result result) {
  switch (result.kind) {
    case BrowserDataMigrator::ResultKind::kSkipped:
    case BrowserDataMigrator::ResultKind::kSucceeded:
    case BrowserDataMigrator::ResultKind::kCancelled:
      attempt_restart_.Run();
      return;
    case BrowserDataMigrator::ResultKind::kFailed:
      if (view_) {
        // Goto Files button should be displayed on migration failure caused by
        // out of disk space.
        const bool show_goto_files = result.required_size.has_value();
        view_->SetFailureStatus(result.required_size, show_goto_files);
      }
      break;
  }
}

void LacrosDataMigrationScreen::OnLocalStateCommited() {
  attempt_restart_.Run();
}

void LacrosDataMigrationScreen::HideImpl() {
  GetWakeLock()->CancelWakeLock();
  power_manager_subscription_.Reset();
}

void LacrosDataMigrationScreen::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  UpdateLowBatteryStatus();
}

void LacrosDataMigrationScreen::UpdateLowBatteryStatus() {
  const absl::optional<power_manager::PowerSupplyProperties>& proto =
      chromeos::PowerManagerClient::Get()->GetLastStatus();
  if (!proto.has_value())
    return;
  if (view_) {
    view_->SetLowBatteryStatus(
        proto->battery_state() ==
            power_manager::PowerSupplyProperties_BatteryState_DISCHARGING &&
        proto->battery_percent() < kInsufficientBatteryPercent);
  }
}

device::mojom::WakeLock* LacrosDataMigrationScreen::GetWakeLock() {
  // |wake_lock_| is lazy bound and reused, even after a connection error.
  if (wake_lock_)
    return wake_lock_.get();

  mojo::PendingReceiver<device::mojom::WakeLock> receiver =
      wake_lock_.BindNewPipeAndPassReceiver();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  content::GetDeviceService().BindWakeLockProvider(
      wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventAppSuspension,
      device::mojom::WakeLockReason::kOther,
      "Profile migration is in progress...", std::move(receiver));
  return wake_lock_.get();
}

void LacrosDataMigrationScreen::SetSkipPostShowButtonForTesting(bool value) {
  skip_post_show_button_for_testing_ = value;
}

void LacrosDataMigrationScreen::SetMigratorForTesting(
    std::unique_ptr<BrowserDataMigrator> migrator) {
  migrator_ = std::move(migrator);
}

void LacrosDataMigrationScreen::SetAttemptRestartForTesting(
    const base::RepeatingClosure& attempt_restart) {
  DCHECK(!attempt_restart.is_null());
  attempt_restart_ = attempt_restart;
}

}  // namespace ash
