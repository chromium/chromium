// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crosapi/browser_manager_observer.h"
#include "chrome/browser/chromeos/crosapi/environment_provider.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace component_updater {
class CrOSComponentManager;
}  // namespace component_updater

namespace crosapi {

class AshChromeServiceImpl;
class BrowserLoader;
class TestMojoConnectionManager;

// Manages the lifetime of lacros-chrome, and its loading status. This class is
// a part of ash-chrome.
class BrowserManager : public session_manager::SessionManagerObserver {
 public:
  // Static getter of BrowserManager instance. In real use cases,
  // BrowserManager instance should be unique in the process.
  static BrowserManager* Get();

  explicit BrowserManager(
      scoped_refptr<component_updater::CrOSComponentManager> manager);

  BrowserManager(const BrowserManager&) = delete;
  BrowserManager& operator=(const BrowserManager&) = delete;

  ~BrowserManager() override;

  // Returns true if the binary is ready to launch or already launched.
  // Typical usage is to check IsReady(), then if it returns false,
  // call SetLoadCompleteCallback() to be notified when the download completes.
  bool IsReady() const;

  // Returns true if Lacros is in running state.
  // Virtual for testing.
  virtual bool IsRunning() const;

  // Sets a callback to be called when the binary download completes. The
  // download may not be successful.
  using LoadCompleteCallback = base::OnceCallback<void(bool success)>;
  void SetLoadCompleteCallback(LoadCompleteCallback callback);

  // Opens the browser window in lacros-chrome.
  // If lacros-chrome is not yet launched, it triggers to launch. If this is
  // called again during the setup phase of the launch process, it will be
  // ignored. This needs to be called after loading. The condition can be
  // checked IsReady(), and if not yet, SetLoadCompletionCallback can be used to
  // wait for the loading.
  // TODO(crbug.com/1101676): Notify callers the result of opening window
  // request. Because of asynchronous operations crossing processes,
  // there's no guarantee that the opening window request succeeds.
  // Currently, its condition and result are completely hidden behind this
  // class, so there's no way for callers to handle such error cases properly.
  // This design often leads the flakiness behavior of the product and testing,
  // so should be avoided.
  void NewWindow();

  // Returns true if crosapi interface supports GetFeedbackData API.
  bool GetFeedbackDataSupported() const;

  using GetFeedbackDataCallback = base::OnceCallback<void(base::Value)>;
  // Gathers Lacros feedback data.
  // Virtual for testing.
  virtual void GetFeedbackData(GetFeedbackDataCallback callback);

  // Returns true if crosapi interface supports GetHistograms API.
  bool GetHistogramsSupported() const;

  using GetHistogramsCallback = base::OnceCallback<void(const std::string&)>;
  // Gets Lacros histograms.
  void GetHistograms(GetHistogramsCallback callback);

  // Returns true if crosapi interface supports GetActiveTabUrl API.
  bool GetActiveTabUrlSupported() const;

  using GetActiveTabUrlCallback =
      base::OnceCallback<void(const base::Optional<GURL>&)>;
  // Gets Url of the active tab from lacros if there is any.
  void GetActiveTabUrl(GetActiveTabUrlCallback callback);

  void AddObserver(BrowserManagerObserver* observer);
  void RemoveObserver(BrowserManagerObserver* observer);

  const std::string& lacros_version() const { return lacros_version_; }
  void set_lacros_version(const std::string& version) {
    lacros_version_ = version;
  }

 protected:
  // Notifies Mojo connection to lacros-chrome has been disconnected.
  void NotifyMojoDisconnected();

 private:
  enum class State {
    // Lacros is not initialized yet.
    // Lacros-chrome loading depends on user type, so it needs to wait
    // for user session.
    NOT_INITIALIZED,

    // User session started, and now it's loading (downloading and installing)
    // lacros-chrome.
    LOADING,

    // Lacros-chrome is unavailable. I.e., failed to load for some reason
    // or disabled.
    UNAVAILABLE,

    // Lacros-chrome is loaded and ready for launching.
    STOPPED,

    // Lacros-chrome is creating a new log file to log to.
    CREATING_LOG_FILE,

    // Lacros-chrome is launching.
    STARTING,

    // Mojo connection to lacros-chrome is established so, it's in
    // the running state.
    RUNNING,

    // Lacros-chrome is being terminated soon.
    TERMINATING,
  };

  // Posts CreateLogFile() and StartWithLogFile() to the thread pooll.
  void Start();

  // Starts the lacros-chrome process and redirects stdout/err to file pointed
  // by logfd.
  void StartWithLogFile(base::ScopedFD logfd);

  // Called when PendingReceiver of AshChromeService is passed from
  // lacros-chrome.
  void OnAshChromeServiceReceiverReceived(
      mojo::PendingReceiver<crosapi::mojom::AshChromeService> pending_receiver);

  // Called when the Mojo connection to lacros-chrome is disconnected.
  // It may be "just a Mojo error" or "lacros-chrome crash".
  // In either case, terminates lacros-chrome, because there's no longer a
  // way to communicate with lacros-chrome.
  void OnMojoDisconnected();

  // Called when lacros-chrome is terminated and successfully wait(2)ed.
  void OnLacrosChromeTerminated();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Called on load completion.
  void OnLoadComplete(const base::FilePath& path);

  // Callback of QueryVersion for LacrosChromeService.
  void OnLacrosChromeServiceVersionReady(uint32_t version);

  State state_ = State::NOT_INITIALIZED;

  // May be null in tests.
  scoped_refptr<component_updater::CrOSComponentManager> component_manager_;

  std::unique_ptr<crosapi::BrowserLoader> browser_loader_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  // Version of lacros-chrome displayed to user in feedback report, etc.
  // It includes both browser version and channel in the format of:
  // {browser version} {channel}
  // For example, "87.0.0.1 dev", "86.0.4240.38 beta".
  std::string lacros_version_;

  // Version of LacrosChromeService mojo interface.
  uint32_t lacros_chrome_service_version_ = 0;

  // Called when the binary download completes.
  LoadCompleteCallback load_complete_callback_;

  // Time when the lacros process was launched.
  base::TimeTicks lacros_launch_time_;

  // Process handle for the lacros-chrome process.
  base::Process lacros_process_;

  // Proxy to LacrosChromeService mojo service in lacros-chrome.
  // Available during lacros-chrome is running.
  mojo::Remote<crosapi::mojom::LacrosChromeService> lacros_chrome_service_;

  // Implementation of AshChromeService Mojo APIs.
  // Instantiated on receiving the PendingReceiver from lacros-chrome.
  std::unique_ptr<AshChromeServiceImpl> ash_chrome_service_;

  // Helps set up and manage the mojo connections between lacros-chrome and
  // ash-chrome in testing environment. Only applicable when
  // '--lacros-mojo-socket-for-testing' is present in the command line.
  std::unique_ptr<TestMojoConnectionManager> test_mojo_connection_manager_;

  // Used to pass ash-chrome specific flags/configurations to lacros-chrome.
  std::unique_ptr<EnvironmentProvider> environment_provider_;

  base::ObserverList<BrowserManagerObserver> observers_;

  base::WeakPtrFactory<BrowserManager> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_MANAGER_H_
