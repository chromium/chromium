// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps_base.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

  EXPECT_EQ(intent_filter->conditions.size(), 4U);

  {
    const Condition& condition = *intent_filter->conditions[0];
    EXPECT_EQ(condition.condition_type, ConditionType::kAction);
    EXPECT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, "view");
  }

  {
    const Condition& condition = *intent_filter->conditions[1];
    EXPECT_EQ(condition.condition_type, ConditionType::kScheme);
    EXPECT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, url.scheme());
  }

  {
    const Condition& condition = *intent_filter->conditions[2];
    EXPECT_EQ(condition.condition_type, ConditionType::kHost);
    EXPECT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, url.host());
  }

  {
    const Condition& condition = *intent_filter->conditions[3];
    EXPECT_EQ(condition.condition_type, ConditionType::kPattern);
    EXPECT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kPrefix);
    EXPECT_EQ(condition.condition_values[0]->value, url.path());
  }

  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(url), intent_filter));

  EXPECT_FALSE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(different_url), intent_filter));
}

void CheckShareFileFilter(const apps::mojom::IntentFilterPtr& intent_filter,
                          const std::vector<std::string>& content_types,
                          const std::string& different_content_type) {
  EXPECT_FALSE(intent_filter->activity_name.has_value());
  EXPECT_FALSE(intent_filter->activity_label.has_value());

  EXPECT_EQ(intent_filter->conditions.size(), 2U);

  {
    const Condition& condition = *intent_filter->conditions[0];
    EXPECT_EQ(condition.condition_type, ConditionType::kAction);
    EXPECT_EQ(condition.condition_values.size(), 2U);

    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, "send");

    EXPECT_EQ(condition.condition_values[1]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[1]->value, "send_multiple");
  }

  {
    const Condition& condition = *intent_filter->conditions[1];
    EXPECT_EQ(condition.condition_type, ConditionType::kMimeType);
    EXPECT_EQ(condition.condition_values.size(), content_types.size());

    for (unsigned i = 0; i < content_types.size(); ++i) {
      EXPECT_EQ(condition.condition_values[i]->match_type,
                PatternMatchType::kNone);
      EXPECT_EQ(condition.condition_values[i]->value, content_types[i]);

      {
        std::vector<GURL> filesystem_urls(1U);
        std::vector<std::string> mime_types(1U, content_types[i]);
        EXPECT_TRUE(apps_util::IntentMatchesFilter(
            apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types),
            intent_filter));
      }

      {
        std::vector<GURL> filesystem_urls(3U);
        std::vector<std::string> mime_types(3U, content_types[i]);
        EXPECT_TRUE(apps_util::IntentMatchesFilter(
            apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types),
            intent_filter));
      }
    }
  }

  {
    std::vector<GURL> filesystem_urls(1U);
    std::vector<std::string> mime_types(1U, different_content_type);
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

  EXPECT_EQ(target.size(), 2U);

  CheckUrlScopeFilter(target[0], app_url.GetWithoutFilename(),
                      /*different_url=*/GURL("file:///"));

  const std::vector<std::string> content_types({"text/csv", "image/svg+xml"});
  CheckShareFileFilter(
      target[1], content_types,
      /*different_content_type=*/"application/vnd.android.package-archive");
}

}  // namespace apps
