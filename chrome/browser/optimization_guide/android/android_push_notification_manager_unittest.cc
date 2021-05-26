// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/android_push_notification_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/android/native_j_unittests_jni_headers/OptimizationGuidePushNotificationTestHelper_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace android {

class TestDelegate : public PushNotificationManager::Delegate {
 public:
  using RemoveMultiplePair =
      std::pair<proto::KeyRepresentation, base::flat_set<std::string>>;

  TestDelegate() = default;
  ~TestDelegate() = default;

  void SetRunSuccessCallbacks(bool run_success_callbacks) {
    run_success_callbacks_ = run_success_callbacks;
  }

  const RemoveMultiplePair& last_remove_many() const {
    return last_remove_many_;
  }

  const std::vector<RemoveMultiplePair>& all_remove_multiples() const {
    return all_remove_multiples_;
  }

  bool did_call_purge() const { return did_call_purge_; }

  void RemoveFetchedEntriesByHintKeys(
      base::OnceClosure on_success,
      proto::KeyRepresentation key_representation,
      const base::flat_set<std::string>& hint_keys) override {
    last_remove_many_ = std::make_pair(key_representation, hint_keys);
    all_remove_multiples_.push_back(last_remove_many_);
    if (run_success_callbacks_)
      std::move(on_success).Run();
  }

  void PurgeFetchedEntries(base::OnceClosure on_success) override {
    did_call_purge_ = true;
    if (run_success_callbacks_)
      std::move(on_success).Run();
  }

 private:
  RemoveMultiplePair last_remove_many_;
  std::vector<RemoveMultiplePair> all_remove_multiples_;
  bool did_call_purge_ = false;
  bool run_success_callbacks_ = true;
};

const proto::OptimizationType kOptType1 =
    proto::OptimizationType::PERFORMANCE_HINTS;
const proto::OptimizationType kOptType2 =
    proto::OptimizationType::LINK_PERFORMANCE;

const int kOverflowSize = 5;

class AndroidPushNotificationManagerJavaTest : public testing::Test {
 public:
  AndroidPushNotificationManagerJavaTest()
      : env_(base::android::AttachCurrentThread()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitAndEnableFeature(
        optimization_guide::features::kPushNotifications);
  }
  ~AndroidPushNotificationManagerJavaTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp(temp_dir_.GetPath()));
    profile_ = profile_manager_.CreateTestingProfile(chrome::kInitialProfile);

    // It takes two session starts for experimental params to be picked up by
    // Java, so override it manually.
    Java_OptimizationGuidePushNotificationTestHelper_setOverflowSizeForTesting(
        env_, kOverflowSize);
  }

  void TearDown() override {
    Java_OptimizationGuidePushNotificationTestHelper_clearAllCaches(env_);
  }

  JNIEnv* env() { return env_; }

  Profile* profile() { return profile_; }

  PrefService* prefs() { return profile()->GetPrefs(); }

  void CauseOverflow(proto::OptimizationType opt_type) {
    for (int i = 0; i < kOverflowSize + 1; i++) {
      SCOPED_TRACE(i);
      proto::HintNotificationPayload notification;
      notification.set_hint_key("hint_" + base::NumberToString(i));
      notification.set_optimization_type(opt_type);
      notification.set_key_representation(proto::KeyRepresentation::HOST);
      ASSERT_TRUE(CacheNotification(notification));
    }
  }

  bool CacheNotification(const proto::HintNotificationPayload& notification) {
    std::string encoded_notification;
    if (!notification.SerializeToString(&encoded_notification))
      return false;

    return Java_OptimizationGuidePushNotificationTestHelper_cacheNotification(
        env_, base::android::ToJavaByteArray(env_, encoded_notification));
  }

  bool PushNotification(const proto::HintNotificationPayload& notification) {
    std::string encoded_notification;
    if (!notification.SerializeToString(&encoded_notification))
      return false;

    return Java_OptimizationGuidePushNotificationTestHelper_pushNotification(
        env_, base::android::ToJavaByteArray(env_, encoded_notification));
  }

  bool DidOverflow(proto::OptimizationType opt_type) {
    return Java_OptimizationGuidePushNotificationTestHelper_didOverflow(
        env_, static_cast<int>(opt_type));
  }

  size_t AndroidNotificationCacheSize(proto::OptimizationType opt_type) {
    return Java_OptimizationGuidePushNotificationTestHelper_countCachedNotifications(
        env_, static_cast<int>(opt_type));
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  JNIEnv* env_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AndroidPushNotificationManagerJavaTest,
       SingleCachedNotification_SuccessCallback) {
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(kOptType1);
  ASSERT_TRUE(CacheNotification(notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(kOptType1));

  manager.OnDelegateReady();
  const auto& last_remove_many = delegate.last_remove_many();
  EXPECT_EQ(proto::KeyRepresentation::HOST, last_remove_many.first);
  EXPECT_EQ(base::flat_set<std::string>{"hintkey"}, last_remove_many.second);
  EXPECT_EQ(0U, AndroidNotificationCacheSize(kOptType1));
}

TEST_F(AndroidPushNotificationManagerJavaTest,
       SingleCachedNotification_FailedCallback) {
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(false);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(kOptType1);
  ASSERT_TRUE(CacheNotification(notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(kOptType1));

  manager.OnDelegateReady();
  const auto& last_remove_many = delegate.last_remove_many();
  EXPECT_EQ(proto::KeyRepresentation::HOST, last_remove_many.first);
  EXPECT_EQ(base::flat_set<std::string>{"hintkey"}, last_remove_many.second);

  // The callback wasn't run, indicating failure, so the notification should be
  // put back into the Android cache.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, AndroidNotificationCacheSize(kOptType1));
}

TEST_F(AndroidPushNotificationManagerJavaTest, Overflow_HandledSuccess) {
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  CauseOverflow(kOptType1);
  ASSERT_TRUE(DidOverflow(kOptType1));

  manager.OnDelegateReady();
  EXPECT_TRUE(delegate.did_call_purge());

  EXPECT_FALSE(DidOverflow(kOptType1));
  EXPECT_EQ(0U, AndroidNotificationCacheSize(kOptType1));
}

TEST_F(AndroidPushNotificationManagerJavaTest, Overflow_HandledFailure) {
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(false);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  CauseOverflow(kOptType1);
  ASSERT_TRUE(DidOverflow(kOptType1));

  manager.OnDelegateReady();
  EXPECT_TRUE(delegate.did_call_purge());

  EXPECT_TRUE(DidOverflow(kOptType1));
}

TEST_F(AndroidPushNotificationManagerJavaTest, OverflowPurgesAllTypes) {
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload supported_notification;
  supported_notification.set_hint_key("other hintkey");
  supported_notification.set_key_representation(proto::KeyRepresentation::HOST);
  supported_notification.set_optimization_type(kOptType2);
  ASSERT_TRUE(CacheNotification(supported_notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(kOptType2));

  CauseOverflow(kOptType1);
  ASSERT_TRUE(DidOverflow(kOptType1));

  manager.OnDelegateReady();
  EXPECT_TRUE(delegate.did_call_purge());

  // All types should be purged.
  EXPECT_FALSE(DidOverflow(kOptType1));
  EXPECT_FALSE(DidOverflow(kOptType2));
  EXPECT_EQ(0U, AndroidNotificationCacheSize(kOptType1));
  EXPECT_EQ(0U, AndroidNotificationCacheSize(kOptType2));
}

TEST_F(AndroidPushNotificationManagerJavaTest,
       PushNotificationCachedWhenNoDelegate) {
  AndroidPushNotificationManager manager(prefs());

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(kOptType1);

  manager.OnNewPushNotification(notification);

  // Because there was not delegate, the notification should be cached.
  EXPECT_EQ(1U, AndroidNotificationCacheSize(kOptType1));

  // But the same notification can be pulled with a delegate.
  TestDelegate delegate;
  manager.SetDelegate(&delegate);
  manager.OnDelegateReady();
  const auto& last_remove_many = delegate.last_remove_many();
  EXPECT_EQ(proto::KeyRepresentation::HOST, last_remove_many.first);
  EXPECT_EQ(base::flat_set<std::string>{"hintkey"}, last_remove_many.second);
}

TEST_F(AndroidPushNotificationManagerJavaTest, MultipleKeyRepresentations) {
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload host_notification;
  host_notification.set_hint_key("host-key.com");
  host_notification.set_key_representation(proto::KeyRepresentation::HOST);
  host_notification.set_optimization_type(kOptType1);
  ASSERT_TRUE(CacheNotification(host_notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(kOptType1));

  proto::HintNotificationPayload url_notification;
  url_notification.set_hint_key("http://url-key.com/page");
  url_notification.set_key_representation(proto::KeyRepresentation::FULL_URL);
  url_notification.set_optimization_type(kOptType1);
  ASSERT_TRUE(CacheNotification(url_notification));
  ASSERT_EQ(2U, AndroidNotificationCacheSize(kOptType1));

  manager.OnDelegateReady();

  const std::vector<TestDelegate::RemoveMultiplePair>& multi_pair_removes =
      delegate.all_remove_multiples();
  EXPECT_EQ(2U, multi_pair_removes.size());
  EXPECT_TRUE(base::Contains(
      multi_pair_removes,
      std::make_pair(proto::KeyRepresentation::HOST,
                     base::flat_set<std::string>{"host-key.com"})));
  EXPECT_TRUE(base::Contains(
      multi_pair_removes,
      std::make_pair(proto::KeyRepresentation::FULL_URL,
                     base::flat_set<std::string>{"http://url-key.com/page"})));

  EXPECT_EQ(0U, AndroidNotificationCacheSize(kOptType1));
}

}  // namespace android
}  // namespace optimization_guide
