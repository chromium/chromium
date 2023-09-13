// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using content::BrowserThread;

const char kAppInstalledNotifierId[] = "background-mode.app-installed";

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // This functionality is only defined for default profile, currently.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserDataDir))
    return;
  task_runner_->PostTask(
      FROM_HERE,
      should_launch
          ? base::BindOnce(auto_launch_util::EnableBackgroundStartAtLogin)
          : base::BindOnce(auto_launch_util::DisableBackgroundStartAtLogin));
}

void BackgroundModeManager::DisplayClientInstalledNotification(
    const std::u16string& name) {
  // Create a status tray notification balloon explaining to the user what has
  // been installed.
  CreateStatusTrayIcon();
  status_icon_->DisplayBalloon(
      gfx::ImageSkia(),
      l10n_util::GetStringUTF16(IDS_BACKGROUND_APP_INSTALLED_BALLOON_TITLE),
      l10n_util::GetStringFUTF16(IDS_BACKGROUND_APP_INSTALLED_BALLOON_BODY,
                                 name,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kAppInstalledNotifierId));
}

scoped_refptr<base::SequencedTaskRunner>
BackgroundModeManager::CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}
