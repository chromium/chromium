// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <list>
#include <map>
#include <memory>
#include <string_view>

#include "ash/webui/camera_app_ui/url_constants.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/search_engines/template_url_service.h"
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
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "crypto/crypto_buildflags.h"
#include "media/media_buildflags.h"
#include "net/base/url_util.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/test/test_network_context.h"
#include "services/video_effects/public/cpp/video_effects_service_host.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/switches.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
// `gn check` is failing on Android for this particular include even though
// the (conditional) dependency path exists, adding `nogncheck`:
#include "components/media_effects/media_effects_service.h"  // nogncheck
#include "components/password_manager/core/common/password_manager_features.h"
// `gn check` is failing on Android for this particular include even though
// the (conditional) dependency path exists, adding `nogncheck`:
#include "services/video_effects/test/fake_video_effects_service.h"  // nogncheck
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"
#include "ui/base/page_transition_types.h"
#else
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "ash/webui/print_management/url_constants.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "ash/webui/scanning/url_constants.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_untrusted_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/media_app/media_app_guest_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/terminal_ui.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/web_applications/web_app_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN)
#include "base/test/mock_entropy_provider.h"
#include "chrome/test/base/scoped_metrics_service_for_synthetic_trials.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
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

// Check that chrome-native: URLs do not assign a site for their
// SiteInstances. This works because `kChromeNativeScheme` is registered as an
// empty document scheme in ChromeContentClient.
TEST_F(ChromeContentBrowserClientTest, ShouldAssignSiteForURL) {
  EXPECT_FALSE(content::SiteInstance::ShouldAssignSiteForURL(
      GURL("chrome-native://test")));
  EXPECT_TRUE(content::SiteInstance::ShouldAssignSiteForURL(
      GURL("http://www.google.com")));
  EXPECT_TRUE(content::SiteInstance::ShouldAssignSiteForURL(
      GURL("https://www.google.com")));
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

// TODO(crbug.com/40447789): Remove the need for
// ShouldStayInParentProcessForNTP()
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
  std::optional<url::Origin> initiator_origin = std::nullopt;

  GURL remote_ntp_url("chrome-search://remote-ntp");
  transition = ui::PAGE_TRANSITION_LINK;
  is_renderer_initiated = true;
  // The origin is a placeholder to test that |initiator_origin| is set to
  // std::nullopt and is not meant to represent what would happen in practice.
  initiator_origin = url::Origin::Create(GURL("https://www.example.com"));
  client.OverrideNavigationParams(remote_ntp_url, &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           transition));
  EXPECT_FALSE(is_renderer_initiated);
  EXPECT_EQ(std::nullopt, initiator_origin);

  transition = ui::PAGE_TRANSITION_LINK;
  is_renderer_initiated = true;
  initiator_origin = url::Origin::Create(GURL("https://www.example.com"));
  client.OverrideNavigationParams(GURL(chrome::kChromeUINewTabPageURL),
                                  &transition, &is_renderer_initiated,
                                  &referrer, &initiator_origin);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           transition));
  EXPECT_FALSE(is_renderer_initiated);
  EXPECT_EQ(std::nullopt, initiator_origin);

  // No change for transitions that are not PAGE_TRANSITION_LINK.
  transition = ui::PAGE_TRANSITION_TYPED;
  client.OverrideNavigationParams(GURL(chrome::kChromeUINewTabPageURL),
                                  &transition, &is_renderer_initiated,
                                  &referrer, &initiator_origin);
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED, transition));

  // No change for transitions on a non-NTP page.
  GURL example_url("https://www.example.com");
  transition = ui::PAGE_TRANSITION_LINK;
  client.OverrideNavigationParams(example_url, &transition,
                                  &is_renderer_initiated, &referrer,
                                  &initiator_origin);
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK, transition));
}

// Test that automatic beacon credentials (automatic beacons sent with cookie
// data) are disallowed if the 3PCD preference is enabled.
TEST_F(ChromeContentBrowserClientWindowTest, AutomaticBeaconCredentials) {
  ChromeContentBrowserClient client;

  EXPECT_TRUE(client.AreDeprecatedAutomaticBeaconCredentialsAllowed(
      browser()->profile(), GURL("a.test"),
      url::Origin::Create(GURL("c.test"))));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kTrackingProtection3pcdEnabled, true);
  EXPECT_FALSE(client.AreDeprecatedAutomaticBeaconCredentialsAllowed(
      browser()->profile(), GURL("a.test"),
      url::Origin::Create(GURL("c.test"))));
}

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ChromeContentBrowserClientWindowTest,
       BackForwardCacheIsDisallowedForCacheControlNoStorePageWhenInKioskMode) {
// Enter Kiosk session.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::ScopedUserManager user_manager(
      std::make_unique<user_manager::FakeUserManager>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::SetUpFakeKioskSession();

  ChromeContentBrowserClient client;
  ASSERT_FALSE(client.ShouldAllowBackForwardCacheForCacheControlNoStorePage(
      browser()->profile()));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeContentBrowserClientWindowTest,
       QueryInstalledWebAppsByManifestIdFrameUrlInScope) {
  ChromeContentBrowserClient client;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::AwaitStartWebAppProviderAndSubsystems(browser()->profile());

  const GURL app_url("http://foo.com");
  const GURL frame_url("http://foo.com");

  auto app_id = web_app::test::InstallDummyWebApp(browser()->profile(),
                                                  "dummyapp", app_url);
  base::test::TestFuture<std::optional<blink::mojom::RelatedApplication>>
      future;

  client.QueryInstalledWebAppsByManifestId(
      frame_url, app_url, browser()->profile(), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.has_value());

  web_app::WebAppProvider* const web_app_provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(browser()->profile());
  const web_app::WebAppRegistrar& registrar =
      web_app_provider->registrar_unsafe();

  EXPECT_EQ(result->platform, "webapp");
  EXPECT_FALSE(result->url.has_value());
  EXPECT_FALSE(result->version.has_value());
  EXPECT_EQ(result->id, registrar.GetAppManifestId(app_id));
}

TEST_F(ChromeContentBrowserClientWindowTest,
       QueryInstalledWebAppsByManifestIdFrameUrlOutOfScope) {
  ChromeContentBrowserClient client;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::AwaitStartWebAppProviderAndSubsystems(browser()->profile());

  const GURL app_url("http://foo.com");
  const GURL out_of_scope_frame_url("http://foo-out.com");

  auto app_id = web_app::test::InstallDummyWebApp(browser()->profile(),
                                                  "dummyapp", app_url);
  base::test::TestFuture<std::optional<blink::mojom::RelatedApplication>>
      future;

  client.QueryInstalledWebAppsByManifestId(/*frame_url=*/out_of_scope_frame_url,
                                           app_url, browser()->profile(),
                                           future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get().has_value());
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
    base::AssociateFieldTrialParams(trial_name, kFakeGroupName, params);
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

#if BUILDFLAG(IS_WIN)
TEST_F(ChromeContentBrowserClientGetLoggingFileTest,
       GetLoggingFileFromCommandLine) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitchASCII(switches::kLogFile, "c:\\path\\test_log.txt");
  ChromeContentBrowserClient client;
  base::FilePath log_file_name;
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("test_log.txt")).value(),
            client.GetLoggingFileName(cmd_line).BaseName().value());
  // Path must be absolute.
  EXPECT_TRUE(client.GetLoggingFileName(cmd_line).IsAbsolute());
}
TEST_F(ChromeContentBrowserClientGetLoggingFileTest,
       GetLoggingFileFromCommandLineFallback) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitchASCII(switches::kLogFile, "test_log.txt");
  ChromeContentBrowserClient client;
  base::FilePath log_file_name;
  // Windows falls back to the default if an absolute path is not provided.
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("chrome_debug.log")).value(),
            client.GetLoggingFileName(cmd_line).BaseName().value());
  // Path must be absolute.
  EXPECT_TRUE(client.GetLoggingFileName(cmd_line).IsAbsolute());
}
#else
TEST_F(ChromeContentBrowserClientGetLoggingFileTest,
       GetLoggingFileFromCommandLine) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitchASCII(switches::kLogFile, "test_log.txt");
  ChromeContentBrowserClient client;
  base::FilePath log_file_name;
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("test_log.txt")).value(),
            client.GetLoggingFileName(cmd_line).value());
}
#endif  // BUILDFLAG(IS_WIN)

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
#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  GURL chrome_certificate_manager(chrome::kChromeUICertificateManagerDialogURL);
  EXPECT_TRUE(test_content_browser_client.HandleWebUIReverse(
      &chrome_certificate_manager, &profile_));
#endif
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeContentBrowserClientTest, BindVideoEffectsManager) {
  TestChromeContentBrowserClient test_content_browser_client;
  mojo::Remote<media::mojom::VideoEffectsManager> video_effects_manager;
  test_content_browser_client.BindVideoEffectsManager(
      "test_device_id", &profile_,
      video_effects_manager.BindNewPipeAndPassReceiver());

  base::test::TestFuture<media::mojom::VideoEffectsConfigurationPtr>
      configuration_future;
  video_effects_manager->GetConfiguration(configuration_future.GetCallback());
  // The actual value isn't that important here. What matters is that getting a
  // result means that the plumbing worked.
  EXPECT_FALSE(configuration_future.Get().is_null());
}

TEST_F(ChromeContentBrowserClientTest, BindVideoEffectsProcessor) {
  mojo::Remote<video_effects::mojom::VideoEffectsService> service;
  video_effects::FakeVideoEffectsService fake_effects_service(
      service.BindNewPipeAndPassReceiver());
  auto service_reset =
      video_effects::SetVideoEffectsServiceRemoteForTesting(&service);

  std::unique_ptr<base::test::TestFuture<void>> effects_processor_future =
      fake_effects_service.GetEffectsProcessorCreationFuture();

  TestChromeContentBrowserClient test_content_browser_client;
  mojo::Remote<video_effects::mojom::VideoEffectsProcessor>
      video_effects_processor;
  test_content_browser_client.BindVideoEffectsProcessor(
      "test_device_id", &profile_,
      video_effects_processor.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(effects_processor_future->Wait());
  EXPECT_TRUE(video_effects_processor.is_connected());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeContentBrowserClientTest, PreferenceRankAudioDeviceInfos) {
  blink::WebMediaDeviceInfoArray infos{
      {/*device_id=*/"0", /*label=*/"0", /*group_id=*/"0"},
      {/*device_id=*/"1", /*label=*/"1", /*group_id=*/"1"},
  };

  // Initialize the ranking with device 1 being preferred.
  TestingProfile profile_with_prefs;
  media_prefs::UpdateAudioDevicePreferenceRanking(
      *profile_with_prefs.GetPrefs(), infos.begin() + 1, infos);

  TestChromeContentBrowserClient test_content_browser_client;
  blink::WebMediaDeviceInfoArray expected_infos{
      infos.back(),   // device_id=1
      infos.front(),  // device_id=0
  };
  test_content_browser_client.PreferenceRankAudioDeviceInfos(
      &profile_with_prefs, infos);
  EXPECT_EQ(infos, expected_infos);
}

TEST_F(ChromeContentBrowserClientTest, PreferenceRankVideoDeviceInfos) {
  blink::WebMediaDeviceInfoArray infos{
      blink::WebMediaDeviceInfo{
          media::VideoCaptureDeviceDescriptor{/*display_name=*/"0",
                                              /*device_id=*/"0"}},
      blink::WebMediaDeviceInfo{
          media::VideoCaptureDeviceDescriptor{/*display_name=*/"1",
                                              /*device_id=*/"1"}},
  };

  // Initialize the ranking with device 1 being preferred.
  TestingProfile profile_with_prefs;
  media_prefs::UpdateVideoDevicePreferenceRanking(
      *profile_with_prefs.GetPrefs(), infos.begin() + 1, infos);

  TestChromeContentBrowserClient test_content_browser_client;
  blink::WebMediaDeviceInfoArray expected_infos{
      infos.back(),   // device_id=1
      infos.front(),  // device_id=0
  };
  test_content_browser_client.PreferenceRankVideoDeviceInfos(
      &profile_with_prefs, infos);
  EXPECT_EQ(infos, expected_infos);
}

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(USE_NSS_CERTS)
TEST_F(ChromeContentBrowserClientTest, RedirectCertManagerFeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kEnableCertManagementUIV2);
  TestChromeContentBrowserClient test_content_browser_client;
  GURL settings_cert_url(chrome::kChromeUICertificateRedirectURL);
  test_content_browser_client.HandleWebUI(&settings_cert_url, &profile_);
  // No redirection, feature is off.
  EXPECT_EQ(GURL(chrome::kChromeUICertificateRedirectURL), settings_cert_url);
}
#endif  // BUILDFLAG(USE_NSS_CERTS)

TEST_F(ChromeContentBrowserClientTest, RedirectCertManagerFeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kEnableCertManagementUIV2);

  TestChromeContentBrowserClient test_content_browser_client;
  GURL settings_cert_url(chrome::kChromeUICertificateRedirectURL);
  test_content_browser_client.HandleWebUI(&settings_cert_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUICertificateManagerDialogURL),
            settings_cert_url);
}

#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

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
TEST_F(ChromeContentSettingsRedirectTest, RedirectExploreURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL help_url(ash::kChromeUIHelpAppURL);
  GURL dest_url = help_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(help_url, dest_url);

  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List().Append(
          static_cast<int>(policy::SystemFeature::kExplore)));

  dest_url = help_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectGuestExploreURL) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<ash::HelpAppUntrustedUIConfig>());

  TestChromeContentBrowserClient test_content_browser_client;
  const GURL help_url(ash::kChromeUIHelpAppUntrustedURL);
  GURL dest_url = help_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(help_url, dest_url);

  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List().Append(
          static_cast<int>(policy::SystemFeature::kExplore)));

  dest_url = help_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectGalleryURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL gallery_url(ash::kChromeUIMediaAppURL);
  GURL dest_url = gallery_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(gallery_url, dest_url);

  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List().Append(
          static_cast<int>(policy::SystemFeature::kGallery)));

  dest_url = gallery_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectGuestGalleryURL) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<MediaAppGuestUIConfig>());
  TestChromeContentBrowserClient test_content_browser_client;
  const GURL gallery_url(ash::kChromeUIMediaAppGuestURL);
  GURL dest_url = gallery_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(gallery_url, dest_url);

  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List().Append(
          static_cast<int>(policy::SystemFeature::kGallery)));

  dest_url = gallery_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectTerminalURL) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<TerminalUIConfig>());
  TestChromeContentBrowserClient test_content_browser_client;

  const GURL terminal_url(chrome::kChromeUIUntrustedTerminalURL);
  GURL dest_url = terminal_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(terminal_url, dest_url);

  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List().Append(
          static_cast<int>(policy::SystemFeature::kTerminal)));

  dest_url = terminal_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectPrintJobsURL) {
  TestChromeContentBrowserClient test_content_browser_client;

  const GURL print_jobs_url(ash::kChromeUIPrintManagementAppUrl);
  GURL dest_url = print_jobs_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(print_jobs_url, dest_url);

  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List().Append(
          static_cast<int>(policy::SystemFeature::kPrintJobs)));

  dest_url = print_jobs_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

TEST_F(ChromeContentSettingsRedirectTest, RedirectKeyShortcutsURL) {
  TestChromeContentBrowserClient test_content_browser_client;

  const GURL key_shortcuts_url(ash::kChromeUIShortcutCustomizationAppURL);
  GURL dest_url = key_shortcuts_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(key_shortcuts_url, dest_url);

  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List().Append(
          static_cast<int>(policy::SystemFeature::kKeyShortcuts)));

  dest_url = key_shortcuts_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
}

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

TEST_F(ChromeContentSettingsRedirectTest, RedirectRecorderURL) {
  TestChromeContentBrowserClient test_content_browser_client;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kConch);

  const GURL recorder_url(ash::kChromeUIRecorderAppURL);
  GURL dest_url = recorder_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(recorder_url, dest_url);

  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List().Append(
          static_cast<int>(policy::SystemFeature::kRecorder)));

  dest_url = recorder_url;
  test_content_browser_client.HandleWebUI(&dest_url, &profile_);
  EXPECT_EQ(GURL(chrome::kChromeUIAppDisabledURL), dest_url);
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
            user_manager::UserType::kRegular, &profile_);
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

class CaptivePortalCheckNetworkContext final
    : public network::TestNetworkContext {
 public:
  CaptivePortalCheckNetworkContext(content::BrowserContext* browser_context,
                                   bool expected_disable_secure_dns)
      : expected_disable_secure_dns_(expected_disable_secure_dns) {
    browser_context->GetDefaultStoragePartition()->SetNetworkContextForTesting(
        receiver_.BindNewPipeAndPassRemote());
  }

  CaptivePortalCheckNetworkContext(const CaptivePortalCheckNetworkContext&) =
      delete;
  CaptivePortalCheckNetworkContext& operator=(
      const CaptivePortalCheckNetworkContext&) = delete;

  ~CaptivePortalCheckNetworkContext() override = default;

  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      network::mojom::URLLoaderFactoryParamsPtr params) override {
    invoked_url_factory_.SetValue(true);
    CHECK_EQ(expected_disable_secure_dns_, params->disable_secure_dns);
  }

  bool WaitAndGetInvokedURLLoaderFactory() {
    return invoked_url_factory_.Get();
  }

 private:
  base::test::TestFuture<bool> invoked_url_factory_;
  bool expected_disable_secure_dns_ = false;
  mojo::Receiver<network::mojom::NetworkContext> receiver_{this};
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
    auto rph = std::make_unique<content::MockRenderProcessHost>(
        browser_context,
        content::StoragePartitionConfig::CreateDefault(browser_context),
        false /* is_for_guests_only */);
    content::RenderProcessHost* result = rph.get();
    processes_.push_back(std::move(rph));
    return result;
  }

  void ClearRenderProcessHosts() { processes_.clear(); }

 private:
  std::list<std::unique_ptr<content::MockRenderProcessHost>> processes_;
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

  std::unique_ptr<CaptivePortalCheckNetworkContext> SetupForTracking(
      bool expected_disable_secure_dns) {
    return std::make_unique<CaptivePortalCheckNetworkContext>(
        browser_context(), expected_disable_secure_dns);
  }

  CaptivePortalCheckRenderProcessHostFactory cp_rph_factory_;
};

TEST_F(ChromeContentBrowserClientCaptivePortalBrowserTest,
       NotCaptivePortalWindow) {
  auto network_context =
      SetupForTracking(false /* expected_disable_secure_dns */);
  NavigateAndCommit(GURL("https://www.google.com"), ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(network_context->WaitAndGetInvokedURLLoaderFactory());
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
TEST_F(ChromeContentBrowserClientCaptivePortalBrowserTest,
       CaptivePortalWindow) {
  auto network_context =
      SetupForTracking(true /* expected_disable_secure_dns */);
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      web_contents(), CaptivePortalServiceFactory::GetForProfile(profile()),
      base::NullCallback());
  captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents())
      ->set_window_type(captive_portal::CaptivePortalWindowType::kPopup);
  NavigateAndCommit(GURL("https://www.google.com"), ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(network_context->WaitAndGetInvokedURLLoaderFactory());
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ChromeContentBrowserClientStoragePartitionTest
    : public ChromeContentBrowserClientTest {
 public:
  void SetUp() override {
    content::SiteIsolationPolicy::DisableFlagCachingForTesting();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    web_app::SetSkipMainProfileCheckForTesting(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

 protected:
  static constexpr char kAppId[] = "appid";
  static constexpr char kHttpsScope[] = "https://example.com";
  static constexpr char kIsolatedAppScope[] =
      "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

  content::StoragePartitionConfig CreateDefaultStoragePartitionConfig() {
    return content::StoragePartitionConfig::CreateDefault(&profile_);
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
       DefaultPartitionIsUsedForNonIsolatedPWAs) {
  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL(kHttpsScope));

  EXPECT_EQ(CreateDefaultStoragePartitionConfig(), config);
  EXPECT_FALSE(
      test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
          &profile_, GURL(kHttpsScope)));
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       EnableIsolatedLevelForIsolatedAppSchemeWhenIsolatedAppFeatureIsEnabled) {
  TestChromeContentBrowserClient test_content_browser_client;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatedWebApps);

  EXPECT_THAT(test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
                  &profile_, GURL(kIsolatedAppScope)),
              IsTrue());
}

TEST_F(
    ChromeContentBrowserClientStoragePartitionTest,
    DoNotEnableIsolatedLevelForIsolatedAppSchemeWhenIsolatedAppFeatureIsDisabled) {
  TestChromeContentBrowserClient test_content_browser_client;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebApps);

  EXPECT_THAT(test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
                  &profile_, GURL(kIsolatedAppScope)),
              IsFalse());
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DoNotEnableIsolatedLevelForNonIsolatedApp) {
  TestChromeContentBrowserClient test_content_browser_client;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatedWebApps);

  EXPECT_THAT(test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
                  &profile_, GURL(kHttpsScope)),
              IsFalse());
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DefaultPartitionIsUsedWhenIsolationDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebApps);

  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL(kIsolatedAppScope));

  EXPECT_EQ(CreateDefaultStoragePartitionConfig(), config);
  EXPECT_FALSE(
      test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
          &profile_, GURL(kIsolatedAppScope)));
}

TEST_F(ChromeContentBrowserClientStoragePartitionTest,
       DedicatedPartitionIsUsedForIsolatedApps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatedWebApps);

  TestChromeContentBrowserClient test_content_browser_client;
  content::StoragePartitionConfig config =
      test_content_browser_client.GetStoragePartitionConfigForSite(
          &profile_, GURL(kIsolatedAppScope));

  auto expected_config = content::StoragePartitionConfig::Create(
      &profile_, /*partition_domain=*/
      "ih5acGGEiRXrgomjVcGuM1lp4cp+dagupnpwXmiyoV0s=",
      /*partition_name=*/"",
      /*in_memory=*/false);
  EXPECT_EQ(expected_config, config);
  EXPECT_TRUE(test_content_browser_client.ShouldUrlUseApplicationIsolationLevel(
      &profile_, GURL(kIsolatedAppScope)));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeContentBrowserClientTest, IsolatedWebAppsDisabledOnSignInScreen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatedWebApps);

  std::unique_ptr<TestingProfile> sign_in_screen_profile =
      TestingProfile::Builder()
          .SetPath(base::FilePath(ash::kSigninBrowserContextBaseName))
          .Build();

  ChromeContentBrowserClient client;
  EXPECT_TRUE(client.AreIsolatedWebAppsEnabled(&profile_));
  EXPECT_FALSE(client.AreIsolatedWebAppsEnabled(sign_in_screen_profile.get()));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ChromeContentBrowserClientTest, RequestFileAccessAllow) {
  file_access::MockScopedFileAccessDelegate scoped_file_access;
  base::test::TestFuture<file_access::ScopedFileAccess> continuation_callback;
  base::FilePath path = base::FilePath(FILE_PATH_LITERAL("/path/to/file"));
  EXPECT_CALL(scoped_file_access,
              RequestFilesAccess(testing::ElementsAre(path), GURL(), _))
      .WillOnce(base::test::RunOnceCallback<2>(
          file_access::ScopedFileAccess::Allowed()));
  ChromeContentBrowserClient client;
  client.RequestFilesAccess({path}, GURL(),
                            continuation_callback.GetCallback());
  EXPECT_TRUE(continuation_callback.Take().is_allowed());
}

TEST_F(ChromeContentBrowserClientTest, RequestFileAccessDeny) {
  file_access::MockScopedFileAccessDelegate scoped_file_access;
  base::test::TestFuture<file_access::ScopedFileAccess> continuation_callback;
  base::FilePath path = base::FilePath(FILE_PATH_LITERAL("/path/to/file"));
  EXPECT_CALL(scoped_file_access,
              RequestFilesAccess(testing::ElementsAre(path), GURL(), _))
      .WillOnce(base::test::RunOnceCallback<2>(
          file_access::ScopedFileAccess::Denied()));
  ChromeContentBrowserClient client;
  client.RequestFilesAccess({path}, GURL(),
                            continuation_callback.GetCallback());
  EXPECT_FALSE(continuation_callback.Take().is_allowed());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class ChromeContentBrowserClientSwitchTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeContentBrowserClientSwitchTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

 protected:
  void AppendSwitchInCurrentProcess(std::string_view switch_string) {
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

TEST_F(ChromeContentBrowserClientSwitchTest, DataUrlInSvgDefault) {
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_FALSE(result.HasSwitch(blink::switches::kDataUrlInSvgUseEnabled));
}

TEST_F(ChromeContentBrowserClientSwitchTest, DataUrlInSvgDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kDataUrlInSvgUseEnabled, false);
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_FALSE(result.HasSwitch(blink::switches::kDataUrlInSvgUseEnabled));
}

TEST_F(ChromeContentBrowserClientSwitchTest, DataUrlInSvgEnabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kDataUrlInSvgUseEnabled, true);
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_TRUE(result.HasSwitch(blink::switches::kDataUrlInSvgUseEnabled));
}

TEST_F(ChromeContentBrowserClientSwitchTest, LegacyTechReportDisabled) {
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_FALSE(
      result.HasSwitch(blink::switches::kLegacyTechReportPolicyEnabled));
}

TEST_F(ChromeContentBrowserClientSwitchTest, LegacyTechReportEnabled) {
  base::Value::List policy;
  policy.Append("www.example.com");
  profile()->GetPrefs()->SetList(
      enterprise_reporting::kCloudLegacyTechReportAllowlist, std::move(policy));
  base::CommandLine result = FetchCommandLineSwitchesForRendererProcess();
  EXPECT_TRUE(
      result.HasSwitch(blink::switches::kLegacyTechReportPolicyEnabled));
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

class DisableWebAuthnWithBrokenCertsTest
    : public ChromeRenderViewHostTestHarness {};

TEST_F(DisableWebAuthnWithBrokenCertsTest, SecurityLevelNotAcceptable) {
  GURL url("https://doofenshmirtz.evil");
  TestChromeContentBrowserClient client;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_FALSE(client.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(DisableWebAuthnWithBrokenCertsTest, ExtensionSupported) {
  GURL url("chrome-extension://extensionid");
  TestChromeContentBrowserClient client;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(client.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(DisableWebAuthnWithBrokenCertsTest, EnterpriseOverride) {
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetBoolean(webauthn::pref_names::kAllowWithBrokenCerts, true);
  GURL url("https://doofenshmirtz.evil");
  TestChromeContentBrowserClient client;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(client.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, Localhost) {
  GURL url("http://localhost");
  TestChromeContentBrowserClient client;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  EXPECT_TRUE(client.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, SecurityLevelAcceptable) {
  GURL url("https://owca.org");
  TestChromeContentBrowserClient client;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = 0;  // ok.
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(client.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

// Regression test for crbug.com/1421174.
TEST_F(DisableWebAuthnWithBrokenCertsTest, IgnoreCertificateErrorsFlag) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kIgnoreCertificateErrors);
  GURL url("https://doofenshmirtz.evil");
  TestChromeContentBrowserClient client;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(client.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(ChromeContentBrowserClientTest, ShouldUseSpareRenderProcessHost) {
  using SpareProcessRefusedByEmbedderReason =
      content::ContentBrowserClient::SpareProcessRefusedByEmbedderReason;
  ChromeContentBrowserClient browser_client;

  // Standard web URL
  EXPECT_FALSE(browser_client.ShouldUseSpareRenderProcessHost(
      &profile_, GURL("https://www.example.com")));

  // No profile
  EXPECT_EQ(SpareProcessRefusedByEmbedderReason::NoProfile,
            browser_client.ShouldUseSpareRenderProcessHost(
                nullptr, GURL("https://www.example.com")));

#if !BUILDFLAG(IS_ANDROID)
  // Chrome-search URL
  EXPECT_EQ(SpareProcessRefusedByEmbedderReason::InstantRendererForNewTabPage,
            browser_client.ShouldUseSpareRenderProcessHost(
                &profile_, GURL("chrome-search://test")));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extension URL
  EXPECT_EQ(SpareProcessRefusedByEmbedderReason::ExtensionProcess,
            browser_client.ShouldUseSpareRenderProcessHost(
                &profile_, GURL("chrome-extension://test-extension/")));
#endif
}

#if BUILDFLAG(IS_WIN)
class ChromeContentBrowserClientFieldTrialTest
    : public ChromeContentBrowserClientTest {
 protected:
  ChromeContentBrowserClientFieldTrialTest() {
    base::MockEntropyProvider entropy_provider(0.9);
    trial_ = base::FieldTrialList::FactoryGetFieldTrial(
        "UiaProviderWin", 100, "Default_1234", entropy_provider);
  }

  ChromeContentBrowserClient& client() { return client_; }

 private:
  ScopedTestingLocalState testing_local_state_{
      TestingBrowserProcess::GetGlobal()};
  ScopedMetricsServiceForSyntheticTrials metrics_service_{
      TestingBrowserProcess::GetGlobal()};
  ChromeContentBrowserClient client_;
  scoped_refptr<base::FieldTrial> trial_;
};

TEST_F(ChromeContentBrowserClientFieldTrialTest,
       OnUiaProviderRequestedNoStudy) {
  client().OnUiaProviderRequested(false);
  ASSERT_FALSE(variations::IsInSyntheticTrialGroup("UiaProviderActiveSynthetic",
                                                   "Control"));
  ASSERT_FALSE(variations::IsInSyntheticTrialGroup("UiaProviderActiveSynthetic",
                                                   "Enabled"));
}

TEST_F(ChromeContentBrowserClientFieldTrialTest,
       OnUiaProviderRequestedEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitFromCommandLine(
      "UiaProvider<UiaProviderWin.Enabled_12345:k/v", {});
  client().OnUiaProviderRequested(true);
  ASSERT_FALSE(variations::IsInSyntheticTrialGroup("UiaProviderActiveSynthetic",
                                                   "Control"));
  ASSERT_TRUE(variations::IsInSyntheticTrialGroup("UiaProviderActiveSynthetic",
                                                  "Enabled"));
}

TEST_F(ChromeContentBrowserClientFieldTrialTest,
       OnUiaProviderRequestedControl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "UiaProvider<UiaProviderWin.Control_12345:k/v", {});
  client().OnUiaProviderRequested(false);
  ASSERT_TRUE(variations::IsInSyntheticTrialGroup("UiaProviderActiveSynthetic",
                                                  "Control"));
  ASSERT_FALSE(variations::IsInSyntheticTrialGroup("UiaProviderActiveSynthetic",
                                                   "Enabled"));
}
#endif  // BUILDFLAG(IS_WIN)

class GrantCookieAccessDueToHeuristicTest
    : public ChromeContentBrowserClientTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    profile_.GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(content_settings::CookieControlsMode::kLimited));
    profile_.GetPrefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled,
                                    true);

    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::Create(&profile_);
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &profile_, site_instance);
  }

  bool IgnoreSchemes() { return GetParam(); }

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
};

namespace {

// Helper to easily create a StorageKey from a GURL.
blink::StorageKey FirstPartyStorageKey(const GURL& url) {
  return blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
}

// Helper to easily create a SchemefulSite from a GURL.
net::SchemefulSite SchemefulSite(const GURL& url) {
  return net::SchemefulSite(url::Origin::Create(url));
}

// Return a copy of `url` with the scheme set to "http".
GURL WithHttp(const GURL& url) {
  GURL::Replacements replacements;
  replacements.SetSchemeStr("http");
  return url.ReplaceComponents(replacements);
}

// Return a copy of `url` with the port set to "999".
GURL WithPort999(const GURL& url) {
  GURL::Replacements replacements;
  replacements.SetPortStr("999");
  return url.ReplaceComponents(replacements);
}

}  // namespace

TEST_P(GrantCookieAccessDueToHeuristicTest,
       SchemefulSiteMatches_AccessAlwaysGranted) {
  TestChromeContentBrowserClient client;

  GURL top_level_url("https://www.toplevel.test/index.html");
  GURL url("https://www.subresource.test/favicon.ico");

  ASSERT_FALSE(client.IsFullCookieAccessAllowed(
      profile(), web_contents(), url, FirstPartyStorageKey(top_level_url)));
  client.GrantCookieAccessDueToHeuristic(
      profile(), SchemefulSite(top_level_url), SchemefulSite(url),
      base::Hours(1), IgnoreSchemes());
  ASSERT_TRUE(client.IsFullCookieAccessAllowed(
      profile(), web_contents(), url, FirstPartyStorageKey(top_level_url)));
}

TEST_P(GrantCookieAccessDueToHeuristicTest, SchemeMismatch_AccessMayBeGranted) {
  TestChromeContentBrowserClient client;

  GURL top_level_url("https://www.toplevel.test/index.html");
  GURL url("https://www.subresource.test/favicon.ico");

  client.GrantCookieAccessDueToHeuristic(
      profile(), SchemefulSite(top_level_url), SchemefulSite(url),
      base::Hours(1), IgnoreSchemes());
  // Cookie access granted iff ignore_schemes=true:
  ASSERT_EQ(client.IsFullCookieAccessAllowed(
                profile(), web_contents(), WithHttp(url),
                FirstPartyStorageKey(WithHttp(top_level_url))),
            IgnoreSchemes());
}

TEST_P(GrantCookieAccessDueToHeuristicTest, PortMismatch_AccessAlwaysGranted) {
  TestChromeContentBrowserClient client;

  GURL top_level_url("https://www.toplevel.test/index.html");
  GURL url("https://www.subresource.test/favicon.ico");

  client.GrantCookieAccessDueToHeuristic(
      profile(), SchemefulSite(top_level_url), SchemefulSite(url),
      base::Hours(1), IgnoreSchemes());
  ASSERT_TRUE(client.IsFullCookieAccessAllowed(
      profile(), web_contents(), WithPort999(url),
      FirstPartyStorageKey(WithPort999(top_level_url))));
}

TEST_P(GrantCookieAccessDueToHeuristicTest,
       HostnameMismatch_AccessNeverGranted) {
  TestChromeContentBrowserClient client;

  GURL top_level_url("https://www.toplevel.test/index.html");
  GURL url1("https://www.subresource.test/favicon.ico");
  GURL url2("https://www.subresource.example/favicon.ico");

  client.GrantCookieAccessDueToHeuristic(
      profile(), SchemefulSite(top_level_url), SchemefulSite(url1),
      base::Hours(1), IgnoreSchemes());
  ASSERT_FALSE(client.IsFullCookieAccessAllowed(
      profile(), web_contents(), url2, FirstPartyStorageKey(top_level_url)));
}

TEST_P(GrantCookieAccessDueToHeuristicTest,
       TopLevelHostnameMismatch_AccessNeverGranted) {
  TestChromeContentBrowserClient client;

  GURL top_level_url1("https://www.toplevel.test/index.html");
  GURL top_level_url2("https://www.toplevel.example/index.html");
  GURL url("https://www.subresource.test/favicon.ico");

  client.GrantCookieAccessDueToHeuristic(
      profile(), SchemefulSite(top_level_url1), SchemefulSite(url),
      base::Hours(1), IgnoreSchemes());
  ASSERT_FALSE(client.IsFullCookieAccessAllowed(
      profile(), web_contents(), url, FirstPartyStorageKey(top_level_url2)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GrantCookieAccessDueToHeuristicTest,
                         testing::Bool());
