// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"

class AppShimHostBootstrap : public chrome::mojom::AppShimHostBootstrap {
 public:
  // Creates a new server-side mojo channel at |endpoint|, which should contain
  // a file descriptor of a channel created by an UnixDomainSocketAcceptor, and
  // begins listening for messages on it.
  static void CreateForChannel(mojo::PlatformChannelEndpoint endpoint);
  ~AppShimHostBootstrap() override;

  void OnLaunchAppComplete(apps::AppShimLaunchResult result,
                           chrome::mojom::AppShimRequest app_shim_request);

  chrome::mojom::AppShimHostRequest GetLaunchAppShimHostRequest();
  apps::AppShimLaunchType GetLaunchType() const;
  const std::vector<base::FilePath>& GetLaunchFiles() const;

  apps::AppShimHandler::Host* GetHostForTesting();

 protected:
  AppShimHostBootstrap();
  void ServeChannel(mojo::PlatformChannelEndpoint endpoint);
  void ChannelError(uint32_t custom_reason, const std::string& description);

  // chrome::mojom::AppShimHostBootstrap.
  void LaunchApp(chrome::mojom::AppShimHostRequest app_shim_host_request,
                 const base::FilePath& profile_dir,
                 const std::string& app_id,
                 apps::AppShimLaunchType launch_type,
                 const std::vector<base::FilePath>& files,
                 LaunchAppCallback callback) override;

  mojo::IsolatedConnection bootstrap_mojo_connection_;
  mojo::Binding<chrome::mojom::AppShimHostBootstrap> host_bootstrap_binding_;

  // The arguments from the LaunchApp call, and whether or not it has happened
  // yet.
  bool has_received_launch_app_ = false;
  chrome::mojom::AppShimHostRequest app_shim_host_request_;
  apps::AppShimLaunchType launch_type_;
  std::vector<base::FilePath> files_;
  LaunchAppCallback launch_app_callback_;

  // The AppShimHost that has taken ownership of this object. Weak, set in
  // LaunchApp.
  AppShimHost* connected_host_ = nullptr;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(AppShimHostBootstrap);
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_
