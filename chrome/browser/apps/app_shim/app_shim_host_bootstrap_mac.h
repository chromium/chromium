// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "url/gurl.h"

class AppShimHostBootstrap : public chrome::mojom::AppShimHostBootstrap {
 public:
  // The interface through which the AppShimHostBootstrap registers itself
  // with the AppShimManager.
  class Client {
   public:
    // Invoked by the AppShimHostBootstrap when a shim process has connected to
    // the browser process. This will connect to (creating, if needed) an
    // AppShimHost. |bootstrap| must have OnConnectedToHost or
    // OnFailedToConnectToHost called on it to inform the shim of the result.
    virtual void OnShimProcessConnected(
        std::unique_ptr<AppShimHostBootstrap> bootstrap) = 0;
  };

  // Set the client interface that all objects of this class will use.
  static void SetClient(Client* client);

  // Creates a new server-side mojo channel at |endpoint|, which contains a
  // a Mach port for a channel created by an MachBootstrapAcceptor, and
  // begins listening for messages on it. The audit token of the sender of
  // |endpoint| is stored in |audit_token|.
  static void CreateForChannelAndPeerAuditToken(
      mojo::PlatformChannelEndpoint endpoint,
      audit_token_t audit_token);

  AppShimHostBootstrap(const AppShimHostBootstrap&) = delete;
  AppShimHostBootstrap& operator=(const AppShimHostBootstrap&) = delete;
  ~AppShimHostBootstrap() override;

  // Called in response to connecting (or failing to connect to) an
  // AppShimHost.
  void OnConnectedToHost(
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver);
  void OnFailedToConnectToHost(chrome::mojom::AppShimLaunchResult result);

  base::ProcessId GetAppShimPid() const;
  audit_token_t GetAppShimAuditToken() const { return audit_token_; }

  mojo::PendingReceiver<chrome::mojom::AppShimHost> GetAppShimHostReceiver();
  const std::string& GetAppId() const;
  const GURL& GetAppURL();
  const base::FilePath& GetProfilePath();

  // Indicates the type of launch (by Chrome or from the app).
  chrome::mojom::AppShimLaunchType GetLaunchType() const;

  // If non-empty, holds an array of file paths given as arguments, or dragged
  // onto the app bundle or dock icon.
  const std::vector<base::FilePath>& GetLaunchFiles() const;

  // Indicates if the app launched during OS login.
  chrome::mojom::AppShimLoginItemRestoreState GetLoginItemRestoreState() const;

  // If non-empty, holds an array of urls given as arguments.
  const std::vector<GURL>& GetLaunchUrls() const;

  // Returns the notification action handler receiver (if any) that was passed
  // by the app shim on launch.
  mojo::PendingReceiver<mac_notifications::mojom::MacNotificationActionHandler>
  TakeNotificationActionHandler();

  // Returns true if this app supports multiple profiles. If so, it will not be
  // required that GetProfilePath be a valid profile path.
  bool IsMultiProfile() const;

 protected:
  explicit AppShimHostBootstrap(audit_token_t audit_token);
  void ServeChannel(mojo::PlatformChannelEndpoint endpoint);
  void ChannelError(uint32_t custom_reason, const std::string& description);

  // chrome::mojom::AppShimHostBootstrap.
  void OnShimConnected(
      mojo::PendingReceiver<chrome::mojom::AppShimHost> app_shim_host_receiver,
      chrome::mojom::AppShimInfoPtr app_shim_info,
      OnShimConnectedCallback callback) override;

  mojo::IsolatedConnection bootstrap_mojo_connection_;
  mojo::Receiver<chrome::mojom::AppShimHostBootstrap> host_bootstrap_receiver_{
      this};

  // The arguments from the OnShimConnected call, and whether or not it has
  // happened yet. The |app_shim_info_| is non-null if and only if a shim has
  // connected.
  audit_token_t audit_token_;
  mojo::PendingReceiver<chrome::mojom::AppShimHost> app_shim_host_receiver_;
  chrome::mojom::AppShimInfoPtr app_shim_info_;
  OnShimConnectedCallback shim_connected_callback_;

  THREAD_CHECKER(thread_checker_);
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_
