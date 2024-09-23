// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_launch.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "components/metrics/histogram_child_process.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace apps {
using ShimLaunchedCallback = base::OnceCallback<void(base::Process)>;
using ShimTerminatedCallback = base::OnceClosure;
}  // namespace apps

namespace remote_cocoa {
class ApplicationHost;
}  // namespace remote_cocoa

class AppShimHostBootstrap;

// This is the counterpart to AppShimController in
// chrome/app/chrome_main_app_mode_mac.mm. The AppShimHost is owned by the
// AppShimManager, which implements its client interface.
class AppShimHost : public chrome::mojom::AppShimHost,
                    public metrics::HistogramChildProcess {
 public:
  // The interface through which the AppShimHost interacts with
  // AppShimManager.
  class Client {
   public:
    // Request that the handler launch the app shim process.
    virtual void OnShimLaunchRequested(
        AppShimHost* host,
        web_app::LaunchShimUpdateBehavior update_behavior,
        web_app::ShimLaunchMode launch_mode,
        apps::ShimLaunchedCallback launched_callback,
        apps::ShimTerminatedCallback terminated_callback) = 0;

    // Invoked by the shim host when the connection to the shim process is
    // closed. This is also called when we give up on trying to get a shim to
    // connect.
    virtual void OnShimProcessDisconnected(AppShimHost* host) = 0;

    // Invoked by the shim host when the shim process receives a focus event.
    virtual void OnShimFocus(AppShimHost* host) = 0;

    // Invoked by the shim host when the shim process should reopen if needed.
    virtual void OnShimReopen(AppShimHost* host) = 0;

    // Invoked by the shim host when the shim opens a file, e.g, by dragging
    // a file onto the dock icon.
    virtual void OnShimOpenedFiles(
        AppShimHost* host,
        const std::vector<base::FilePath>& files) = 0;

    // Invoked when a profile is selected from the menu bar.
    virtual void OnShimSelectedProfile(AppShimHost* host,
                                       const base::FilePath& profile_path) = 0;

    //
    virtual void OnShimOpenedAppSettings(AppShimHost* host) = 0;

    // Invoked by the shim host when the shim opens a url, e.g, clicking a link
    // in mail.
    virtual void OnShimOpenedUrls(AppShimHost* host,
                                  const std::vector<GURL>& urls) = 0;

    // Invoked by the shim host when the app should be opened with an override
    // url (e.g. user clicks on an item in the application dock menu).
    virtual void OnShimOpenAppWithOverrideUrl(AppShimHost* host,
                                              const GURL& override_url) = 0;

    // Invoked by the shim host when the app is about to terminate (for example
    // because the user quit it).
    virtual void OnShimWillTerminate(AppShimHost* host) = 0;

    // Invoked by the shim host when a change to the system level notification
    // permission status has been detected.
    virtual void OnNotificationPermissionStatusChanged(
        AppShimHost* host,
        mac_notifications::mojom::PermissionStatus status) = 0;
  };

  AppShimHost(Client* client,
              const std::string& app_id,
              const base::FilePath& profile_path,
              bool uses_remote_views);

  AppShimHost(const AppShimHost&) = delete;
  AppShimHost& operator=(const AppShimHost&) = delete;
  ~AppShimHost() override;

  bool UsesRemoteViews() const { return uses_remote_views_; }

  // Returns true if an AppShimHostBootstrap has already connected to this
  // host.
  bool HasBootstrapConnected() const;

  // Invoked to request that the shim be launched (if it has not been launched
  // already).
  void LaunchShim(
      web_app::ShimLaunchMode launch_mode = web_app::ShimLaunchMode::kNormal);

  // Invoked when the app shim has launched and connected to the browser.
  virtual void OnBootstrapConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap);

  // Functions to allow the handler to determine which app this host corresponds
  // to.
  base::FilePath GetProfilePath() const;
  std::string GetAppId() const;

  // Return the factory to use to create new widgets in the same process.
  remote_cocoa::ApplicationHost* GetRemoteCocoaApplicationHost() const;

  // Return the app shim interface. Virtual for tests.
  virtual chrome::mojom::AppShim* GetAppShim() const;

  void SetOnShimConnectedForTesting(base::OnceClosure closure);

  // Returns kNullProcessId if no process has connected to this host yet.
  base::ProcessId GetAppShimPid() const;

 protected:
  void ChannelError(uint32_t custom_reason, const std::string& description);

  // Helper function to launch the app shim process.
  void LaunchShimInternal(web_app::LaunchShimUpdateBehavior update_behavior,
                          web_app::ShimLaunchMode launch_mode);

  // Called when LaunchShim has launched (or failed to launch) a process.
  void OnShimProcessLaunched(web_app::LaunchShimUpdateBehavior update_behavior,
                             web_app::ShimLaunchMode launch_mode,
                             base::Process shim_process);

  // Called when a shim process returned via OnShimLaunchCompleted has
  // terminated.
  void OnShimProcessTerminated(
      web_app::LaunchShimUpdateBehavior update_behavior,
      web_app::ShimLaunchMode launch_mode);

  // chrome::mojom::AppShimHost.
  void FocusApp() override;
  void ReopenApp() override;
  void FilesOpened(const std::vector<base::FilePath>& files) override;
  void ProfileSelectedFromMenu(const base::FilePath& profile_path) override;
  void OpenAppSettings() override;
  void UrlsOpened(const std::vector<GURL>& urls) override;
  void OpenAppWithOverrideUrl(const GURL& override_url) override;
  void EnableAccessibilitySupport(
      chrome::mojom::AppShimScreenReaderSupportMode mode) override;
  void ApplicationWillTerminate() override;
  void NotificationPermissionStatusChanged(
      mac_notifications::mojom::PermissionStatus status) override;

  // content::HistogramChildProcess:
  void BindChildHistogramFetcherFactory(
      mojo::PendingReceiver<metrics::mojom::ChildHistogramFetcherFactory>
          factory) override;

  // Weak, owns |this|.
  const raw_ptr<Client> client_;

  mojo::Receiver<chrome::mojom::AppShimHost> host_receiver_{this};
  mojo::Remote<chrome::mojom::AppShim> app_shim_;
  mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver_;

  // Only allow LaunchShim to have any effect on the first time it is called. If
  // that launch fails, it will re-launch (requesting that the shim be
  // re-created).
  bool launch_shim_has_been_called_ = false;

  std::unique_ptr<AppShimHostBootstrap> bootstrap_;

  std::unique_ptr<remote_cocoa::ApplicationHost> remote_cocoa_application_host_;

  std::string app_id_;
  base::OnceClosure on_shim_connected_for_testing_;
  base::FilePath profile_path_;
  const bool uses_remote_views_;

  // Not a system-level PID, rather an ID assigned by content::ChildProcessHost
  // used to identify this process when registering with
  // metrics::SubprocessMetricsProvider.
  const int child_process_host_id_;

  // This holds the histogram allocator to be used for this app shim before it
  // gets passed to the remote host when it finished launching.
  std::unique_ptr<base::PersistentMemoryAllocator> histogram_allocator_;

  // This class is only ever to be used on the UI thread.
  THREAD_CHECKER(thread_checker_);

  // This weak factory is used for launch callbacks only.
  base::WeakPtrFactory<AppShimHost> launch_weak_factory_;
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
