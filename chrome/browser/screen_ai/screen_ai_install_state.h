// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_INSTALL_STATE_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_INSTALL_STATE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"

class PrefService;

namespace screen_ai {

class ScreenAIInstallState {
 public:
  enum class State {
    // Component does not exist on device.
    kNotDownloaded,
    // Component download is in progress.
    kDownloading,
    // Either component download or initialization failed. Component load and
    // initialization may fail due to different OS or malware protection
    // restrictions, however this is expected to be quite rare.
    // Note that if library load and initialization crashes, the Failed state
    // may never be set.
    kFailed,
    // Component is downloaded but not loaded yet.
    kDownloaded,
    // Component is initialized successfully by at least one profile.
    kReady
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void StateChanged(State state) {}
    virtual void DownloadProgressChanged(double progress) {}
  };

  ScreenAIInstallState();
  ScreenAIInstallState(const ScreenAIInstallState&) = delete;
  ScreenAIInstallState& operator=(const ScreenAIInstallState&) = delete;
  virtual ~ScreenAIInstallState();

  static ScreenAIInstallState* GetInstance();

  // This function is implemented in `ScreenAIDownloaderChromeOS` and
  // `ScreenAIDownloaderNonChromeOS`.
  static std::unique_ptr<ScreenAIInstallState> Create();

  // Verifies that the library version is compatible with current Chromium
  // version. Will be used to avoid accepting the library if a newer version is
  // expected.
  static bool VerifyLibraryVersion(const std::string& version);

  // Returns true if the library is used recently and we need to keep it on
  // device and updated.
  static bool ShouldInstall(PrefService* local_state);

  // Records an UMA metric on component install/uninstall result.
  static void RecordComponentInstallationResult(bool install, bool successful);

  // Stores current time in a local state preference as the last time that the
  // service is used.
  virtual void SetLastUsageTime() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if current state is suitable for trying to download.
  bool MayTryDownload();

  // Returns true if the component is downloaded and not failed to initialize.
  bool IsComponentAvailable();

  void SetComponentReadyForTesting();

  // Sets the component state and informs the observers.
  void SetState(State state);

  // Triggers component download if it's not already downloaded or is in
  // progress.
  void DownloadComponent();

  // Called by component downloaders to set download progress.
  void SetDownloadProgress(double progress);

  // Stores the path the component folder and sets the state to ready.
  void SetComponentFolder(const base::FilePath& component_folder);

  base::FilePath get_component_binary_path() { return component_binary_path_; }

  State get_state() { return state_; }

  void ResetForTesting();
  void SetStateForTesting(State state);

 private:
  // This function depends on component updater or DLC downloader and since they
  // need have dependencies on browser thread, we need to move it to another
  // build target to avoid circular dependency.
  virtual void DownloadComponentInternal() = 0;

  base::FilePath component_binary_path_;
  State state_ = State::kNotDownloaded;

  std::vector<Observer*> observers_;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_INSTALL_STATE_H_
