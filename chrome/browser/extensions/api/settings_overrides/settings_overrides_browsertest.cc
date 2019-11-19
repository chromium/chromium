// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

namespace {
#if defined(OS_WIN) || defined(OS_MACOSX)
// Prepopulated id hardcoded in test_extension.
const int kTestExtensionPrepopulatedId = 3;
// TemplateURLData with search engines settings from test extension manifest.
// chrome/test/data/extensions/settings_override/manifest.json
std::unique_ptr<TemplateURLData> TestExtensionSearchEngine(PrefService* prefs) {
  auto result = std::make_unique<TemplateURLData>();
  result->SetShortName(base::ASCIIToUTF16("name.de"));
  result->SetKeyword(base::ASCIIToUTF16("keyword.de"));
  result->SetURL("http://www.foo.de/s?q={searchTerms}&id=10");
  result->favicon_url = GURL("http://www.foo.de/favicon.ico?id=10");
  result->suggestions_url = "http://www.foo.de/suggest?q={searchTerms}&id=10";
  result->image_url = "http://www.foo.de/image?q={searchTerms}&id=10";
  result->search_url_post_params = "search_lang=de";
  result->suggestions_url_post_params = "suggest_lang=de";
  result->image_url_post_params = "image_lang=de";
  result->alternate_urls.push_back("http://www.moo.de/s?q={searchTerms}&id=10");
  result->alternate_urls.push_back("http://www.noo.de/s?q={searchTerms}&id=10");
  result->input_encodings.push_back("UTF-8");

  std::unique_ptr<TemplateURLData> prepopulated =
      TemplateURLPrepopulateData::GetPrepopulatedEngine(
          prefs, kTestExtensionPrepopulatedId);
  // Values below do not exist in extension manifest and are taken from
  // prepopulated engine with prepopulated_id set in extension manifest.
  result->contextual_search_url = prepopulated->contextual_search_url;
  result->new_tab_url = prepopulated->new_tab_url;
  return result;
}

testing::AssertionResult VerifyTemplateURLServiceLoad(
    TemplateURLService* service) {
  if (service->loaded())
    return testing::AssertionSuccess();
  search_test_utils::WaitForTemplateURLServiceToLoad(service);
  if (service->loaded())
    return testing::AssertionSuccess();
  return testing::AssertionFailure() << "TemplateURLService isn't loaded";
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OverrideHomePageSettings) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs);
  prefs->SetString(prefs::kHomePage, "http://google.com/");
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  const extensions::Extension* extension = LoadExtensionWithInstallParam(
      test_data_dir_.AppendASCII("settings_override"), kFlagEnableFileAccess,
      "10");
  ASSERT_TRUE(extension);
  EXPECT_EQ("http://www.homepage.de/?param=10",
            prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  UnloadExtension(extension->id());
  EXPECT_EQ("http://google.com/", prefs->GetString(prefs::kHomePage));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OverrideStartupPagesSettings) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs);
  const GURL urls[] = {GURL("http://foo"), GURL("http://bar")};
  SessionStartupPref startup_pref(SessionStartupPref::LAST);
  startup_pref.urls.assign(urls, urls + base::size(urls));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  const extensions::Extension* extension = LoadExtensionWithInstallParam(
      test_data_dir_.AppendASCII("settings_override"),
      kFlagEnableFileAccess,
      "10");
  ASSERT_TRUE(extension);
  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::URLS, startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(1, GURL("http://www.startup.de/?param=10")),
            startup_pref.urls);
  UnloadExtension(extension->id());
  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::LAST, startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(urls, urls + base::size(urls)),
            startup_pref.urls);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OverrideDSE) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs);
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(url_service);
  EXPECT_TRUE(VerifyTemplateURLServiceLoad(url_service));
  const TemplateURL* default_provider = url_service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_provider);
  EXPECT_EQ(TemplateURL::NORMAL, default_provider->type());

  const extensions::Extension* extension = LoadExtensionWithInstallParam(
      test_data_dir_.AppendASCII("settings_override"), kFlagEnableFileAccess,
      "10");
  ASSERT_TRUE(extension);
  const TemplateURL* current_dse = url_service->GetDefaultSearchProvider();
  EXPECT_EQ(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, current_dse->type());

  std::unique_ptr<TemplateURLData> extension_dse =
      TestExtensionSearchEngine(prefs);
  ExpectSimilar(extension_dse.get(), &current_dse->data());

  UnloadExtension(extension->id());
  EXPECT_EQ(default_provider, url_service->GetDefaultSearchProvider());
}

// Install and load extension into test profile.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PRE_OverridenDSEPersists) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs);
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(url_service);
  EXPECT_TRUE(VerifyTemplateURLServiceLoad(url_service));
  const TemplateURL* default_provider = url_service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_provider);
  // Check that default provider is normal before extension is
  // installed and loaded.
  EXPECT_EQ(TemplateURL::NORMAL, default_provider->type());
  EXPECT_NE(base::ASCIIToUTF16("name.de"), default_provider->short_name());
  EXPECT_NE(base::ASCIIToUTF16("keyword.de"), default_provider->keyword());

  // Install extension that overrides DSE.
  const extensions::Extension* extension = LoadExtensionWithInstallParam(
      test_data_dir_.AppendASCII("settings_override"), kFlagEnableFileAccess,
      "10");
  ASSERT_TRUE(extension);
}

// PRE_OverridenDSEPersists already installed extension with overriden DSE into
// profile. Current test checks that after extension is installed,
// default_search_manager correctly determines extension overriden DSE
// from profile.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OverridenDSEPersists) {
  Profile* profile = browser()->profile();
  DefaultSearchManager default_manager(
      profile->GetPrefs(), DefaultSearchManager::ObserverCallback());

  DefaultSearchManager::Source source;
  const TemplateURLData* current_dse =
      default_manager.GetDefaultSearchEngine(&source);

  ASSERT_TRUE(current_dse);
  std::unique_ptr<TemplateURLData> extension_dse =
      TestExtensionSearchEngine(profile->GetPrefs());
  ExpectSimilar(extension_dse.get(), current_dse);
  EXPECT_EQ(DefaultSearchManager::FROM_EXTENSION, source);

  // Check that new tab url is correctly overriden by extension.
  TemplateURL ext_turl(*extension_dse,
                       TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);

  std::string new_tab_url_ext = ext_turl.new_tab_url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(base::string16()),
      UIThreadSearchTermsData());

  EXPECT_EQ(new_tab_url_ext, search::GetNewTabPageURL(profile).spec());

  // Check that after template url service is loaded, extension dse persists.
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(url_service);
  EXPECT_TRUE(VerifyTemplateURLServiceLoad(url_service));
  EXPECT_TRUE(url_service->IsExtensionControlledDefaultSearch());
  const TemplateURL* default_provider = url_service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_provider);
  ExpectSimilar(extension_dse.get(), &default_provider->data());
}

// This test checks that an extension overriding the default search engine can
// be correctly loaded before the TemplateURLService is loaded.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, BeforeTemplateUrlServiceLoad) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs);
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(url_service);

  EXPECT_FALSE(url_service->IsExtensionControlledDefaultSearch());
  const extensions::Extension* extension = LoadExtensionWithInstallParam(
      test_data_dir_.AppendASCII("settings_override"), kFlagEnableFileAccess,
      "10");
  ASSERT_TRUE(extension);
  const TemplateURL* current_dse = url_service->GetDefaultSearchProvider();
  EXPECT_EQ(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, current_dse->type());
  EXPECT_TRUE(url_service->IsExtensionControlledDefaultSearch());

  std::unique_ptr<TemplateURLData> extension_dse =
      TestExtensionSearchEngine(prefs);
  ExpectSimilar(extension_dse.get(), &current_dse->data());

  UnloadExtension(extension->id());
  EXPECT_NE(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION,
            url_service->GetDefaultSearchProvider()->type());
  EXPECT_FALSE(url_service->IsExtensionControlledDefaultSearch());
}

constexpr char kExternalId[] = "eloigjjcaheakkihngcgjhbopomgemdj";

class ExtensionsDisabledWithSettingsOverrideAPI : public ExtensionBrowserTest {
 public:
  ExtensionsDisabledWithSettingsOverrideAPI()
      : prompt_for_external_extensions_(
            FeatureSwitch::prompt_for_external_extensions(),
            false) {}
  ~ExtensionsDisabledWithSettingsOverrideAPI() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // A little tricky: we disable extensions (via the commandline) on the
    // non-PRE run. The PRE run is responsible for installing the external
    // extension.
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    const char* test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (!base::StartsWith(test_name, "PRE_", base::CompareCase::SENSITIVE)) {
      command_line->AppendSwitch(::switches::kDisableExtensions);
    }
  }

 private:
  FeatureSwitch::ScopedOverride prompt_for_external_extensions_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsDisabledWithSettingsOverrideAPI);
};

// The following test combo is a regression test for https://crbug.com/828295.
// When extensions are disabled with --disable-extensions, no
// extension-controlled prefs were loaded. However, external extensions (such as
// those from policy or specified in the registry) are still loaded with
// --disable-extensions (this itself is somewhat strange; see
// https://crbug.com/833540). This caused a CHECK failure in the settings
// overrides API when an external extension used the API and
// --disable-extensions was also used.
// As a fix, we ensure that we load extension-controlled preferences for
// extensions that will be loaded, even with --disable-extensions.
IN_PROC_BROWSER_TEST_F(ExtensionsDisabledWithSettingsOverrideAPI,
                       PRE_TestSettingsOverridesWithExtensionsDisabled) {
  // This first part of the test adds an external extension that uses the
  // settings overrides API.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  TestExtensionRegistryObserver observer(registry);
  auto provider = std::make_unique<MockExternalProvider>(
      extension_service(), Manifest::EXTERNAL_PREF);
  provider->UpdateOrAddExtension(
      kExternalId, "2.1",
      test_data_dir_.AppendASCII("api_test/settings_overrides/homepage.crx"));
  extension_service()->AddProviderForTesting(std::move(provider));
  extension_service()->CheckForExternalUpdates();
  const Extension* extension = observer.WaitForExtensionLoaded();
  EXPECT_EQ(kExternalId, extension->id());
}
IN_PROC_BROWSER_TEST_F(ExtensionsDisabledWithSettingsOverrideAPI,
                       TestSettingsOverridesWithExtensionsDisabled) {
  // The external extension was actually uninstalled at this point (see
  // https://crbug.com/833540). However, it was first loaded, before being
  // orphaned, which would have caused the settings API to look at the
  // extension controlled preferences. As long as this didn't crash, the test
  // succeeded.
  EXPECT_TRUE(ExtensionsBrowserClient::Get()->AreExtensionsDisabled(
      *base::CommandLine::ForCurrentProcess(), profile()));
}

#else
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, SettingsOverridesDisallowed) {
  const extensions::Extension* extension =
      LoadExtensionWithFlags(test_data_dir_.AppendASCII("settings_override"),
                             kFlagIgnoreManifestWarnings);
  ASSERT_TRUE(extension);
  ASSERT_EQ(1u, extension->install_warnings().size());
  EXPECT_EQ(std::string("'chrome_settings_overrides' "
                        "is not allowed for specified platform."),
            extension->install_warnings().front().message);
}
#endif

}  // namespace
}  // namespace extensions
