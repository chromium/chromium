// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "components/component_updater/component_updater_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/ui_base_types.h"

namespace component_updater {
class ComponentManagerAsh;
}  // namespace component_updater

namespace crosapi {

class BrowserLoader;

using ash::standalone_browser::LacrosSelection;
using component_updater::ComponentUpdateService;

// Manages the lifetime of lacros-chrome, and its loading status. Observes the
// component updater for future updates. This class is a part of ash-chrome.
class BrowserManager : public session_manager::SessionManagerObserver {
 public:
  // Static getter of BrowserManager instance. In real use cases,
  // BrowserManager instance should be unique in the process.
  static BrowserManager* Get();

  explicit BrowserManager(
      scoped_refptr<component_updater::ComponentManagerAsh> manager);
  // Constructor for testing.
  BrowserManager(std::unique_ptr<BrowserLoader> browser_loader,
                 ComponentUpdateService* update_service);

  BrowserManager(const BrowserManager&) = delete;
  BrowserManager& operator=(const BrowserManager&) = delete;

  ~BrowserManager() override;

  // Initialize resources and start Lacros.
  //
  // NOTE: If InitializeAndStartIfNeeded finds Lacros disabled, it unloads
  // Lacros via BrowserLoader::Unload, which also deletes the user data
  // directory.
  virtual void InitializeAndStartIfNeeded();

  // Notifies the BrowserManager that it should prepare for shutdown. This is
  // called in the early stages of ash shutdown to give Lacros sufficient time
  // for a graceful exit.
  void Shutdown();

 protected:
  // NOTE: You may have to update tests if you make changes to State, as state_
  // is exposed via autotest_private.
  enum class State {
    // Lacros is not initialized yet.
    // Lacros-chrome loading depends on user type, so it needs to wait
    // for user session.
    NOT_INITIALIZED,

    // Lacros-chrome is unavailable. I.e., failed to load for some reason
    // or disabled.
    UNAVAILABLE,
  };
  // Changes |state| value.
  void SetState(State state);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest, LacrosKeepAlive);
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest,
                           LacrosKeepAliveReloadsWhenUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest,
                           LacrosKeepAliveDoesNotBlockRestart);
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest,
                           NewWindowReloadsWhenUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest, OnLacrosUserDataDirRemoved);

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Start a sequence to clear Lacros related data. It posts a task to remove
  // Lacros user data directory and if that is successful, calls
  // `OnLacrosUserDataDirRemoved()` to clear some prefs set by Lacros in Ash.
  // Call if Lacros is disabled and not running.
  void ClearLacrosData();

  // Called as a callback to `RemoveLacrosUserDataDir()`. `cleared` is set to
  // true if the directory existed and was removed successfully.
  void OnLacrosUserDataDirRemoved(bool cleared);

  // NOTE: The state is exposed to tests via autotest_private.
  State state_ = State::NOT_INITIALIZED;

  std::unique_ptr<crosapi::BrowserLoader> browser_loader_;

  // Tracks whether Shutdown() has been signalled by ash. This flag ensures any
  // new or existing lacros startup tasks are not executed during shutdown.
  bool shutdown_requested_ = false;

  base::WeakPtrFactory<BrowserManager> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
