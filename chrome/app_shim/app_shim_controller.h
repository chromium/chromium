// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_
#define CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_

#include <vector>

#import <AppKit/AppKit.h>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/common/mac/app_shim_param_traits.h"
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

@class AppShimDelegate;
@class ProfileMenuTarget;

// The AppShimController is responsible for launching and maintaining the
// connection with the main Chrome process, and generally controls the lifetime
// of the app shim process.
class AppShimController : public chrome::mojom::AppShim {
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
    base::string16 app_name;
    GURL app_url;
  };

  explicit AppShimController(const Params& params);
  ~AppShimController() override;

  chrome::mojom::AppShimHost* host() const { return host_.get(); }

  // Called when the app is activated, e.g. by clicking on it in the dock, by
  // dropping a file on the dock icon, or by Cmd+Tabbing to it.
  // Returns whether the message was sent.
  bool SendFocusApp(apps::AppShimFocusType focus_type,
                    const std::vector<base::FilePath>& files);

  // Called when a profile is selected from the profiles NSMenu.
  void ProfileMenuItemSelected(uint32_t index);

 private:
  friend class TestShimClient;
  friend class apps::MachBootstrapAcceptorTest;

  // Create a channel from the Mojo |endpoint| and send a LaunchApp message.
  void CreateChannelAndSendLaunchApp(mojo::PlatformChannelEndpoint endpoint);

  // Builds main menu bar items.
  void SetUpMenu();
  void ChannelError(uint32_t custom_reason, const std::string& description);
  void BootstrapChannelError(uint32_t custom_reason,
                             const std::string& description);
  void OnShimConnectedResponse(
      apps::AppShimLaunchResult result,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver);

  // chrome::mojom::AppShim implementation.
  void CreateRemoteCocoaApplication(
      mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
          receiver) override;
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override;
  void SetBadgeLabel(const std::string& badge_label) override;
  void SetUserAttention(apps::AppShimAttentionType attention_type) override;
  void UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>
                             profile_menu_items) override;

  // Terminates the app shim process.
  void Close();

  // Returns the connection to the AppShimListener in the browser. Returns
  // an invalid endpoint if it is not available yet.
  mojo::PlatformChannelEndpoint GetBrowserEndpoint();

  // Sets up a connection to the AppShimListener at the given Mach
  // endpoint name.
  static mojo::PlatformChannelEndpoint ConnectToBrowser(
      const mojo::NamedPlatformChannel::ServerName& server_name);

  // Connects to Chrome and sends a LaunchApp message.
  void InitBootstrapPipe(mojo::PlatformChannelEndpoint endpoint);

  // If the app was launched with a specified Chrome pid, then set
  // |chrome_to_connect_to_| to this process. Otherwise, search for a running
  // Chrome instance to connect to, and if none is found to, launch Chrome and
  // set |chrome_launched_by_app_| to the launched process.
  void FindOrLaunchChrome();

  // Search for a Chrome instance holding chrome::kSingletonLockFilename.
  base::scoped_nsobject<NSRunningApplication> FindChromeFromSingletonLock()
      const;

  // Check to see if Chrome's AppShimListener has been initialized. If it
  // has, then connect.
  void PollForChromeReady(const base::TimeDelta& time_until_timeout);

  const Params params_;

  // This is the Chrome process that this app is committed to connecting to.
  // The app will quit if this process is terminated before the mojo connection
  // is established.
  // This process is determined by either:
  // - The pid specified to the app's command line (if it exists).
  // - The pid specified in the chrome::kSingletonLockFilename file.
  base::scoped_nsobject<NSRunningApplication> chrome_to_connect_to_;

  // The Chrome process that was launched by this app in FindOrLaunchChrome.
  // Note that the app is not compelled to connect to this process (consider the
  // case where multiple apps launch at the same time, and all launch their own
  // Chrome -- only one will grab the chrome::kSingletonLockFilename, and all
  // apps should connect to that).
  base::scoped_nsobject<NSRunningApplication> chrome_launched_by_app_;

  mojo::IsolatedConnection bootstrap_mojo_connection_;
  mojo::Remote<chrome::mojom::AppShimHostBootstrap> host_bootstrap_;

  mojo::Receiver<chrome::mojom::AppShim> shim_receiver_{this};
  mojo::Remote<chrome::mojom::AppShimHost> host_;
  mojo::PendingReceiver<chrome::mojom::AppShimHost> host_receiver_;

  base::scoped_nsobject<AppShimDelegate> delegate_;
  bool launch_app_done_;
  NSInteger attention_request_id_;

  // The target for NSMenuItems in the profile menu.
  base::scoped_nsobject<ProfileMenuTarget> profile_menu_target_;

  // The items in the profile menu.
  std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items_;

  DISALLOW_COPY_AND_ASSIGN(AppShimController);
};

#endif  // CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_
