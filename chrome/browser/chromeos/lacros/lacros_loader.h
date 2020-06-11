// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

// Manages download and launch of the lacros-chrome binary.
class LacrosLoader : public session_manager::SessionManagerObserver {
 public:
  // Direct getter because there are no accessors to the owning object.
  static LacrosLoader* Get();

  explicit LacrosLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager);
  LacrosLoader(const LacrosLoader&) = delete;
  LacrosLoader& operator=(const LacrosLoader&) = delete;
  ~LacrosLoader() override;

  // Returns true if the binary is ready to launch. Typical usage is to check
  // IsReady(), then if it returns false, call SetLoadCompleteCallback() to be
  // notified when the download completes.
  bool IsReady() const;

  // Sets a callback to be called when the binary download completes. The
  // download may not be successful.
  using LoadCompleteCallback = base::OnceCallback<void(bool success)>;
  void SetLoadCompleteCallback(LoadCompleteCallback callback);

  // Starts the lacros-chrome binary.
  void Start();

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  // Starting Lacros requires a hop to a background thread. The flow is
  // Start(), then StartBackground(), then StartForeground().
  //
  // StartBackground returns whether Lacros is already running.
  bool StartBackground();

  // The parameter |already_running| refers to whether the Lacros binary is
  // already launched and running.
  void StartForeground(bool already_running);

  // The path to the Lacros log file.
  static std::string LogPath();

  void OnLoadComplete(component_updater::CrOSComponentManager::Error error,
                      const base::FilePath& path);

  // Removes any state that Lacros left behind.
  void CleanUp(bool previously_installed);

  // Checks whether Lacros is already running.
  bool IsLacrosRunning();

  // May be null in tests.
  scoped_refptr<component_updater::CrOSComponentManager>
      cros_component_manager_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  // Called when the binary download completes.
  LoadCompleteCallback load_complete_callback_;

  // Process handle for the lacros-chrome process.
  // TODO(https://crbug.com/1091863): There is currently no notification for
  // when lacros-chrome is killed, so the underlying pid may be pointing at a
  // non-existent process, or a new, unrelated process with the same pid.
  base::Process lacros_process_;

  base::WeakPtrFactory<LacrosLoader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CHROMEOS_LACROS_LACROS_LOADER_H_
