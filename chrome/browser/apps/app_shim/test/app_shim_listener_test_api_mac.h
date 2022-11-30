// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_TEST_APP_SHIM_LISTENER_TEST_API_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_TEST_APP_SHIM_LISTENER_TEST_API_MAC_H_

#include "base/memory/raw_ptr.h"

class AppShimListener;

namespace base {
class FilePath;
}

namespace apps {
class MachBootstrapAcceptor;
}  // namespace apps

namespace test {

class AppShimListenerTestApi {
 public:
  explicit AppShimListenerTestApi(AppShimListener* listener);
  AppShimListenerTestApi(const AppShimListenerTestApi&) = delete;
  AppShimListenerTestApi& operator=(const AppShimListenerTestApi&) = delete;

  apps::MachBootstrapAcceptor* mach_acceptor();

  const base::FilePath& directory_in_tmp();

 private:
  raw_ptr<AppShimListener> listener_;  // Not owned.
};

}  // namespace test

#endif  // CHROME_BROWSER_APPS_APP_SHIM_TEST_APP_SHIM_LISTENER_TEST_API_MAC_H_
