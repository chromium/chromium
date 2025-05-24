// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_util.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/files_app_launcher.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-shared.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "components/crash/core/common/crash_key.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/device_ownership_waiter.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/user_prefs/user_prefs.h"
#include "components/version_info/version_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/screen.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

// TODO(crbug.com/40703689): Currently, this source has log spamming
// by LOG(WARNING) for non critical errors to make it easy
// to debug and develop. Get rid of the log spamming
// when it gets stable enough.

namespace crosapi {

namespace {

// Pointer to the global instance of BrowserManager.
BrowserManager* g_instance = nullptr;

base::FilePath GetUserDataDir() {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // NOTE: On device this function is privacy/security sensitive. The
    // directory must be inside the encrypted user partition.
    return base::FilePath(crosapi::kLacrosUserDataPath);
  }
  // For developers on Linux desktop, put the directory under the developer's
  // specified --user-data-dir.
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &base_path);
  return base_path.Append("lacros");
}

bool RemoveLacrosUserDataDir() {
  const base::FilePath lacros_data_dir = GetUserDataDir();
  return base::PathExists(lacros_data_dir) &&
         base::DeletePathRecursively(lacros_data_dir);
}

}  // namespace

// static
BrowserManager* BrowserManager::Get() {
  return g_instance;
}

BrowserManager::BrowserManager() {
  DCHECK(!g_instance);
  g_instance = this;

  // Wait to query the flag until the user has entered the session. Enterprise
  // devices restart Chrome during login to apply flags. We don't want to run
  // the flag-off cleanup logic until we know we have the final flag state.
  if (session_manager::SessionManager::Get()) {
    session_manager::SessionManager::Get()->AddObserver(this);
  }

  if (!CrosapiManager::IsInitialized()) {
    CHECK_IS_TEST();
  }
}

BrowserManager::~BrowserManager() {
  // Unregister, just in case the manager is destroyed before
  // OnUserSessionStarted() is called.
  if (session_manager::SessionManager::Get()) {
    session_manager::SessionManager::Get()->RemoveObserver(this);
  }

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void BrowserManager::InitializeAndStartIfNeeded() {
  DCHECK_EQ(state_, State::NOT_INITIALIZED);

  // Ensure this isn't run multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  SetState(State::UNAVAILABLE);
  ClearLacrosData();
}

void BrowserManager::SetState(State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
}

void BrowserManager::ClearLacrosData() {
  // Check that Lacros is not running.
  CHECK_EQ(state_, State::UNAVAILABLE);
  // Skip if Chrome is in safe mode to avoid deleting
  // user data when Lacros is disabled only temporarily.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSafeMode)) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(RemoveLacrosUserDataDir),
      base::BindOnce(&BrowserManager::OnLacrosUserDataDirRemoved,
                     weak_factory_.GetWeakPtr()));
}

void BrowserManager::OnLacrosUserDataDirRemoved(bool cleared) {
  if (!cleared) {
    // Do nothing if Lacros user data dir did not exist or could not be deleted.
    return;
  }

  LOG(WARNING) << "Lacros user data directory was cleared. Now clearing lacros "
                  "related prefs.";

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user) {
    CHECK_IS_TEST();
    return;
  }
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user);
  if (!context) {
    CHECK_IS_TEST();
    return;
  }

  // Do a one time clearing of `kUserUninstalledPreinstalledWebAppPref`. This is
  // because some users who had Lacros enabled before M114 had this pref set by
  // accident for preinstalled web apps such as Calendar or Gmail. Without
  // clearing this pref, if users disable Lacros, these apps will be considered
  // uninstalled (and cannot easily be reinstalled). Note that this means that
  // some users who intentionally uninstalled these apps on Lacros will find
  // these apps reappear until they unistall them again.
  PrefService* pref_service = user_prefs::UserPrefs::Get(context);
  web_app::UserUninstalledPreinstalledWebAppPrefs(pref_service).ClearAllApps();
}

void BrowserManager::OnSessionStateChanged() {
  TRACE_EVENT0("login", "BrowserManager::OnSessionStateChanged");

  // Wait for session to become active.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager->session_state() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  if (state_ == State::NOT_INITIALIZED) {
    InitializeAndStartIfNeeded();
  }
}

}  // namespace crosapi
