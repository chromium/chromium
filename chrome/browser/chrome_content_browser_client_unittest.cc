// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <list>
#include <map>
#include <memory>

#include "ash/webui/camera_app_ui/url_constants.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/services/storage/public/cpp/storage_prefs.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "media/media_buildflags.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "ui/base/page_transition_types.h"
#else
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/scanning/url_constants.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/storage_partition_config.h"
#include "third_party/blink/public/common/features.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using ::content::BrowsingDataFilterBuilder;
using ::testing::_;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::NotNull;

class ChromeContentBrowserClientTest : public testing::Test {
 public:
  ChromeContentBrowserClientTest()
#if BUILDFLAG(IS_CHROMEOS_ASH)
      : test_system_web_app_manager_creator_(base::BindRepeating(
            &ChromeContentBrowserClientTest::CreateSystemWebAppManager,
            base::Unretained(this)))
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  {
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<KeyedService> CreateSystemWebAppManager(Profile* profile) {
    // Unit tests need SWAs from production. Creates real SystemWebAppManager
    // instead of `TestSystemWebAppManager::BuildDefault()` for
    // `TestingProfile`.
    auto swa_manager = std::make_unique<ash::SystemWebAppManager>(profile);
    return swa_manager;
  }
  // The custom manager creator should be constructed before `TestingProfile`.
  ash::TestSystemWebAppManagerCreator test_system_web_app_manager_creator_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ChromeContentBrowserClientTest, ShouldAssignSiteForURL) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(client.ShouldAssignSiteForURL(GURL("chrome-native://test")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("http://www.google.com")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("https://www.google.com")));
}

// BrowserWithTestWindowTest doesn't work on Android.
#if !BUILDFLAG(IS_ANDROID)

using ChromeContentBrowserClientWindowTest = BrowserWithTestWindowTest;

static void DidOpenURLForWindowTest(content::WebContents** target_contents,
                                    content::WebContents* opened_contents) {
  DCHECK(target_contents);

  *target_contents = opened_contents;
}

// This test opens two URLs using ContentBrowserClient::OpenURL. It expects the
// URLs to be opened in new tabs and activated, changing the active tabs after
// each call and increasing the tab count by 2.
TEST_F(ChromeContentBrowserClientWindowTest, OpenURL) {
  ChromeContentBrowserClient client;

  int previous_count = browser()->tab_strip_model()->count();

  GURL urls[] = {GURL("https://www.google.com"),
                 GURL("https://www.chromium.org")};

  for (const GURL& url : urls) {
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    // TODO(peter): We should have more in-depth browser tests for the window
    // opening functionality, which also covers Android. This test can currently
    // only be ran on platforms where OpenURL is implemented synchronously.
    // See https://crbug.com/457667.
    content::WebContents* web_contents = nullptr;
    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::Create(browser()->profile());
    client.OpenURL(site_instance.get(), params,
                   base::BindOnce(&DidOpenURLForWindowTest, &web_contents));

    EXPECT_TRUE(web_contents);

    content::WebContents* active_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_contents, active_contents);
    EXPECT_EQ(url, active_contents->GetVisibleURL());
  }

  EXPECT_EQ(previous_count + 2, browser()->tab_strip_model()->count());
}

// TODO(crbug.com/566091): Remove the need for ShouldStayInParentProcessForNTP()
//    and associated test.
TEST_F(ChromeContentBrowserClientWindowTest, ShouldStayInParentProcessForNTP) {
  ChromeContentBrowserClient client;
  // Remote 3P NTPs effectively have a URL chrome-search://remote-ntp. This
  // is so an iframe with the src of chrome-search://most-visited/title.html can
  // be embedded within the remote NTP.
  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::CreateForURL(browser()->profile(),
                                          GURL("chrome-search://remote-ntp"));
  EXPECT_TRUE(client.ShouldStayInParentProcessForNTP(
      GURL("chrome-search://most-visited/title.html"),
      site_instance->GetSiteURL()));

  // Only the most visited tiles host is allowed to stay in the 3P NTP.
  EXPECT_FALSE(client.ShouldStayInParentProcessForNTP(
      GURL("chrome-search://foo/"), site_instance->GetSiteURL()));
  EXPECT_FALSE(client.ShouldStayInParentProcessForNTP(
      GURL("chrome://new-tab-page"), site_instance->GetSiteURL()));

  site_instance = content::SiteInstance::CreateForURL(
      browser()->profile(), GURL("chrome://new-tab-page"));

  // chrome://new-tab-page is an NTP replacing local-ntp and supports OOPIFs.
  // ShouldStayInParentProcessForNTP() should only return true for NTPs hosted
  // under the chrome-search: scheme.
  EXPECT_FALSE(client.ShouldStayInParentProcessForNTP(
      GURL("chrome://new-tab-page"), site_instance->GetSiteURL()));

  // For now, we also allow chrome-search://most-visited to stay in 1P NTP,
  // chrome://new-tab-page.  We should consider tightening this to only allow
  // most-visited tiles to stay in 3P NTP.
  EXPECT_TRUE(client.ShouldStayInParentProcessForNTP(
      GURL("chrome-search://most-visited"), site_instance->GetSiteURL()));
}

TEST_F(ChromeContentBrowserClientWindowTest, OverrideNavigationParams) {
  ChromeContentBrowserClient client;
  ui::PageTransition transition;
  bool is_renderer_initiated;
  content::Referrer referrer = content::Referrer();
  absl::optional<url::Origin> initiator_origin = absl::nullopt;

  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::CreateForURL(browser()->profile(),
                                          GURL("chrome-search://remote-ntp"));
  transition = ui::PAGE_TRANSITION_LINK;
  is_renderer_initiated = true;
  // The origin is a placeholder to test that |initiator_origin| is set to
  // absl::nullopt and is not meant to represent what would happen in practice.
  initiator_origin = url::Origin::Create(GURL("https://www.example.com"));
  client.OverrideNavigationParams(site_instance.get(), &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           transition));
  EXPECT_FALSE(is_renderer_initiated);
  EXPECT_EQ(absl::nullopt, initiator_origin);

  site_instance = content::SiteInstance::CreateForURL(
      browser()->profile(), GURL("chrome://new-tab-page"));
  transition = ui::PAGE_TRANSITION_LINK;
  is_renderer_initiated = true;
  initiator_origin = url::Origin::Create(GURL("https://www.example.com"));
  client.OverrideNavigationParams(site_instance.get(), &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           transition));
  EXPECT_FALSE(is_renderer_initiated);
  EXPECT_EQ(absl::nullopt, initiator_origin);

  // No change for transitions that are not PAGE_TRANSITION_LINK.
  site_instance = content::SiteInstance::CreateForURL(
      browser()->profile(), GURL("chrome://new-tab-page"));
  transition = ui::PAGE_TRANSITION_TYPED;
  client.OverrideNavigationParams(site_instance.get(), &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED, transition));

  // No change for transitions on a non-NTP page.
  site_instance = content::SiteInstance::CreateForURL(
      browser()->profile(), GURL("https://www.example.com"));
  transition = ui::PAGE_TRANSITION_LINK;
  client.OverrideNavigationParams(site_instance.get(), &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK, transition));
}

#endif  // !BUILDFLAG(IS_ANDROID)

// NOTE: Any updates to the expectations in these tests should also be done in
// the browser test WebRtcDisableEncryptionFlagBrowserTest.
class DisableWebRtcEncryptionFlagTest : public testing::Test {
 public:
  DisableWebRtcEncryptionFlagTest()
      : from_command_line_(base::CommandLine::NO_PROGRAM),
        to_command_line_(base::CommandLine::NO_PROGRAM) {}

  DisableWebRtcEncryptionFlagTest(const DisableWebRtcEncryptionFlagTest&) =
      delete;
  DisableWebRtcEncryptionFlagTest& operator=(
      const DisableWebRtcEncryptionFlagTest&) = delete;

 protected:
  void SetUp() override {
    from_command_line_.AppendSwitch(switches::kDisableWebRtcEncryption);
  }

  void MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel channel) {
    ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
        &to_command_line_, from_command_line_, channel);
  }

  base::CommandLine from_command_line_;
  base::CommandLine to_command_line_;
};

TEST_F(DisableWebRtcEncryptionFlagTest, UnknownChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::UNKNOWN);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, CanaryChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::CANARY);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, DevChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::DEV);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, BetaChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::BETA);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
#else
  EXPECT_FALSE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
#endif
}

TEST_F(DisableWebRtcEncryptionFlagTest, StableChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::STABLE);
  EXPECT_FALSE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

class BlinkSettingsFieldTrialTest : public testing::Test {
 public:
  static const char kDisallowFetchFieldTrialName[];
  static const char kFakeGroupName[];

  BlinkSettingsFieldTrialTest()
      : command_line_(base::CommandLine::NO_PROGRAM),
        testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    command_line_.AppendSwitchASCII(switches::kProcessType,
                                    switches::kRendererProcess);
  }

  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  void CreateFieldTrial(const char* trial_name, const char* group_name) {
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  }

  void CreateFieldTrialWithParams(const char* trial_name,
                                  const char* group_name,
                                  const char* key1,
                                  const char* value1,
                                  const char* key2,
                                  const char* value2) {
    std::map<std::string, std::string> params;
    params.insert(std::make_pair(key1, value1));
    params.insert(std::make_pair(key2, value2));
    CreateFieldTrial(trial_name, kFakeGroupName);
    variations::AssociateVariationParams(trial_name, kFakeGroupName, params);
  }

  void AppendContentBrowserClientSwitches() {
    client_.AppendExtraCommandLineSwitches(&command_line_, kFakeChildProcessId);
  }

  const base::CommandLine& command_line() const { return command_line_; }

  void AppendBlinkSettingsSwitch(const char* value) {
    command_line_.AppendSwitchASCII(blink::switches::kBlinkSettings, value);
  }

 private:
  static const int kFakeChildProcessId = 1;

  ChromeContentBrowserClient client_;
  base::CommandLine command_line_;

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_;
};

const char BlinkSettingsFieldTrialTest::kDisallowFetchFieldTrialName[] =
    "DisallowFetchForDocWrittenScriptsInMainFrame";
const char BlinkSettingsFieldTrialTest::kFakeGroupName[] = "FakeGroup";

TEST_F(BlinkSettingsFieldTrialTest, NoFieldTrial) {
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(command_line().HasSwitch(blink::switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, FieldTrialWithoutParams) {
  CreateFieldTrial(kDisallowFetchFieldTrialName, kFakeGroupName);
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(command_line().HasSwitch(blink::switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, BlinkSettingsSwitchAlreadySpecified) {
  AppendBlinkSettingsSwitch("foo");
  CreateFieldTrialWithParams(kDisallowFetchFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(blink::switches::kBlinkSettings));
  EXPECT_EQ("foo", command_line().GetSwitchValueASCII(
                       blink::switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, FieldTrialEnabled) {
  CreateFieldTrialWithParams(kDisallowFetchFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(blink::switches::kBlinkSettings));
  EXPECT_EQ("key1=value1,key2=value2", command_line().GetSwitchValueASCII(
                                           blink::switches::kBlinkSettings));
}

#if !BUILDFLAG(IS_ANDROID)
namespace content {

class InstantNTPURLRewriteTest : public BrowserWithTestWindowTest {
 protected:
  void InstallTemplateURLWithNewTabPage(GURL new_tab_page_url) {
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetShortName(u"foo.com");
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.new_tab_url = new_tab_page_url.spec();
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }
};

TEST_F(InstantNTPURLRewriteTest, UberURLHandler_InstantExtendedNewTabPage) {
  const GURL url_original(chrome::kChromeUINewTabURL);
  const GURL url_rewritten("https://www.example.com/newtab");
  InstallTemplateURLWithNewTabPage(url_rewritten);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 use_cacheable_ntp:1"));

  AddTab(browser(), GURL(url::kAboutBlankURL));
  NavigateAndCommitActiveTab(url_original);

  NavigationEntry* entry = browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetController()
                               .GetLastCommittedEntry();
  ASSERT_THAT(entry, NotNull());
  EXPECT_EQ(url_rewritten, entry->GetURL());
  EXPECT_EQ(url_original, entry->GetVirtualURL());
}

}  // namespace content
#endif  // !BUILDFLAG(IS_ANDROID)

class ChromeContentBrowserClientGetLoggingFileTest : public testing::Test {};

TEST_F(ChromeContentBrowserClientGetLoggingFileTest, GetLoggingFile) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  ChromeContentBrowserClient client;
  base::FilePath log_file_name;
  EXPECT_FALSE(client.GetLoggingFileName(cmd_line).empty());
}

TEST_F(ChromeContentBrowserClientGetLoggingFileTest,
       GetLoggingFileFromCommandLine) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitchASCII(switches::kLogFile, "test_log.txt");
  ChromeContentBrowserClient client;
  base::FilePath log_file_name;
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("test_log.txt")).value(),
            client.GetLoggingFileName(cmd_line).value());
}

class TestChromeContentBrowserClient : public ChromeContentBrowserClient {
 public:
  using ChromeContentBrowserClient::HandleWebUI;
  using ChromeContentBrowserClient::HandleWebUIReverse;
};

TEST_F(ChromeContentBrowserClientTest, HandleWebUI) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL http_help("http://help/");
  GURL should_not_redirect = http_help;
  test_content_browser_client.HandleWebUI(&should_not_redirect, &profile_);
  EXPECT_EQ(http_help, should_not_redirect);

  const GURL chrome_help(chrome::kChromeUIHelpURL);
  GURL should_redirect = chrome_help;
  test_content_browser_client.HandleWebUI(&should_redirect, &profile_);
  EXPECT_NE(chrome_help, should_redirect);
}

TEST_F(ChromeContentBrowserClientTest, HandleWebUIReverse) {
  TestChromeContentBrowserClient test_content_browser_client;
  GURL http_settings("http://settings/");
  EXPECT_FALSE(test_content_browser_client.HandleWebUIReverse(&http_settings,
                                                              &profile_));
  GURL chrome_settings(chrome::kChromeUISettingsURL);
  EXPECT_TRUE(test_content_browser_client.HandleWebUIReverse(&chrome_settings,
                                                             &profile_));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeContentBrowserClientTest, RedirectPrivacySandboxURL) {
  base::test::ScopedFeatureList feature_list(
      privacy_sandbox::kPrivacySandboxSettings4);

  TestChromeContentBrowserClient test_content_browser_client;
  base::HistogramTester histogram_tester;
  const std::string histogram_name =
      "Settings.PrivacySandbox.DeprecatedRedirect";

  GURL settings_url = GURL(chrome::kChromeUISettingsURL);
  settings_url = net::AppendQueryParameter(settings_url, "foo", "bar");

  GURL::Replacements replacements;
  replacements.SetPathStr(chrome::kPrivacySandboxSubPagePath);
  GURL old_settings_url = settings_url.ReplaceComponents(replacements);

  replacements.SetPathStr(chrome::kAdPrivacySubPagePath);
  GURL new_settings_url = settings_url.ReplaceComponents(replacements);

  test_content_browser_client.HandleWebUI(&old_settings_url, &profile_);
  EXPECT_EQ(new_settings_url, old_settings_url);
  histogram_tester.ExpectUniqueSample(histogram_name, true, 1);

  test_content_browser_client.HandleWebUI(&new_settings_url, &profile_);
  histogram_tester.ExpectBucketCount(histogram_name, false, 1);
  histogram_tester.ExpectTotalCount(histogram_name, 2);
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
class ChromeContentSettingsRedirectTest
    : public ChromeContentBrowserClientTest {
 public:
  ChromeContentSettingsRedirectTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

 protected:
  ScopedTestingLocalState testing_local_state_;
};

TEST_F(ChromeContentSettingsRedirectTest, RedirectSettingsURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL settings_url(chrome::kChromeUISettingsURL);
  GURL dest_url = settings_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(settings_url, dest_url);

  base::Value::List list;
  list.Append(static_cast<int>(policy::SystemFeature::kBrowserSettings));
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList, std::move(list));

  dest_url = settings_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeContentSettingsRedirectTest, RedirectOSSettingsURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL os_settings_url(chrome::kChromeUIOSSettingsURL);
  GURL dest_url = os_settings_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(os_settings_url, dest_url);

  base::Value::List list;
  list.Append(static_cast<int>(policy::SystemFeature::kOsSettings));
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList, std::move(list));

  dest_url = os_settings_url;
  EXPECT_TRUE(test_content_browser_client.HandleWebUI(&dest_url, &profile_));
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);

  GURL os_settings_pwa_url =
      GURL(chrome::kChromeUIOSSettingsURL).Resolve("pwa.html");
  dest_url = os_settings_pwa_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(os_settings_pwa_url, dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectScanningAppURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL scanning_app_url(ash::kChromeUIScanningAppUrl);
  GURL dest_url = scanning_app_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(scanning_app_url, dest_url);

  base::Value::List list;
  list.Append(static_cast<int>(policy::SystemFeature::kScanning));
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList, std::move(list));

  dest_url = scanning_app_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectCameraAppURL) {
  // This test needs `SystemWebAppType::CAMERA` (`CameraSystemAppDelegate`)
  // registered in `SystemWebAppManager`.
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL camera_app_url(ash::kChromeUICameraAppMainURL);
  GURL dest_url = camera_app_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(camera_app_url, dest_url);

  base::Value::List list;
  list.Append(static_cast<int>(policy::SystemFeature::kCamera));
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList, std::move(list));

  dest_url = camera_app_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectHelpURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL help_url(chrome::kChromeUIHelpURL);
  GURL dest_url = help_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL("chrome://settings/help"), dest_url);

  base::Value::List list;
  list.Append(static_cast<int>(policy::SystemFeature::kBrowserSettings));
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList, std::move(list));

  dest_url = help_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

namespace {
constexpr char kEmail[] = "test@test.com";
std::unique_ptr<KeyedService> CreateTestPolicyCertService(
    content::BrowserContext* context) {
  return policy::PolicyCertService::CreateForTesting(
      Profile::FromBrowserContext(context));
}
}  // namespace

// Test to verify that the PolicyCertService is correctly updated when a policy
// provided trust anchor is used.
class ChromeContentSettingsPolicyTrustAnchor
    : public ChromeContentBrowserClientTest {
 public:
  ChromeContentSettingsPolicyTrustAnchor()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    // Add a profile
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmail, "gaia_id");
    user_manager::User* user =
        fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
            account_id, false /*is_affiliated*/,
            user_manager::USER_TYPE_REGULAR, &profile_);
    fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    false /* browser_restart */,
                                    false /* is_child */);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
    // Create a PolicyCertServiceFactory
    ASSERT_TRUE(
        policy::PolicyCertServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_, base::BindRepeating(&CreateTestPolicyCertService)));
  }

  void TearDown() override { scoped_user_manager_.reset(); }

 protected:
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(ChromeContentSettingsPolicyTrustAnchor, PolicyTrustAnchor) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(policy::PolicyCertServiceFactory::GetForProfile(&profile_)
                   ->UsedPolicyCertificates());
  client.OnTrustAnchorUsed(&profile_);
  EXPECT_TRUE(policy::PolicyCertServiceFactory::GetForProfile(&profile_)
                  ->UsedPolicyCertificates());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_CHROMEOS)

class CaptivePortalCheckProcessHost : public content::MockRenderProcessHost {
 public:
  explicit CaptivePortalCheckProcessHost(
      content::BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {}

  CaptivePortalCheckProcessHost(const CaptivePortalCheckProcessHost&) = delete;
  CaptivePortalCheckProcessHost& operator=(
      const CaptivePortalCheckProcessHost&) = delete;

  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      network::mojom::URLLoaderFactoryParamsPtr params) override {
    *invoked_url_factory_ = true;
    DCHECK_EQ(expected_disable_secure_dns_, params->disable_secure_dns);
  }

  void SetupForTracking(bool* invoked_url_factory,
                        bool expected_disable_secure_dns) {
    invoked_url_factory_ = invoked_url_factory;
    expected_disable_secure_dns_ = expected_disable_secure_dns;
  }

 private:
  raw_ptr<bool> invoked_url_factory_ = nullptr;
  bool expected_disable_secure_dns_ = false;
};

class CaptivePortalCheckRenderProcessHostFactory
    : public content::RenderProcessHostFactory {
 public:
  CaptivePortalCheckRenderProcessHostFactory() = default;

  CaptivePortalCheckRenderProcessHostFactory(
      const CaptivePortalCheckRenderProcessHostFactory&) = delete;
  CaptivePortalCheckRenderProcessHostFactory& operator=(
      const CaptivePortalCheckRenderProcessHostFactory&) = delete;

  content::RenderProcessHost* CreateRenderProcessHost(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) override {
    auto rph = std::make_unique<CaptivePortalCheckProcessHost>(browser_context);
    content::RenderProcessHost* result = rph.get();
    processes_.push_back(std::move(rph));
    return result;
  }

  void SetupForTracking(bool* invoked_url_factory,
                        bool expected_disable_secure_dns) {
    processes_.back()->SetupForTracking(invoked_url_factory,
                                        expected_disable_secure_dns);
  }

  void ClearRenderProcessHosts() { processes_.clear(); }

 private:
  std::list<std::unique_ptr<CaptivePortalCheckProcessHost>> processes_;
};

class ChromeContentBrowserClientCaptivePortalBrowserTest
    : public ChromeRenderViewHostTestHarness {
 public:
 protected:
  void SetUp() override {
    SetRenderProcessHostFactory(&cp_rph_factory_);
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    DeleteContents();
    cp_rph_factory_.ClearRenderProcessHosts();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  CaptivePortalCheckRenderProcessHostFactory cp_rph_factory_;
};

TEST_F(ChromeContentBrowserClientCaptivePortalBrowserTest,
       NotCaptivePortalWindow) {
  bool invoked_url_factory = false;
  cp_rph_factory_.SetupForTracking(&invoked_url_factory,
                                   false /* expected_disable_secure_dns */);
  NavigateAndCommit(GURL("https://www.google.com"), ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(invoked_url_factory);
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
TEST_F(ChromeContentBrowserClientCaptivePortalBrowserTest,
       CaptivePortalWindow) {
  bool invoked_url_factory = false;
  cp_rph_factory_.SetupForTracking(&invoked_url_factory,
                                   true /* expected_disable_secure_dns */);
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      web_contents(), CaptivePortalServiceFactory::GetForProfile(profile()),
      base::NullCallback());
  captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents())
      ->set_is_captive_portal_window();
  NavigateAndCommit(GURL("https://www.google.com"), ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(invoked_url_factory);
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ChromeContentBrowserClientStoragePartitionTest
    : public ChromeContentBrowserClientTest {
 public:
  void SetUp() override {
    content::SiteIsolationPolicy::DisableFlagCachingForTesting();
  }

 protected:
  static constexpr char kAppId[] = "appid";
  static constexpr char kHttpsScope[] = "https://example.com";
  static constexpr char kIsolatedAppScope[] =
      "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";

  content::StoragePartitionConfig CreateDefaultStoragePartitionConfig() {
    return content::StoragePartitionConfig::CreateDefault(&profile_);
  }

  void RegisterAppIsolationState(const std::string& app_id,
                                 const std::string& scope,
                                 bool isolated) {
    web_app::WebApp app(app_id);
    app.SetScope(GURL(scope));
    app.SetStorageIsolated(isolated);
    web_app::RecordOrRemoveAppIsolationState(profile_.GetPrefs(), app);
  }
};
// static
constexpr char ChromeContentBrowserClientStoragePartitionTest::kAppId[];
constexpr char ChromeContentBrowserClientStoragePartitionTest::kHttpsScope[];
constexpr char
    ChromeContentBrowserClientStoragePartitionTest::kIsolatedAppScope[];

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DefaultPartitionIsUsedForNormalSites) {
  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL("https://google.com"));

  EXPECT_EQ(CreateDefaultStoragePartitionConfig(), config);
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DefaultPartitionIsUsedOnHttpsWhenIsolationDisabled) {
  RegisterAppIsolationState(kAppId, kHttpsScope, /*isolated=*/true);

  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL(kHttpsScope));

  EXPECT_EQ(CreateDefaultStoragePartitionConfig(), config);
  EXPECT_FALSE(
      test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
          &profile_, GURL(kHttpsScope),
          /*origin_matches_flag=*/false));
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DefaultPartitionIsUsedForNonIsolatedPWAs) {
  RegisterAppIsolationState(kAppId, kHttpsScope, /*isolated=*/false);

  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL(kHttpsScope));

  EXPECT_EQ(CreateDefaultStoragePartitionConfig(), config);
  EXPECT_FALSE(
      test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
          &profile_, GURL(kHttpsScope),
          /*origin_matches_flag=*/false));
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       EnableIsolatedLevelForIsolatedAppSchemeWhenIsolatedAppFeatureIsEnabled) {
  TestChromeContentBrowserClient test_content_browser_client;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatedWebApps);

  EXPECT_THAT(
      test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
          &profile_, GURL(kIsolatedAppScope), /*origin_matches_flag=*/false),
      IsTrue());
}

TEST_F(
    ChromeContentBrowserClientStoragePartitionTest,
    DoNotEnableIsolatedLevelForIsolatedAppSchemeWhenIsolatedAppFeatureIsDisabled) {
  TestChromeContentBrowserClient test_content_browser_client;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebApps);

  EXPECT_THAT(test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
                  &profile_, GURL(kIsolatedAppScope),
                  /*origin_matches_flag=*/false),
              IsFalse());
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DoNotEnableIsolatedLevelForNonIsolatedApp) {
  TestChromeContentBrowserClient test_content_browser_client;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatedWebApps);

  EXPECT_THAT(test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
                  &profile_, GURL(kHttpsScope),
                  /*origin_matches_flag=*/false),
              IsFalse());
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DedicatedPartitionIsUsedForIsolatedHttpsApps) {
  RegisterAppIsolationState(kAppId, kHttpsScope, /*isolated=*/true);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatedWebApps);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kIsolatedAppOrigins, kHttpsScope);

  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL(kHttpsScope));

  auto expected_config = content::StoragePartitionConfig::Create(
      &profile_, /*partition_domain=*/kAppId, /*partition_name=*/"",
      /*in_memory=*/false);
  EXPECT_EQ(expected_config, config);
  EXPECT_TRUE(test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
      &profile_, GURL(kHttpsScope),
      /*origin_matches_flag=*/true));
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DefaultPartitionIsUsedWhenIsolationDisabled) {
  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL(kIsolatedAppScope));

  EXPECT_EQ(CreateDefaultStoragePartitionConfig(), config);
  EXPECT_FALSE(
      test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
          &profile_, GURL(kIsolatedAppScope),
          /*origin_matches_flag=*/false));
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DedicatedPartitionIsUsedForIsolatedApps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatedWebApps);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kIsolatedAppOrigins, kIsolatedAppScope);

  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL(kIsolatedAppScope));

  auto expected_config = content::StoragePartitionConfig::Create(
      &profile_, /*partition_domain=*/
      "iwa-aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac",
      /*partition_name=*/"",
      /*in_memory=*/false);
  EXPECT_EQ(expected_config, config);
  EXPECT_TRUE(test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
      &profile_, GURL(kIsolatedAppScope),
      /*origin_matches_flag=*/false));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class ChromeContentBrowserClientSwitchTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeContentBrowserClientSwitchTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

 protected:
  void AppendSwitchInCurrentProcess(const base::StringPiece& switch_string) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switch_string);
  }

  base::CommandLine FetchCommandLineSwitchesForRendererProcess() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kProcessType,
                                   switches::kRendererProcess);

    client_.AppendExtraCommandLineSwitches(&command_line, process()->GetID());
    return command_line;
  }

 private:
  ScopedTestingLocalState testing_local_state_;
  ChromeContentBrowserClient client_;
};

TEST_F(ChromeContentBrowserClientSwitchTest, WebSQLAccessDefault) {
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_FALSE(result.HasSwitch(blink::switches::kWebSQLAccess));
}

TEST_F(ChromeContentBrowserClientSwitchTest, WebSQLAccessDisabled) {
  profile()->GetPrefs()->SetBoolean(storage::kWebSQLAccess, false);
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_FALSE(result.HasSwitch(blink::switches::kWebSQLAccess));
}

TEST_F(ChromeContentBrowserClientSwitchTest, WebSQLAccessEnabled) {
  profile()->GetPrefs()->SetBoolean(storage::kWebSQLAccess, true);
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_TRUE(result.HasSwitch(blink::switches::kWebSQLAccess));
}

TEST_F(ChromeContentBrowserClientSwitchTest,
       WebSQLNonSecureContextEnabledDefault) {
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_FALSE(
      result.HasSwitch(blink::switches::kWebSQLNonSecureContextEnabled));
}

TEST_F(ChromeContentBrowserClientSwitchTest,
       WebSQLNonSecureContextEnabledDisabled) {
  profile()->GetPrefs()->SetBoolean(storage::kWebSQLNonSecureContextEnabled,
                                    false);
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_FALSE(
      result.HasSwitch(blink::switches::kWebSQLNonSecureContextEnabled));
}

TEST_F(ChromeContentBrowserClientSwitchTest,
       WebSQLNonSecureContextEnabledEnabled) {
  profile()->GetPrefs()->SetBoolean(storage::kWebSQLNonSecureContextEnabled,
                                    true);
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_TRUE(
      result.HasSwitch(blink::switches::kWebSQLNonSecureContextEnabled));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ChromeContentBrowserClientSwitchTest,
       ShouldSetForceAppModeSwitchInRendererProcessIfItIsSetInCurrentProcess) {
  AppendSwitchInCurrentProcess(switches::kForceAppMode);
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_TRUE(result.HasSwitch(switches::kForceAppMode));
}

TEST_F(
    ChromeContentBrowserClientSwitchTest,
    ShouldNotSetForceAppModeSwitchInRendererProcessIfItIsUnsetInCurrentProcess) {
  // We don't set the `kForceAppMode` flag in the current process.
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_FALSE(result.HasSwitch(switches::kForceAppMode));
}
#endif  // BUILDFLAG(IS_CHROMEOS)
