// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_utils.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "components/webapk/webapk.pb.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace apps {

using WebApkUtilsBrowserTest = web_app::WebAppControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(WebApkUtilsBrowserTest, GetWebApk) {
  const GURL start_url =
      https_server()->GetURL("/web_share_target/charts.html");
  const GURL expected_manifest_url =
      https_server()->GetURL("/web_share_target/charts.json");
  const GURL expected_action_url =
      https_server()->GetURL("/web_share_target/share.html");
  const web_app::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), start_url);

  base::test::TestFuture<crosapi::mojom::WebApkCreationParamsPtr> future;
  GetWebApkCreationParams(profile(), app_id, future.GetCallback());

  auto webapk_creation_params = future.Take();

  webapk::WebAppManifest manifest;
  ASSERT_TRUE(manifest.ParseFromArray(
      webapk_creation_params->webapk_manifest_proto_bytes.data(),
      webapk_creation_params->webapk_manifest_proto_bytes.size()));

  const std::string& manifest_url = webapk_creation_params->manifest_url;
  EXPECT_EQ(manifest_url, expected_manifest_url.spec());
  EXPECT_EQ(manifest.short_name(), "Charts web app");
  EXPECT_EQ(manifest.start_url(), start_url.spec());
  EXPECT_FALSE(manifest.has_display_mode());

  ASSERT_EQ(manifest.icons_size(), 1);

  ASSERT_EQ(manifest.share_targets_size(), 1);
  const webapk::ShareTarget& share_target = manifest.share_targets(0);
  EXPECT_EQ(share_target.action(), expected_action_url.spec());
  EXPECT_EQ(share_target.method(), "POST");
  EXPECT_EQ(share_target.enctype(), "multipart/form-data");
  ASSERT_TRUE(share_target.has_params());
  const webapk::ShareTargetParams params = share_target.params();
  EXPECT_EQ(params.title(), "headline");
  EXPECT_EQ(params.text(), "author");
  EXPECT_EQ(params.url(), "link");
  ASSERT_EQ(params.files_size(), 3);
  {
    const webapk::ShareTargetParamsFile& params_file = params.files(0);
    EXPECT_EQ(params_file.name(), "records");
  }
  {
    const webapk::ShareTargetParamsFile& params_file = params.files(1);
    EXPECT_EQ(params_file.name(), "graphs");
  }
  {
    const webapk::ShareTargetParamsFile& params_file = params.files(2);
    EXPECT_EQ(params_file.name(), "notes");
  }
}

}  // namespace apps
