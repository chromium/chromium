// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace crosapi {

namespace {

// The default user-data-directory for Lacros.
constexpr char kLacrosUserDataPath[] = "/home/chronos/user/lacros";

// Pointer to the global instance of BrowserManager.
BrowserManager* g_instance = nullptr;

base::FilePath GetUserDataDir() {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // NOTE: On device this function is privacy/security sensitive. The
    // directory must be inside the encrypted user partition.
    return base::FilePath(kLacrosUserDataPath);
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
