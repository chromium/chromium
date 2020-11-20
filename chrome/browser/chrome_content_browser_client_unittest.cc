// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <list>
#include <map>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

#if defined(USE_X11) || defined(USE_OZONE)
#include <sys/utsname.h>
#endif

#if !defined(OS_ANDROID)
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

#if defined(OS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/components/scanning/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowsingDataFilterBuilder;
using testing::_;
using ChromeContentBrowserClientTest = testing::Test;

namespace {

void CheckUserAgentStringOrdering(bool mobile_device) {
  std::vector<std::string> pieces;

  // Check if the pieces of the user agent string come in the correct order.
  ChromeContentBrowserClient content_browser_client;
  std::string buffer = content_browser_client.GetUserAgent();

  pieces = base::SplitStringUsingSubstr(
      buffer, "Mozilla/5.0 (", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  EXPECT_EQ("", pieces[0]);

  pieces = base::SplitStringUsingSubstr(
      buffer, ") AppleWebKit/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string os_str = pieces[0];

  pieces =
      base::SplitStringUsingSubstr(buffer, " (KHTML, like Gecko) ",
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string webkit_version_str = pieces[0];

  pieces = base::SplitStringUsingSubstr(
      buffer, " Safari/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  std::string product_str = pieces[0];
  std::string safari_version_str = pieces[1];

  EXPECT_FALSE(os_str.empty());

  pieces = base::SplitStringUsingSubstr(os_str, "; ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
#if defined(OS_WIN)
  // Windows NT 10.0; Win64; x64
  // Windows NT 10.0; WOW64
  // Windows NT 10.0
  std::string os_and_version = pieces[0];
  for (unsigned int i = 1; i < pieces.size(); ++i) {
    bool equals = ((pieces[i] == "WOW64") || (pieces[i] == "Win64") ||
                   pieces[i] == "x64");
    ASSERT_TRUE(equals);
  }
  pieces = base::SplitStringUsingSubstr(pieces[0], " ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  ASSERT_EQ(3u, pieces.size());
  ASSERT_EQ("Windows", pieces[0]);
  ASSERT_EQ("NT", pieces[1]);
  double version;
  ASSERT_TRUE(base::StringToDouble(pieces[2], &version));
  ASSERT_LE(4.0, version);
  ASSERT_GT(11.0, version);
#elif defined(OS_MAC)
  // Macintosh; Intel Mac OS X 10_15_4
  ASSERT_EQ(2u, pieces.size());
  ASSERT_EQ("Macintosh", pieces[0]);
  pieces = base::SplitStringUsingSubstr(pieces[1], " ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  ASSERT_EQ(5u, pieces.size());
  ASSERT_EQ("Intel", pieces[0]);
  ASSERT_EQ("Mac", pieces[1]);
  ASSERT_EQ("OS", pieces[2]);
  ASSERT_EQ("X", pieces[3]);
  pieces = base::SplitStringUsingSubstr(pieces[4], "_", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  {
    int major, minor, patch;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &patch);
    ASSERT_EQ(base::StringPrintf("%d", major), pieces[0]);
  }
  int value;
  ASSERT_TRUE(base::StringToInt(pieces[1], &value));
  ASSERT_LE(0, value);
  ASSERT_TRUE(base::StringToInt(pieces[2], &value));
  ASSERT_LE(0, value);
#elif defined(USE_X11) || defined(USE_OZONE)
  // X11; Linux x86_64
  // X11; CrOS armv7l 4537.56.0
  struct utsname unixinfo;
  uname(&unixinfo);
  std::string machine = unixinfo.machine;
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32_t)) {
    machine = "i686 (x86_64)";
  }
  ASSERT_EQ(2u, pieces.size());
  ASSERT_EQ("X11", pieces[0]);
  pieces = base::SplitStringUsingSubstr(pieces[1], " ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
#if defined(OS_CHROMEOS)
  // X11; CrOS armv7l 4537.56.0
  //      ^^
  ASSERT_EQ(3u, pieces.size());
  ASSERT_EQ("CrOS", pieces[0]);
  ASSERT_EQ(machine, pieces[1]);
  pieces = base::SplitStringUsingSubstr(pieces[2], ".", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  for (unsigned int i = 1; i < pieces.size(); ++i) {
    int value;
    ASSERT_TRUE(base::StringToInt(pieces[i], &value));
  }
#else
  // X11; Linux x86_64
  //      ^^
  ASSERT_EQ(2u, pieces.size());
  // This may not be Linux in all cases in the wild, but it is on the bots.
  ASSERT_EQ("Linux", pieces[0]);
  ASSERT_EQ(machine, pieces[1]);
#endif
#elif defined(OS_ANDROID)
  // Linux; Android 7.1.1; Samsung Chromebook 3
  ASSERT_GE(3u, pieces.size());
  ASSERT_EQ("Linux", pieces[0]);
  std::string model;
  if (pieces.size() > 2)
    model = pieces[2];

  pieces = base::SplitStringUsingSubstr(pieces[1], " ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  ASSERT_EQ("Android", pieces[0]);
  pieces = base::SplitStringUsingSubstr(pieces[1], ".", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  for (unsigned int i = 1; i < pieces.size(); ++i) {
    int value;
    ASSERT_TRUE(base::StringToInt(pieces[i], &value));
  }

  if (!model.empty()) {
    if (base::SysInfo::GetAndroidBuildCodename() == "REL")
      ASSERT_EQ(base::SysInfo::HardwareModelName(), model);
    else
      ASSERT_EQ("", model);
  }
#elif defined(OS_FUCHSIA)
  // X11; Fuchsia
  ASSERT_EQ(2u, pieces.size());
  ASSERT_EQ("X11", pieces[0]);
  ASSERT_EQ("Fuchsia", pieces[1]);
#endif

  // Check that the version numbers match.
  EXPECT_FALSE(webkit_version_str.empty());
  EXPECT_FALSE(safari_version_str.empty());
  EXPECT_EQ(webkit_version_str, safari_version_str);

  EXPECT_TRUE(
      base::StartsWith(product_str, "Chrome/", base::CompareCase::SENSITIVE));
  if (mobile_device) {
    // "Mobile" gets tacked on to the end for mobile devices, like phones.
    EXPECT_TRUE(
        base::EndsWith(product_str, " Mobile", base::CompareCase::SENSITIVE));
  }
}

}  // namespace

TEST_F(ChromeContentBrowserClientTest, ShouldAssignSiteForURL) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(client.ShouldAssignSiteForURL(GURL("chrome-native://test")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("http://www.google.com")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("https://www.google.com")));
}

// BrowserWithTestWindowTest doesn't work on Android.
#if !defined(OS_ANDROID)

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

  GURL urls[] = { GURL("https://www.google.com"),
                  GURL("https://www.chromium.org") };

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

    content::WebContents* active_contents = browser()->tab_strip_model()->
        GetActiveWebContents();
    EXPECT_EQ(web_contents, active_contents);
    EXPECT_EQ(url, active_contents->GetVisibleURL());
  }

  EXPECT_EQ(previous_count + 2, browser()->tab_strip_model()->count());
}

// TODO(crbug.com/566091): Remove the need for ShouldStayInParentProcessForNTP()
//    and associated test.
TEST_F(ChromeContentBrowserClientWindowTest, ShouldStayInParentProcessForNTP) {
  ChromeContentBrowserClient client;
  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::CreateForURL(
          browser()->profile(),
          GURL("chrome-search://local-ntp/local-ntp.html"));
  EXPECT_TRUE(client.ShouldStayInParentProcessForNTP(
      GURL("chrome-search://local-ntp/local-ntp.html"), site_instance.get()));

  site_instance = content::SiteInstance::CreateForURL(
      browser()->profile(), GURL("chrome://new-tab-page"));
  // chrome://new-tab-page is an NTP replacing local-ntp and supports OOPIFs.
  // ShouldStayInParentProcessForNTP() should only return true for NTPs hosted
  // under the chrome-search: scheme.
  EXPECT_FALSE(client.ShouldStayInParentProcessForNTP(
      GURL("chrome://new-tab-page"), site_instance.get()));
}

TEST_F(ChromeContentBrowserClientWindowTest, OverrideNavigationParams) {
  ChromeContentBrowserClient client;
  ui::PageTransition transition;
  bool is_renderer_initiated;
  content::Referrer referrer = content::Referrer();
  base::Optional<url::Origin> initiator_origin = base::nullopt;

  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::CreateForURL(
          browser()->profile(),
          GURL("chrome-search://local-ntp/local-ntp.html"));
  transition = ui::PAGE_TRANSITION_LINK;
  is_renderer_initiated = true;
  // The origin is a placeholder to test that |initiator_origin| is set to
  // base::nullopt and is not meant to represent what would happen in practice.
  initiator_origin = url::Origin::Create(GURL("https://www.example.com"));
  client.OverrideNavigationParams(nullptr, site_instance.get(), &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           transition));
  EXPECT_FALSE(is_renderer_initiated);
  EXPECT_EQ(base::nullopt, initiator_origin);

  site_instance = content::SiteInstance::CreateForURL(
      browser()->profile(), GURL("chrome://new-tab-page"));
  transition = ui::PAGE_TRANSITION_LINK;
  is_renderer_initiated = true;
  initiator_origin = url::Origin::Create(GURL("https://www.example.com"));
  client.OverrideNavigationParams(nullptr, site_instance.get(), &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           transition));
  EXPECT_FALSE(is_renderer_initiated);
  EXPECT_EQ(base::nullopt, initiator_origin);

  // No change for transitions that are not PAGE_TRANSITION_LINK.
  site_instance = content::SiteInstance::CreateForURL(
      browser()->profile(), GURL("chrome://new-tab-page"));
  transition = ui::PAGE_TRANSITION_TYPED;
  client.OverrideNavigationParams(nullptr, site_instance.get(), &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED, transition));

  // No change for transitions on a non-NTP page.
  site_instance = content::SiteInstance::CreateForURL(
      browser()->profile(), GURL("https://www.example.com"));
  transition = ui::PAGE_TRANSITION_LINK;
  client.OverrideNavigationParams(nullptr, site_instance.get(), &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK, transition));
}

#endif  // !defined(OS_ANDROID)

// NOTE: Any updates to the expectations in these tests should also be done in
// the browser test WebRtcDisableEncryptionFlagBrowserTest.
class DisableWebRtcEncryptionFlagTest : public testing::Test {
 public:
  DisableWebRtcEncryptionFlagTest()
      : from_command_line_(base::CommandLine::NO_PROGRAM),
        to_command_line_(base::CommandLine::NO_PROGRAM) {}

 protected:
  void SetUp() override {
    from_command_line_.AppendSwitch(switches::kDisableWebRtcEncryption);
  }

  void MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel channel) {
    ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
        &to_command_line_,
        from_command_line_,
        channel);
  }

  base::CommandLine from_command_line_;
  base::CommandLine to_command_line_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisableWebRtcEncryptionFlagTest);
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
#if defined(OS_ANDROID)
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
      : command_line_(base::CommandLine::NO_PROGRAM) {}

  void SetUp() override {
    command_line_.AppendSwitchASCII(
        switches::kProcessType, switches::kRendererProcess);
  }

  void TearDown() override {
    variations::testing::ClearAllVariationParams();
  }

  void CreateFieldTrial(const char* trial_name, const char* group_name) {
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  }

  void CreateFieldTrialWithParams(
      const char* trial_name,
      const char* group_name,
      const char* key1, const char* value1,
      const char* key2, const char* value2) {
    std::map<std::string, std::string> params;
    params.insert(std::make_pair(key1, value1));
    params.insert(std::make_pair(key2, value2));
    CreateFieldTrial(trial_name, kFakeGroupName);
    variations::AssociateVariationParams(trial_name, kFakeGroupName, params);
  }

  void AppendContentBrowserClientSwitches() {
    client_.AppendExtraCommandLineSwitches(&command_line_, kFakeChildProcessId);
  }

  const base::CommandLine& command_line() const {
    return command_line_;
  }

  void AppendBlinkSettingsSwitch(const char* value) {
    command_line_.AppendSwitchASCII(blink::switches::kBlinkSettings, value);
  }

 private:
  static const int kFakeChildProcessId = 1;

  ChromeContentBrowserClient client_;
  base::CommandLine command_line_;

  content::BrowserTaskEnvironment task_environment_;
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

#if !defined(OS_ANDROID)
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
    data.SetShortName(base::ASCIIToUTF16("foo.com"));
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
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));

  AddTab(browser(), GURL(url::kAboutBlankURL));
  NavigateAndCommitActiveTab(url_original);

  NavigationEntry* entry = browser()->tab_strip_model()->
      GetActiveWebContents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_rewritten, entry->GetURL());
  EXPECT_EQ(url_original, entry->GetVirtualURL());
}

}  // namespace content
#endif  // !defined(OS_ANDROID)

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

TEST(ChromeContentBrowserClientTest, HandleWebUI) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL http_help("http://help/");
  GURL should_not_redirect = http_help;
  test_content_browser_client.HandleWebUI(&should_not_redirect, nullptr);
  EXPECT_EQ(http_help, should_not_redirect);

  const GURL chrome_help(chrome::kChromeUIHelpURL);
  GURL should_redirect = chrome_help;
  test_content_browser_client.HandleWebUI(&should_redirect, nullptr);
  EXPECT_NE(chrome_help, should_redirect);

  // Confirm that the deprecated cookies settings URL is rewritten.
  GURL cookies_url = GURL(chrome::kChromeUICookieSettingsDeprecatedURL);
  test_content_browser_client.HandleWebUI(&cookies_url, nullptr);
  EXPECT_EQ(GURL(chrome::kChromeUICookieSettingsURL), cookies_url);
}

TEST(ChromeContentBrowserClientTest, HandleWebUIReverse) {
  TestChromeContentBrowserClient test_content_browser_client;
  GURL http_settings("http://settings/");
  EXPECT_FALSE(
      test_content_browser_client.HandleWebUIReverse(&http_settings, nullptr));
  GURL chrome_settings(chrome::kChromeUISettingsURL);
  EXPECT_TRUE(test_content_browser_client.HandleWebUIReverse(&chrome_settings,
                                                             nullptr));
}

TEST(ChromeContentBrowserClientTest, UserAgentStringFrozen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kFreezeUserAgent);

#if defined(OS_ANDROID)
  // Verify the correct user agent is returned when the UseMobileUserAgent
  // command line flag is present.
  const char* const kArguments[] = {"chrome"};
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->InitFromArgv(1, kArguments);

  // Verify the mobile user agent string is not returned when not using a mobile
  // user agent.
  ASSERT_FALSE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  {
    ChromeContentBrowserClient content_browser_client;
    std::string buffer = content_browser_client.GetUserAgent();
    EXPECT_EQ(buffer, base::StringPrintf(
                          content::frozen_user_agent_strings::kAndroid,
                          version_info::GetMajorVersionNumber().c_str()));
  }

  // Verify the mobile user agent string is returned when using a mobile user
  // agent.
  command_line->AppendSwitch(switches::kUseMobileUserAgent);
  ASSERT_TRUE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  {
    ChromeContentBrowserClient content_browser_client;
    std::string buffer = content_browser_client.GetUserAgent();
    EXPECT_EQ(buffer, base::StringPrintf(
                          content::frozen_user_agent_strings::kAndroidMobile,
                          version_info::GetMajorVersionNumber().c_str()));
  }
#else
  {
    ChromeContentBrowserClient content_browser_client;
    std::string buffer = content_browser_client.GetUserAgent();
    EXPECT_EQ(buffer, base::StringPrintf(
                          content::frozen_user_agent_strings::kDesktop,
                          version_info::GetMajorVersionNumber().c_str()));
  }
#endif
}

TEST(ChromeContentBrowserClientTest, UserAgentStringOrdering) {
#if defined(OS_ANDROID)
  const char* const kArguments[] = {"chrome"};
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->InitFromArgv(1, kArguments);

  // Do it for regular devices.
  ASSERT_FALSE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  CheckUserAgentStringOrdering(false);

  // Do it for mobile devices.
  command_line->AppendSwitch(switches::kUseMobileUserAgent);
  ASSERT_TRUE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  CheckUserAgentStringOrdering(true);
#else
  CheckUserAgentStringOrdering(false);
#endif
}

TEST(ChromeContentBrowserClientTest, UserAgentMetadata) {
  ChromeContentBrowserClient content_browser_client;
  auto metadata = content_browser_client.GetUserAgentMetadata();

  std::string major_version = version_info::GetMajorVersionNumber();

  // According to spec, Sec-CH-UA should contain what project the browser is
  // based on (i.e. Chromium in this case) as well as the actual product.
  // In CHROMIUM_BRANDING builds this will check chromium twice. That should be
  // ok though.

  const blink::UserAgentBrandVersion chromium_brand_version = {"Chromium",
                                                               major_version};
  const blink::UserAgentBrandVersion product_brand_version = {
      version_info::GetProductName(), version_info::GetMajorVersionNumber()};
  bool contains_chromium_brand_version = false;
  bool contains_product_brand_version = false;

  for (const auto& brand_version : metadata.brand_version_list) {
    if (brand_version == chromium_brand_version) {
      contains_chromium_brand_version = true;
    }
    if (brand_version == product_brand_version) {
      contains_product_brand_version = true;
    }
  }

  EXPECT_TRUE(contains_chromium_brand_version);
  EXPECT_TRUE(contains_product_brand_version);

  EXPECT_EQ(metadata.full_version, version_info::GetVersionNumber());
  EXPECT_EQ(metadata.platform_version,
            content::GetOSVersion(content::IncludeAndroidBuildNumber::Exclude,
                                  content::IncludeAndroidModel::Exclude));
  // This makes sure no extra information is added to the platform version.
  EXPECT_EQ(metadata.platform_version.find(";"), std::string::npos);
  EXPECT_EQ(metadata.platform, version_info::GetOSType());
  EXPECT_EQ(metadata.architecture, content::GetLowEntropyCpuArchitecture());
  EXPECT_EQ(metadata.model, content::BuildModelInfo());
}

TEST(ChromeContentBrowserClientTest, GenerateBrandVersionList) {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list =
      GenerateBrandVersionList(84, base::nullopt, "84", base::nullopt);
  std::string brand_list = metadata.SerializeBrandVersionList();
  EXPECT_EQ(R"(" Not A;Brand";v="99", "Chromium";v="84")", brand_list);

  metadata.brand_version_list =
      GenerateBrandVersionList(85, base::nullopt, "85", base::nullopt);
  std::string brand_list_diff = metadata.SerializeBrandVersionList();
  // Make sure the lists are different for different seeds
  EXPECT_EQ(R"("Chromium";v="85", " Not;A Brand";v="99")", brand_list_diff);
  EXPECT_NE(brand_list, brand_list_diff);

  metadata.brand_version_list =
      GenerateBrandVersionList(84, "Totally A Brand", "84", base::nullopt);
  std::string brand_list_w_brand = metadata.SerializeBrandVersionList();
  EXPECT_EQ(
      R"(" Not A;Brand";v="99", "Chromium";v="84", "Totally A Brand";v="84")",
      brand_list_w_brand);

  metadata.brand_version_list =
      GenerateBrandVersionList(84, base::nullopt, "84", "Clean GREASE");
  std::string brand_list_grease_override = metadata.SerializeBrandVersionList();
  EXPECT_EQ(R"("Clean GREASE";v="99", "Chromium";v="84")",
            brand_list_grease_override);
  EXPECT_NE(brand_list, brand_list_grease_override);

  // Should DCHECK on negative numbers
  EXPECT_DCHECK_DEATH(
      GenerateBrandVersionList(-1, base::nullopt, "99", base::nullopt));
}

TEST(ChromeContentBrowserClientTest, LowEntropyCpuArchitecture) {
  std::string arch = content::GetLowEntropyCpuArchitecture();

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_POSIX) && !defined(OS_ANDROID))
  EXPECT_TRUE("arm" == arch || "x86" == arch);
#else
  EXPECT_EQ("", arch);
#endif
}

#if defined(OS_CHROMEOS)
class ChromeContentSettingsRedirectTest
    : public ChromeContentBrowserClientTest {
 public:
  ChromeContentSettingsRedirectTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kScanningUI},
                                          {});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_;
  TestingProfile profile_;
};

TEST_F(ChromeContentSettingsRedirectTest, RedirectOSSettingsURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL os_settings_url(chrome::kChromeUIOSSettingsURL);
  GURL dest_url = os_settings_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(os_settings_url, dest_url);

  base::Value list(base::Value::Type::LIST);
  list.Append(policy::SystemFeature::OS_SETTINGS);
  testing_local_state_.Get()->Set(
      policy::policy_prefs::kSystemFeaturesDisableList, std::move(list));

  dest_url = os_settings_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);

  GURL os_settings_pwa_url =
      GURL(chrome::kChromeUIOSSettingsURL).Resolve("pwa.html");
  dest_url = os_settings_pwa_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(os_settings_pwa_url, dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectSettingsURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL settings_url(chrome::kChromeUISettingsURL);
  GURL dest_url = settings_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(settings_url, dest_url);

  base::Value list(base::Value::Type::LIST);
  list.Append(policy::SystemFeature::BROWSER_SETTINGS);
  testing_local_state_.Get()->Set(
      policy::policy_prefs::kSystemFeaturesDisableList, std::move(list));

  dest_url = settings_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectScanningAppURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL scanning_app_url(chromeos::kChromeUIScanningAppUrl);
  GURL dest_url = scanning_app_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(scanning_app_url, dest_url);

  base::Value list(base::Value::Type::LIST);
  list.Append(policy::SystemFeature::SCANNING);
  testing_local_state_.Get()->Set(
      policy::policy_prefs::kSystemFeaturesDisableList, std::move(list));

  dest_url = scanning_app_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}
namespace {
constexpr char kEmail[] = "test@test.com";
std::unique_ptr<KeyedService> CreateTestPolicyCertService(
    content::BrowserContext* context) {
  return policy::PolicyCertService::CreateForTesting(
      kEmail, user_manager::UserManager::Get());
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
    auto fake_user_manager =
        std::make_unique<chromeos::FakeChromeUserManager>();
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
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  TestingProfile profile_;
};

TEST_F(ChromeContentSettingsPolicyTrustAnchor, PolicyTrustAnchor) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(
      policy::PolicyCertServiceFactory::UsedPolicyCertificates(kEmail));
  client.OnTrustAnchorUsed(&profile_);
  EXPECT_TRUE(policy::PolicyCertServiceFactory::UsedPolicyCertificates(kEmail));
}

#endif  // defined(OS_CHROMEOS)

class CaptivePortalCheckProcessHost : public content::MockRenderProcessHost {
 public:
  explicit CaptivePortalCheckProcessHost(
      content::BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {}

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
  bool* invoked_url_factory_ = nullptr;
  bool expected_disable_secure_dns_ = false;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalCheckProcessHost);
};

class CaptivePortalCheckRenderProcessHostFactory
    : public content::RenderProcessHostFactory {
 public:
  CaptivePortalCheckRenderProcessHostFactory() = default;

  content::RenderProcessHost* CreateRenderProcessHost(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) override {
    rph_ = new CaptivePortalCheckProcessHost(browser_context);
    return rph_;
  }

  void SetupForTracking(bool* invoked_url_factory,
                        bool expected_disable_secure_dns) {
    rph_->SetupForTracking(invoked_url_factory, expected_disable_secure_dns);
  }

 private:
  CaptivePortalCheckProcessHost* rph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalCheckRenderProcessHostFactory);
};

class ChromeContentBrowserClientCaptivePortalBrowserTest
    : public ChromeRenderViewHostTestHarness {
 public:
 protected:
  void SetUp() override {
    SetRenderProcessHostFactory(&cp_rph_factory_);
    ChromeRenderViewHostTestHarness::SetUp();
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
