// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {
void SetUserRemovedLoginItemPrefOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrefService* service = g_browser_process->local_state();
  service->SetBoolean(prefs::kUserRemovedLoginItem, true);
}

void SetCreatedLoginItemPrefOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrefService* service = g_browser_process->local_state();
  service->SetBoolean(prefs::kChromeCreatedLoginItem, true);
}

void DisableLaunchOnStartupOnWorkerThread() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If the LoginItem is not hidden, it means it's user created, so don't
  // delete it.
  bool is_hidden = false;
  if (base::mac::CheckLoginItemStatus(&is_hidden) && is_hidden)
    base::mac::RemoveFromLoginItems();
}

void CheckForUserRemovedLoginItemOnWorkerThread() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::mac::CheckLoginItemStatus(NULL)) {
    // There's no LoginItem, so set the kUserRemovedLoginItem pref.
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(SetUserRemovedLoginItemPrefOnUIThread));
  }
}

void EnableLaunchOnStartupOnWorkerThread(bool need_migration) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (need_migration) {
    // This is the first time running Chrome since the kChromeCreatedLoginItem
    // pref was added. Initialize the status of this pref based on whether
    // there is already a hidden login item.
    bool is_hidden = false;
    if (base::mac::CheckLoginItemStatus(&is_hidden)) {
      if (is_hidden) {
      // We already have a hidden login item, so set the kChromeCreatedLoginItem
      // flag.
      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(SetCreatedLoginItemPrefOnUIThread));
      }
      // LoginItem already exists - just exit.
      return;
    }
  }

  // Check if Chrome is already a Login Item - if not, create one.
  if (!base::mac::CheckLoginItemStatus(NULL)) {
    // Call back to the UI thread to set our preference so we know that Chrome
    // created the login item (which means we are allowed to delete it later).
    // There's a race condition here if the user disables launch on startup
    // before our callback is run, but the user can manually disable
    // "Open At Login" via the dock if this happens.
    base::mac::AddToLoginItems(true);  // Hide on startup.
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(SetCreatedLoginItemPrefOnUIThread));
  }
}

}  // namespace

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // LoginItems are associated with an executable, not with a specific
  // user-data-dir, so only mess with the LoginItem when running with the
  // default user-data-dir. So if a user is running multiple instances of
  // Chrome with different user-data-dirs, they won't conflict in their
  // use of LoginItems.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserDataDir))
    return;

  // There are a few cases we need to handle:
  //
  // 1) Chrome is transitioning to "launch on startup" state, and there's no
  // login item currently. We create a new item if the kUserRemovedLoginItem
  // and kChromeCreatedLoginItem flags are already false, and set the
  // kChromeCreatedLoginItem flag to true. If kChromeCreatedLoginItem is
  // already set (meaning that we created a login item that has since been
  // deleted) then we will set the kUserRemovedLoginItem so we do not create
  // login items in the future.
  //
  // 2) Chrome is transitioning to the "do not launch on startup" state. If
  // the kChromeCreatedLoginItem flag is false, we do nothing. Otherwise, we
  // will delete the login item if it's present, and not we will set
  // kUserRemovedLoginItem to true to prevent future login items from being
  // created.
  if (should_launch) {
    PrefService* service = g_browser_process->local_state();
    // If the user removed the login item, don't ever create another one.
    if (service->GetBoolean(prefs::kUserRemovedLoginItem))
      return;

    if (service->GetBoolean(prefs::kChromeCreatedLoginItem)) {
      DCHECK(service->GetBoolean(prefs::kMigratedLoginItemPref));
      // If we previously created a login item, we don't need to create
      // a new one - just check to see if the user removed it so we don't
      // ever create another one.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(CheckForUserRemovedLoginItemOnWorkerThread));
    } else {
      bool need_migration = !service->GetBoolean(
          prefs::kMigratedLoginItemPref);
      service->SetBoolean(prefs::kMigratedLoginItemPref, true);
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(EnableLaunchOnStartupOnWorkerThread, need_migration));
    }
  } else {
    PrefService* service = g_browser_process->local_state();
    // If Chrome didn't create any login items, just exit.
    if (!service->GetBoolean(prefs::kChromeCreatedLoginItem))
      return;

    // Clear the pref now that we're removing the login item.
    service->ClearPref(prefs::kChromeCreatedLoginItem);

    // If the user removed our login item, note this so we don't ever create
    // another one.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(CheckForUserRemovedLoginItemOnWorkerThread));

    // Call to the File thread to remove the login item since it requires
    // accessing the disk.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(DisableLaunchOnStartupOnWorkerThread));
  }
}

void BackgroundModeManager::DisplayClientInstalledNotification(
    const base::string16& name) {
  // TODO(atwilson): Display a platform-appropriate notification here.
  // http://crbug.com/74970
}

scoped_refptr<base::SequencedTaskRunner>
BackgroundModeManager::CreateTaskRunner() {
  return base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}
