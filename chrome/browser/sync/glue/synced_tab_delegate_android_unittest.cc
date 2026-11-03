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
#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"
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
#include "components/sync_sessions/synced_window_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
              (const override));
};

class SessionIdTestTabModel : public TestTabModel {
 public:
  using TabModel::SetSessionId;
  using TestTabModel::TestTabModel;
};

const int kVersion = 2;

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

  void MockBufferFromPickle(base::Pickle& pickle) {
    JNIEnv* env = base::android::AttachCurrentThread();

    auto jbuffer = jni_zero::AdoptRef(
        env, env->NewDirectByteBuffer(
                 reinterpret_cast<void*>(pickle.AsWritableBytes().data()),
                 pickle.size()));

    std::unique_ptr<WebContentsStateByteBuffer> buffer =
        std::make_unique<WebContentsStateByteBuffer>(jbuffer, kVersion);
    EXPECT_CALL(mock_tab_android_data_provider_, GetWebContentsByteBuffer())
        .WillOnce(Return(std::move(buffer)));
  }

  const url::Origin kInitiatorOrigin;
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

TEST_F(SyncedTabDelegateAndroidTest,
       SyncedWindowDelegatesGetterAndroidIsScopedToProfile) {
  std::unique_ptr<TestingProfile> other_profile =
      TestingProfile::Builder().Build();
  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  SessionIdTestTabModel profile_model_2(profile_.get());
  const SessionID profile_window_id_2 = SessionID::NewUnique();
  profile_model_2.SetSessionId(profile_window_id_2);
  TabModelList::AddTabModel(&profile_model_2);

  SessionIdTestTabModel other_model(other_profile.get());
  const SessionID other_window_id = SessionID::NewUnique();
  other_model.SetSessionId(other_window_id);
  TabModelList::AddTabModel(&other_model);

  SessionIdTestTabModel otr_model(otr_profile);
  const SessionID otr_window_id = SessionID::NewUnique();
  otr_model.SetSessionId(otr_window_id);
  TabModelList::AddTabModel(&otr_model);

  SyncedWindowDelegatesGetterAndroid profile_getter(profile_.get());
  const sync_sessions::SyncedWindowDelegatesGetter::SyncedWindowDelegateMap
      profile_delegates = profile_getter.GetSyncedWindowDelegates();
  EXPECT_EQ(2u, profile_delegates.size());
  EXPECT_EQ(test_tab_model_->GetSyncedWindowDelegate(),
            profile_delegates.at(test_tab_model_->GetSessionId()));
  EXPECT_EQ(profile_model_2.GetSyncedWindowDelegate(),
            profile_delegates.at(profile_window_id_2));
  EXPECT_FALSE(profile_delegates.contains(other_window_id));
  EXPECT_FALSE(profile_delegates.contains(otr_window_id));

  EXPECT_EQ(profile_model_2.GetSyncedWindowDelegate(),
            profile_getter.FindById(profile_window_id_2));
  EXPECT_EQ(nullptr, profile_getter.FindById(other_window_id));
  EXPECT_EQ(nullptr, profile_getter.FindById(otr_window_id));
  EXPECT_EQ(nullptr, profile_getter.FindById(SessionID::NewUnique()));

  SyncedWindowDelegatesGetterAndroid other_getter(other_profile.get());
  const sync_sessions::SyncedWindowDelegatesGetter::SyncedWindowDelegateMap
      other_delegates = other_getter.GetSyncedWindowDelegates();
  EXPECT_EQ(1u, other_delegates.size());
  EXPECT_EQ(other_model.GetSyncedWindowDelegate(),
            other_delegates.at(other_window_id));

  EXPECT_EQ(other_model.GetSyncedWindowDelegate(),
            other_getter.FindById(other_window_id));
  EXPECT_EQ(nullptr, other_getter.FindById(profile_window_id_2));
  EXPECT_EQ(nullptr, other_getter.FindById(otr_window_id));
}

}  // namespace
