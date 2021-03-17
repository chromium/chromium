// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/auto_fetch_page_load_watcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
using ::offline_pages::auto_fetch_internal::AndroidTabFinder;
using ::offline_pages::auto_fetch_internal::InternalImpl;
using ::offline_pages::auto_fetch_internal::RequestInfo;
using ::offline_pages::auto_fetch_internal::TabInfo;
using ::testing::_;

const int kDefaultTabId = 123;
const base::Time kEpoch = base::Time::FromDoubleT(1.0e6);

GURL TestURL() {
  return GURL("http://www.url.com");
}
GURL OtherURL() {
  return GURL("http://other.com");
}

RequestInfo TestInfo(
    int64_t id,
    const GURL& url = TestURL(),
    SavePageRequest::AutoFetchNotificationState notification_state =
        SavePageRequest::AutoFetchNotificationState::kUnknown) {
  RequestInfo info;
  info.request_id = id;
  info.url = url;
  info.notification_state = notification_state;
  info.metadata.android_tab_id = kDefaultTabId;
  return info;
}

class MockAutoFetchNotifier : public AutoFetchNotifier {
 public:
  MOCK_METHOD1(NotifyInProgress, void(int in_flight_count));
  MOCK_METHOD1(InProgressCountChanged, void(int in_flight_count));
};

class FakeInternalImplDelegate : public InternalImpl::Delegate {
 public:
  ~FakeInternalImplDelegate() override {}
  void SetNotificationStateToShown(int64_t request_id) override {
    set_notification_state_requests.push_back(request_id);
  }
  void RemoveRequests(const std::vector<int64_t>& request_ids) override {
    removed_requests.insert(removed_requests.end(), request_ids.begin(),
                            request_ids.end());
  }

  std::vector<int64_t> removed_requests;
  std::vector<int64_t> set_notification_state_requests;
};

// Note that TabAndroid doesn't work in unit tests, so this stubs out access to
// tab information.
class StubTabFinder : public AutoFetchPageLoadWatcher::AndroidTabFinder {
 public:
  ~StubTabFinder() override {}

  // AutoFetchPageLoadWatcher::AndroidTabFinder.
  std::map<int, TabInfo> FindAndroidTabs(
      std::vector<int> android_tab_ids) override {
    std::map<int, TabInfo> result;
    for (const int tab_id : android_tab_ids) {
      if (tabs_.count(tab_id)) {
        result[tab_id] = TabInfo{tab_id, tabs_[tab_id]};
      }
    }
    return result;
  }

  base::Optional<TabInfo> FindNavigationTab(
      content::WebContents* web_contents) override {
    if (!tabs_.count(current_tab_id_))
      return base::nullopt;
    return TabInfo{current_tab_id_, tabs_[current_tab_id_]};
  }

  // Methods to alter stub behavior.
  void SetTabs(std::map<int, GURL> tab_urls) { tabs_ = std::move(tab_urls); }
  void SetCurrentTabId(int tab_id) { current_tab_id_ = tab_id; }

 private:
  std::map<int, GURL> tabs_;
  // ID of the current tab to return from FindNavigationTab.
  int current_tab_id_ = kDefaultTabId;
};

// Note: This unittest doesn't attempt to directly test
// |AutoFetchPageLoadWatcher| because the set-up is difficult, especially for
// testing various call orderings. Additionally, TabAndroid can't be tested
// in a unit test. Instead, see OfflinePageAutoFetchTest.java for coverage.
class AutoFetchInternalImplTest : public testing::Test {
 public:
 protected:
  // A WebContents* is needed for some |InternalImpl| methods, but nullptr is
  // sufficient because |StubTabFinder| doesn't inspect the value.
  content::WebContents* const web_contents_ = nullptr;
  MockAutoFetchNotifier notifier_;
  FakeInternalImplDelegate delegate_;
  StubTabFinder* tab_finder_ = new StubTabFinder;
  InternalImpl impl_{&notifier_, &delegate_, base::WrapUnique(tab_finder_)};
};

TEST_F(AutoFetchInternalImplTest, NoInitialization) {
  // Just verify there is no crash.
  impl_.SuccessfulPageNavigation(TestURL());
  impl_.NavigationFrom(TestURL(), web_contents_);
}

TEST_F(AutoFetchInternalImplTest, RemoveRequestOnSuccessfulNavigation) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});
  impl_.SuccessfulPageNavigation(TestURL());

  EXPECT_EQ(std::vector<int64_t>({1}), delegate_.removed_requests);
}

TEST_F(AutoFetchInternalImplTest,
       RemoveRequestOnSuccessfulNavigationBeforeInitialization) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.SuccessfulPageNavigation(TestURL());
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});

  EXPECT_EQ(std::vector<int64_t>({1}), delegate_.removed_requests);
}

TEST_F(AutoFetchInternalImplTest,
       RemoveRequestOnSuccessfulNavigationBeforeTabModelReady) {
  impl_.SuccessfulPageNavigation(TestURL());
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.TabModelReady();
  EXPECT_EQ(std::vector<int64_t>({1}), delegate_.removed_requests);
}

// Successful navigation to a URL that we are not auto fetching should not
// remove any requests from the list of in progress auto fetches.
TEST_F(AutoFetchInternalImplTest, SuccessfulNavigationToOtherURL) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});
  impl_.SuccessfulPageNavigation(OtherURL());

  EXPECT_EQ(std::vector<int64_t>(), delegate_.removed_requests);
}

TEST_F(AutoFetchInternalImplTest,
       SuccessfulNavigationToOtherURLBeforeInitialization) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.SuccessfulPageNavigation(OtherURL());
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});

  EXPECT_EQ(std::vector<int64_t>(), delegate_.removed_requests);
}

TEST_F(AutoFetchInternalImplTest, NavigatingFromNotifies) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});

  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, OtherURL()}});
  impl_.NavigationFrom(TestURL(), web_contents_);
  EXPECT_EQ(std::vector<int64_t>({1}),
            delegate_.set_notification_state_requests);

  EXPECT_CALL(notifier_, NotifyInProgress(1));
  impl_.SetNotificationStateComplete(1, true);
}

TEST_F(AutoFetchInternalImplTest, CompletedRequestUpdatesInProgressCount) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, OtherURL()}});
  impl_.NavigationFrom(TestURL(), web_contents_);

  EXPECT_CALL(notifier_, InProgressCountChanged(0));
  impl_.RequestRemoved(TestInfo(1, TestURL()));
}

TEST_F(AutoFetchInternalImplTest, NavigatingFromNotifiesTwoRequests) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.RequestListInitialized(
      {TestInfo(1, TestURL()), TestInfo(2, TestURL())});

  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, OtherURL()}});
  impl_.NavigationFrom(TestURL(), web_contents_);
  EXPECT_EQ(std::vector<int64_t>({1, 2}),
            delegate_.set_notification_state_requests);

  EXPECT_CALL(notifier_, NotifyInProgress(2)).Times(2);
  impl_.SetNotificationStateComplete(1, true);
  impl_.SetNotificationStateComplete(2, true);
}

TEST_F(AutoFetchInternalImplTest, NavigatingFromBeforeInitialization) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, OtherURL()}});
  impl_.RequestListInitialized(
      {TestInfo(1, TestURL()), TestInfo(2, TestURL())});

  EXPECT_EQ(std::vector<int64_t>({1, 2}),
            delegate_.set_notification_state_requests);

  EXPECT_CALL(notifier_, NotifyInProgress(2)).Times(2);
  impl_.SetNotificationStateComplete(1, true);
  impl_.SetNotificationStateComplete(2, true);
}

TEST_F(AutoFetchInternalImplTest, TabCloseNotifies) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});

  impl_.TabClosed(kDefaultTabId);

  EXPECT_EQ(std::vector<int64_t>({1}),
            delegate_.set_notification_state_requests);

  EXPECT_CALL(notifier_, NotifyInProgress(1)).Times(1);
  impl_.SetNotificationStateComplete(1, true);
}

TEST_F(AutoFetchInternalImplTest, RequestRemovedWhileSettingNotificationState) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, OtherURL()}});
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});

  EXPECT_EQ(std::vector<int64_t>({1}),
            delegate_.set_notification_state_requests);

  impl_.RequestRemoved({TestInfo(1, TestURL())});

  EXPECT_CALL(notifier_, NotifyInProgress(1)).Times(0);
  impl_.SetNotificationStateComplete(1, false);
}

TEST_F(AutoFetchInternalImplTest, OtherTabClosedDoesNotNotify) {
  impl_.TabModelReady();
  tab_finder_->SetTabs(std::map<int, GURL>{{kDefaultTabId, TestURL()}});
  impl_.RequestListInitialized(
      std::vector<RequestInfo>{TestInfo(1, TestURL())});

  impl_.TabClosed(kDefaultTabId + 1);

  EXPECT_EQ(std::vector<int64_t>(), delegate_.set_notification_state_requests);
}

}  // namespace
}  // namespace offline_pages
