// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/usage_clock.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);

class MockTabLifecycleObserver : public TabLifecycleObserver {
 public:
  MockTabLifecycleObserver() = default;

  MOCK_METHOD3(OnDiscardedStateChange,
               void(content::WebContents* contents,
                    LifecycleUnitDiscardReason reason,
                    bool is_discarded));
  MOCK_METHOD2(OnAutoDiscardableStateChange,
               void(content::WebContents* contents, bool is_auto_discardable));
  MOCK_METHOD2(OnFrozenStateChange,
               void(content::WebContents* contents, bool is_frozen));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabLifecycleObserver);
};

}  // namespace

class MockLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  MockLifecycleUnitObserver() = default;

  MOCK_METHOD3(OnLifecycleUnitStateChanged,
               void(LifecycleUnit*,
                    LifecycleUnitState,
                    LifecycleUnitStateChangeReason));
  MOCK_METHOD1(OnLifecycleUnitDestroyed, void(LifecycleUnit*));
  MOCK_METHOD2(OnLifecycleUnitVisibilityChanged,
               void(LifecycleUnit*, content::Visibility));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLifecycleUnitObserver);
};

class TabLifecycleUnitTest : public testing::ChromeTestHarnessWithLocalDB {
 protected:
  using TabLifecycleUnit = TabLifecycleUnitSource::TabLifecycleUnit;

  // This is an internal class so that it is also friends with
  // TabLifecycleUnitTest.
  class ScopedEnterpriseOptOut;

  TabLifecycleUnitTest() : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Advance the clock so that it doesn't yield null time ticks.
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    observers_.AddObserver(&observer_);
  }

  void SetUp() override {
    ChromeTestHarnessWithLocalDB::SetUp();

    metrics::DesktopSessionDurationTracker::Initialize();
    usage_clock_ = std::make_unique<UsageClock>();

    // Force TabManager/TabLifecycleUnitSource creation.
    g_browser_process->GetTabManager();

    std::unique_ptr<content::WebContents> test_web_contents =
        CreateTestWebContents();
    web_contents_ = test_web_contents.get();
    auto* tester = content::WebContentsTester::For(web_contents_);
    tester->SetLastActiveTime(NowTicks());
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

    testing::WaitForLocalDBEntryToBeInitialized(
        web_contents_,
        base::BindRepeating([]() { base::RunLoop().RunUntilIdle(); }));

    testing::ExpireLocalDBObservationWindows(web_contents_);
  }

  void TearDown() override {
    while (!tab_strip_model_->empty())
      tab_strip_model_->DetachWebContentsAt(0);
    tab_strip_model_.reset();
    usage_clock_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
    ChromeTestHarnessWithLocalDB::TearDown();
  }

  void TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason failure_reason,
      void (SiteCharacteristicsDataWriter::*notify_feature_usage_method)());

  // Create a new test WebContents and append it to the tab strip to allow
  // testing discarding and freezing operations on it. The returned WebContents
  // is in the hidden state.
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
  base::ObserverList<TabLifecycleObserver>::Unchecked observers_;
  content::WebContents* web_contents_;  // Owned by tab_strip_model_.
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::SimpleTestTickClock test_clock_;
  std::unique_ptr<UsageClock> usage_clock_;

 private:
  // So that the main thread looks like the UI thread as expected.
  TestTabStripModelDelegate tab_strip_model_delegate_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitTest);
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

void TabLifecycleUnitTest::TestCannotDiscardBasedOnHeuristicUsage(
    DecisionFailureReason failure_reason,
    void (SiteCharacteristicsDataWriter::*notify_feature_usage_method)()) {
  testing::GetLocalSiteCharacteristicsDataImplForWC(web_contents_)
      ->ClearObservationsAndInvalidateReadOperationForTesting();
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  auto* observer = ResourceCoordinatorTabHelper::FromWebContents(web_contents_)
                       ->local_site_characteristics_wc_observer();
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  testing::MarkWebContentsAsLoadedInBackground(web_contents_);
  if (notify_feature_usage_method) {
    // If |notify_feature_usage_method| is not null then all the observation
    // windows should be expired to make sure that |CanDiscard| doesn't return
    // false simply because of a lack of observations.
    testing::ExpireLocalDBObservationWindows(web_contents_);
    (observer->GetWriterForTesting()->*notify_feature_usage_method)();
  }
  {
    DecisionDetails decision_details;
    EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(
        LifecycleUnitDiscardReason::PROACTIVE, &decision_details));
    EXPECT_FALSE(decision_details.IsPositive());
    EXPECT_EQ(failure_reason, decision_details.FailureReason());
    // There should only be one reason (e.g. no duplicates).
    EXPECT_THAT(
        decision_details.reasons(),
        ::testing::ElementsAre(DecisionDetails::Reason(failure_reason)));
  }

  // Heuristics shouldn't be considered for urgent or external tab discarding.
  {
    DecisionDetails decision_details;
    EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(
        LifecycleUnitDiscardReason::EXTERNAL, &decision_details));
    EXPECT_TRUE(decision_details.IsPositive());
  }
  {
    DecisionDetails decision_details;
    EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(
        LifecycleUnitDiscardReason::URGENT, &decision_details));
    EXPECT_TRUE(decision_details.IsPositive());
  }

  testing::GetLocalSiteCharacteristicsDataImplForWC(web_contents_)
      ->NotifySiteUnloaded(performance_manager::TabVisibility::kBackground);
}

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
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, SetFocused) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  EXPECT_EQ(NowTicks(), tab_lifecycle_unit.GetLastFocusedTime());
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  tab_lifecycle_unit.SetFocused(true);
  tab_strip_model_->ActivateTabAt(0);
  web_contents_->WasShown();
  EXPECT_EQ(base::TimeTicks::Max(), tab_lifecycle_unit.GetLastFocusedTime());
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_VISIBLE);

  tab_lifecycle_unit.SetFocused(false);
  tab_strip_model_->ActivateTabAt(1);
  web_contents_->WasHidden();
  EXPECT_EQ(test_clock_.NowTicks(), tab_lifecycle_unit.GetLastFocusedTime());
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, AutoDiscardable) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());

  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  EXPECT_CALL(observer_, OnAutoDiscardableStateChange(web_contents_, false));
  tab_lifecycle_unit.SetAutoDiscardable(false);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_FALSE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit,
      DecisionFailureReason::LIVE_STATE_EXTENSION_DISALLOWED);

  EXPECT_CALL(observer_, OnAutoDiscardableStateChange(web_contents_, true));
  tab_lifecycle_unit.SetAutoDiscardable(true);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardCrashed) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());

  web_contents_->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

#if !defined(OS_CHROMEOS)
TEST_F(TabLifecycleUnitTest, CannotDiscardActive) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());

  tab_strip_model_->ActivateTabAt(0);

  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_VISIBLE);
}

TEST_F(TabLifecycleUnitTest, UrgentDiscardProtections) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Initial external and proactive discarding are fine, but urgent discarding
  // is blocked because the tab is too recent.
  ExpectCanDiscardTrue(&tab_lifecycle_unit,
                       LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardTrue(&tab_lifecycle_unit,
                       LifecycleUnitDiscardReason::PROACTIVE);
  ExpectCanDiscardFalseTrivial(&tab_lifecycle_unit,
                               LifecycleUnitDiscardReason::URGENT);

  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The tab should now be discardable for all reasons.
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  // Mark the tab as having been discarded.
  tab_lifecycle_unit.SetDiscardCountForTesting(1);

  // Advance time enough that the time protection no longer applies. The tab
  // should still not be urgent discardable at this point, because it has
  // already been discarded at least once.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // Proactive and external discarding should be fine, but not urgent.
  ExpectCanDiscardTrue(&tab_lifecycle_unit,
                       LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardTrue(&tab_lifecycle_unit,
                       LifecycleUnitDiscardReason::PROACTIVE);
  ExpectCanDiscardFalseTrivial(&tab_lifecycle_unit,
                               LifecycleUnitDiscardReason::URGENT);

  // The tab should be discardable a second time when the memory limit
  // enterprise policy is set.
  GetTabLifecycleUnitSource()->SetMemoryLimitEnterprisePolicyFlag(true);
  ExpectCanDiscardTrue(&tab_lifecycle_unit, LifecycleUnitDiscardReason::URGENT);
}
#endif  // !defined(OS_CHROMEOS)

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
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  blink::MediaStreamDevices video_devices{blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_media_device",
      "fake_media_device")};
  std::unique_ptr<content::MediaStreamUI> ui =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          ->RegisterMediaStream(web_contents_, video_devices);
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback());
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_CAPTURING);

  ui.reset();
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardDesktopCapture) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  blink::MediaStreamDevices desktop_capture_devices{blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      "fake_media_device", "fake_media_device")};
  std::unique_ptr<content::MediaStreamUI> ui =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          ->RegisterMediaStream(web_contents_, desktop_capture_devices);
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback());
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
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  // Cannot discard when the "recently audible" bit is set.
  tab_lifecycle_unit.SetRecentlyAudible(true);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit is still set. The tab cannot be discarded.
  test_clock_.Advance(kTabAudioProtectionTime);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit was unset less than
  // kTabAudioProtectionTime ago. The tab cannot be discarded.
  tab_lifecycle_unit.SetRecentlyAudible(false);
  test_clock_.Advance(kShortDelay);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit was unset kTabAudioProtectionTime ago. The tab
  // can be discarded.
  test_clock_.Advance(kTabAudioProtectionTime - kShortDelay);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  // Calling SetRecentlyAudible(false) again does not change the fact that the
  // tab can be discarded.
  tab_lifecycle_unit.SetRecentlyAudible(false);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotFreezeOrDiscardWebUsbConnectionsOpen) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  // Connect with the FakeUsbDeviceManager.
  device::FakeUsbDeviceManager device_manager;
  mojo::PendingRemote<device::mojom::UsbDeviceManager> pending_device_manager;
  device_manager.AddReceiver(
      pending_device_manager.InitWithNewPipeAndPassReceiver());
  UsbChooserContextFactory::GetForProfile(profile())
      ->SetDeviceManagerForTesting(std::move(pending_device_manager));

  UsbTabHelper* usb_tab_helper =
      UsbTabHelper::GetOrCreateForWebContents(web_contents_);
  mojo::Remote<blink::mojom::WebUsbService> web_usb_service;
  usb_tab_helper->CreateWebUsbService(
      web_contents_->GetMainFrame(),
      web_usb_service.BindNewPipeAndPassReceiver());

  // Page could be intending to use the WebUSB API, but there's no connection
  // open yet, so it can still be discarded/frozen.
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_TRUE(decision_details.IsPositive());

  // Open a USB connection. Shouldn't be freezable/discardable anymore.
  usb_tab_helper->IncrementConnectionCount(web_contents_->GetMainFrame());
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_USING_WEB_USB);
  decision_details = DecisionDetails();
  EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::LIVE_STATE_USING_WEB_USB,
            decision_details.FailureReason());

  // Close the USB connection. Should be freezable/discardable again.
  usb_tab_helper->DecrementConnectionCount(web_contents_->GetMainFrame());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
  decision_details = DecisionDetails();
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_TRUE(decision_details.IsPositive());
}

TEST_F(TabLifecycleUnitTest, CanDiscardNeverAudibleTab) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
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
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  content::WebContentsTester::For(web_contents_)
      ->SetMainFrameMimeType("application/pdf");
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_IS_PDF);
}

TEST_F(TabLifecycleUnitTest, CannotProactivelyDiscardTabWithAudioHeuristic) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_AUDIO,
      &SiteCharacteristicsDataWriter::NotifyUsesAudioInBackground);
}

TEST_F(TabLifecycleUnitTest, CannotProactivelyDiscardTabWithFaviconHeuristic) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_FAVICON,
      &SiteCharacteristicsDataWriter::NotifyUpdatesFaviconInBackground);
}

TEST_F(TabLifecycleUnitTest,
       CannotProactivelyDiscardTabWithNotificationsHeuristic) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_NOTIFICATIONS,
      &SiteCharacteristicsDataWriter::NotifyUsesNotificationsInBackground);
}

TEST_F(TabLifecycleUnitTest, CannotProactivelyDiscardTabWithTitleHeuristic) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_TITLE,
      &SiteCharacteristicsDataWriter::NotifyUpdatesTitleInBackground);
}

TEST_F(TabLifecycleUnitTest,
       CannotProactivelyDiscardTabIfInsufficientObservation) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_INSUFFICIENT_OBSERVATION, nullptr);
}

TEST_F(TabLifecycleUnitTest, CannotFreezeAFrozenTab) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  {
    DecisionDetails decision_details;
    EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
  }
  EXPECT_CALL(observer_, OnFrozenStateChange(web_contents_, true));
  tab_lifecycle_unit.Freeze();
  ::testing::Mock::VerifyAndClear(&observer_);
  {
    DecisionDetails decision_details;
    EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
  }
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

TEST_F(TabLifecycleUnitTest, CannotFreezeOrDiscardIfSharingBrowsingInstance) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);

  // Creates a second WebContents that use the same SiteInstance.
  auto* site_instance = web_contents_->GetSiteInstance();
  std::unique_ptr<content::WebContents> contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        site_instance);
  // Navigate this second WebContents to another URL, after this these 2
  // WebContents will use separate SiteInstances owned by the same
  // BrowsingInstance.
  contents->GetController().LoadURL(GURL("http://another-url.com/"),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_NE(web_contents_->GetSiteInstance(), contents->GetSiteInstance());
  EXPECT_EQ(2U,
            web_contents_->GetSiteInstance()->GetRelatedActiveContentsCount());

  DecisionDetails decision_details;
  EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::LIVE_STATE_SHARING_BROWSING_INSTANCE,
            decision_details.FailureReason());

  decision_details.Clear();
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(
      LifecycleUnitDiscardReason::PROACTIVE, &decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::LIVE_STATE_SHARING_BROWSING_INSTANCE,
            decision_details.FailureReason());

  {
    GetMutableStaticProactiveTabFreezeAndDiscardParamsForTesting()
        ->should_protect_tabs_sharing_browsing_instance = false;
    decision_details.Clear();
    EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
    EXPECT_TRUE(decision_details.IsPositive());

    decision_details.Clear();

    EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(
        LifecycleUnitDiscardReason::PROACTIVE, &decision_details));
    EXPECT_TRUE(decision_details.IsPositive());
  }
}

TEST_F(TabLifecycleUnitTest, CannotDiscardIfEnterpriseOptOutUsed) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  {
    ScopedEnterpriseOptOut enterprise_opt_out;
    DecisionDetails decision_details;
    EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(
        LifecycleUnitDiscardReason::PROACTIVE, &decision_details));
    EXPECT_EQ(DecisionFailureReason::LIFECYCLES_ENTERPRISE_POLICY_OPT_OUT,
              decision_details.FailureReason());
    EXPECT_FALSE(decision_details.IsPositive());

    decision_details.Clear();
    EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(
        LifecycleUnitDiscardReason::URGENT, &decision_details));
    EXPECT_TRUE(decision_details.IsPositive());
  }

  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotFreezeIfEnterpriseOptOutUsed) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);

  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_TRUE(decision_details.IsPositive());

  {
    ScopedEnterpriseOptOut enterprise_opt_out;
    decision_details.Clear();
    EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
    EXPECT_FALSE(decision_details.IsPositive());
    EXPECT_EQ(DecisionFailureReason::LIFECYCLES_ENTERPRISE_POLICY_OPT_OUT,
              decision_details.FailureReason());
  }

  decision_details.Clear();
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_TRUE(decision_details.IsPositive());
}

TEST_F(TabLifecycleUnitTest, ReloadingAFrozenTabUnfreezeIt) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));

  EXPECT_CALL(observer_, OnFrozenStateChange(web_contents_, true));
  tab_lifecycle_unit.Freeze();
  ::testing::Mock::VerifyAndClear(&observer_);

  web_contents_->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_NE(LifecycleUnitState::FROZEN, tab_lifecycle_unit.GetState());
}

TEST_F(TabLifecycleUnitTest, DisableHeuristicsFlag) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);

  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_TRUE(decision_details.IsPositive());
  decision_details.Clear();

  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(
      LifecycleUnitDiscardReason::PROACTIVE, &decision_details));
  EXPECT_TRUE(decision_details.IsPositive());
  decision_details.Clear();

  // Use one of the heuristics on the tab to prevent it from being frozen or
  // discarded.
  ScopedEnterpriseOptOut enterprise_opt_out;

  EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  decision_details.Clear();

  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(
      LifecycleUnitDiscardReason::PROACTIVE, &decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  decision_details.Clear();

  // Disable the heuristics and check that the tab can now be safely discarded.
  base::AutoReset<bool> disable_heuristics_protections(
      &GetMutableStaticProactiveTabFreezeAndDiscardParamsForTesting()
           ->disable_heuristics_protections,
      true);

  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_TRUE(decision_details.IsPositive());
  decision_details.Clear();

  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(
      LifecycleUnitDiscardReason::PROACTIVE, &decision_details));
  EXPECT_TRUE(decision_details.IsPositive());
  decision_details.Clear();
}

TEST_F(TabLifecycleUnitTest, CannotFreezeOrDiscardIfConnectedToBluetooth) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  content::WebContentsTester::For(web_contents_)
      ->SetIsConnectedToBluetoothDevice(true);

  DecisionDetails decision_details;
  EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::LIVE_STATE_USING_BLUETOOTH,
            decision_details.FailureReason());

  decision_details.Clear();
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(
      LifecycleUnitDiscardReason::PROACTIVE, &decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::LIVE_STATE_USING_BLUETOOTH,
            decision_details.FailureReason());

  content::WebContentsTester::For(web_contents_)
      ->SetIsConnectedToBluetoothDevice(false);
}

TEST_F(TabLifecycleUnitTest, CannotFreezeIfHoldingWebLock) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));

  tab_lifecycle_unit.SetIsHoldingWebLock(true);

  // A tab holding a WebLock can't be frozen.
  decision_details.Clear();
  EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::LIVE_STATE_USING_WEBLOCK,
            decision_details.FailureReason());

  // This tab should still be discardable.
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  tab_lifecycle_unit.SetIsHoldingWebLock(false);
}

TEST_F(TabLifecycleUnitTest, CannotFreezeIfHoldingIndexedDBLock) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));

  tab_lifecycle_unit.SetIsHoldingIndexedDBLock(true);

  // A tab holding an IndexedDB Lock can't be frozen.
  decision_details.Clear();
  EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::LIVE_STATE_USING_INDEXEDDB_LOCK,
            decision_details.FailureReason());

  // This tab should still be discardable.
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  tab_lifecycle_unit.SetIsHoldingIndexedDBLock(false);
}

TEST_F(TabLifecycleUnitTest, TabUnfreezeOnWebLockAcquisition) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));

  // Freeze the tab.
  EXPECT_CALL(observer_, OnFrozenStateChange(web_contents_, true));
  tab_lifecycle_unit.Freeze();
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(LifecycleUnitState::PENDING_FREEZE, tab_lifecycle_unit.GetState());

  // Indicates that the tab has acquired a WebLock, this should unfreeze it.
  EXPECT_CALL(observer_, OnFrozenStateChange(web_contents_, false));
  tab_lifecycle_unit.SetIsHoldingWebLock(true);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(LifecycleUnitState::PENDING_UNFREEZE,
            tab_lifecycle_unit.GetState());

  tab_lifecycle_unit.SetIsHoldingWebLock(false);
}

TEST_F(TabLifecycleUnitTest, TabUnfreezeOnIndexedDBLockAcquisition) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));

  // Freeze the tab.
  EXPECT_CALL(observer_, OnFrozenStateChange(web_contents_, true));
  tab_lifecycle_unit.Freeze();
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(LifecycleUnitState::PENDING_FREEZE, tab_lifecycle_unit.GetState());

  // Indicates that the tab has acquired a WebLock, this should unfreeze it.
  EXPECT_CALL(observer_, OnFrozenStateChange(web_contents_, false));
  tab_lifecycle_unit.SetIsHoldingIndexedDBLock(true);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(LifecycleUnitState::PENDING_UNFREEZE,
            tab_lifecycle_unit.GetState());

  tab_lifecycle_unit.SetIsHoldingIndexedDBLock(false);
}

TEST_F(TabLifecycleUnitTest, TabUnfreezeOnOriginTrialOptOut) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), &observers_,
                                      usage_clock_.get(), web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  // Advance time enough that the tab is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  DecisionDetails decision_details;
  EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));

  // Freeze the tab.
  EXPECT_CALL(observer_, OnFrozenStateChange(web_contents_, true));
  tab_lifecycle_unit.Freeze();
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(LifecycleUnitState::PENDING_FREEZE, tab_lifecycle_unit.GetState());

  // Indicates that the tab opted out from the Freeze policy via Origin Trial,
  // this should unfreeze it.
  EXPECT_CALL(observer_, OnFrozenStateChange(web_contents_, false));
  tab_lifecycle_unit.UpdateOriginTrialFreezePolicy(
      performance_manager::mojom::InterventionPolicy::kOptOut);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(LifecycleUnitState::PENDING_UNFREEZE,
            tab_lifecycle_unit.GetState());
}

}  // namespace resource_coordinator
