// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_base.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

using apps::mojom::Condition;
using apps::mojom::ConditionType;
using apps::mojom::PatternMatchType;

namespace apps {

namespace {

void CheckUrlScopeFilter(const apps::mojom::IntentFilterPtr& intent_filter,
                         const GURL& url,
                         const GURL& different_url) {
  EXPECT_FALSE(intent_filter->activity_name.has_value());
  EXPECT_FALSE(intent_filter->activity_label.has_value());

  ASSERT_EQ(intent_filter->conditions.size(), 4U);

  {
    const Condition& condition = *intent_filter->conditions[0];
    EXPECT_EQ(condition.condition_type, ConditionType::kAction);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, "view");
  }

  {
    const Condition& condition = *intent_filter->conditions[1];
    EXPECT_EQ(condition.condition_type, ConditionType::kScheme);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, url.scheme());
  }

  {
    const Condition& condition = *intent_filter->conditions[2];
    EXPECT_EQ(condition.condition_type, ConditionType::kHost);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, url.host());
  }

  {
    const Condition& condition = *intent_filter->conditions[3];
    EXPECT_EQ(condition.condition_type, ConditionType::kPattern);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kPrefix);
    EXPECT_EQ(condition.condition_values[0]->value, url.path());
  }

  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(url), intent_filter));

  EXPECT_FALSE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(different_url), intent_filter));
}

void CheckShareTextFilter(const apps::mojom::IntentFilterPtr& intent_filter) {
  EXPECT_FALSE(intent_filter->activity_name.has_value());
  EXPECT_FALSE(intent_filter->activity_label.has_value());

  ASSERT_EQ(intent_filter->conditions.size(), 2U);

  {
    const Condition& condition = *intent_filter->conditions[0];
    EXPECT_EQ(condition.condition_type, ConditionType::kAction);
    ASSERT_EQ(condition.condition_values.size(), 1U);

    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, "send");
  }

  const Condition& condition = *intent_filter->conditions[1];
  EXPECT_EQ(condition.condition_type, ConditionType::kMimeType);
  ASSERT_EQ(condition.condition_values.size(), 1U);

  EXPECT_EQ(condition.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(condition.condition_values[0]->value, "text/plain");

  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateShareIntentFromText("text", "title"), intent_filter));

  std::vector<GURL> filesystem_urls(1U);
  std::vector<std::string> mime_types(1U, "audio/mp3");
  EXPECT_FALSE(apps_util::IntentMatchesFilter(
      apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types),
      intent_filter));
}

void CheckShareFileFilter(const apps::mojom::IntentFilterPtr& intent_filter,
                          const std::vector<std::string>& filter_types,
                          const std::vector<std::string>& accepted_types,
                          const std::vector<std::string>& rejected_types) {
  EXPECT_FALSE(intent_filter->activity_name.has_value());
  EXPECT_FALSE(intent_filter->activity_label.has_value());

  ASSERT_EQ(intent_filter->conditions.size(), filter_types.empty() ? 1U : 2U);

  {
    const Condition& condition = *intent_filter->conditions[0];
    EXPECT_EQ(condition.condition_type, ConditionType::kAction);
    ASSERT_EQ(condition.condition_values.size(), 2U);

    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, "send");

    EXPECT_EQ(condition.condition_values[1]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[1]->value, "send_multiple");
  }

  if (!filter_types.empty()) {
    const Condition& condition = *intent_filter->conditions[1];
    EXPECT_EQ(condition.condition_type, ConditionType::kMimeType);
    ASSERT_EQ(condition.condition_values.size(), filter_types.size());

    for (unsigned i = 0; i < filter_types.size(); ++i) {
      EXPECT_EQ(condition.condition_values[i]->match_type,
                PatternMatchType::kMimeType);
      EXPECT_EQ(condition.condition_values[i]->value, filter_types[i]);
    }
  }

  for (const std::string& accepted_type : accepted_types) {
    {
      std::vector<GURL> filesystem_urls(1U);
      std::vector<std::string> mime_types(1U, accepted_type);
      EXPECT_TRUE(apps_util::IntentMatchesFilter(
          apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types),
          intent_filter));
    }

    {
      std::vector<GURL> filesystem_urls(3U);
      std::vector<std::string> mime_types(3U, accepted_type);
      EXPECT_TRUE(apps_util::IntentMatchesFilter(
          apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types),
          intent_filter));
    }
  }

  for (const std::string& rejected_type : rejected_types) {
    std::vector<GURL> filesystem_urls(1U);
    std::vector<std::string> mime_types(1U, rejected_type);
    EXPECT_FALSE(apps_util::IntentMatchesFilter(
        apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types),
        intent_filter));
  }
}

}  // namespace

class WebAppsBaseBrowserTest : public InProcessBrowserTest {
 public:
  WebAppsBaseBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kIntentHandlingSharing);
  }
  ~WebAppsBaseBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, PopulateIntentFilters) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/charts.html"));

  std::vector<mojom::IntentFilterPtr> target;
  {
    const web_app::WebAppRegistrar* registrar =
        web_app::WebAppProvider::Get(browser()->profile())
            ->registrar()
            .AsWebAppRegistrar();
    const web_app::AppId app_id =
        web_app::InstallWebAppFromManifest(browser(), app_url);
    const web_app::WebApp* web_app = registrar->GetAppById(app_id);
    ASSERT_TRUE(web_app);
    PopulateIntentFilters(*web_app, target);
  }

  ASSERT_EQ(target.size(), 3U);

  CheckUrlScopeFilter(target[0], app_url.GetWithoutFilename(),
                      /*different_url=*/GURL("file:///"));

  CheckShareTextFilter(target[1]);

  const std::vector<std::string> filter_types(
      {"text/*", "image/svg+xml", "*/*"});
  const std::vector<std::string> accepted_types(
      {"text/plain", "image/svg+xml", "video/webm"});
  const std::vector<std::string> rejected_types;  // No types are rejected.
  CheckShareFileFilter(target[2], filter_types, accepted_types, rejected_types);
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, PartialWild) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/partial-wild.html"));

  std::vector<mojom::IntentFilterPtr> target;
  {
    const web_app::WebAppRegistrar* registrar =
        web_app::WebAppProvider::Get(browser()->profile())
            ->registrar()
            .AsWebAppRegistrar();
    const web_app::AppId app_id =
        web_app::InstallWebAppFromManifest(browser(), app_url);
    const web_app::WebApp* web_app = registrar->GetAppById(app_id);
    ASSERT_TRUE(web_app);
    PopulateIntentFilters(*web_app, target);
  }

  ASSERT_EQ(target.size(), 2U);

  CheckUrlScopeFilter(target[0], app_url.GetWithoutFilename(),
                      /*different_url=*/GURL("file:///"));

  const std::vector<std::string> filter_types({"image/*"});
  const std::vector<std::string> accepted_types({"image/png", "image/svg+xml"});
  const std::vector<std::string> rejected_types(
      {"application/vnd.android.package-archive", "text/plain"});
  CheckShareFileFilter(target[1], filter_types, accepted_types, rejected_types);
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, ShareTargetWithoutFiles) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/poster.html"));

  std::vector<mojom::IntentFilterPtr> target;
  {
    const web_app::WebAppRegistrar* registrar =
        web_app::WebAppProvider::Get(browser()->profile())
            ->registrar()
            .AsWebAppRegistrar();
    const web_app::AppId app_id =
        web_app::InstallWebAppFromManifest(browser(), app_url);
    const web_app::WebApp* web_app = registrar->GetAppById(app_id);
    ASSERT_TRUE(web_app);
    PopulateIntentFilters(*web_app, target);
  }

  ASSERT_EQ(target.size(), 2U);

  CheckUrlScopeFilter(target[0], app_url.GetWithoutFilename(),
                      /*different_url=*/GURL("file:///"));

  CheckShareTextFilter(target[1]);
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, LaunchWithIntent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/charts.html"));
  Profile* const profile = browser()->profile();
  const web_app::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), app_url);

  base::RunLoop run_loop;
  web_app::WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting(
          [&run_loop](apps::AppLaunchParams&& params) -> content::WebContents* {
            EXPECT_EQ(*params.intent->action, apps_util::kIntentActionSend);
            EXPECT_EQ(*params.intent->mime_type, "text/csv");
            EXPECT_EQ(params.intent->file_urls->size(), 1U);
            run_loop.Quit();
            return nullptr;
          }));

  std::vector<base::FilePath> file_paths(
      {chromeos::CrosDisksClient::GetArchiveMountPoint().Append(
          "numbers.csv")});
  std::vector<std::string> content_types({"text/csv"});
  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile, std::move(file_paths), std::move(content_types));
  const int32_t event_flags =
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      app_id, event_flags, std::move(intent),
      apps::mojom::LaunchSource::kFromSharesheet,
      apps::MakeWindowInfo(display::kDefaultDisplayId));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, IntentWithoutFiles) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/poster.html"));
  Profile* const profile = browser()->profile();
  const web_app::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), app_url);

  base::RunLoop run_loop;
  web_app::WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting(
          [&run_loop](apps::AppLaunchParams&& params) -> content::WebContents* {
            EXPECT_EQ(*params.intent->action,
                      apps_util::kIntentActionSendMultiple);
            EXPECT_EQ(*params.intent->mime_type, "*/*");
            EXPECT_EQ(params.intent->file_urls->size(), 0U);
            run_loop.Quit();
            return nullptr;
          }));

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile, /*file_paths=*/std::vector<base::FilePath>(),
      /*mime_types=*/std::vector<std::string>(),
      /*share_text=*/"Message",
      /*share_title=*/"Subject");

  const int32_t event_flags =
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      app_id, event_flags, std::move(intent),
      apps::mojom::LaunchSource::kFromSharesheet,
      apps::MakeWindowInfo(display::kDefaultDisplayId));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, ExposeAppServicePublisherId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL("/web_apps/basic.html"));

  // Install file handling web app.
  const web_app::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), app_url);
  const web_app::WebAppRegistrar* registrar =
      web_app::WebAppProvider::Get(browser()->profile())
          ->registrar()
          .AsWebAppRegistrar();
  const web_app::WebApp* web_app = registrar->GetAppById(app_id);
  ASSERT_TRUE(web_app);

  // Check the publisher_id is the app's start url.
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [&](const apps::AppUpdate& update) {
        EXPECT_EQ(web_app->start_url().spec(), update.PublisherId());
      });
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, LaunchAppIconKeyUnchanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL("/web_apps/basic.html"));
  const web_app::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), app_url);
  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());

  apps::mojom::IconKeyPtr original_key;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&original_key](const apps::AppUpdate& update) {
        original_key = update.IconKey().Clone();
      });

  const int32_t event_flags =
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  proxy->Launch(app_id, event_flags, apps::mojom::LaunchSource::kUnknown,
                apps::MakeWindowInfo(display::kDefaultDisplayId));
  proxy->FlushMojoCallsForTesting();

  proxy->AppRegistryCache().ForOneApp(
      app_id, [&original_key](const apps::AppUpdate& update) {
        ASSERT_EQ(original_key, update.IconKey());
      });
}

}  // namespace apps
