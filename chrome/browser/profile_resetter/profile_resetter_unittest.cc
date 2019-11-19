// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/profile_reset_report.pb.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/shortcut.h"
#endif


namespace {

const char kDistributionConfig[] = "{"
    " \"homepage\" : \"http://www.foo.com\","
    " \"homepage_is_newtabpage\" : false,"
    " \"browser\" : {"
    "   \"show_home_button\" : true"
    "  },"
    " \"session\" : {"
    "   \"restore_on_startup\" : 4,"
    "   \"startup_urls\" : [\"http://goo.gl\", \"http://foo.de\"]"
    "  },"
    " \"search_provider_overrides\" : ["
    "    {"
    "      \"name\" : \"first\","
    "      \"keyword\" : \"firstkey\","
    "      \"search_url\" : \"http://www.foo.com/s?q={searchTerms}\","
    "      \"favicon_url\" : \"http://www.foo.com/favicon.ico\","
    "      \"suggest_url\" : \"http://www.foo.com/s?q={searchTerms}\","
    "      \"encoding\" : \"UTF-8\","
    "      \"id\" : 1001"
    "    }"
    "  ],"
    " \"extensions\" : {"
    "   \"settings\" : {"
    "     \"placeholder_for_id\": {"
    "      }"
    "    }"
    "  }"
    "}";

const char kXmlConfig[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<response protocol=\"3.0\" server=\"prod\">"
      "<app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
        "<data index=\"skipfirstrunui-importsearch-defaultbrowser\" "
          "name=\"install\" status=\"ok\">"
          "placeholder_for_data"
        "</data>"
      "</app>"
    "</response>";

using extensions::Extension;
using extensions::Manifest;


// ProfileResetterTest --------------------------------------------------------

// ProfileResetterTest sets up the extension, WebData and TemplateURL services.
class ProfileResetterTest : public extensions::ExtensionServiceTestBase,
                            public ProfileResetterTestBase {
 public:
  ProfileResetterTest();
  ~ProfileResetterTest() override;

 protected:
  void SetUp() override;

  TestingProfile* profile() { return profile_.get(); }

 private:
#if defined(OS_WIN)
  base::ScopedPathOverride user_desktop_override_;
  base::ScopedPathOverride app_dir_override_;
  base::ScopedPathOverride start_menu_override_;
  base::ScopedPathOverride taskbar_pins_override_;
  base::win::ScopedCOMInitializer com_init_;
#endif
};

ProfileResetterTest::ProfileResetterTest()
#if defined(OS_WIN)
    : user_desktop_override_(base::DIR_USER_DESKTOP),
      app_dir_override_(base::DIR_APP_DATA),
      start_menu_override_(base::DIR_START_MENU),
      taskbar_pins_override_(base::DIR_TASKBAR_PINS)
#endif
{}

ProfileResetterTest::~ProfileResetterTest() {
}

void ProfileResetterTest::SetUp() {
  extensions::ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  profile()->CreateWebDataService();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&CreateTemplateURLServiceForTesting));
  resetter_.reset(new ProfileResetter(profile()));
}

// PinnedTabsResetTest --------------------------------------------------------

class PinnedTabsResetTest : public BrowserWithTestWindowTest,
                            public ProfileResetterTestBase {
 protected:
  void SetUp() override;

  std::unique_ptr<content::WebContents> CreateWebContents();
};

void PinnedTabsResetTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  resetter_.reset(new ProfileResetter(profile()));
}

std::unique_ptr<content::WebContents> PinnedTabsResetTest::CreateWebContents() {
  return content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
}


// ConfigParserTest -----------------------------------------------------------

class ConfigParserTest : public testing::Test {
 protected:
  ConfigParserTest();
  ~ConfigParserTest() override;

  std::unique_ptr<BrandcodeConfigFetcher> WaitForRequest(const GURL& url);

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  std::unique_ptr<network::SimpleURLLoader> CreateFakeURLLoader(
      const GURL& url,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status);

  MOCK_METHOD0(Callback, void(void));

  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

ConfigParserTest::ConfigParserTest()
    : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

ConfigParserTest::~ConfigParserTest() {}

std::unique_ptr<BrandcodeConfigFetcher> ConfigParserTest::WaitForRequest(
    const GURL& url) {
  EXPECT_CALL(*this, Callback());
  std::string upload_data;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        upload_data = network::GetUploadData(request);
      }));
  std::unique_ptr<BrandcodeConfigFetcher> fetcher(new BrandcodeConfigFetcher(
      &test_url_loader_factory_,
      base::Bind(&ConfigParserTest::Callback, base::Unretained(this)), url,
      "ABCD"));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fetcher->IsActive());
  // Look for the brand code in the request.
  EXPECT_NE(std::string::npos, upload_data.find("ABCD"));
  return fetcher;
}

// A helper class to create/delete/check a Chrome desktop shortcut on Windows.
class ShortcutHandler {
 public:
  ShortcutHandler();
  ~ShortcutHandler();

  static bool IsSupported();
  ShortcutCommand CreateWithArguments(const base::string16& name,
                                      const base::string16& args);
  void CheckShortcutHasArguments(const base::string16& desired_args) const;
  void Delete();

 private:
#if defined(OS_WIN)
  base::FilePath shortcut_path_;
#endif
  DISALLOW_COPY_AND_ASSIGN(ShortcutHandler);
};

#if defined(OS_WIN)
ShortcutHandler::ShortcutHandler() {
}

ShortcutHandler::~ShortcutHandler() {
  if (!shortcut_path_.empty())
    Delete();
}

// static
bool ShortcutHandler::IsSupported() {
  return true;
}

ShortcutCommand ShortcutHandler::CreateWithArguments(
    const base::string16& name,
    const base::string16& args) {
  EXPECT_TRUE(shortcut_path_.empty());
  base::FilePath path_to_create;
  EXPECT_TRUE(base::PathService::Get(base::DIR_USER_DESKTOP, &path_to_create));
  path_to_create = path_to_create.Append(name);
  EXPECT_FALSE(base::PathExists(path_to_create)) << path_to_create.value();

  base::FilePath path_exe;
  EXPECT_TRUE(base::PathService::Get(base::FILE_EXE, &path_exe));
  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_target(path_exe);
  shortcut_properties.set_arguments(args);
  EXPECT_TRUE(base::win::CreateOrUpdateShortcutLink(
      path_to_create, shortcut_properties,
      base::win::SHORTCUT_CREATE_ALWAYS)) << path_to_create.value();
  shortcut_path_ = path_to_create;
  return ShortcutCommand(shortcut_path_, args);
}

void ShortcutHandler::CheckShortcutHasArguments(
    const base::string16& desired_args) const {
  EXPECT_FALSE(shortcut_path_.empty());
  base::string16 args;
  EXPECT_TRUE(base::win::ResolveShortcut(shortcut_path_, NULL, &args));
  EXPECT_EQ(desired_args, args);
}

void ShortcutHandler::Delete() {
  EXPECT_FALSE(shortcut_path_.empty());
  EXPECT_TRUE(base::DeleteFile(shortcut_path_, false));
  shortcut_path_.clear();
}
#else
ShortcutHandler::ShortcutHandler() {}

ShortcutHandler::~ShortcutHandler() {}

// static
bool ShortcutHandler::IsSupported() {
  return false;
}

ShortcutCommand ShortcutHandler::CreateWithArguments(
    const base::string16& name,
    const base::string16& args) {
  return ShortcutCommand();
}

void ShortcutHandler::CheckShortcutHasArguments(
    const base::string16& desired_args) const {
}

void ShortcutHandler::Delete() {
}
#endif  // defined(OS_WIN)

// MockInstantService
class MockInstantService : public InstantService {
 public:
  explicit MockInstantService(Profile* profile) : InstantService(profile) {}
  ~MockInstantService() override = default;

  MOCK_METHOD0(ResetToDefault, void());
};

// helper functions -----------------------------------------------------------

scoped_refptr<Extension> CreateExtension(const base::string16& name,
                                         const base::FilePath& path,
                                         Manifest::Location location,
                                         extensions::Manifest::Type type,
                                         bool installed_by_default) {
  base::DictionaryValue manifest;
  manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extensions::manifest_keys::kName, name);
  manifest.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
  switch (type) {
    case extensions::Manifest::TYPE_THEME:
      manifest.Set(extensions::manifest_keys::kTheme,
                   std::make_unique<base::DictionaryValue>());
      break;
    case extensions::Manifest::TYPE_HOSTED_APP:
      manifest.SetString(extensions::manifest_keys::kLaunchWebURL,
                         "http://www.google.com");
      manifest.SetString(extensions::manifest_keys::kUpdateURL,
                         "http://clients2.google.com/service/update2/crx");
      break;
    case extensions::Manifest::TYPE_EXTENSION:
      // do nothing
      break;
    default:
      NOTREACHED();
  }
  manifest.SetString(extensions::manifest_keys::kOmniboxKeyword, name);
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path,
      location,
      manifest,
      installed_by_default ? Extension::WAS_INSTALLED_BY_DEFAULT
                           : Extension::NO_FLAGS,
      &error);
  EXPECT_TRUE(extension.get() != NULL) << error;
  return extension;
}

void ReplaceString(std::string* str,
                   const std::string& placeholder,
                   const std::string& substitution) {
  ASSERT_NE(static_cast<std::string*>(NULL), str);
  size_t placeholder_pos = str->find(placeholder);
  ASSERT_NE(std::string::npos, placeholder_pos);
  str->replace(placeholder_pos, placeholder.size(), substitution);
}


/********************* Tests *********************/

TEST_F(ProfileResetterTest, ResetNothing) {
  // The callback should be called even if there is nothing to reset.
  ResetAndWait(0);
}

TEST_F(ProfileResetterTest, ResetDefaultSearchEngineNonOrganic) {
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE, kDistributionConfig);

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile());
  const TemplateURL* default_engine = model->GetDefaultSearchProvider();
  ASSERT_NE(static_cast<TemplateURL*>(NULL), default_engine);
  EXPECT_EQ(base::ASCIIToUTF16("first"), default_engine->short_name());
  EXPECT_EQ(base::ASCIIToUTF16("firstkey"), default_engine->keyword());
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}", default_engine->url());
}

TEST_F(ProfileResetterTest, ResetDefaultSearchEnginePartially) {
  // Search engine's logic is tested by
  // TemplateURLServiceTest.RepairPrepopulatedSearchEngines.
  // Make sure TemplateURLService has loaded.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE);

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLService::TemplateURLVector urls = model->GetTemplateURLs();

  // The second call should produce no effect.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE);

  EXPECT_EQ(urls, model->GetTemplateURLs());
}

TEST_F(ProfileResetterTest, ResetHomepageNonOrganic) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  prefs->SetString(prefs::kHomePage, "http://google.com");
  prefs->SetBoolean(prefs::kShowHomeButton, false);

  ResetAndWait(ProfileResetter::HOMEPAGE, kDistributionConfig);

  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ("http://www.foo.com", prefs->GetString(prefs::kHomePage));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(ProfileResetterTest, ResetHomepagePartially) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, "http://www.foo.com");
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  ResetAndWait(ProfileResetter::HOMEPAGE);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ("http://www.foo.com", prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(ProfileResetterTest, ResetContentSettings) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  GURL url("http://example.org");
  std::map<ContentSettingsType, ContentSetting> default_settings;

  // TODO(raymes): Clean up this test so that we don't have such ugly iteration
  // over the content settings.
  content_settings::ContentSettingsRegistry* registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType content_type = info->website_settings_info()->type();
    if (content_type == ContentSettingsType::MIXEDSCRIPT ||
        content_type == ContentSettingsType::PROTOCOL_HANDLERS) {
      // These types are excluded because one can't call
      // GetDefaultContentSetting() for them.
      continue;
    }
    ContentSetting default_setting =
        host_content_settings_map->GetDefaultContentSetting(content_type, NULL);
    default_settings[content_type] = default_setting;
    ContentSetting wildcard_setting = default_setting == CONTENT_SETTING_BLOCK
                                          ? CONTENT_SETTING_ALLOW
                                          : CONTENT_SETTING_BLOCK;
    ContentSetting site_setting = default_setting == CONTENT_SETTING_ALLOW
                                      ? CONTENT_SETTING_ALLOW
                                      : CONTENT_SETTING_BLOCK;
    if (info->IsSettingValid(wildcard_setting)) {
      host_content_settings_map->SetDefaultContentSetting(content_type,
                                                          wildcard_setting);
    }
    if (info->IsSettingValid(site_setting)) {
      host_content_settings_map->SetContentSettingDefaultScope(
          url, url, content_type, std::string(), site_setting);
      ContentSettingsForOneType host_settings;
      host_content_settings_map->GetSettingsForOneType(
          content_type, std::string(), &host_settings);
      EXPECT_EQ(2U, host_settings.size());
    }
  }

  ResetAndWait(ProfileResetter::CONTENT_SETTINGS);

  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType content_type = info->website_settings_info()->type();
    if (content_type == ContentSettingsType::MIXEDSCRIPT ||
        content_type == ContentSettingsType::PROTOCOL_HANDLERS)
      continue;
    ContentSetting default_setting =
        host_content_settings_map->GetDefaultContentSetting(content_type,
                                                              NULL);
    EXPECT_TRUE(default_settings.count(content_type));
    EXPECT_EQ(default_settings[content_type], default_setting);
    ContentSetting site_setting = host_content_settings_map->GetContentSetting(
        GURL("example.org"), GURL(), content_type, std::string());
    EXPECT_EQ(default_setting, site_setting);

    ContentSettingsForOneType host_settings;
    host_content_settings_map->GetSettingsForOneType(
        content_type, std::string(), &host_settings);
    EXPECT_EQ(1U, host_settings.size());
  }
}

TEST_F(ProfileResetterTest, ResetExtensionsByDisabling) {
  service_->Init();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(theme_service));

  scoped_refptr<Extension> theme = CreateExtension(
      base::ASCIIToUTF16("example1"), temp_dir.GetPath(), Manifest::UNPACKED,
      extensions::Manifest::TYPE_THEME, false);
  service_->FinishInstallationForTest(theme.get());
  theme_change_observer.Wait();

  EXPECT_FALSE(theme_service->UsingDefaultTheme());

  scoped_refptr<Extension> ext2 = CreateExtension(
      base::ASCIIToUTF16("example2"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")), Manifest::UNPACKED,
      extensions::Manifest::TYPE_EXTENSION, false);
  service_->AddExtension(ext2.get());
  // Component extensions and policy-managed extensions shouldn't be disabled.
  scoped_refptr<Extension> ext3 = CreateExtension(
      base::ASCIIToUTF16("example3"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")),
      Manifest::COMPONENT,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext3.get());
  scoped_refptr<Extension> ext4 =
      CreateExtension(base::ASCIIToUTF16("example4"),
                      base::FilePath(FILE_PATH_LITERAL("//nonexistent3")),
                      Manifest::EXTERNAL_POLICY_DOWNLOAD,
                      extensions::Manifest::TYPE_EXTENSION,
                      false);
  service_->AddExtension(ext4.get());
  scoped_refptr<Extension> ext5 = CreateExtension(
      base::ASCIIToUTF16("example5"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent4")),
      Manifest::EXTERNAL_COMPONENT,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext5.get());
  scoped_refptr<Extension> ext6 = CreateExtension(
      base::ASCIIToUTF16("example6"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent5")),
      Manifest::EXTERNAL_POLICY,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext6.get());
  EXPECT_EQ(6u, registry()->enabled_extensions().size());

  ResetAndWait(ProfileResetter::EXTENSIONS);
  EXPECT_EQ(4u, registry()->enabled_extensions().size());
  EXPECT_FALSE(registry()->enabled_extensions().Contains(theme->id()));
  EXPECT_FALSE(registry()->enabled_extensions().Contains(ext2->id()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(ext3->id()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(ext4->id()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(ext5->id()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(ext6->id()));
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
}

TEST_F(ProfileResetterTest, ResetExtensionsByDisablingNonOrganic) {
  scoped_refptr<Extension> ext2 = CreateExtension(
      base::ASCIIToUTF16("example2"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")), Manifest::UNPACKED,
      extensions::Manifest::TYPE_EXTENSION, false);
  service_->AddExtension(ext2.get());
  // Components and external policy extensions shouldn't be deleted.
  scoped_refptr<Extension> ext3 = CreateExtension(
      base::ASCIIToUTF16("example3"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")), Manifest::UNPACKED,
      extensions::Manifest::TYPE_EXTENSION, false);
  service_->AddExtension(ext3.get());
  EXPECT_EQ(2u, registry()->enabled_extensions().size());

  std::string master_prefs(kDistributionConfig);
  ReplaceString(&master_prefs, "placeholder_for_id", ext3->id());

  ResetAndWait(ProfileResetter::EXTENSIONS, master_prefs);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(ext3->id()));
}

TEST_F(ProfileResetterTest, ResetExtensionsAndDefaultApps) {
  service_->Init();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(theme_service));

  scoped_refptr<Extension> ext1 = CreateExtension(
      base::ASCIIToUTF16("example1"), temp_dir.GetPath(), Manifest::UNPACKED,
      extensions::Manifest::TYPE_THEME, false);
  service_->FinishInstallationForTest(ext1.get());
  theme_change_observer.Wait();

  EXPECT_FALSE(theme_service->UsingDefaultTheme());

  scoped_refptr<Extension> ext2 = CreateExtension(
      base::ASCIIToUTF16("example2"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")), Manifest::UNPACKED,
      extensions::Manifest::TYPE_EXTENSION, false);
  service_->AddExtension(ext2.get());

  scoped_refptr<Extension> ext3 = CreateExtension(
      base::ASCIIToUTF16("example2"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent3")), Manifest::UNPACKED,
      extensions::Manifest::TYPE_HOSTED_APP, true);
  service_->AddExtension(ext3.get());
  EXPECT_EQ(3u, registry()->enabled_extensions().size());

  ResetAndWait(ProfileResetter::EXTENSIONS);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_FALSE(registry()->enabled_extensions().Contains(ext1->id()));
  EXPECT_FALSE(registry()->enabled_extensions().Contains(ext2->id()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(ext3->id()));
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
}

TEST_F(ProfileResetterTest, ResetExtensionsByReenablingExternalComponents) {
  service_->Init();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<Extension> ext =
      CreateExtension(base::ASCIIToUTF16("example"),
                      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
                      Manifest::EXTERNAL_COMPONENT,
                      extensions::Manifest::TYPE_EXTENSION, false);
  service_->AddExtension(ext.get());

  service_->DisableExtension(ext->id(),
                             extensions::disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(registry()->enabled_extensions().Contains(ext->id()));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(ext->id()));

  ResetAndWait(ProfileResetter::EXTENSIONS);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(ext->id()));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(ext->id()));
}

TEST_F(ProfileResetterTest, ResetStartPageNonOrganic) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);

  SessionStartupPref startup_pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  ResetAndWait(ProfileResetter::STARTUP_PAGES, kDistributionConfig);

  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::URLS, startup_pref.type);
  const GURL urls[] = {GURL("http://goo.gl"), GURL("http://foo.de")};
  EXPECT_EQ(std::vector<GURL>(urls, urls + base::size(urls)),
            startup_pref.urls);
}


TEST_F(ProfileResetterTest, ResetStartPagePartially) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);

  const GURL urls[] = {GURL("http://foo"), GURL("http://bar")};
  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.assign(urls, urls + base::size(urls));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  ResetAndWait(ProfileResetter::STARTUP_PAGES, std::string());

  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(), startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(urls, urls + base::size(urls)),
            startup_pref.urls);
}

TEST_F(PinnedTabsResetTest, ResetPinnedTabs) {
  std::unique_ptr<content::WebContents> contents1(CreateWebContents());
  std::unique_ptr<content::WebContents> contents2(CreateWebContents());
  std::unique_ptr<content::WebContents> contents3(CreateWebContents());
  std::unique_ptr<content::WebContents> contents4(CreateWebContents());
  content::WebContents* raw_contents1 = contents1.get();
  content::WebContents* raw_contents2 = contents2.get();
  content::WebContents* raw_contents3 = contents3.get();
  content::WebContents* raw_contents4 = contents4.get();
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AppendWebContents(std::move(contents4), true);
  tab_strip_model->AppendWebContents(std::move(contents3), true);
  tab_strip_model->AppendWebContents(std::move(contents2), true);
  tab_strip_model->SetTabPinned(2, true);
  tab_strip_model->AppendWebContents(std::move(contents1), true);
  tab_strip_model->SetTabPinned(3, true);

  EXPECT_EQ(raw_contents2, tab_strip_model->GetWebContentsAt(0));
  EXPECT_EQ(raw_contents1, tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(raw_contents4, tab_strip_model->GetWebContentsAt(2));
  EXPECT_EQ(raw_contents3, tab_strip_model->GetWebContentsAt(3));
  EXPECT_EQ(2, tab_strip_model->IndexOfFirstNonPinnedTab());

  ResetAndWait(ProfileResetter::PINNED_TABS);

  EXPECT_EQ(raw_contents2, tab_strip_model->GetWebContentsAt(0));
  EXPECT_EQ(raw_contents1, tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(raw_contents4, tab_strip_model->GetWebContentsAt(2));
  EXPECT_EQ(raw_contents3, tab_strip_model->GetWebContentsAt(3));
  EXPECT_EQ(0, tab_strip_model->IndexOfFirstNonPinnedTab());
}

TEST_F(ProfileResetterTest, ResetShortcuts) {
  ShortcutHandler shortcut;
  ShortcutCommand command_line = shortcut.CreateWithArguments(
      base::ASCIIToUTF16("chrome.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));
  shortcut.CheckShortcutHasArguments(base::ASCIIToUTF16(
      "--profile-directory=Default foo.com"));

  ResetAndWait(ProfileResetter::SHORTCUTS);

  shortcut.CheckShortcutHasArguments(base::ASCIIToUTF16(
      "--profile-directory=Default"));
}

TEST_F(ProfileResetterTest, ResetFewFlags) {
  // mock_object_ is a StrictMock, so we verify that it is called only once.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::CONTENT_SETTINGS);
}

// Tries to load unavailable config file.
TEST_F(ConfigParserTest, NoConnectivity) {
  const GURL url("http://test");
  test_url_loader_factory().AddResponse(
      url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::HTTP_INTERNAL_SERVER_ERROR));

  std::unique_ptr<BrandcodeConfigFetcher> fetcher = WaitForRequest(GURL(url));
  EXPECT_FALSE(fetcher->GetSettings());
}

// Tries to load available config file.
TEST_F(ConfigParserTest, ParseConfig) {
  const GURL url("http://test");
  std::string xml_config(kXmlConfig);
  ReplaceString(&xml_config, "placeholder_for_data", kDistributionConfig);
  ReplaceString(&xml_config,
                "placeholder_for_id",
                "abbaabbaabbaabbaabbaabbaabbaabba");
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 200 OK\nContent-type: text/xml\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "text/xml";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = xml_config.size();
  test_url_loader_factory().AddResponse(url, std::move(head), xml_config,
                                        status);

  std::unique_ptr<BrandcodeConfigFetcher> fetcher = WaitForRequest(GURL(url));
  std::unique_ptr<BrandcodedDefaultSettings> settings = fetcher->GetSettings();
  ASSERT_TRUE(settings);

  std::vector<std::string> extension_ids;
  EXPECT_TRUE(settings->GetExtensions(&extension_ids));
  EXPECT_EQ(1u, extension_ids.size());
  EXPECT_EQ("abbaabbaabbaabbaabbaabbaabbaabba", extension_ids[0]);

  std::string homepage;
  EXPECT_TRUE(settings->GetHomepage(&homepage));
  EXPECT_EQ("http://www.foo.com", homepage);

  std::unique_ptr<base::ListValue> startup_list(
      settings->GetUrlsToRestoreOnStartup());
  EXPECT_TRUE(startup_list);
  std::vector<std::string> startup_pages;
  for (auto i = startup_list->begin(); i != startup_list->end(); ++i) {
    std::string url;
    EXPECT_TRUE(i->GetAsString(&url));
    startup_pages.push_back(url);
  }
  ASSERT_EQ(2u, startup_pages.size());
  EXPECT_EQ("http://goo.gl", startup_pages[0]);
  EXPECT_EQ("http://foo.de", startup_pages[1]);
}

TEST_F(ProfileResetterTest, CheckSnapshots) {
  ResettableSettingsSnapshot empty_snap(profile());
  EXPECT_EQ(0, empty_snap.FindDifferentFields(empty_snap));

  scoped_refptr<Extension> ext = CreateExtension(
      base::ASCIIToUTF16("example"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")), Manifest::UNPACKED,
      extensions::Manifest::TYPE_EXTENSION, false);
  ASSERT_TRUE(ext.get());
  service_->AddExtension(ext.get());

  std::string master_prefs(kDistributionConfig);
  std::string ext_id = ext->id();
  ReplaceString(&master_prefs, "placeholder_for_id", ext_id);

  // Reset to non organic defaults.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::STARTUP_PAGES,
               master_prefs);
  ShortcutHandler shortcut_hijacked;
  ShortcutCommand command_line = shortcut_hijacked.CreateWithArguments(
      base::ASCIIToUTF16("chrome1.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));
  shortcut_hijacked.CheckShortcutHasArguments(
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));
  ShortcutHandler shortcut_ok;
  shortcut_ok.CreateWithArguments(
      base::ASCIIToUTF16("chrome2.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default1"));

  ResettableSettingsSnapshot nonorganic_snap(profile());
  nonorganic_snap.RequestShortcuts(base::Closure());
  // Let it enumerate shortcuts on a blockable task runner.
  content::RunAllTasksUntilIdle();
  int diff_fields = ResettableSettingsSnapshot::ALL_FIELDS;
  if (!ShortcutHandler::IsSupported())
    diff_fields &= ~ResettableSettingsSnapshot::SHORTCUTS;
  EXPECT_EQ(diff_fields,
            empty_snap.FindDifferentFields(nonorganic_snap));
  empty_snap.Subtract(nonorganic_snap);
  EXPECT_TRUE(empty_snap.startup_urls().empty());
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(),
            empty_snap.startup_type());
  EXPECT_TRUE(empty_snap.homepage().empty());
  EXPECT_TRUE(empty_snap.homepage_is_ntp());
  EXPECT_FALSE(empty_snap.show_home_button());
  EXPECT_NE(std::string::npos, empty_snap.dse_url().find("{google:baseURL}"));
  EXPECT_EQ(ResettableSettingsSnapshot::ExtensionList(),
            empty_snap.enabled_extensions());
  EXPECT_EQ(std::vector<ShortcutCommand>(), empty_snap.shortcuts());

  // Reset to organic defaults.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::STARTUP_PAGES |
               ProfileResetter::EXTENSIONS |
               ProfileResetter::SHORTCUTS);

  ResettableSettingsSnapshot organic_snap(profile());
  organic_snap.RequestShortcuts(base::Closure());
  // Let it enumerate shortcuts on a blockable task runner.
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(diff_fields, nonorganic_snap.FindDifferentFields(organic_snap));
  nonorganic_snap.Subtract(organic_snap);
  const GURL urls[] = {GURL("http://foo.de"), GURL("http://goo.gl")};
  EXPECT_EQ(std::vector<GURL>(urls, urls + base::size(urls)),
            nonorganic_snap.startup_urls());
  EXPECT_EQ(SessionStartupPref::URLS, nonorganic_snap.startup_type());
  EXPECT_EQ("http://www.foo.com", nonorganic_snap.homepage());
  EXPECT_FALSE(nonorganic_snap.homepage_is_ntp());
  EXPECT_TRUE(nonorganic_snap.show_home_button());
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}", nonorganic_snap.dse_url());
  EXPECT_EQ(ResettableSettingsSnapshot::ExtensionList(
      1, std::make_pair(ext_id, "example")),
      nonorganic_snap.enabled_extensions());
  if (ShortcutHandler::IsSupported()) {
    std::vector<ShortcutCommand> shortcuts = nonorganic_snap.shortcuts();
    ASSERT_EQ(1u, shortcuts.size());
    EXPECT_EQ(command_line.first.value(), shortcuts[0].first.value());
    EXPECT_EQ(command_line.second, shortcuts[0].second);
  }
}

TEST_F(ProfileResetterTest, FeedbackSerializationAsProtoTest) {
  // Reset to non organic defaults.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::STARTUP_PAGES,
               kDistributionConfig);

  scoped_refptr<Extension> ext = CreateExtension(
      base::ASCIIToUTF16("example"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")), Manifest::UNPACKED,
      extensions::Manifest::TYPE_EXTENSION, false);
  ASSERT_TRUE(ext.get());
  service_->AddExtension(ext.get());

  ShortcutHandler shortcut;
  ShortcutCommand command_line = shortcut.CreateWithArguments(
      base::ASCIIToUTF16("chrome.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));

  ResettableSettingsSnapshot nonorganic_snap(profile());
  nonorganic_snap.RequestShortcuts(base::Closure());
  // Let it enumerate shortcuts on a blockable task runner.
  content::RunAllTasksUntilIdle();

  static_assert(ResettableSettingsSnapshot::ALL_FIELDS == 31,
                "this test needs to be expanded");
  for (int field_mask = 0; field_mask <= ResettableSettingsSnapshot::ALL_FIELDS;
       ++field_mask) {
    std::unique_ptr<reset_report::ChromeResetReport> report =
        SerializeSettingsReportToProto(nonorganic_snap, field_mask);

    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::STARTUP_MODE),
              report->startup_url_path_size() > 0);
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::STARTUP_MODE),
              report->has_startup_type());
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::HOMEPAGE),
              report->has_homepage_path());
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::HOMEPAGE),
              report->has_homepage_is_new_tab_page());
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::HOMEPAGE),
              report->has_show_home_button());
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::DSE_URL),
              report->has_default_search_engine_path());
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::EXTENSIONS),
              report->enabled_extensions_size() > 0);
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::SHORTCUTS) &&
                  ShortcutHandler::IsSupported(),
              report->shortcuts_size() > 0);
  }
}

struct FeedbackCapture {
  void SetFeedback(Profile* profile,
                   const ResettableSettingsSnapshot& snapshot) {
    list_ = GetReadableFeedbackForSnapshot(profile, snapshot);
    OnUpdatedList();
  }

  void Fail() {
    ADD_FAILURE() << "This method shouldn't be called.";
  }

  MOCK_METHOD0(OnUpdatedList, void(void));

  std::unique_ptr<base::ListValue> list_;
};

// Make sure GetReadableFeedback handles non-ascii letters.
TEST_F(ProfileResetterTest, GetReadableFeedback) {
  scoped_refptr<Extension> ext = CreateExtension(
      base::WideToUTF16(L"Tiësto"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")), Manifest::UNPACKED,
      extensions::Manifest::TYPE_EXTENSION, false);
  ASSERT_TRUE(ext.get());
  service_->AddExtension(ext.get());

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  // The URL is "http://россия.рф".
  std::wstring url(L"http://"
    L"\u0440\u043e\u0441\u0441\u0438\u044f.\u0440\u0444");
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, base::WideToUTF8(url));

  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.push_back(GURL(base::WideToUTF8(url)));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  ShortcutHandler shortcut;
  ShortcutCommand command_line = shortcut.CreateWithArguments(
      base::ASCIIToUTF16("chrome.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));

  FeedbackCapture capture;
  EXPECT_CALL(capture, OnUpdatedList());
  ResettableSettingsSnapshot snapshot(profile());
  snapshot.RequestShortcuts(base::Bind(&FeedbackCapture::SetFeedback,
                                       base::Unretained(&capture), profile(),
                                       std::cref(snapshot)));
  // Let it enumerate shortcuts on a blockable task runner.
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(snapshot.shortcuts_determined());
  ::testing::Mock::VerifyAndClearExpectations(&capture);
  // The homepage and the startup page are in punycode. They are unreadable.
  // Trying to find the extension name.
  std::unique_ptr<base::ListValue> list = std::move(capture.list_);
  ASSERT_TRUE(list);
  bool checked_extensions = false;
  bool checked_shortcuts = false;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* dict = NULL;
    ASSERT_TRUE(list->GetDictionary(i, &dict));
    std::string value;
    ASSERT_TRUE(dict->GetString("key", &value));
    if (value == "Extensions") {
      base::string16 extensions;
      EXPECT_TRUE(dict->GetString("value", &extensions));
      EXPECT_EQ(base::WideToUTF16(L"Tiësto"), extensions);
      checked_extensions = true;
    } else if (value == "Shortcut targets") {
      base::string16 targets;
      EXPECT_TRUE(dict->GetString("value", &targets));
      EXPECT_NE(base::string16::npos,
                targets.find(base::ASCIIToUTF16("foo.com"))) << targets;
      checked_shortcuts = true;
    }
  }
  EXPECT_TRUE(checked_extensions);
  EXPECT_EQ(ShortcutHandler::IsSupported(), checked_shortcuts);
}

TEST_F(ProfileResetterTest, DestroySnapshotFast) {
  FeedbackCapture capture;
  std::unique_ptr<ResettableSettingsSnapshot> deleted_snapshot(
      new ResettableSettingsSnapshot(profile()));
  deleted_snapshot->RequestShortcuts(base::Bind(&FeedbackCapture::Fail,
                                                base::Unretained(&capture)));
  deleted_snapshot.reset();
  // Running remaining tasks shouldn't trigger the callback to be called as
  // |deleted_snapshot| was deleted before it could run.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ProfileResetterTest, ResetNTPCustomizationsTest) {
  MockInstantService mock_ntp_service(profile());
  resetter_->ntp_service_ = &mock_ntp_service;

  EXPECT_CALL(mock_ntp_service, ResetToDefault());
  ResetAndWait(ProfileResetter::NTP_CUSTOMIZATIONS);
}

}  // namespace
