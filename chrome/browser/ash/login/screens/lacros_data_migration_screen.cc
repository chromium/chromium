// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace ash {
namespace {
constexpr char kUserActionCancel[] = "cancel";
constexpr base::TimeDelta kShowSkipButtonDuration = base::Seconds(20);

class MigratorDelegateImpl
    : public LacrosDataMigrationScreen::MigratorDelegate {
  base::OnceClosure Migrate(
      const std::string& user_id_hash,
      const base::RepeatingCallback<void(int)>& progress_callback) override {
    return BrowserDataMigrator::Migrate(
        user_id_hash, progress_callback,
        base::BindOnce(&chrome::AttemptRestart));
  }
};

}  // namespace

LacrosDataMigrationScreen::LacrosDataMigrationScreen(
    LacrosDataMigrationScreenView* view)
    : BaseScreen(LacrosDataMigrationScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(view),
      migrator_delegate_(std::make_unique<MigratorDelegateImpl>()) {
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

  // user_id_hash_ is not empty if it is already set by
  // `SetUserIdHashForTesting()`.
  if (user_id_hash_.empty()) {
    user_id_hash_ = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kBrowserDataMigrationForUser);
  }
  DCHECK(!user_id_hash_.empty()) << "user_id_hash_ should not be empty.";

  base::RepeatingCallback<void(int)> progress_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(&LacrosDataMigrationScreen::OnProgressUpdate,
                          weak_factory_.GetWeakPtr()),
      FROM_HERE);

  cancel_callback_ =
      migrator_delegate_->Migrate(user_id_hash_, progress_callback);

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
    if (cancel_callback_) {
      LOG(WARNING) << "User has cancelled the migration.";
      std::move(cancel_callback_).Run();
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

void LacrosDataMigrationScreen::SetMigratorDelegateForTesting(
    std::unique_ptr<MigratorDelegate> migrator_delegate) {
  migrator_delegate_ = std::move(migrator_delegate);
}

void LacrosDataMigrationScreen::SetUserIdHashForTesting(
    const std::string& user_id_hash) {
  user_id_hash_ = user_id_hash;
}

void LacrosDataMigrationScreen::SetSkipPostShowButtonForTesting(bool value) {
  skip_post_show_button_for_testing_ = value;
}

}  // namespace ash
