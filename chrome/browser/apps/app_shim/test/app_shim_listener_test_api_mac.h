// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_TEST_APP_SHIM_LISTENER_TEST_API_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_TEST_APP_SHIM_LISTENER_TEST_API_MAC_H_

#include <memory>

#include "base/macros.h"

class AppShimListener;

namespace base {
class FilePath;
}

namespace apps {
class ExtensionAppShimHandler;
class MachBootstrapAcceptor;
}  // namespace apps

namespace test {

class AppShimListenerTestApi {
 public:
  explicit AppShimListenerTestApi(AppShimListener* listener);

  apps::MachBootstrapAcceptor* mach_acceptor();

  const base::FilePath& directory_in_tmp();

  void SetExtensionAppShimHandler(
      std::unique_ptr<apps::ExtensionAppShimHandler> handler);

 private:
  AppShimListener* listener_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(AppShimListenerTestApi);
};

}  // namespace test

#endif  // CHROME_BROWSER_APPS_APP_SHIM_TEST_APP_SHIM_LISTENER_TEST_API_MAC_H_
