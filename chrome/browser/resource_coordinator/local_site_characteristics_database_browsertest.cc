// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_reader.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_reader.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker_test_support.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_switches.h"
#endif

namespace resource_coordinator {

namespace {

using WebContents = content::WebContents;
using WebContentsTester = content::WebContentsTester;

constexpr char kTestPage[] =
    "/resource_coordinator/site_characteristics_test_page.html";

// Returns the longest feature observation window.
base::TimeDelta GetLongestObservationWindow() {
  const SiteCharacteristicsDatabaseParams& params =
      GetStaticSiteCharacteristicsDatabaseParams();
  return std::max({params.favicon_update_observation_window,
                   params.title_update_observation_window,
                   params.audio_usage_observation_window,
                   params.notifications_usage_observation_window});
}

// Returns the longest grace period.
base::TimeDelta GetLongestGracePeriod() {
  const SiteCharacteristicsDatabaseParams& params =
      GetStaticSiteCharacteristicsDatabaseParams();
  return std::max({params.title_or_favicon_change_post_load_grace_period,
                   params.feature_usage_post_background_grace_period});
}

// Returns the LocalSiteCharacteristicsDataImpl that backs |reader|.
internal::LocalSiteCharacteristicsDataImpl* GetImplFromReader(
    SiteCharacteristicsDataReader* reader) {
  return static_cast<LocalSiteCharacteristicsDataReader*>(reader)
      ->impl_for_testing();
}

// Returns the SiteDataProto that backs |reader|.
const SiteDataProto* GetSiteDataProtoFromReader(
    SiteCharacteristicsDataReader* reader) {
  return &GetImplFromReader(reader)->site_characteristics_for_testing();
}

}  // namespace

class LocalSiteCharacteristicsDatabaseTest : public InProcessBrowserTest {
 public:
  LocalSiteCharacteristicsDatabaseTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_),
        test_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}
  ~LocalSiteCharacteristicsDatabaseTest() override = default;

  void SetUp() override {
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteCharacteristicsDatabase);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Setup the test server.
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    test_server_.ServeFilesFromDirectory(
        test_data_dir.AppendASCII("chrome/test/data/"));
    ASSERT_TRUE(test_server_.InitializeAndListen());
    test_server_.StartAcceptingConnections();
    const std::string real_host = test_server_.host_port_pair().host();
    host_resolver()->AddRule("*", real_host);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);

    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other origins without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::unique_ptr<SiteCharacteristicsDataReader> GetReaderForOrigin(
      Profile* profile,
      const url::Origin& origin) {
    SiteCharacteristicsDataStore* data_store =
        LocalSiteCharacteristicsDataStoreFactory::GetForProfile(profile);
    EXPECT_TRUE(data_store);
    std::unique_ptr<SiteCharacteristicsDataReader> reader =
        data_store->GetReaderForOrigin(origin);
    base::RunLoop run_loop;
    reader->RegisterDataLoadedCallback(run_loop.QuitClosure());
    run_loop.Run();

    const internal::LocalSiteCharacteristicsDataImpl* impl =
        GetImplFromReader(reader.get());
    EXPECT_TRUE(impl->fully_initialized_for_testing());
    return reader;
  }

  // Test that feature usage is tracked correctly:
  //   - kSiteFeatureUsageUnknown if never observed and observation window
  //     hasn't expired.
  //   - kSiteFeatureNotInUse if never observed and observation window has
  //     expired.
  //   - kSiteFeatureInUse if observed.
  // |feature_detection_method| is the SiteCharacteristicsDataReader method that
  // will be called to query the status of this feature. |triggering_closure| is
  // the closure to run to cause this feature to be used (this will get called
  // while the tab is in background) and |allowing_closure| is an optional
  // closure that should run before testing the feature usage (to allow it to
  // be used).
  void TestFeatureUsageDetection(
      performance_manager::SiteFeatureUsage (
          SiteCharacteristicsDataReader::*feature_detection_method)() const,
      internal::LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures
          feature_type,
      base::RepeatingClosure triggering_closure,
      base::RepeatingClosure allowing_closure = base::DoNothing::Repeatedly()) {
    // Test that feature usage is tracked correctly before the expiration of its
    // observation window.
    TestFeatureUsageDetectionImpl(feature_detection_method, feature_type,
                                  allowing_closure, triggering_closure, false);
    // Test that feature usage is tracked correctly after the expiration of its
    // observation window.
    TestFeatureUsageDetectionImpl(feature_detection_method, feature_type,
                                  std::move(allowing_closure),
                                  std::move(triggering_closure), true);
  }

  void ExecuteScriptInMainFrame(const char* script) {
    content::RenderFrameHost* main_frame =
        GetActiveWebContents()->GetMainFrame();
    EXPECT_TRUE(content::ExecuteScript(main_frame, script));
  }

  void PlayAudioInActiveWebContents() {
    ExecuteScriptInMainFrame("PlayAudio();");
  }

  void ChangeTitleOfActiveWebContents() {
    ExecuteScriptInMainFrame("ChangeTitle('new_title')");
  }

  void ChangeFaviconOfActiveWebContents() {
    ExecuteScriptInMainFrame("ChangeFavicon()");
  }

  void TriggerNonPersistentNotificationInActiveWebContents() {
    ExecuteScriptInMainFrame(
        "DisplayAndCloseNonPersistentNotification('foo');");
  }

  // By default a tab has to play audio while being visible if it wants to be
  // able to play audio in background (see
  // ChromeContentRendererClient::DeferMediaLoad). This makes the current active
  // WebContents visible, play some audio and background it. After calling
  // this the background tab is allowed play audio.
  void AllowBackgroundAudioInActiveTab() {
    content::WebContents* active_webcontents = GetActiveWebContents();
    active_webcontents->WasShown();
    PlayAudioInActiveWebContents();
    // Wait for the audio to start playing.
    while (!active_webcontents->WasEverAudible())
      base::RunLoop().RunUntilIdle();

    active_webcontents->GetController().Reload(content::ReloadType::NORMAL,
                                               false);
    content::WaitForLoadStop(GetActiveWebContents());
    // Background the tab and reload it so the audio will stop playing if it's
    // still playing.
    GetActiveWebContents()->WasHidden();
    test_clock_.Advance(GetStaticSiteCharacteristicsDatabaseParams()
                            .feature_usage_post_background_grace_period);
  }

  // Ensure that the current tab is allowed to display non-persistent
  // notifications.
  void AllowBackgroundNotificationInActiveTab() {
    content::WebContents* web_contents = GetActiveWebContents();
    NotificationPermissionContext::UpdatePermission(
        browser()->profile(), web_contents->GetLastCommittedURL(),
        CONTENT_SETTING_ALLOW);

    ExecuteScriptInMainFrame("RequestNotificationsPermission();");
  }

  void ExpireTitleOrFaviconGracePeriod() {
    test_clock_.Advance(GetStaticSiteCharacteristicsDatabaseParams()
                            .title_or_favicon_change_post_load_grace_period);
  }

  base::SimpleTestTickClock& test_clock() { return test_clock_; }
  net::test_server::EmbeddedTestServer& test_server() { return test_server_; }

 private:
  void TestFeatureUsageDetectionImpl(
      performance_manager::SiteFeatureUsage (
          SiteCharacteristicsDataReader::*feature_detection_method)() const,
      const internal::LocalSiteCharacteristicsDataImpl::
          TrackedBackgroundFeatures feature_type,
      base::OnceClosure allowing_closure,
      base::RepeatingClosure triggering_closure,
      bool wait_for_observation_window_to_expire);

  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer test_server_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDatabaseTest);
};

void LocalSiteCharacteristicsDatabaseTest::TestFeatureUsageDetectionImpl(
    performance_manager::SiteFeatureUsage (
        SiteCharacteristicsDataReader::*feature_detection_method)() const,
    const internal::LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures
        feature_type,
    base::OnceClosure allowing_closure,
    base::RepeatingClosure triggering_closure,
    bool wait_for_observation_window_to_expire) {
  // Use a different origin depending on the type of test to make sure that
  // previous observations don't get re-used.
  const char* kOrigin =
      wait_for_observation_window_to_expire ? "foo.com" : "bar.com";
  GURL test_url(test_server_.GetURL(kOrigin, kTestPage));

  // Get the reader for this origin.
  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            (reader.get()->*feature_detection_method)());

  // Navigate to the test url and background it.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  GetActiveWebContents()->WasHidden();
  WaitForTransitionToLoaded(GetActiveWebContents());

  // If needed, wait for all feature observation windows to expire.
  if (wait_for_observation_window_to_expire) {
    test_clock_.Advance(GetLongestObservationWindow());
    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
              (reader.get()->*feature_detection_method)());
  }

  // Call the allowing closure.
  std::move(allowing_closure).Run();

  // Ensure that the closure hasn't caused the feature usage status to
  // change.
  if (wait_for_observation_window_to_expire) {
    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
              (reader.get()->*feature_detection_method)());
  } else {
    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
              (reader.get()->*feature_detection_method)());
  }

  base::RunLoop run_loop;
  GetImplFromReader(reader.get())
      ->RegisterFeatureUsageCallbackForTesting(feature_type,
                                               run_loop.QuitClosure());

  // Cause the feature to be used.
  triggering_closure.Run();

  run_loop.Run();

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            (reader.get()->*feature_detection_method)());

  // Advance the clock, make sure that the feature usage status doesn't
  // change.
  test_clock_.Advance(GetLongestObservationWindow());

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            (reader.get()->*feature_detection_method)());
}

// Test that doesn't use any feature.
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest, NoFeatureUsed) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(test_url, contents->GetLastCommittedURL());
  WaitForTransitionToLoaded(contents);

  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UsesNotificationsInBackground());

  test_clock().Advance(GetLongestObservationWindow());

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader->UsesNotificationsInBackground());
}

// Test that use features while in foreground, this shouldn't be recorded.
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       FeatureUsedInForegroundOnly) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  GetActiveWebContents()->WasShown();

  ChangeTitleOfActiveWebContents();
  ChangeFaviconOfActiveWebContents();
  PlayAudioInActiveWebContents();
  // TODO(sebmarchand): Also trigger a background notification once.

  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());

  // Advance the clock while the tab is still in foreground and make sure that
  // the state hasn't changed.
  test_clock().Advance(GetLongestObservationWindow());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UsesNotificationsInBackground());
}

// Test that the audio feature usage in background gets detected properly.
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       AudioFeatureUsage) {
  TestFeatureUsageDetection(
      &SiteCharacteristicsDataReader::UsesAudioInBackground,
      internal::LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures::
          kAudioUsage,
      base::BindRepeating(
          &LocalSiteCharacteristicsDatabaseTest::PlayAudioInActiveWebContents,
          base::Unretained(this)),
      base::BindRepeating(&LocalSiteCharacteristicsDatabaseTest::
                              AllowBackgroundAudioInActiveTab,
                          base::Unretained(this)));
}

// Test that the notification feature usage in background gets detected
// properly.
// TODO(sebmarchand): Figure out how to trigger a non-persistent notification in
// this test.
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       DISABLED_NotificationFeatureUsage) {
  TestFeatureUsageDetection(
      &SiteCharacteristicsDataReader::UsesNotificationsInBackground,
      internal::LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures::
          kNotificationUsageUsage,
      base::BindRepeating(
          &LocalSiteCharacteristicsDatabaseTest::
              TriggerNonPersistentNotificationInActiveWebContents,
          base::Unretained(this)),
      base::BindRepeating(&LocalSiteCharacteristicsDatabaseTest::
                              AllowBackgroundNotificationInActiveTab,
                          base::Unretained(this)));
}

IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       TitleUpdateFeatureUsage) {
  TestFeatureUsageDetection(
      &SiteCharacteristicsDataReader::UpdatesTitleInBackground,
      internal::LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures::
          kTitleUpdate,
      base::BindRepeating(
          &LocalSiteCharacteristicsDatabaseTest::ChangeTitleOfActiveWebContents,
          base::Unretained(this)),
      base::BindRepeating(&LocalSiteCharacteristicsDatabaseTest::
                              ExpireTitleOrFaviconGracePeriod,
                          base::Unretained(this)));
}

// Test that the favicon update feature usage in background gets detected
// properly.
// TODO(crbug.com/1004641): Investigate and reenable.
#if defined(OS_WIN)
#define MAYBE_FaviconUpdateFeatureUsage DISABLED_FaviconUpdateFeatureUsage
#else
#define MAYBE_FaviconUpdateFeatureUsage FaviconUpdateFeatureUsage
#endif
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       MAYBE_FaviconUpdateFeatureUsage) {
  TestFeatureUsageDetection(
      &SiteCharacteristicsDataReader::UpdatesFaviconInBackground,
      internal::LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures::
          kFaviconUpdate,
      base::BindRepeating(&LocalSiteCharacteristicsDatabaseTest::
                              ChangeFaviconOfActiveWebContents,
                          base::Unretained(this)),
      base::BindRepeating(&LocalSiteCharacteristicsDatabaseTest::
                              ExpireTitleOrFaviconGracePeriod,
                          base::Unretained(this)));
}

// Test that loads the same origin into multiple tabs and ensure that they get
// tracked properly.
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       LoadedStateGetsTrackedProperly) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));
  auto test_reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));

  const size_t kTabCount = 3;

  // Load all the tabs and background them.
  for (size_t i = 0; i < kTabCount; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), test_url,
        i == 0 ? WindowOpenDisposition::CURRENT_TAB
               : WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(i);
    WaitForTransitionToLoaded(contents);
    EXPECT_EQ(TabLoadTracker::LoadingState::LOADED,
              TabLoadTracker::Get()->GetLoadingState(contents));
    contents->WasHidden();
  }

  const internal::LocalSiteCharacteristicsDataImpl* impl =
      GetImplFromReader(test_reader.get());
  EXPECT_TRUE(impl);
  EXPECT_EQ(3U, impl->loaded_tabs_count_for_testing());
  EXPECT_EQ(3U, impl->loaded_tabs_in_background_count_for_testing());

  // Change the visibility of the tabs.
  for (size_t i = 0; i < kTabCount; ++i) {
    browser()->tab_strip_model()->GetWebContentsAt(i)->WasShown();

    EXPECT_EQ(kTabCount, impl->loaded_tabs_count_for_testing());
    EXPECT_EQ(kTabCount - (i + 1),
              impl->loaded_tabs_in_background_count_for_testing());
  }

  for (size_t i = 0; i < kTabCount; ++i)
    browser()->tab_strip_model()->GetWebContentsAt(i)->WasHidden();

  EXPECT_EQ(3U, impl->loaded_tabs_in_background_count_for_testing());

  // Close the tabs.
  for (size_t i = 0; i < kTabCount; ++i) {
    EXPECT_TRUE(browser()->tab_strip_model()->CloseWebContentsAt(0, 0));
    EXPECT_EQ(kTabCount - (i + 1), impl->loaded_tabs_count_for_testing());
    EXPECT_EQ(kTabCount - (i + 1),
              impl->loaded_tabs_in_background_count_for_testing());
  }
}

// Ensure that the observations gets persisted on disk.
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       PRE_DatabaseGetsPersisted) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));

  // Get the reader for this origin.
  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());

  // Navigate to the test url and background it.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  WaitForTransitionToLoaded(GetActiveWebContents());
  GetActiveWebContents()->WasHidden();

  test_clock().Advance(GetLongestGracePeriod());

  base::RunLoop run_loop;
  GetImplFromReader(reader.get())
      ->RegisterFeatureUsageCallbackForTesting(
          internal::LocalSiteCharacteristicsDataImpl::
              TrackedBackgroundFeatures::kTitleUpdate,
          run_loop.QuitClosure());

  // Cause the "title update in background" feature to be used.
  ChangeTitleOfActiveWebContents();

  run_loop.Run();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());
}

IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       DatabaseGetsPersisted) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));

  // Get the reader for this origin.
  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));

  // We should remember the observation made previously.
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());
}

// Ensure that clearing the history removes the observations from disk.
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest, PRE_ClearHistory) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));

  // Get the reader for this origin.
  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());

  // Navigate to the test url and background it.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  WaitForTransitionToLoaded(GetActiveWebContents());
  GetActiveWebContents()->WasHidden();

  test_clock().Advance(GetLongestGracePeriod());

  base::RunLoop run_loop;
  GetImplFromReader(reader.get())
      ->RegisterFeatureUsageCallbackForTesting(
          internal::LocalSiteCharacteristicsDataImpl::
              TrackedBackgroundFeatures::kTitleUpdate,
          run_loop.QuitClosure());

  // Cause the "title update in background" feature to be used.
  ChangeTitleOfActiveWebContents();

  run_loop.Run();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());

  HistoryServiceFactory::GetForProfile(browser()->profile(),
                                       ServiceAccessType::IMPLICIT_ACCESS)
      ->DeleteURLs({test_url});
  // The history gets cleared asynchronously.
  while (reader->UpdatesTitleInBackground() !=
         performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown) {
    base::RunLoop().RunUntilIdle();
  }
}

IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest, ClearHistory) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));

  // Get the reader for this origin.
  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));

  // The history has been cleared, we shouldn't know if this feature is used.
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
}

// Ensure that the observation time gets tracked across sessions.
IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       PRE_DatabaseObservationTimeTrackedAcrossSessions) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));

  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));
  const SiteDataProto* site_characteristics =
      GetSiteDataProtoFromReader(reader.get());

  EXPECT_EQ(0, site_characteristics->updates_title_in_background()
                   .observation_duration());

  // Navigate to the test url and background it.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  WaitForTransitionToLoaded(GetActiveWebContents());
  GetActiveWebContents()->WasHidden();

  test_clock().Advance(base::TimeDelta::FromSeconds(10));
  EXPECT_GE(10, site_characteristics->updates_title_in_background()
                    .observation_duration());
}

IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDatabaseTest,
                       DatabaseObservationTimeTrackedAcrossSessions) {
  GURL test_url(test_server().GetURL("foo.com", kTestPage));

  auto reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));
  const SiteDataProto* site_characteristics =
      GetSiteDataProtoFromReader(reader.get());

  auto observation_duration =
      site_characteristics->updates_title_in_background()
          .observation_duration();
  // The observation duration shouldn't be null as this has been augmented in
  // the 'PRE' part of this test.
  EXPECT_NE(0, observation_duration);

  // Advance the clock and reset this reader. This should increase the
  // observation duration that we have for this origin in the database.
  test_clock().Advance(base::TimeDelta::FromSeconds(10));
  reader.reset();

  reader =
      GetReaderForOrigin(browser()->profile(), url::Origin::Create(test_url));
  site_characteristics = GetSiteDataProtoFromReader(reader.get());

  EXPECT_GE(observation_duration,
            site_characteristics->updates_title_in_background()
                .observation_duration());
}

}  // namespace resource_coordinator
