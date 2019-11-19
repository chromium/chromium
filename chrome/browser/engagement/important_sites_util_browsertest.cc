// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/important_sites_util.h"

#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class ImportantSitesUtilBrowserTest : public AndroidBrowserTest {
 public:
  ImportantSitesUtilBrowserTest() = default;
  ~ImportantSitesUtilBrowserTest() override = default;

 protected:
  const GURL& default_search_url() const { return default_search_url_; }

  Profile* profile() {
    auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
    return Profile::FromBrowserContext(web_contents->GetBrowserContext());
  }

  void GetDefaultSearchURL() {
    auto* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(template_url_service);
    const auto* template_url = template_url_service->GetDefaultSearchProvider();
    ASSERT_TRUE(template_url);
    default_search_url_ = template_url->GenerateSearchURL(
        template_url_service->search_terms_data());
    ASSERT_FALSE(default_search_url_.is_empty());
  }

  void GrantNotificationPermissionForOrigin(const url::Origin& origin) {
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    ASSERT_TRUE(host_content_settings_map);
    host_content_settings_map->SetContentSettingDefaultScope(
        origin.GetURL(), GURL(), ContentSettingsType::NOTIFICATIONS,
        std::string() /* resource_identifier */, CONTENT_SETTING_ALLOW);
  }

  std::vector<std::string> GetImportantDomains(Profile* profile) {
    std::vector<std::string> important_domains;
    for (const ImportantSitesUtil::ImportantDomainInfo& info :
         ImportantSitesUtil::GetImportantRegisterableDomains(profile, 10)) {
      important_domains.push_back(info.registerable_domain);
    }
    return important_domains;
  }

  // AndroidBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_NO_FATAL_FAILURE(GetDefaultSearchURL());
  }

 private:
  GURL default_search_url_;

  DISALLOW_COPY_AND_ASSIGN(ImportantSitesUtilBrowserTest);
};

// An origin with notification permission should be considered important, unless
// it is the default search engine, which gets the permission auto-granted.
IN_PROC_BROWSER_TEST_F(ImportantSitesUtilBrowserTest,
                       DSENotConsideredImportantInRegularMode) {
  const char kTestURL[] = "https://a.com/";
  const url::Origin kDSEOrigin = url::Origin::Create(default_search_url());
  const url::Origin kNonDSEOrigin = url::Origin::Create(GURL(kTestURL));

  GrantNotificationPermissionForOrigin(kDSEOrigin);
  GrantNotificationPermissionForOrigin(kNonDSEOrigin);
  EXPECT_THAT(GetImportantDomains(profile()),
              ::testing::ElementsAre(kNonDSEOrigin.host()));

  // Important site calculation in incognito mode used to crash in Android
  // pre-O where notification channels are not yet used, see crbug.com/989890,
  // and with that bug fixed, it is now just broken, see crbug.com/993021.
  //
  // In more detail, while the notification permission is not inherited when
  // determining if the notification capability should be available to a
  // website, it *is* inherited for the purposes of calculating whether the site
  // is important.
  //
  // The unexpected consequence is, however, that because in incognito mode the
  // notification permission is no longer technically auto-granted to the DSE
  // origin, the importance calculating logic no longer blocklists it, which
  // means that in incognito mode the DSE ends up being considered important.
  // Because nothing useful in incognito mode is gated on the site being
  // important, this really is just a nuisance that, however, led to a
  // significant number of crashes.
  //
  // For now this test codifies the incorrect behavior just for the purposes of
  // making sure we no longer crash.
  auto* incognito_profile = profile()->GetOffTheRecordProfile();
  ASSERT_TRUE(incognito_profile);
  ASSERT_TRUE(incognito_profile->IsOffTheRecord());

  EXPECT_THAT(
      GetImportantDomains(incognito_profile),
      ::testing::UnorderedElementsAre(
          ImportantSitesUtil::GetRegisterableDomainOrIP(kDSEOrigin.GetURL()),
          kNonDSEOrigin.host()));
}
