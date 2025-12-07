// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/google_update_metrics_provider_mac.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_testutils.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

TEST(GoogleUpdateMetricsProviderMacTest, SetsVersions) {
  base::test::SingleThreadTaskEnvironment task_environment;
  updater::UpdateService::AppState app1;
  app1.app_id = updater::kUpdaterAppId;
  app1.version = "1.2.3.4";
  updater::UpdateService::AppState app2;
  app2.app_id = BrowserUpdaterClient::GetAppId();
  app2.ecp = BrowserUpdaterClient::GetExpectedEcp();
  app2.version = "5.6.7.8";
  {
    base::RunLoop loop;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(updater::UpdateService::Result::kSuccess,
                                 {
                                     app1,
                                     app2,
                                 }),
        updater::UpdaterScope::kUser)
        ->IsBrowserRegistered(base::BindLambdaForTesting(
            [&](bool registered) { loop.QuitWhenIdle(); }));
    loop.Run();
  }

  metrics::SystemProfileProto proto;
  GoogleUpdateMetricsProviderMac().ProvideSystemProfileMetrics(&proto);
  EXPECT_EQ(proto.google_update().client_status().version(), "5.6.7.8");
  EXPECT_EQ(proto.google_update().google_update_status().version(), "1.2.3.4");
}
