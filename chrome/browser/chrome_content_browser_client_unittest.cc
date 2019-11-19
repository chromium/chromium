// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <list>
#include <map>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
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
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#endif

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

  // Not sure what can be done to better check the OS string, since it's highly
  // platform-dependent.
  EXPECT_FALSE(os_str.empty());

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
                   base::Bind(&DidOpenURLForWindowTest, &web_contents));

    EXPECT_TRUE(web_contents);

    content::WebContents* active_contents = browser()->tab_strip_model()->
        GetActiveWebContents();
    EXPECT_EQ(web_contents, active_contents);
    EXPECT_EQ(url, active_contents->GetVisibleURL());
  }

  EXPECT_EQ(previous_count + 2, browser()->tab_strip_model()->count());
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
    command_line_.AppendSwitchASCII(switches::kBlinkSettings, value);
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
  EXPECT_FALSE(command_line().HasSwitch(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, FieldTrialWithoutParams) {
  CreateFieldTrial(kDisallowFetchFieldTrialName, kFakeGroupName);
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(command_line().HasSwitch(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, BlinkSettingsSwitchAlreadySpecified) {
  AppendBlinkSettingsSwitch("foo");
  CreateFieldTrialWithParams(kDisallowFetchFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("foo",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, FieldTrialEnabled) {
  CreateFieldTrialWithParams(kDisallowFetchFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("key1=value1,key2=value2",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
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

TEST(ChromeContentBrowserClientTest, GetMetricSuffixForURL) {
  ChromeContentBrowserClient client;
  // Search is detected.
  EXPECT_EQ("search", client.GetMetricSuffixForURL(GURL(
                          "https://www.google.co.jp/search?q=whatsgoingon")));
  // Not a Search host.
  EXPECT_EQ("", client.GetMetricSuffixForURL(GURL(
                    "https://www.google.example.com/search?q=whatsgoingon")));
  // For now, non-https is considered a Search host.
  EXPECT_EQ("search", client.GetMetricSuffixForURL(
                          GURL("http://www.google.com/search?q=whatsgoingon")));
  // Not a Search result page (no query).
  EXPECT_EQ("", client.GetMetricSuffixForURL(
                    GURL("https://www.google.com/search?notaquery=nope")));
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
    EXPECT_EQ(buffer, content::frozen_user_agent_strings::kAndroid);
  }

  // Verify the mobile user agent string is returned when using a mobile user
  // agent.
  command_line->AppendSwitch(switches::kUseMobileUserAgent);
  ASSERT_TRUE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  {
    ChromeContentBrowserClient content_browser_client;
    std::string buffer = content_browser_client.GetUserAgent();
    EXPECT_EQ(buffer, content::frozen_user_agent_strings::kAndroidMobile);
  }
#else
  {
    ChromeContentBrowserClient content_browser_client;
    std::string buffer = content_browser_client.GetUserAgent();
    EXPECT_EQ(buffer, content::frozen_user_agent_strings::kDesktop);
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

  EXPECT_EQ(metadata.brand, version_info::GetProductName());
  EXPECT_EQ(metadata.full_version, version_info::GetVersionNumber());
  EXPECT_EQ(metadata.major_version, version_info::GetMajorVersionNumber());
  EXPECT_EQ(metadata.platform, version_info::GetOSType());
  EXPECT_EQ(metadata.architecture, "");
  EXPECT_EQ(metadata.model, "");
}
