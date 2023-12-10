// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace policy {

IN_PROC_BROWSER_TEST_F(PolicyTest, DefaultSearchProvider) {
  // Verifies that a default search is made using the provider configured via
  // policy. Also checks that default search can be completely disabled.
  const std::u16string kKeyword(u"testsearch");
  const std::string kSearchURL("http://search.example/search?q={searchTerms}");
  const std::string kAlternateURL0(
      "http://search.example/search#q={searchTerms}");
  const std::string kAlternateURL1("http://search.example/#q={searchTerms}");
  const std::string kImageURL("http://test.com/searchbyimage/upload");
  const std::string kImageURLPostParams(
      "image_content=content,image_url=http://test.com/test.png");
  const std::string kNewTabURL("http://search.example/newtab");

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(service);
  const TemplateURL* default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_EQ(default_search->created_by_policy(),
            TemplateURLData::CreatedByPolicy::kNoPolicy);
  EXPECT_NE(kKeyword, default_search->keyword());
  EXPECT_NE(kSearchURL, default_search->url());
  EXPECT_FALSE(default_search->alternate_urls().size() == 2 &&
               default_search->alternate_urls()[0] == kAlternateURL0 &&
               default_search->alternate_urls()[1] == kAlternateURL1 &&
               default_search->image_url() == kImageURL &&
               default_search->image_url_post_params() == kImageURLPostParams &&
               default_search->new_tab_url() == kNewTabURL);

  // Override the default search provider using policies.
  PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  policies.Set(key::kDefaultSearchProviderKeyword, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kKeyword),
               nullptr);
  policies.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kSearchURL),
               nullptr);
  base::Value::List alternate_urls;
  alternate_urls.Append(kAlternateURL0);
  alternate_urls.Append(kAlternateURL1);
  policies.Set(key::kDefaultSearchProviderAlternateURLs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(alternate_urls)), nullptr);
  policies.Set(key::kDefaultSearchProviderImageURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kImageURL),
               nullptr);
  policies.Set(key::kDefaultSearchProviderImageURLPostParams,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(kImageURLPostParams), nullptr);
  policies.Set(key::kDefaultSearchProviderNewTabURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kNewTabURL),
               nullptr);
  UpdateProviderPolicy(policies);
  default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_EQ(default_search->created_by_policy(),
            TemplateURLData::CreatedByPolicy::kDefaultSearchProvider);
  EXPECT_EQ(kKeyword, default_search->keyword());
  EXPECT_EQ(kSearchURL, default_search->url());
  EXPECT_EQ(2U, default_search->alternate_urls().size());
  EXPECT_EQ(kAlternateURL0, default_search->alternate_urls()[0]);
  EXPECT_EQ(kAlternateURL1, default_search->alternate_urls()[1]);
  EXPECT_EQ(kImageURL, default_search->image_url());
  EXPECT_EQ(kImageURLPostParams, default_search->image_url_post_params());
  EXPECT_EQ(kNewTabURL, default_search->new_tab_url());

  // Verify that searching from the omnibox uses kSearchURL.
  chrome::FocusLocationBar(browser());
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "stuff to search for");
  OmniboxEditModel* model =
      browser()->window()->GetLocationBar()->GetOmniboxView()->model();
  EXPECT_TRUE(model->CurrentMatch(nullptr).destination_url.is_valid());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL expected("http://search.example/search?q=stuff+to+search+for");
  EXPECT_EQ(expected, web_contents->GetVisibleURL());

  // Verify that searching from the omnibox can be disabled.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  EXPECT_TRUE(service->GetDefaultSearchProvider());
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(service->GetDefaultSearchProvider());
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "should not work");
  // This means that submitting won't trigger any action.
  EXPECT_FALSE(model->CurrentMatch(nullptr).destination_url.is_valid());
  EXPECT_EQ(GURL(url::kAboutBlankURL), web_contents->GetLastCommittedURL());
}

}  // namespace policy
