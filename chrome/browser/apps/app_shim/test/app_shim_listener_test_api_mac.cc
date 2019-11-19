// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/test/app_shim_listener_test_api_mac.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "base/files/file_path.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace test {

AppShimListenerTestApi::AppShimListenerTestApi(AppShimListener* listener)
    : listener_(listener) {
  DCHECK(listener_);
}

apps::MachBootstrapAcceptor* AppShimListenerTestApi::mach_acceptor() {
  return listener_->mach_acceptor_.get();
}

const base::FilePath& AppShimListenerTestApi::directory_in_tmp() {
  return listener_->directory_in_tmp_;
}

void AppShimListenerTestApi::SetExtensionAppShimHandler(
    std::unique_ptr<apps::ExtensionAppShimHandler> handler) {
  AppShimHostBootstrap::SetClient(handler.get());
  listener_->extension_app_shim_handler_.swap(handler);

  // Remove old handler from all AppLifetimeMonitors. Usually this is done at
  // profile destruction.
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    apps::AppLifetimeMonitorFactory::GetForBrowserContext(profile)
        ->RemoveObserver(handler.get());
  }
}

}  // namespace test
