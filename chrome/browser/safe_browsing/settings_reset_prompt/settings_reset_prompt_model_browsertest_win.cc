// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_test_utils.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

using extensions::Extension;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

const char kDefaultHomepage[] = "http://myhomepage.com";
const char kDefaultSearchUrl[] = "http://mygoogle.com/s?q={searchTerms}";
const char kDefaultStartupUrl1[] = "http://mystart1.com";
const char kDefaultStartupUrl2[] = "http://mystart2.com";

const char kHomepage1[] = "http://homepage.com/";
const char kHomepage2[] = "http://otherhomepage.com/";
const char kSearchUrl1[] = "http://mysearch.com/s?q={searchTerms}";
const char kSearchUrl2[] = "http://othersearch.com/s?q={searchTerms}";
const char kStartupUrl1[] = "http://super-startup.com";
const char kStartupUrl2[] = "http://awesome-start-page.com";

// Extension manifests to override settings.
const char kManifestNoOverride[] =
    R"({
         "name": "Safe Extension",
         "version": "1",
         "manifest_version": 2
       })";

const char kManifestToOverrideHomepage[] =
    R"({
         "name": "Homepage Extension",
         "version": "1",
         "manifest_version": 2,
         "chrome_settings_overrides" : {
           "homepage": "%s"
         }
       })";

const char kManifestToOverrideSearch[] =
    R"({
         "name": "Search Extension",
         "version": "0.1",
         "manifest_version": 2,
         "chrome_settings_overrides": {
           "search_provider": {
              "name": "name",
              "keyword": "keyword",
              "search_url": "%s",
              "favicon_url": "http://someplace.com/favicon.ico",
              "encoding": "UTF-8",
              "is_default": true
           }
         }
       })";

const char kManifestToOverrideStartupUrls[] =
    R"({
         "name": "Startup URLs Extension",
         "version": "1",
         "manifest_version": 2,
         "chrome_settings_overrides" : {
           "startup_pages": ["%s"]
         }
       })";

class SettingsResetPromptModelBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  virtual void OnResetDone() { ++reset_callbacks_; }

 protected:
  using ModelPointer = std::unique_ptr<SettingsResetPromptModel>;

  SettingsResetPromptModelBrowserTest()
      : startup_pref_(SessionStartupPref::URLS) {}

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    // Set up an active homepage with visible homepage button.
    PrefService* prefs = profile()->GetPrefs();
    ASSERT_TRUE(prefs);
    prefs->SetBoolean(prefs::kShowHomeButton, true);
    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
    prefs->SetString(prefs::kHomePage, kDefaultHomepage);

    // Set up a default search with known URL.
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(template_url_service);

    TemplateURLData data;
    data.SetShortName(u"default");
    data.SetKeyword(u"default");
    data.SetURL(kDefaultSearchUrl);

    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

    // Set up a couple of startup URLs.
    startup_pref_.urls.push_back(GURL(kDefaultStartupUrl1));
    startup_pref_.urls.push_back(GURL(kDefaultStartupUrl2));
    SessionStartupPref::SetStartupPref(profile(), startup_pref_);
    ASSERT_EQ(SessionStartupPref::PrefValueToType(
                  GetPrefs()->GetInteger(prefs::kRestoreOnStartup)),
              startup_pref_.type);
  }

  PrefService* GetPrefs() { return profile()->GetPrefs(); }

  void LoadHomepageExtension(const char* homepage,
                             const Extension** out_extension) {
    const std::string manifest =
        base::StringPrintf(kManifestToOverrideHomepage, homepage);
    LoadManifest(manifest, out_extension);
    ASSERT_EQ(std::string(homepage), GetPrefs()->GetString(prefs::kHomePage));
  }

  void LoadSearchExtension(const char* search_url,
                           const Extension** out_extension) {
    const std::string manifest =
        base::StringPrintf(kManifestToOverrideSearch, search_url);
    LoadManifest(manifest, out_extension);

    TemplateURLService* service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(service);

    const TemplateURL* dse = service->GetDefaultSearchProvider();
    EXPECT_EQ(std::string(search_url), dse->url());
  }

  void LoadStartupUrlExtension(const char* startup_url,
                               const Extension** out_extension) {
    const std::string manifest =
        base::StringPrintf(kManifestToOverrideStartupUrls, startup_url);
    LoadManifest(manifest, out_extension);

    // Ensure that the startup url seen in the prefs is same as |startup_url|.
    const base::Value::List& url_list =
        GetPrefs()->GetList(prefs::kURLsToRestoreOnStartup);
    ASSERT_EQ(url_list.size(), 1U);
    ASSERT_TRUE(url_list[0].is_string());
    ASSERT_EQ(GURL(url_list[0].GetString()), GURL(startup_url));
  }

  void LoadManifest(const std::string& manifest,
                    const Extension** out_extension) {
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifest(manifest);
    *out_extension = LoadExtension(extension_dir.UnpackedPath());
    ASSERT_TRUE(*out_extension);
  }

  // Returns a model with a mock config that will return negative IDs for every
  // URL.
  ModelPointer CreateModel() {
    return CreateModelForTesting(profile(), std::unordered_set<std::string>(),
                                 nullptr);
  }

  // Returns a model with a mock config that will return positive IDs for each
  // URL in |reset_urls|.
  ModelPointer CreateModel(std::unordered_set<std::string> reset_urls) {
    return CreateModelForTesting(profile(), reset_urls, nullptr);
  }

  // Returns a model with a mock config that will return positive IDs for each
  // URL in |reset_urls|.
  ModelPointer CreateModel(std::unordered_set<std::string> reset_urls,
                           std::unique_ptr<ProfileResetter> profile_resetter) {
    return CreateModelForTesting(profile(), reset_urls,
                                 std::move(profile_resetter));
  }

  SessionStartupPref startup_pref_;
  int reset_callbacks_ = 0;
};

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       OverridenByExtension_Homepage) {
  // Homepage does not require reset to start with.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(
        model->homepage_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  }

  // Let homepage require reset.
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
  }

  // Load an extension that does not override settings. Homepage still requires
  // reset, since it was not set by the extension.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
  }

  // Load extension that overrides homepage to a domain that is not to be reset.
  // Homepage no longer needs to be reset.
  const Extension* homepage_extension1 = nullptr;
  LoadHomepageExtension(kHomepage1, &homepage_extension1);
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(
        model->homepage_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  }

  // Let the domain used by the extension require reset. Homepage can no longer
  // be reset.
  {
    ModelPointer model = CreateModel({kDefaultHomepage, kHomepage1});
    EXPECT_EQ(
        model->homepage_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_EXTENSION_OVERRIDE);
  }

  // Add a second homepage-overriding extension. Homepage no longer needs to be
  // reset since domains don't match.
  const Extension* homepage_extension2 = nullptr;
  LoadHomepageExtension(kHomepage2, &homepage_extension2);
  {
    ModelPointer model = CreateModel({kDefaultHomepage, kHomepage1});
    EXPECT_EQ(
        model->homepage_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  }
}

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       OverridenByExtension_DefaultSearch) {
  // Search does not need to be reset to start with.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(
        model->default_search_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  }

  // Let the default search domain require reset.
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
  }

  // Load an extension that does not override settings. Search still needs to be
  // reset.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
  }

  // Load extension that overrides search to a domain that doesn't require
  // reset. Search no longer needs to be reset.
  const Extension* search_extension1 = nullptr;
  LoadSearchExtension(kSearchUrl1, &search_extension1);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(
        model->default_search_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  }

  // Let the domain used by the extension require reset. Search can no longer be
  // reset.
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl, kSearchUrl1});
    EXPECT_EQ(
        model->default_search_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_EXTENSION_OVERRIDE);
  }

  // Add a second search overriding extension for a domain that doesn't require
  // reset. Search no longer needs to be reset.
  const Extension* search_extension2 = nullptr;
  LoadSearchExtension(kSearchUrl2, &search_extension2);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl, kSearchUrl1});
    EXPECT_EQ(
        model->default_search_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  }
}

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       OverridenByExtension_StartupUrls) {
  // Startup urls do not require reset to begin with.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(
        model->startup_urls_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->startup_urls(),
                UnorderedElementsAre(GURL(kDefaultStartupUrl1),
                                     GURL(kDefaultStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
  }

  // Let a default startup URL require reset.
  {
    ModelPointer model = CreateModel({kDefaultStartupUrl1});
    EXPECT_EQ(model->startup_urls_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->startup_urls(),
                UnorderedElementsAre(GURL(kDefaultStartupUrl1),
                                     GURL(kDefaultStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(),
                ElementsAre(GURL(kDefaultStartupUrl1)));
  }

  // Load an extension that does not override settings. Startup URLs still needs
  // to be reset.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  {
    ModelPointer model = CreateModel({kDefaultStartupUrl1});
    EXPECT_EQ(model->startup_urls_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->startup_urls(),
                UnorderedElementsAre(GURL(kDefaultStartupUrl1),
                                     GURL(kDefaultStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(),
                ElementsAre(GURL(kDefaultStartupUrl1)));
  }

  // Load two other extensions that each override startup urls.
  const Extension* startup_url_extension1 = nullptr;
  LoadStartupUrlExtension(kStartupUrl1, &startup_url_extension1);
  const Extension* startup_url_extension2 = nullptr;
  LoadStartupUrlExtension(kStartupUrl2, &startup_url_extension2);

  // Startup URLs should not require reset when a default startup URL that is
  // not active (since it is now overridden by an extension) requires reset.
  {
    ModelPointer model = CreateModel({kDefaultStartupUrl1});
    EXPECT_EQ(
        model->startup_urls_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
  }

  // Let the first extension's URL require reset. Startup URLs do not need to be
  // reset because the second extension's URL is the active one and does not
  // require reset.
  {
    ModelPointer model = CreateModel({kStartupUrl1});
    EXPECT_EQ(
        model->startup_urls_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
  }

  // Let the second extension's URL also require reset. Startup URLs can no
  // longer to be reset.
  {
    ModelPointer model = CreateModel({kStartupUrl1, kStartupUrl2});
    EXPECT_EQ(
        model->startup_urls_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_EXTENSION_OVERRIDE);
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(),
                ElementsAre(GURL(kStartupUrl2)));
  }
}

}  // namespace
}  // namespace safe_browsing
