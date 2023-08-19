// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/mock_crosapi_app_service_proxy.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

namespace {

const char kAppId[] = "test_app";
const char kUrl[] = "https://www.google.com";
const int32_t event_flag = ui::EF_NONE;
const apps::LaunchSource launch_source = apps::LaunchSource::kFromTest;

// Expected container and disposition for ui::EF_NONE event flag;
const crosapi::mojom::LaunchContainer expected_container =
    crosapi::mojom::LaunchContainer::kLaunchContainerNone;
const crosapi::mojom::WindowOpenDisposition expected_disposition =
    crosapi::mojom::WindowOpenDisposition::kNewForegroundTab;

}  // namespace

namespace apps {

TEST(AppServiceProxyLacrosTest, Launch) {
  // Since this unit test runs without Ash, Lacros won't get the Ash
  // extension keeplist data from Ash (passed via crosapi). Therefore,
  // set empty ash keeplist for test.
  extensions::SetEmptyAshKeeplistForTest();
  base::test::SingleThreadTaskEnvironment task_environment;

  AppServiceProxy proxy(nullptr);
  MockCrosapiAppServiceProxy mock_proxy;
  proxy.SetCrosapiAppServiceProxyForTesting(&mock_proxy);

  base::RunLoop waiter;
  proxy.LaunchAppWithUrl(
      kAppId, event_flag, GURL(kUrl), launch_source,
      std::make_unique<WindowInfo>(display::kDefaultDisplayId),
      base::BindOnce(
          [](base::OnceClosure callback, LaunchResult&& result_arg) {
            EXPECT_EQ(LaunchResult::State::SUCCESS, result_arg.state);
            std::move(callback).Run();
          },
          waiter.QuitClosure()));
  mock_proxy.Wait();
  waiter.Run();

  ASSERT_EQ(mock_proxy.launched_apps().size(), 1U);
  auto& launched_app = mock_proxy.launched_apps()[0];
  EXPECT_EQ(launched_app->app_id, kAppId);
  EXPECT_EQ(launched_app->container, expected_container);
  EXPECT_EQ(launched_app->disposition, expected_disposition);
  EXPECT_EQ(launched_app->launch_source, launch_source);
  auto intent = apps_util::ConvertAppServiceToCrosapiIntent(
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, GURL(kUrl)),
      nullptr);
  EXPECT_EQ(launched_app->intent, intent);
}

TEST(AppServiceProxyLacrosTest, SetSupportedLinksPreference) {
  base::test::SingleThreadTaskEnvironment task_environment;

  AppServiceProxy proxy(nullptr);
  MockCrosapiAppServiceProxy mock_proxy;
  proxy.SetCrosapiAppServiceProxyForTesting(&mock_proxy);

  proxy.SetSupportedLinksPreference("foo");

  ASSERT_THAT(mock_proxy.supported_link_apps(), testing::ElementsAre("foo"));

  proxy.SetSupportedLinksPreference("bar");

  ASSERT_THAT(mock_proxy.supported_link_apps(),
              testing::ElementsAre("foo", "bar"));
}

}  // namespace apps
