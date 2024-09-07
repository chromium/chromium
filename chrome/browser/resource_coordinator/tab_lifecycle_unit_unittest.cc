// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/usage_clock.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

constexpr base::TimeDelta kShortDelay = base::Seconds(1);

class MockTabLifecycleObserver : public TabLifecycleObserver {
 public:
  MockTabLifecycleObserver() = default;

  MockTabLifecycleObserver(const MockTabLifecycleObserver&) = delete;
  MockTabLifecycleObserver& operator=(const MockTabLifecycleObserver&) = delete;

  MOCK_METHOD3(OnDiscardedStateChange,
               void(content::WebContents* contents,
                    LifecycleUnitDiscardReason reason,
                    bool is_discarded));
  MOCK_METHOD2(OnAutoDiscardableStateChange,
               void(content::WebContents* contents, bool is_auto_discardable));
};

}  // namespace

class MockLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  MockLifecycleUnitObserver() = default;

  MockLifecycleUnitObserver(const MockLifecycleUnitObserver&) = delete;
  MockLifecycleUnitObserver& operator=(const MockLifecycleUnitObserver&) =
      delete;

  MOCK_METHOD3(OnLifecycleUnitStateChanged,
               void(LifecycleUnit*,
                    LifecycleUnitState,
                    LifecycleUnitStateChangeReason));
  MOCK_METHOD1(OnLifecycleUnitDestroyed, void(LifecycleUnit*));
  MOCK_METHOD2(OnLifecycleUnitVisibilityChanged,
               void(LifecycleUnit*, content::Visibility));
};

class TabLifecycleUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  using TabLifecycleUnit = TabLifecycleUnitSource::TabLifecycleUnit;

  // This is an internal class so that it is also friends with
  // TabLifecycleUnitTest.
  class ScopedEnterpriseOptOut;

  TabLifecycleUnitTest()
      : scoped_set_clocks_for_testing_(&test_clock_, &test_tick_clock_) {
    test_clock_.SetNow(base::Time::NowFromSystemTime());
    // Advance the clock so that it doesn't yield null time ticks.
    test_tick_clock_.Advance(base::Seconds(1));
    observers_.AddObserver(&observer_);
  }

  TabLifecycleUnitTest(const TabLifecycleUnitTest&) = delete;
  TabLifecycleUnitTest& operator=(const TabLifecycleUnitTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    metrics::DesktopSessionDurationTracker::Initialize();
    usage_clock_ = std::make_unique<UsageClock>();

    // Force TabManager/TabLifecycleUnitSource creation.
    g_browser_process->GetTabManager();

    std::unique_ptr<content::WebContents> test_web_contents =
        CreateTestWebContents();
    web_contents_ = test_web_contents.get();
    auto* tester = content::WebContentsTester::For(web_contents_);
    tester->SetLastActiveTimeTicks(NowTicks());
    tester->SetLastActiveTime(Now());
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents_);
    // Commit an URL to allow discarding.
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://www.example.com"), web_contents_);
    navigation->SetKeepLoading(true);
    navigation->Commit();

    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());
    tab_strip_model_->AppendWebContents(std::move(test_web_contents), false);
    web_contents_->WasHidden();

    std::unique_ptr<content::WebContents> second_web_contents =
        CreateTestWebContents();
    content::WebContents* raw_second_web_contents = second_web_contents.get();
    tab_strip_model_->AppendWebContents(std::move(second_web_contents),
                                        /*foreground=*/true);
    raw_second_web_contents->WasHidden();
  }

  void TearDown() override {
    while (!tab_strip_model_->empty())
      tab_strip_model_->DetachAndDeleteWebContentsAt(0);
    tab_strip_model_.reset();
    usage_clock_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Create a new test WebContents and append it to the tab strip to allow
  // testing discarding operations on it. The returned WebContents is in the
  // hidden state.
  content::WebContents* AddNewHiddenWebContentsToTabStrip() {
    std::unique_ptr<content::WebContents> test_web_contents =
        CreateTestWebContents();
    content::WebContents* web_contents = test_web_contents.get();
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents);
    tab_strip_model_->AppendWebContents(std::move(test_web_contents), false);
    web_contents->WasHidden();
    return web_contents;
  }

  ::testing::StrictMock<MockTabLifecycleObserver> observer_;
  base::ObserverList<TabLifecycleObserver>::UncheckedAndDanglingUntriaged
      observers_;
  raw_ptr<content::WebContents, DanglingUntriaged>
      web_contents_;  // Owned by tab_strip_model_.
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;
  std::unique_ptr<UsageClock> usage_clock_;

 private:
  // So that the main thread looks like the UI thread as expected.
  TestTabStripModelDelegate tab_strip_model_delegate_;
  ScopedSetClocksForTesting scoped_set_clocks_for_testing_;
  tabs::PreventTabFeatureInitialization prevent_;
};

class TabLifecycleUnitTest::ScopedEnterpriseOptOut {
 public:
  ScopedEnterpriseOptOut() {
    GetTabLifecycleUnitSource()->SetTabLifecyclesEnterprisePolicy(false);
  }

  ~ScopedEnterpriseOptOut() {
    GetTabLifecycleUnitSource()->SetTabLifecyclesEnterprisePolicy(true);
  }
};

TEST_F(TabLifecycleUnitTest, AsTabLifecycleUnitExternal) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  EXPECT_TRUE(tab_lifecycle_unit.AsTabLifecycleUnitExternal());
}

TEST_F(TabLifecycleUnitTest, CanDiscardByDefault) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, SetFocused) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  EXPECT_EQ(NowTicks(), tab_lifecycle_unit.GetLastFocusedTimeTicks());
  EXPECT_EQ(Now(), tab_lifecycle_unit.GetLastFocusedTime());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  tab_lifecycle_unit.SetFocused(true);
  tab_strip_model_->ActivateTabAt(0);
  web_contents_->WasShown();
  EXPECT_EQ(base::TimeTicks::Max(),
            tab_lifecycle_unit.GetLastFocusedTimeTicks());
  EXPECT_EQ(base::Time::Max(), tab_lifecycle_unit.GetLastFocusedTime());
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_VISIBLE);

  tab_lifecycle_unit.SetFocused(false);
  tab_strip_model_->ActivateTabAt(1);
  web_contents_->WasHidden();
  EXPECT_EQ(test_tick_clock_.NowTicks(),
            tab_lifecycle_unit.GetLastFocusedTimeTicks());
  EXPECT_EQ(test_clock_.Now(), tab_lifecycle_unit.GetLastFocusedTime());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, AutoDiscardable) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());

  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  EXPECT_CALL(observer_,
              OnAutoDiscardableStateChange(web_contents_.get(), false));
  tab_lifecycle_unit.SetAutoDiscardable(false);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_FALSE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit,
      DecisionFailureReason::LIVE_STATE_EXTENSION_DISALLOWED);

  EXPECT_CALL(observer_,
              OnAutoDiscardableStateChange(web_contents_.get(), true));
  tab_lifecycle_unit.SetAutoDiscardable(true);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardCrashed) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());

  auto* tester = content::WebContentsTester::For(web_contents_);
  tester->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TabLifecycleUnitTest, CannotDiscardActive) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());

  tab_strip_model_->ActivateTabAt(0);

  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_VISIBLE);
}

TEST_F(TabLifecycleUnitTest, UrgentDiscardProtections) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Initial external discarding are fine, but urgent discarding is blocked
  // because the tab is too recent.
  ExpectCanDiscardTrue(&tab_lifecycle_unit,
                       LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardFalseTrivial(&tab_lifecycle_unit,
                               LifecycleUnitDiscardReason::URGENT);

  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The tab should now be discardable for all reasons.
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  // Mark the tab as having been discarded.
  tab_lifecycle_unit.SetDiscardCountForTesting(1);

  // Advance time enough that the time protection no longer applies. The tab
  // should still not be urgent discardable at this point, because it has
  // already been discarded at least once.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

  // External discarding should be fine, but not urgent.
  ExpectCanDiscardTrue(&tab_lifecycle_unit,
                       LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardFalseTrivial(&tab_lifecycle_unit,
                               LifecycleUnitDiscardReason::URGENT);

  // The tab should be discardable a second time when the memory limit
  // enterprise policy is set.
  GetTabLifecycleUnitSource()->SetMemoryLimitEnterprisePolicyFlag(true);
  ExpectCanDiscardTrue(&tab_lifecycle_unit, LifecycleUnitDiscardReason::URGENT);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(TabLifecycleUnitTest, CannotDiscardInvalidURL) {
  content::WebContents* web_contents = AddNewHiddenWebContentsToTabStrip();
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents,
                                      tab_strip_model_.get());
  // TODO(sebmarchand): Fix this test, this doesn't really test that it's not
  // possible to discard an invalid URL, TestWebContents::GetLastCommittedURL()
  // doesn't return the URL set with "SetLastCommittedURL" if this one is
  // invalid.
  content::WebContentsTester::For(web_contents)
      ->SetLastCommittedURL(GURL("Invalid :)"));
  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardEmptyURL) {
  content::WebContents* web_contents = AddNewHiddenWebContentsToTabStrip();
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents,
                                      tab_strip_model_.get());

  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardVideoCapture) {
#if BUILDFLAG(IS_CHROMEOS)
  // Mock system-level microphone permission.
  system_permission_settings::ScopedSettingsForTesting mic_settings(
      ContentSettingsType::MEDIASTREAM_MIC, false);
#endif  // BUILDFLAG(IS_CHROMEOS)

  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  blink::mojom::StreamDevices devices;
  devices.video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_media_device",
      "fake_media_device");

  std::unique_ptr<content::MediaStreamUI> ui =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          ->RegisterMediaStream(web_contents_, devices);
  ui->OnStarted(base::RepeatingClosure(),
                content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_CAPTURING);

  ui.reset();
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardHasFormInteractions) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  FormInteractionTabHelper::CreateForWebContents(web_contents_);
  FormInteractionTabHelper::FromWebContents(web_contents_)
      ->OnHadFormInteractionChangedForTesting(true);
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_FORM_ENTRY);

  FormInteractionTabHelper::FromWebContents(web_contents_)
      ->OnHadFormInteractionChangedForTesting(false);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardDesktopCapture) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  blink::mojom::StreamDevices devices;
  devices.video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      "fake_media_device", "fake_media_device");
  std::unique_ptr<content::MediaStreamUI> ui =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          ->RegisterMediaStream(web_contents_, devices);
  ui->OnStarted(base::RepeatingClosure(),
                content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_DESKTOP_CAPTURE);

  ui.reset();
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardRecentlyAudible) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  // Cannot discard when the "recently audible" bit is set.
  tab_lifecycle_unit.SetRecentlyAudible(true);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit is still set. The tab cannot be discarded.
  test_tick_clock_.Advance(kTabAudioProtectionTime);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit was unset less than
  // kTabAudioProtectionTime ago. The tab cannot be discarded.
  tab_lifecycle_unit.SetRecentlyAudible(false);
  test_tick_clock_.Advance(kShortDelay);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit was unset kTabAudioProtectionTime ago. The tab
  // can be discarded.
  test_tick_clock_.Advance(kTabAudioProtectionTime - kShortDelay);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  // Calling SetRecentlyAudible(false) again does not change the fact that the
  // tab can be discarded.
  tab_lifecycle_unit.SetRecentlyAudible(false);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CanDiscardNeverAudibleTab) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  tab_lifecycle_unit.SetRecentlyAudible(false);
  // Since the tab was never audible, it should be possible to discard it,
  // even if there was a recent call to SetRecentlyAudible(false).
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardPDF) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  content::WebContentsTester::For(web_contents_)
      ->SetMainFrameMimeType("application/pdf");
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_IS_PDF);
}

TEST_F(TabLifecycleUnitTest, NotifiedOfWebContentsVisibilityChanges) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());

  ::testing::StrictMock<MockLifecycleUnitObserver> observer;
  tab_lifecycle_unit.AddObserver(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &tab_lifecycle_unit, content::Visibility::VISIBLE));
  web_contents_->WasShown();
  ::testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &tab_lifecycle_unit, content::Visibility::HIDDEN));
  web_contents_->WasHidden();
  ::testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &tab_lifecycle_unit, content::Visibility::VISIBLE));
  web_contents_->WasShown();
  ::testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer,
              OnLifecycleUnitVisibilityChanged(&tab_lifecycle_unit,
                                               content::Visibility::OCCLUDED));
  web_contents_->WasOccluded();
  ::testing::Mock::VerifyAndClear(&observer);

  tab_lifecycle_unit.RemoveObserver(&observer);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardPictureInPictureWindow) {
  // Create picture-in-picture browser.
  auto pip_window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_PICTURE_IN_PICTURE;
  params.window = pip_window.get();
  std::unique_ptr<Browser> pip_browser;
  pip_browser.reset(Browser::Create(params));
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  ResourceCoordinatorTabHelper::CreateForWebContents(raw_contents);
  pip_browser->tab_strip_model()->AppendWebContents(std::move(contents),
                                                    /*foreground=*/true);

  // Attempt to discard the tab. This should fail as picture-in-picture tabs
  // should not be discardable.
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), raw_contents,
                                      pip_browser->tab_strip_model());
  EXPECT_FALSE(
      tab_lifecycle_unit.Discard(LifecycleUnitDiscardReason::EXTERNAL, 0));

  // Tear down picture-in-picture browser.
  pip_browser->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  pip_browser.reset();
  pip_window.reset();
}

}  // namespace resource_coordinator
