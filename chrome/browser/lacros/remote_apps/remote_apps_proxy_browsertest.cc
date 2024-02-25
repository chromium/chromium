// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros.h"
#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

using RemoteAppsLacrosBrowserTest = InProcessBrowserTest;

// Test that RemoteAppsProxyLacros::SetPinnedApps correctly handles version
// skew. Depending on the availability of corresponding mojo interface it will
// report different errors.
IN_PROC_BROWSER_TEST_F(RemoteAppsLacrosBrowserTest, SetPinnedApps) {
  chromeos::RemoteAppsProxyLacros* remote_apps_proxy =
      chromeos::RemoteAppsProxyLacrosFactory::GetInstance()
          ->GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(remote_apps_proxy);

  base::test::TestFuture<std::optional<std::string>> future;

  std::vector<std::string> ids = {"invalid id"};
  remote_apps_proxy->SetPinnedApps(
      ids, future.GetCallback<const std::optional<std::string>&>());

  std::optional<const std::string> result = future.Get();
  ASSERT_TRUE(result);
  uint32_t ash_remote_apps_version =
      remote_apps_proxy->AshRemoteAppsVersionForTests();
  if (ash_remote_apps_version <
      chromeos::remote_apps::mojom::RemoteApps::MethodMinVersions::
          kSetPinnedAppsMinVersion) {
    EXPECT_EQ(*result,
              chromeos::RemoteAppsProxyLacros::kErrorSetPinnedAppsNotAvailable);
  } else {
    // TODO(b/280039149): extract `kErrFailedToPinAnApp` from
    // chrome/browser/ash/remote_apps/remote_apps_impl.cc to some common header
    // and reuse it here instead of hardcoding the same string.
    EXPECT_EQ(*result, "Invalid app ID or corresponding app is already pinned");
  }
}
