// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_store.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/favicon_url.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

using LoadingState = TabLoadTracker::LoadingState;

// A mock implementation of a SiteCharacteristicsDataWriter.
class LenientMockDataWriter : public SiteCharacteristicsDataWriter {
 public:
  explicit LenientMockDataWriter(const url::Origin& origin) : origin_(origin) {}
  ~LenientMockDataWriter() override { OnDestroy(); }

  // Mock function to be notified when this object gets destroyed.
  MOCK_METHOD0(OnDestroy, void());

  MOCK_METHOD0(NotifySiteLoaded, void());
  MOCK_METHOD0(NotifySiteUnloaded, void());
  MOCK_METHOD1(NotifySiteVisibilityChanged,
               void(performance_manager::TabVisibility));
  MOCK_METHOD0(NotifyUpdatesFaviconInBackground, void());
  MOCK_METHOD0(NotifyUpdatesTitleInBackground, void());
  MOCK_METHOD0(NotifyUsesAudioInBackground, void());
  MOCK_METHOD0(NotifyUsesNotificationsInBackground, void());
  MOCK_METHOD3(NotifyLoadTimePerformanceMeasurement,
               void(base::TimeDelta, base::TimeDelta, uint64_t));

  const url::Origin& Origin() const { return origin_; }

 private:
  url::Origin origin_;

  DISALLOW_COPY_AND_ASSIGN(LenientMockDataWriter);
};
using MockDataWriter = ::testing::StrictMock<LenientMockDataWriter>;

// A data store that serves MockDataWriter objects.
class MockDataStore : public SiteCharacteristicsDataStore {
 public:
  MockDataStore() = default;
  ~MockDataStore() override {}

  // SiteCharacteristicsDataStore:
  std::unique_ptr<SiteCharacteristicsDataReader> GetReaderForOrigin(
      const url::Origin& origin) override {
    return nullptr;
  }
  std::unique_ptr<SiteCharacteristicsDataWriter> GetWriterForOrigin(
      const url::Origin& origin,
      performance_manager::TabVisibility tab_visibility) override {
    return std::make_unique<MockDataWriter>(origin);
  }
  bool IsRecordingForTesting() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDataStore);
};

std::unique_ptr<KeyedService> BuildMockDataStoreForContext(
    content::BrowserContext* browser_context) {
  return std::make_unique<MockDataStore>();
}

class LocalSiteCharacteristicsWebContentsObserverTest
    : public testing::ChromeTestHarnessWithLocalDB {
 protected:
  LocalSiteCharacteristicsWebContentsObserverTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {}
  ~LocalSiteCharacteristicsWebContentsObserverTest() override = default;

  void SetUp() override {
    testing::ChromeTestHarnessWithLocalDB::SetUp();

    test_clock().Advance(base::TimeDelta::FromSeconds(1));
    // Set the testing factory for the test browser context.
    LocalSiteCharacteristicsDataStoreFactory::GetInstance()->SetTestingFactory(
        browser_context(), base::BindRepeating(&BuildMockDataStoreForContext));

    TabLoadTracker::Get()->StartTracking(web_contents());
    observer_ = std::make_unique<LocalSiteCharacteristicsWebContentsObserver>(
        web_contents());
  }

  void TearDown() override {
    TabLoadTracker::Get()->StopTracking(web_contents());
    DeleteContents();
    observer_.reset();
    testing::ChromeTestHarnessWithLocalDB::TearDown();
  }

  MockDataWriter* NavigateAndReturnMockWriter(const GURL& url) {
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    EXPECT_TRUE(web_contents_tester);
    web_contents_tester->NavigateAndCommit(url);
    return static_cast<MockDataWriter*>(observer_->GetWriterForTesting());
  }

  const GURL kTestUrl1 = GURL("http://foo.com");
  const GURL kTestUrl2 = GURL("http://bar.com");

  LocalSiteCharacteristicsWebContentsObserver* observer() {
    return observer_.get();
  }

  base::SimpleTestTickClock& test_clock() { return test_clock_; }

 private:
  std::unique_ptr<LocalSiteCharacteristicsWebContentsObserver> observer_;
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsWebContentsObserverTest);
};

TEST_F(LocalSiteCharacteristicsWebContentsObserverTest,
       NavigationEventsBasicTests) {
  // Send a navigation event with the |committed| bit set and make sure that a
  // writer has been created for this origin.

  EXPECT_FALSE(observer()->GetWriterForTesting());
  MockDataWriter* mock_writer = NavigateAndReturnMockWriter(kTestUrl1);
  EXPECT_TRUE(mock_writer);

  auto writer_origin = observer()->writer_origin();

  EXPECT_EQ(url::Origin::Create(kTestUrl1), writer_origin);

  // A navigation to the same origin shouldn't cause caused this writer to get
  // destroyed.
  mock_writer = NavigateAndReturnMockWriter(kTestUrl1);
  ::testing::Mock::VerifyAndClear(mock_writer);

  // Navigate to a different origin but don't set the |committed| bit, this
  // shouldn't affect the writer.
  content::MockNavigationHandle navigation_handle(
      kTestUrl2, web_contents()->GetMainFrame());
  observer()->DidFinishNavigation(&navigation_handle);
  ::testing::Mock::VerifyAndClear(mock_writer);

  // Set the |committed| bit and ensure that the navigation event cause the
  // destruction of the writer.
  EXPECT_CALL(*mock_writer, OnDestroy());
  mock_writer = NavigateAndReturnMockWriter(kTestUrl2);
  ::testing::Mock::VerifyAndClear(mock_writer);

  EXPECT_FALSE(writer_origin == observer()->writer_origin());
  writer_origin = observer()->writer_origin();

  EXPECT_EQ(url::Origin::Create(kTestUrl2), mock_writer->Origin());

  EXPECT_CALL(*mock_writer, OnDestroy());
}

// Test that the feature usage events get forwarded to the writer when the tab
// is in background.
TEST_F(LocalSiteCharacteristicsWebContentsObserverTest,
       FeatureEventsGetForwardedWhenInBackground) {
  MockDataWriter* mock_writer = NavigateAndReturnMockWriter(kTestUrl1);

  // Send dummy events to simulate the initial title/favicon update (as these
  // are ignored).
  observer()->DidUpdateFaviconURL({});
  observer()->TitleWasSet(nullptr);

  EXPECT_CALL(*mock_writer, NotifySiteLoaded());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents(),
                                                   LoadingState::LOADED);
  EXPECT_CALL(*mock_writer,
              NotifySiteVisibilityChanged(
                  performance_manager::TabVisibility::kForeground));
  EXPECT_CALL(*mock_writer, NotifySiteLoaded());
  web_contents()->WasShown();
  observer()->OnLoadingStateChange(web_contents(),
                                   TabLoadTracker::LoadingState::LOADING,
                                   TabLoadTracker::LoadingState::LOADED);
  ::testing::Mock::VerifyAndClear(mock_writer);

  // Ensure that no event gets forwarded if the tab is not in background.
  observer()->DidUpdateFaviconURL({});
  ::testing::Mock::VerifyAndClear(mock_writer);
  observer()->TitleWasSet(nullptr);
  ::testing::Mock::VerifyAndClear(mock_writer);
  observer()->OnAudioStateChanged(true);
  ::testing::Mock::VerifyAndClear(mock_writer);

  observer()->OnNonPersistentNotificationCreated();
  ::testing::Mock::VerifyAndClear(mock_writer);

  EXPECT_CALL(*mock_writer,
              NotifySiteVisibilityChanged(
                  performance_manager::TabVisibility::kBackground));
  web_contents()->WasHidden();
  ::testing::Mock::VerifyAndClear(mock_writer);

  auto params = GetStaticSiteCharacteristicsDatabaseParams();
  // Title and Favicon should be ignored during the post-loading grace period.
  observer()->DidUpdateFaviconURL({});
  observer()->TitleWasSet(nullptr);
  ::testing::Mock::VerifyAndClear(mock_writer);

  test_clock().Advance(params.title_or_favicon_change_post_load_grace_period);

  EXPECT_CALL(*mock_writer, NotifyUpdatesFaviconInBackground());
  observer()->DidUpdateFaviconURL({});
  ::testing::Mock::VerifyAndClear(mock_writer);
  EXPECT_CALL(*mock_writer, NotifyUpdatesTitleInBackground());
  observer()->TitleWasSet(nullptr);
  ::testing::Mock::VerifyAndClear(mock_writer);

  // Brievly switch the tab to foreground to reset the last backgrounded time.
  EXPECT_CALL(*mock_writer,
              NotifySiteVisibilityChanged(
                  performance_manager::TabVisibility::kForeground));
  EXPECT_CALL(*mock_writer,
              NotifySiteVisibilityChanged(
                  performance_manager::TabVisibility::kBackground));
  web_contents()->WasShown();
  web_contents()->WasHidden();
  ::testing::Mock::VerifyAndClear(mock_writer);

  // Events should be ignored during the post-background grace period.
  observer()->OnAudioStateChanged(true);
  observer()->DidUpdateFaviconURL({});
  observer()->TitleWasSet(nullptr);
  observer()->OnNonPersistentNotificationCreated();
  ::testing::Mock::VerifyAndClear(mock_writer);

  test_clock().Advance(params.feature_usage_post_background_grace_period);
  EXPECT_CALL(*mock_writer, NotifyUsesAudioInBackground());
  EXPECT_CALL(*mock_writer, NotifyUpdatesFaviconInBackground());
  EXPECT_CALL(*mock_writer, NotifyUpdatesTitleInBackground());
  EXPECT_CALL(*mock_writer, NotifyUsesNotificationsInBackground());
  observer()->OnAudioStateChanged(true);
  observer()->DidUpdateFaviconURL({});
  observer()->TitleWasSet(nullptr);
  observer()->OnNonPersistentNotificationCreated();
  ::testing::Mock::VerifyAndClear(mock_writer);

  EXPECT_CALL(*mock_writer, OnDestroy());
}

TEST_F(LocalSiteCharacteristicsWebContentsObserverTest,
       FeatureEventsIgnoredWhenLoadingInBackground) {
  MockDataWriter* mock_writer = NavigateAndReturnMockWriter(kTestUrl1);

  // Send dummy events to simulate the initial title/favicon update (as these
  // are ignored).
  observer()->DidUpdateFaviconURL({});
  observer()->TitleWasSet(nullptr);

  TabLoadTracker::Get()->TransitionStateForTesting(web_contents(),
                                                   LoadingState::LOADING);

  EXPECT_CALL(*mock_writer,
              NotifySiteVisibilityChanged(
                  performance_manager::TabVisibility::kBackground));
  web_contents()->WasHidden();
  ::testing::Mock::VerifyAndClear(mock_writer);

  observer()->DidUpdateFaviconURL({});
  ::testing::Mock::VerifyAndClear(mock_writer);
  observer()->TitleWasSet(nullptr);
  ::testing::Mock::VerifyAndClear(mock_writer);
  observer()->OnAudioStateChanged(true);
  ::testing::Mock::VerifyAndClear(mock_writer);

  EXPECT_CALL(*mock_writer, OnDestroy());
}

TEST_F(LocalSiteCharacteristicsWebContentsObserverTest,
       NotificationEventsWhenLoadingInBackground) {
  MockDataWriter* mock_writer = NavigateAndReturnMockWriter(kTestUrl1);

  TabLoadTracker::Get()->TransitionStateForTesting(web_contents(),
                                                   LoadingState::LOADING);

  EXPECT_CALL(*mock_writer,
              NotifySiteVisibilityChanged(
                  performance_manager::TabVisibility::kBackground));
  web_contents()->WasHidden();
  ::testing::Mock::VerifyAndClear(mock_writer);

  // Events should be ignored during the post-background grace period.
  observer()->OnNonPersistentNotificationCreated();
  ::testing::Mock::VerifyAndClear(mock_writer);
  test_clock().Advance(GetStaticSiteCharacteristicsDatabaseParams()
                           .feature_usage_post_background_grace_period);

  EXPECT_CALL(*mock_writer, NotifyUsesNotificationsInBackground());
  observer()->OnNonPersistentNotificationCreated();
  ::testing::Mock::VerifyAndClear(mock_writer);

  EXPECT_CALL(*mock_writer, OnDestroy());
}

TEST_F(LocalSiteCharacteristicsWebContentsObserverTest, VisibilityEvent) {
  MockDataWriter* mock_writer = NavigateAndReturnMockWriter(kTestUrl1);

  // Test that the visibility events get forwarded to the writer.

  EXPECT_CALL(*mock_writer,
              NotifySiteVisibilityChanged(
                  performance_manager::TabVisibility::kBackground))
      .Times(2);
  observer()->OnVisibilityChanged(content::Visibility::OCCLUDED);
  observer()->OnVisibilityChanged(content::Visibility::HIDDEN);
  ::testing::Mock::VerifyAndClear(mock_writer);

  EXPECT_CALL(*mock_writer,
              NotifySiteVisibilityChanged(
                  performance_manager::TabVisibility::kForeground));
  observer()->OnVisibilityChanged(content::Visibility::VISIBLE);
  ::testing::Mock::VerifyAndClear(mock_writer);

  EXPECT_CALL(*mock_writer, OnDestroy());
}

TEST_F(LocalSiteCharacteristicsWebContentsObserverTest, LoadEvent) {
  MockDataWriter* mock_writer = NavigateAndReturnMockWriter(kTestUrl1);

  // Test that the load/unload events get forwarded to the writer.

  EXPECT_CALL(*mock_writer, NotifySiteLoaded());
  observer()->OnLoadingStateChange(web_contents(),
                                   TabLoadTracker::LoadingState::LOADING,
                                   TabLoadTracker::LoadingState::LOADED);
  ::testing::Mock::VerifyAndClear(mock_writer);

  EXPECT_CALL(*mock_writer, NotifySiteUnloaded());
  observer()->OnLoadingStateChange(web_contents(),
                                   TabLoadTracker::LoadingState::LOADED,
                                   TabLoadTracker::LoadingState::LOADING);
  ::testing::Mock::VerifyAndClear(mock_writer);

  observer()->OnLoadingStateChange(web_contents(),
                                   TabLoadTracker::LoadingState::LOADING,
                                   TabLoadTracker::LoadingState::UNLOADED);
  ::testing::Mock::VerifyAndClear(mock_writer);

  // Ensure that a transition from UNLOADED to LOADING doesn't cause any call to
  // NotifySiteUnloaded.
  observer()->OnLoadingStateChange(web_contents(),
                                   TabLoadTracker::LoadingState::LOADING,
                                   TabLoadTracker::LoadingState::UNLOADED);

  EXPECT_CALL(*mock_writer, OnDestroy());
}

}  // namespace resource_coordinator
