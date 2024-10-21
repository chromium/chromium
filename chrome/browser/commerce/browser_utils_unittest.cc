// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/browser_utils.h"

#include <memory>

#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

class CommerceBrowserUtilsTest : public testing::Test {
 public:
  CommerceBrowserUtilsTest()
      : account_checker_(std::make_unique<commerce::MockAccountChecker>()),
        prefs_(std::make_unique<TestingPrefServiceSimple>()) {}
  void SetUp() override {
    commerce::RegisterCommercePrefs(prefs_->registry());
    account_checker_->SetPrefs(prefs_.get());
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
    profile_ = profile_builder.Build();
    auto* shopping_service = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(profile_.get()));
    shopping_service->SetAccountChecker(account_checker_.get());
    // By default, the account checker and prefs are set up to enable product
    // specifications.
    commerce::EnableProductSpecificationsDataFetch(account_checker_.get(),
                                                   prefs_.get());
  }

 protected:
  const std::string kHttpsUrl = "https://example.com/";
  const std::string kHttpUrl = "http://example.com/";
  const std::string kChromeUrl = "chrome://example/";

  TestingProfile* profile() { return profile_.get(); }
  std::unique_ptr<WebContents> CreateWebContentsAndCommitFakeUrl(
      std::string url) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(GURL(url));
    return web_contents;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<commerce::MockAccountChecker> account_checker_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(CommerceBrowserUtilsTest, TestIsWebContentsListEligibleForProductSpecs) {
  auto https_web_contents_1 = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);
  auto https_web_contents_2 = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);

  std::vector<WebContents*> web_contents_list{https_web_contents_1.get(),
                                              https_web_contents_2.get()};

  ASSERT_TRUE(
      commerce::IsWebContentsListEligibleForProductSpecs(web_contents_list));
}

TEST_F(CommerceBrowserUtilsTest, IsProductSpecsMultiSelectMenuEnabled) {
  scoped_feature_list_.InitWithFeatures({commerce::kProductSpecifications}, {});
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));

  auto web_contents = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);
  ASSERT_TRUE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      profile(), web_contents.get()));

  // Navigate to a chrome:// url.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL(kChromeUrl));
  ASSERT_FALSE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      profile(), web_contents.get()));
}

TEST_F(CommerceBrowserUtilsTest,
       IsProductSpecsMultiSelectMenuEnabled_HttpOrigin) {
  scoped_feature_list_.InitWithFeatures({commerce::kProductSpecifications}, {});
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));
  auto http_web_contents = CreateWebContentsAndCommitFakeUrl(kHttpUrl);
  ASSERT_TRUE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      profile(), http_web_contents.get()));
}

TEST_F(CommerceBrowserUtilsTest,
       IsProductSpecsMultiSelectMenuEnabled_FeatureDisabled) {
  scoped_feature_list_.InitWithFeatures({}, {commerce::kProductSpecifications});
  ASSERT_FALSE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));
  auto https_web_contents = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);
  ASSERT_FALSE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      profile(), https_web_contents.get()));
}

TEST_F(CommerceBrowserUtilsTest,
       IsProductSpecsMultiSelectMenuEnabled_FeatureCannotFetch) {
  scoped_feature_list_.InitWithFeatures({commerce::kProductSpecifications}, {});
  // Update account checker to disable product specifications data fetch.
  account_checker_->SetIsSubjectToParentalControls(true);
  ASSERT_FALSE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));

  auto https_web_contents = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);
  ASSERT_FALSE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      profile(), https_web_contents.get()));
}

TEST_F(CommerceBrowserUtilsTest,
       IsProductSpecsMultiSelectMenuEnabled_NoIneligibleWebContents) {
  scoped_feature_list_.InitWithFeatures({commerce::kProductSpecifications}, {});
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));
  auto chrome_web_contents = CreateWebContentsAndCommitFakeUrl(kChromeUrl);
  ASSERT_FALSE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      profile(), chrome_web_contents.get()));
}

TEST_F(CommerceBrowserUtilsTest,
       IsProductSpecsMultiSelectMenuEnabled_NoOffTheRecord) {
  scoped_feature_list_.InitWithFeatures({commerce::kProductSpecifications}, {});
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));
  auto https_web_contents = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);

  // Incognito profile
  TestingProfile::Builder profile_builder;
  auto* incognito_profile = profile_builder.BuildIncognito(profile());
  ASSERT_FALSE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      incognito_profile, https_web_contents.get()));

  // OffTheRecord profile
  auto* otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(), true);
  ASSERT_FALSE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      otr_profile, https_web_contents.get()));
}

TEST_F(CommerceBrowserUtilsTest,
       IsProductSpecsMultiSelectMenuEnabled_NoGuestSession) {
  scoped_feature_list_.InitWithFeatures({commerce::kProductSpecifications}, {});
  ASSERT_TRUE(
      commerce::CanFetchProductSpecificationsData(account_checker_.get()));

  TestingProfile::Builder profile_builder;
  auto guest_session = profile_builder.SetGuestSession().Build();

  auto https_web_contents = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);
  ASSERT_FALSE(commerce::IsProductSpecsMultiSelectMenuEnabled(
      guest_session.get(), https_web_contents.get()));
}

TEST_F(CommerceBrowserUtilsTest, IsWebContentsListEligibleForProductSpecs) {
  auto http_web_contents = CreateWebContentsAndCommitFakeUrl(kHttpUrl);
  auto https_web_contents_1 = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);
  auto https_web_contents_2 = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);

  std::vector<WebContents*> web_contents_list{http_web_contents.get(),
                                              https_web_contents_1.get(),
                                              https_web_contents_2.get()};
  ASSERT_TRUE(
      commerce::IsWebContentsListEligibleForProductSpecs(web_contents_list));
}

TEST_F(CommerceBrowserUtilsTest,
       IsWebContentsListEligibleForProductSpecs_NotEnoughUrls) {
  auto chrome_web_contents_1 = CreateWebContentsAndCommitFakeUrl(kChromeUrl);
  auto chrome_web_contents_2 = CreateWebContentsAndCommitFakeUrl(kChromeUrl);
  auto https_web_contents = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);

  std::vector<WebContents*> web_contents_list{chrome_web_contents_1.get(),
                                              chrome_web_contents_2.get(),
                                              https_web_contents.get()};
  ASSERT_FALSE(
      commerce::IsWebContentsListEligibleForProductSpecs(web_contents_list));
}

TEST_F(CommerceBrowserUtilsTest, GetListOfProductSpecsEligibleUrls) {
  auto chrome_web_contents_1 = CreateWebContentsAndCommitFakeUrl(kChromeUrl);
  auto chrome_web_contents_2 = CreateWebContentsAndCommitFakeUrl(kChromeUrl);
  auto https_web_contents = CreateWebContentsAndCommitFakeUrl(kHttpsUrl);

  std::vector<WebContents*> web_contents_list{chrome_web_contents_1.get(),
                                              chrome_web_contents_2.get(),
                                              https_web_contents.get()};

  auto eligible_urls =
      commerce::GetListOfProductSpecsEligibleUrls(web_contents_list);
  ASSERT_EQ(1U, eligible_urls.size());
}
