// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps_publisher_host.h"

#include <iterator>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace apps {

class MockAppPublisher : public crosapi::mojom::AppPublisher {
 public:
  MockAppPublisher() { run_loop_ = std::make_unique<base::RunLoop>(); }
  ~MockAppPublisher() override = default;

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  const std::vector<apps::mojom::AppPtr>& get_deltas() const { return deltas_; }

 private:
  // crosapi::mojom::AppPublisher:
  void OnApps(std::vector<apps::mojom::AppPtr> deltas) override {
    deltas_.insert(deltas_.end(), std::make_move_iterator(deltas.begin()),
                   std::make_move_iterator(deltas.end()));
    run_loop_->Quit();
  }

  std::vector<apps::mojom::AppPtr> deltas_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

using WebAppsPublisherHostBrowserTest = web_app::WebAppControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, PublishApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  web_app::InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));
  web_app::InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/web_share_target/charts.html"));
  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost::SetPublisherForTesting(&mock_app_publisher);

  WebAppsPublisherHost web_apps_publisher_host(profile());
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);

  web_app::AppId app_id = web_app::InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  mock_app_publisher.Wait();
  // We may receive more than one delta for the new app.
  EXPECT_GE(mock_app_publisher.get_deltas().size(), 3U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->readiness,
            apps::mojom::Readiness::kReady);

  {
    base::RunLoop run_loop;
    web_app::UninstallWebAppWithCallback(
        profile(), app_id,
        base::BindLambdaForTesting([&run_loop](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
    mock_app_publisher.Wait();
    EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
    EXPECT_EQ(mock_app_publisher.get_deltas().back()->readiness,
              apps::mojom::Readiness::kUninstalledByUser);
  }
}

}  // namespace apps
