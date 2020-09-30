// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/phonehub/browser_tabs_metadata_fetcher_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace chromeos {
namespace phonehub {
namespace {

using testing::_;

const base::Time kTimeA = base::Time::FromDoubleT(1);
const base::Time kTimeB = base::Time::FromDoubleT(2);
const base::Time kTimeC = base::Time::FromDoubleT(3);
const base::Time kTimeD = base::Time::FromDoubleT(4);
const base::Time kTimeE = base::Time::FromDoubleT(5);

gfx::Image GetDummyImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorBLUE);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

favicon_base::FaviconImageResult GetDummyFaviconResult() {
  favicon_base::FaviconImageResult result;
  result.icon_url = GURL("http://example.com/favicon.ico");
  result.image = GetDummyImage();
  return result;
}
}  // namespace

class BrowserTabsMetadataFetcherImplTest : public testing::Test {
 public:
  BrowserTabsMetadataFetcherImplTest()
      : browser_tabs_metadata_job_(&favicon_service_),
        synced_session_(std::make_unique<sync_sessions::SyncedSession>()) {}

  BrowserTabsMetadataFetcherImplTest(
      const BrowserTabsMetadataFetcherImplTest&) = delete;
  BrowserTabsMetadataFetcherImplTest& operator=(
      const BrowserTabsMetadataFetcherImplTest&) = delete;
  ~BrowserTabsMetadataFetcherImplTest() override = default;

  using BrowserTabMetadata = BrowserTabsModel::BrowserTabMetadata;

  void OnBrowserTabMetadataFetched(
      base::Optional<std::vector<BrowserTabsModel::BrowserTabMetadata>>
          browser_tab_metadatas) {
    actual_browser_tabs_metadata_ = browser_tab_metadatas;
  }

  void AddTab(sync_sessions::SyncedSessionWindow* synced_session_window,
              const base::string16& title,
              const GURL& url,
              const base::Time& time) {
    auto tab1 = std::make_unique<sessions::SessionTab>();

    tab1->current_navigation_index = 0;
    auto navigation = sessions::SerializedNavigationEntryTestHelper::
        CreateNavigationForTest();

    navigation.set_title(title);
    navigation.set_virtual_url(url);
    navigation.set_timestamp(time);

    tab1->timestamp = time;
    tab1->navigations.push_back(navigation);
    tab1->navigations.back().set_encoded_page_state("");

    synced_session_window->wrapped_window.tabs.push_back(std::move(tab1));
  }

  void AddWindow(std::unique_ptr<sync_sessions::SyncedSessionWindow>
                     synced_session_window) {
    SessionID session_id = SessionID::NewUnique();
    synced_session_->windows.insert(
        std::pair<SessionID,
                  std::unique_ptr<sync_sessions::SyncedSessionWindow>>{
            session_id, std::move(synced_session_window)});
  }

  void ExpectFaviconUrlFetchAttempt(const GURL& url) {
    EXPECT_CALL(favicon_service_, GetFaviconImageForPageURL(url, /*callback=*/_,
                                                            /*tracker=*/_))
        .WillRepeatedly(
            [&](auto, favicon_base::FaviconImageCallback callback, auto) {
              // Randomize the order in which callbacks may return.
              if (std::rand() % 2)
                favicon_service_responses_.emplace_front(std::move(callback));
              else
                favicon_service_responses_.emplace_back(std::move(callback));
              return base::CancelableTaskTracker::kBadTaskId;
            });
  }

  void AttemptFetch() {
    browser_tabs_metadata_job_.Fetch(
        synced_session_.get(),
        base::BindOnce(
            &BrowserTabsMetadataFetcherImplTest::OnBrowserTabMetadataFetched,
            base::Unretained(this)));
  }

  void InvokeNextFaviconCallbacks(size_t num_successful_fetches) {
    for (size_t i = 0; i < num_successful_fetches; i++) {
      std::move(favicon_service_responses_.front())
          .Run(GetDummyFaviconResult());
      favicon_service_responses_.pop_front();
    }
  }

  void CheckIsExpectedMetadata(
      const std::vector<BrowserTabMetadata> browser_tabs_metadata) {
    EXPECT_EQ(browser_tabs_metadata, actual_browser_tabs_metadata_);

    // Check favicons as they are not includes in equality.
    for (size_t i = 0; i < browser_tabs_metadata.size(); i++) {
      EXPECT_TRUE(gfx::test::AreImagesEqual(
          browser_tabs_metadata[i].favicon,
          (*actual_browser_tabs_metadata_)[i].favicon));
    }
  }

  const base::Optional<std::vector<BrowserTabsModel::BrowserTabMetadata>>&
  actual_browser_tabs_metadata() const {
    return actual_browser_tabs_metadata_;
  }

 private:
  testing::NiceMock<favicon::MockFaviconService> favicon_service_;
  BrowserTabsMetadataFetcherImpl browser_tabs_metadata_job_;
  base::Optional<std::vector<BrowserTabsModel::BrowserTabMetadata>>
      actual_browser_tabs_metadata_;

  std::map<SessionID, std::unique_ptr<sync_sessions::SyncedSessionWindow>>
      windows;
  std::unique_ptr<sync_sessions::SyncedSession> synced_session_;
  std::deque<favicon_base::FaviconImageCallback> favicon_service_responses_;
};

TEST_F(BrowserTabsMetadataFetcherImplTest, NewFetchDuringOldFetchInProgress) {
  const base::string16 kTitleA = base::UTF8ToUTF16("A");
  const GURL kUrlA = GURL("http://a.com");

  const base::string16 kTitleB = base::UTF8ToUTF16("B");
  const GURL kUrlB = GURL("http://b.com");

  const base::string16 kTitleC = base::UTF8ToUTF16("C");
  const GURL kUrlC = GURL("http://c.com");

  const base::string16 kTitleD = base::UTF8ToUTF16("D");
  const GURL kUrlD = GURL("http://d.com");

  auto synced_session_window =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  AddTab(synced_session_window.get(), kTitleB, kUrlB, kTimeB);
  AddTab(synced_session_window.get(), kTitleA, kUrlA, kTimeA);
  AddWindow(std::move(synced_session_window));

  ExpectFaviconUrlFetchAttempt(kUrlB);
  ExpectFaviconUrlFetchAttempt(kUrlA);

  AttemptFetch();
  InvokeNextFaviconCallbacks(/*num_successful_fetches=*/1);

  auto synced_session_window_two =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  AddTab(synced_session_window_two.get(), kTitleD, kUrlD, kTimeD);
  AddTab(synced_session_window_two.get(), kTitleC, kUrlC, kTimeC);
  AddWindow(std::move(synced_session_window_two));

  ExpectFaviconUrlFetchAttempt(kUrlD);
  ExpectFaviconUrlFetchAttempt(kUrlC);
  ExpectFaviconUrlFetchAttempt(kUrlB);
  ExpectFaviconUrlFetchAttempt(kUrlA);

  AttemptFetch();
  EXPECT_FALSE(actual_browser_tabs_metadata());

  // 5 callbacks called accounting for the additional missed one for tab A.
  InvokeNextFaviconCallbacks(/*num_successful_fetches=*/5);
  CheckIsExpectedMetadata(std::vector<BrowserTabMetadata>({
      BrowserTabMetadata(kUrlD, kTitleD, kTimeD, GetDummyImage()),
      BrowserTabMetadata(kUrlC, kTitleC, kTimeC, GetDummyImage()),
      BrowserTabMetadata(kUrlB, kTitleB, kTimeB, GetDummyImage()),
      BrowserTabMetadata(kUrlA, kTitleA, kTimeA, GetDummyImage()),
  }));
}

TEST_F(BrowserTabsMetadataFetcherImplTest, NoTabsOpen) {
  auto synced_session_window =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  AddWindow(std::move(synced_session_window));

  AttemptFetch();
  CheckIsExpectedMetadata({});
}

TEST_F(BrowserTabsMetadataFetcherImplTest, BelowMaximumNumberOfTabs) {
  const base::string16 kTitleC = base::UTF8ToUTF16("C");
  const GURL kUrlC = GURL("http://c.com");

  const base::string16 kTitleD = base::UTF8ToUTF16("D");
  const GURL kUrlD = GURL("http://d.com");

  auto synced_session_window =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  AddTab(synced_session_window.get(), kTitleD, kUrlD, kTimeD);
  AddTab(synced_session_window.get(), kTitleC, kUrlC, kTimeC);
  AddWindow(std::move(synced_session_window));

  ExpectFaviconUrlFetchAttempt(kUrlC);
  ExpectFaviconUrlFetchAttempt(kUrlD);

  AttemptFetch();
  InvokeNextFaviconCallbacks(/*num_successful_fetches=*/2);
  CheckIsExpectedMetadata(std::vector<BrowserTabMetadata>({
      BrowserTabMetadata(kUrlD, kTitleD, kTimeD, GetDummyImage()),
      BrowserTabMetadata(kUrlC, kTitleC, kTimeC, GetDummyImage()),
  }));
}

TEST_F(BrowserTabsMetadataFetcherImplTest, ExceedMaximumNumberOfTabs) {
  const base::string16 kTitleA = base::UTF8ToUTF16("A");
  const GURL kUrlA = GURL("http://a.com");

  const base::string16 kTitleB = base::UTF8ToUTF16("B");
  const GURL kUrlB = GURL("http://b.com");

  const base::string16 kTitleC = base::UTF8ToUTF16("C");
  const GURL kUrlC = GURL("http://c.com");

  const base::string16 kTitleD = base::UTF8ToUTF16("D");
  const GURL kUrlD = GURL("http://d.com");

  const base::string16 kTitleE = base::UTF8ToUTF16("E");
  const GURL kUrlE = GURL("http://e.com");

  auto synced_session_window =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  AddTab(synced_session_window.get(), kTitleA, kUrlA, kTimeA);
  AddTab(synced_session_window.get(), kTitleE, kUrlE, kTimeE);
  AddTab(synced_session_window.get(), kTitleB, kUrlB, kTimeB);
  AddTab(synced_session_window.get(), kTitleD, kUrlD, kTimeD);
  AddTab(synced_session_window.get(), kTitleC, kUrlC, kTimeC);
  AddWindow(std::move(synced_session_window));

  ExpectFaviconUrlFetchAttempt(kUrlB);
  ExpectFaviconUrlFetchAttempt(kUrlC);
  ExpectFaviconUrlFetchAttempt(kUrlD);
  ExpectFaviconUrlFetchAttempt(kUrlE);

  AttemptFetch();
  InvokeNextFaviconCallbacks(/*num_successful_fetches=*/4);

  // Tab A is not present because it has the oldest timestamp, and the maximum
  // number of BrowserTabMetadata has been met.
  CheckIsExpectedMetadata(std::vector<BrowserTabMetadata>({
      BrowserTabMetadata(kUrlE, kTitleE, kTimeE, GetDummyImage()),
      BrowserTabMetadata(kUrlD, kTitleD, kTimeD, GetDummyImage()),
      BrowserTabMetadata(kUrlC, kTitleC, kTimeC, GetDummyImage()),
      BrowserTabMetadata(kUrlB, kTitleB, kTimeB, GetDummyImage()),
  }));
}

TEST_F(BrowserTabsMetadataFetcherImplTest, MultipleWindows) {
  const base::string16 kTitleB = base::UTF8ToUTF16("B");
  const GURL kUrlB = GURL("http://b.com");

  const base::string16 kTitleC = base::UTF8ToUTF16("C");
  const GURL kUrlC = GURL("http://c.com");

  const base::string16 kTitleD = base::UTF8ToUTF16("D");
  const GURL kUrlD = GURL("http://d.com");

  const base::string16 kTitleE = base::UTF8ToUTF16("E");
  const GURL kUrlE = GURL("http://e.com");

  auto synced_session_window_one =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  AddTab(synced_session_window_one.get(), kTitleE, kUrlE, kTimeE);
  AddTab(synced_session_window_one.get(), kTitleB, kUrlB, kTimeB);
  AddWindow(std::move(synced_session_window_one));

  auto synced_session_window_two =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  AddTab(synced_session_window_two.get(), kTitleD, kUrlD, kTimeD);
  AddTab(synced_session_window_two.get(), kTitleC, kUrlC, kTimeC);
  AddWindow(std::move(synced_session_window_two));

  ExpectFaviconUrlFetchAttempt(kUrlB);
  ExpectFaviconUrlFetchAttempt(kUrlC);
  ExpectFaviconUrlFetchAttempt(kUrlD);
  ExpectFaviconUrlFetchAttempt(kUrlE);

  AttemptFetch();
  InvokeNextFaviconCallbacks(/*num_successful_fetches=*/4);
  CheckIsExpectedMetadata(std::vector<BrowserTabMetadata>({
      BrowserTabMetadata(kUrlE, kTitleE, kTimeE, GetDummyImage()),
      BrowserTabMetadata(kUrlD, kTitleD, kTimeD, GetDummyImage()),
      BrowserTabMetadata(kUrlC, kTitleC, kTimeC, GetDummyImage()),
      BrowserTabMetadata(kUrlB, kTitleB, kTimeB, GetDummyImage()),
  }));
}

}  // namespace phonehub
}  // namespace chromeos
