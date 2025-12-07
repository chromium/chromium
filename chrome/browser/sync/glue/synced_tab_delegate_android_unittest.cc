// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/compiler_specific.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/tab_android_data_provider.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/features.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_sync {

namespace {

using testing::_;

using testing::Mock;
using testing::Return;

class MockTabAndroidDataProvider : public TabAndroidDataProvider {
 public:
  MOCK_METHOD(SessionID, GetWindowId, (), (const override));
  MOCK_METHOD(int, GetAndroidId, (), (const override));
  MOCK_METHOD(std::unique_ptr<WebContentsStateByteBuffer>,
              GetWebContentsByteBuffer,
              (),
              (override));
};

const int kVersion = 2;
const url::Origin kInitiatorOrigin;

class SyncedTabDelegateAndroidTest : public testing::Test {
 protected:
  void SetUp() override {
    CHECK(profile_manager_.SetUp());
    profile_ = TestingProfile::Builder().Build();
    test_tab_model_ = std::make_unique<TestTabModel>(profile_.get());
    TabModelList::AddTabModel(test_tab_model_.get());

    Mock::VerifyAndClear(&mock_sync_sessions_client_);
    ON_CALL(mock_sync_sessions_client_, ShouldSyncURL(GURL(kInterestingUrl)))
        .WillByDefault(Return(true));
    ON_CALL(mock_sync_sessions_client_, ShouldSyncURL(GURL(kBoringUrl)))
        .WillByDefault(Return(false));
    ON_CALL(mock_tab_android_data_provider_, GetWindowId())
        .WillByDefault(Return(test_tab_model_->GetSessionId()));
  }

  void TearDown() override {
    TabModelList::RemoveTabModel(test_tab_model_.get());
  }

  void MockBufferFromPickle(const base::Pickle& pickle) {
    base::raw_span<const uint8_t> UNSAFE_TODO(
        nav_span{pickle.data(), pickle.size()});
    std::unique_ptr<WebContentsStateByteBuffer> buffer =
        std::make_unique<WebContentsStateByteBuffer>(nav_span, kVersion);
    EXPECT_CALL(mock_tab_android_data_provider_, GetWebContentsByteBuffer())
        .WillOnce(Return(std::move(buffer)));
  }

  const std::u16string kTitle = u"";
  const GURL kInterestingUrl = GURL("fake://interesting");
  const GURL kBoringUrl = GURL("fake://boring");
  const content::Referrer kReferrer =
      content::Referrer(GURL(""), network::mojom::ReferrerPolicy::kDefault);

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfileManager profile_manager_ =
      TestingProfileManager(TestingBrowserProcess::GetGlobal());
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestTabModel> test_tab_model_;
  testing::NiceMock<sync_sessions::MockSyncSessionsClient>
      mock_sync_sessions_client_;
  testing::NiceMock<MockTabAndroidDataProvider> mock_tab_android_data_provider_;
  SyncedTabDelegateAndroid synced_tab_delegate_android_ =
      SyncedTabDelegateAndroid(&mock_tab_android_data_provider_);
};

TEST_F(SyncedTabDelegateAndroidTest, ReadPlaceholderNullBuffer) {
  EXPECT_CALL(mock_tab_android_data_provider_, GetWebContentsByteBuffer())
      .WillOnce(Return(nullptr));

  std::unique_ptr<sync_sessions::SyncedTabDelegate> placeholder =
      synced_tab_delegate_android_.ReadPlaceholderTabSnapshotIfItShouldSync(
          &mock_sync_sessions_client_);

  EXPECT_EQ(nullptr, placeholder.get());
}

TEST_F(SyncedTabDelegateAndroidTest, ReadPlaceholderBoring) {
  base::Pickle pickle = WebContentsState::CreateSingleNavigationStateAsPickle(
      &*profile_, kTitle, kBoringUrl, kReferrer, kInitiatorOrigin);
  MockBufferFromPickle(pickle);

  std::unique_ptr<sync_sessions::SyncedTabDelegate> placeholder =
      synced_tab_delegate_android_.ReadPlaceholderTabSnapshotIfItShouldSync(
          &mock_sync_sessions_client_);

  EXPECT_NE(nullptr, placeholder.get());
}

TEST_F(SyncedTabDelegateAndroidTest, ReadPlaceholderBoringWithOptimization) {
  feature_list_.InitAndEnableFeature(
      sync_sessions::kOptimizeAssociateWindowsAndroid);
  base::Pickle pickle = WebContentsState::CreateSingleNavigationStateAsPickle(
      &*profile_, kTitle, kBoringUrl, kReferrer, kInitiatorOrigin);
  MockBufferFromPickle(pickle);

  std::unique_ptr<sync_sessions::SyncedTabDelegate> placeholder =
      synced_tab_delegate_android_.ReadPlaceholderTabSnapshotIfItShouldSync(
          &mock_sync_sessions_client_);

  EXPECT_EQ(nullptr, placeholder.get());
}

TEST_F(SyncedTabDelegateAndroidTest,
       ReadPlaceholderInterestingWithOptimization) {
  feature_list_.InitAndEnableFeature(
      sync_sessions::kOptimizeAssociateWindowsAndroid);
  base::Pickle pickle = WebContentsState::CreateSingleNavigationStateAsPickle(
      &*profile_, kTitle, kInterestingUrl, kReferrer, kInitiatorOrigin);
  MockBufferFromPickle(pickle);

  std::unique_ptr<sync_sessions::SyncedTabDelegate> placeholder =
      synced_tab_delegate_android_.ReadPlaceholderTabSnapshotIfItShouldSync(
          &mock_sync_sessions_client_);

  EXPECT_NE(nullptr, placeholder.get());
}

}  // namespace

}  // namespace browser_sync
