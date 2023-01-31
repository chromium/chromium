// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/test/app_shim_listener_test_api_mac.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "base/files/file_path.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/browser_process.h"

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

}  // namespace test
