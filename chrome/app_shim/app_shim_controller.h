// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_
#define CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_

#include <vector>

#import <AppKit/AppKit.h>

#include "base/files/file_path.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "url/gurl.h"

namespace apps {
class MachBootstrapAcceptorTest;
}

namespace display {
class ScopedNativeScreen;
}

namespace mac_notifications {
class MacNotificationServiceUN;
}

@class AppShimDelegate;
@class ProfileMenuTarget;
@class ApplicationDockMenuTarget;
@protocol RenderWidgetHostViewMacDelegate;

// The AppShimController is responsible for launching and maintaining the
// connection with the main Chrome process, and generally controls the lifetime
// of the app shim process.
class AppShimController
    : public chrome::mojom::AppShim,
      public mac_notifications::mojom::MacNotificationProvider {
 public:
  struct Params {
    Params();
    Params(const Params& other);
    ~Params();
    // The full path of the user data dir.
    base::FilePath user_data_dir;
    // The relative path of the profile.
    base::FilePath profile_dir;
    std::string app_id;
    std::u16string app_name;
    GURL app_url;
    // Task runner for the IO thread, only used to guarantee no race conditions
    // when swapping out FeatureList instances in FinalizeFeatureState();
    scoped_refptr<base::SequencedTaskRunner> io_thread_runner;
  };

  explicit AppShimController(const Params& params);

  AppShimController(const AppShimController&) = delete;
  AppShimController& operator=(const AppShimController&) = delete;

  ~AppShimController() override;

  // Called early in process startup to temporarily initialize base::FeatureList
  // and field trial state with a best guess of what the state should be. This
  // gets state from the command line and/or a file in user_data_dir.
  // FeatureList and field trials are later re-initialized in
  // OnShimConnectedResponse, once communication with the correct chrome
  // instance has been established.
  static void PreInitFeatureState(const base::CommandLine& command_line);

  // Called by OnShimConnectedResponse to finish setting up FeatureList and
  // field trials for this process.
  static void FinalizeFeatureState(
      const variations::VariationsCommandLine& feature_state,
      const scoped_refptr<base::SequencedTaskRunner>& io_thread_runner);

  chrome::mojom::AppShimHost* host() const { return host_.get(); }

  // Called by AppShimDelegate in response to receiving the notification
  // -[NSApplicationDelegate applicationDidFinishLaunching:]. This kicks off
  // the initialization process (connecting to Chrome, etc).
  // `was_notification_action_launch` is set to true if this app shim was
  // launched by the OS in response to the user interacting with a
  // notification.
  void OnAppFinishedLaunching(bool launched_by_notification_action);

  // Called by AppShimDelegate in response a file being opened. If this occurs
  // before OnDidFinishLaunching, then the argument is the files that triggered
  // the launch of the app.
  void OpenFiles(const std::vector<base::FilePath>& files);

  // Called when a profile is selected from the profiles NSMenu.
  void ProfileMenuItemSelected(uint32_t index);

  // Called when a item is selected from the application dock menu.
  void CommandFromDock(uint32_t index);

  // Called when an item is selected from the application menu while no windows
  // are shown.
  void CommandDispatch(int command_id);

  // Called by AppShimDelegate in response to an URL being opened. If this
  // occurs before OnDidFinishLaunching, then the argument is the files that
  // triggered the launch of the app.
  void OpenUrls(const std::vector<GURL>& urls);

  NSMenu* GetApplicationDockMenu();

  // Called when the app is about to terminate.
  void ApplicationWillTerminate();

  // Returns the current MacNotificationService instances as a
  // MacNotificationServiceUN, or nullptr if no notification service has been
  // created yet, or if it is of the wrong type.
  mac_notifications::MacNotificationServiceUN* notification_service_un();

 private:
  friend class TestShimClient;
  friend class apps::MachBootstrapAcceptorTest;

  // The state of initialization.
  enum class InitState {
    // Waiting for OnAppFinishedLaunching to be called.
    kWaitingForAppToFinishLaunch,
    // Waiting for PollForChromeReady to connect to the browser process.
    kWaitingForChromeReady,
    // Has sent OnShimConnected to the browser process, waiting for the
    // response.
    kHasSentOnShimConnected,
    // Has received the OnShimConnected response from the browser,
    // initialization is now complete.
    kHasReceivedOnShimConnectedResponse,
  };

  // Init step 1 after OnAppFinishedLaunching. Find a running instance of Chrome
  // to connect to, or launch Chrome if none is found. Returns true if a
  // running instance was found and polling for readiness is possible.
  bool FindOrLaunchChrome();

  // Init step 2: Poll for the mach server exposed by Chrome's AppShimListener
  // to be initialized. Once it has, proceed to SendBootstrapOnShimConnected.
  // If |time_until_timeout| runs out, quit the app shim.
  void PollForChromeReady(const base::TimeDelta& time_until_timeout);

  // Init step 3: Reinterpret |endpoint| as a mojom::AppShimHostBootstrap, and
  // send the OnShimConnected message.
  void SendBootstrapOnShimConnected(mojo::PlatformChannelEndpoint endpoint);

  // Init step 4 (the last): The browser response to OnShimConnected. On
  // success, this will initialize |shim_receiver_|, through which |this| will
  // be controlled by the browser. On failure, the app will quit.
  void OnShimConnectedResponse(
      chrome::mojom::AppShimLaunchResult result,
      variations::VariationsCommandLine feature_state,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver);

  // Builds main menu bar items.
  void SetUpMenu();
  void ChannelError(uint32_t custom_reason, const std::string& description);
  void BootstrapChannelError(uint32_t custom_reason,
                             const std::string& description);

  // chrome::mojom::AppShim implementation.
  void CreateRemoteCocoaApplication(
      mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
          receiver) override;
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override;
  void SetBadgeLabel(const std::string& badge_label) override;
  void SetUserAttention(
      chrome::mojom::AppShimAttentionType attention_type) override;
  void UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>
                             profile_menu_items) override;
  void UpdateApplicationDockMenu(
      std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items)
      override;
  void BindNotificationProvider(
      mojo::PendingReceiver<mac_notifications::mojom::MacNotificationProvider>
          provider) override;
  void RequestNotificationPermission(
      RequestNotificationPermissionCallback callback) override;
  void BindChildHistogramFetcherFactory(
      mojo::PendingReceiver<metrics::mojom::ChildHistogramFetcherFactory>
          receiver) override;

  // mac_notifications::mojom::MacNotificationProvider implementation.
  void BindNotificationService(
      mojo::PendingReceiver<mac_notifications::mojom::MacNotificationService>
          service,
      mojo::PendingRemote<
          mac_notifications::mojom::MacNotificationActionHandler> handler)
      override;

  // Called when a change in the system notification permission status has been
  // detected.
  void NotificationPermissionStatusChanged(
      mac_notifications::mojom::PermissionStatus status);

  bool WebAppIsAdHocSigned() const;

  // Helper function to set up a connection to the AppShimListener at the given
  // Mach endpoint name.
  static mojo::PlatformChannelEndpoint ConnectToBrowser(
      const mojo::NamedPlatformChannel::ServerName& server_name);

  // Helper function to search for the Chrome instance holding
  // chrome::kSingletonLockFilename in the specified |user_data_dir|.
  static NSRunningApplication* FindChromeFromSingletonLock(
      const base::FilePath& user_data_dir);

  static void CreateRenderWidgetHostNSView(
      uint64_t view_id,
      mojo::ScopedInterfaceEndpointHandle host_handle,
      mojo::ScopedInterfaceEndpointHandle view_request_handle);

  static NSObject<RenderWidgetHostViewMacDelegate>* GetDelegateForHost(
      uint64_t view_id);

  const Params params_;

  // Populated by OpenFiles if it was called before OnAppFinishedLaunching
  // was called.
  std::vector<base::FilePath> launch_files_;

  // Populated by OpenUrls if it was called before OnAppFinishedLaunching
  // was called.
  std::vector<GURL> launch_urls_;

  // Populated by OnAppfinishedLaunching to indicate if this app was launched
  // as a result of a notification action.
  bool launched_by_notification_action_ = false;

  // This is the Chrome process that this app is committed to connecting to.
  // The app will quit if this process is terminated before the mojo connection
  // is established.
  // This process is determined by either:
  // - The pid specified to the app's command line (if it exists).
  // - The pid specified in the chrome::kSingletonLockFilename file.
  NSRunningApplication* __strong chrome_to_connect_to_;

  // The Chrome process that was launched by this app in FindOrLaunchChrome.
  // Note that the app is not compelled to connect to this process (consider the
  // case where multiple apps launch at the same time, and all launch their own
  // Chrome -- only one will grab the chrome::kSingletonLockFilename, and all
  // apps should connect to that).
  NSRunningApplication* __strong chrome_launched_by_app_;

  mojo::IsolatedConnection bootstrap_mojo_connection_;
  mojo::Remote<chrome::mojom::AppShimHostBootstrap> host_bootstrap_;

  mojo::Receiver<chrome::mojom::AppShim> shim_receiver_{this};
  mojo::Remote<chrome::mojom::AppShimHost> host_;
  mojo::PendingReceiver<chrome::mojom::AppShimHost> host_receiver_;

  mojo::Receiver<mac_notifications::mojom::MacNotificationProvider>
      notifications_receiver_{this};

  AppShimDelegate* __strong delegate_;

  InitState init_state_ = InitState::kWaitingForAppToFinishLaunch;

  // The target for NSMenuItems in the profile menu.
  ProfileMenuTarget* __strong profile_menu_target_;

  // The target for NSMenuItems in the application dock menu.
  ApplicationDockMenuTarget* __strong application_dock_menu_target_;

  // The screen object used in the app sim.
  std::unique_ptr<display::ScopedNativeScreen> screen_;

  // The items in the profile menu.
  std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items_;

  // The items in the application dock menu.
  std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items_;

  // MacNotificationService implementation used by Chrome to display
  // notifications in this app shim process.
  std::unique_ptr<mac_notifications::mojom::MacNotificationService>
      notification_service_;

  // Remote and receiver used for passing notification actions to the browser
  // process. The receiver end is passed to the browser process when connection
  // is established, while the remote end is passed to
  // `MacNotificationServiceUN` when it is constructed.
  mojo::PendingRemote<mac_notifications::mojom::MacNotificationActionHandler>
      notification_action_handler_remote_;
  mojo::PendingReceiver<mac_notifications::mojom::MacNotificationActionHandler>
      notification_action_handler_receiver_ =
          notification_action_handler_remote_.InitWithNewPipeAndPassReceiver();

  NSInteger attention_request_id_ = 0;
};

#endif  // CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_
