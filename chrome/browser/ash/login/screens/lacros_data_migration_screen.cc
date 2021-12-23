// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace ash {
namespace {
constexpr char kUserActionCancel[] = "cancel";
constexpr base::TimeDelta kShowSkipButtonDuration = base::Seconds(20);
}  // namespace

LacrosDataMigrationScreen::LacrosDataMigrationScreen(
    LacrosDataMigrationScreenView* view)
    : BaseScreen(LacrosDataMigrationScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(view) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

LacrosDataMigrationScreen::~LacrosDataMigrationScreen() {
  if (view_)
    view_->Unbind();
}

void LacrosDataMigrationScreen::OnViewDestroyed(
    LacrosDataMigrationScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void LacrosDataMigrationScreen::ShowImpl() {
  if (!view_)
    return;

  if (!migrator_) {
    const std::string user_id_hash =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kBrowserDataMigrationForUser);

    if (user_id_hash.empty()) {
      LOG(ERROR) << "Colud not retrieve user_id_hash from switch "
                 << switches::kBrowserDataMigrationForUser
                 << ". Aborting migration.";

      chrome::AttemptRestart();
    }
    DCHECK(!user_id_hash.empty()) << "user_id_hash should not be empty.";

    base::FilePath user_data_dir;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
      LOG(ERROR) << "Could not get the original user data dir path. Aborting "
                    "migration.";
      chrome::AttemptRestart();
      return;
    }

    const base::FilePath profile_data_dir =
        user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));

    base::RepeatingCallback<void(int)> progress_callback = base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(),
        base::BindRepeating(&LacrosDataMigrationScreen::OnProgressUpdate,
                            weak_factory_.GetWeakPtr()),
        FROM_HERE);

    migrator_ = std::make_unique<BrowserDataMigratorImpl>(
        profile_data_dir, user_id_hash, progress_callback,
        base::BindOnce(&chrome::AttemptRestart),
        g_browser_process->local_state());

    migrator_->Migrate();
  }

  // Show the screen.
  view_->Show();

  GetWakeLock()->RequestWakeLock();

  // If set, do not post `SHowSkipButton()`.
  if (skip_post_show_button_for_testing_)
    return;

  // Post a delayed task to show the skip button after
  // `kShowSkipButtonDuration`.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LacrosDataMigrationScreen::ShowSkipButton,
                     weak_factory_.GetWeakPtr()),
      kShowSkipButtonDuration);
}

void LacrosDataMigrationScreen::OnProgressUpdate(int progress) {
  view_->SetProgressValue(progress);
}

void LacrosDataMigrationScreen::ShowSkipButton() {
  view_->ShowSkipButton();
}

void LacrosDataMigrationScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionCancel) {
    if (migrator_) {
      LOG(WARNING) << "User has cancelled the migration.";
      migrator_->Cancel();
    }
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void LacrosDataMigrationScreen::HideImpl() {
  GetWakeLock()->CancelWakeLock();
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

}  // namespace ash
