// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/android_push_notification_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/android/native_j_unittests_jni_headers/OptimizationGuidePushNotificationTestHelper_jni.h"
#include "chrome/browser/optimization_guide/android/optimization_guide_bridge.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/hint_cache.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

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

class MockObserver : public PushNotificationManager::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;
  MOCK_METHOD(void,
              OnNotificationPayload,
              (proto::OptimizationType, const optimization_guide::proto::Any&),
              (override));
};

const int kOverflowSize = 5;

class AndroidPushNotificationManagerJavaTest : public testing::Test {
 public:
  AndroidPushNotificationManagerJavaTest()
      : j_test_(Java_OptimizationGuidePushNotificationTestHelper_Constructor(
            base::android::AttachCurrentThread())),
        env_(base::android::AttachCurrentThread()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitAndEnableFeature(
        optimization_guide::features::kPushNotifications);
  }
  ~AndroidPushNotificationManagerJavaTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp(temp_dir_.GetPath()));
    profile_ = profile_manager_.CreateTestingProfile(chrome::kInitialProfile);

    Java_OptimizationGuidePushNotificationTestHelper_setUpMocks(env_, j_test_);

    service_ = static_cast<OptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(&AndroidPushNotificationManagerJavaTest::
                                        CreateServiceForProfile,
                                    base::Unretained(this))));
    service_->GetHintsManager()->push_notification_manager()->AddObserver(
        &observer_);

    // It takes two session starts for experimental params and feature flags to
    // be picked up by Java, so override them manually.
    Java_OptimizationGuidePushNotificationTestHelper_setOverflowSizeForTesting(
        env_, kOverflowSize);
    Java_OptimizationGuidePushNotificationTestHelper_setFeatureEnabled(env_);
  }

  void TearDown() override {
    Java_OptimizationGuidePushNotificationTestHelper_clearAllCaches(env_);
  }

  std::unique_ptr<KeyedService> CreateServiceForProfile(
      content::BrowserContext* browser_context) {
    return std::make_unique<OptimizationGuideKeyedService>(
        Profile::FromBrowserContext(browser_context));
  }

  void PushNotificationNative(
      const proto::HintNotificationPayload& notification) {
    std::string encoded_notification;
    notification.SerializeToString(&encoded_notification);

    OptimizationGuideBridge bridge(service());
    bridge.OnNewPushNotification(
        env_, base::android::ToJavaByteArray(env_, encoded_notification));
  }

  bool PushNotificationJava(
      const proto::HintNotificationPayload& notification) {
    std::string encoded_notification;
    if (!notification.SerializeToString(&encoded_notification))
      return false;

    return Java_OptimizationGuidePushNotificationTestHelper_pushNotification(
        env_, base::android::ToJavaByteArray(env_, encoded_notification));
  }

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

  bool DidOverflow(proto::OptimizationType opt_type) {
    return Java_OptimizationGuidePushNotificationTestHelper_didOverflow(
        env_, static_cast<int>(opt_type));
  }

  size_t AndroidNotificationCacheSize(proto::OptimizationType opt_type) {
    return Java_OptimizationGuidePushNotificationTestHelper_countCachedNotifications(
        env_, static_cast<int>(opt_type));
  }

  OptimizationGuideKeyedService* service() { return service_; }

  optimization_guide::ChromeHintsManager* hints_manager() {
    return service()->GetHintsManager();
  }

  MockObserver* observer() { return &observer_; }

  JNIEnv* env() { return env_; }

  TestingProfile* profile() { return profile_; }

  PrefService* prefs() { return profile()->GetPrefs(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
  raw_ptr<JNIEnv> env_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  MockObserver observer_;
  raw_ptr<OptimizationGuideKeyedService> service_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AndroidPushNotificationManagerJavaTest,
       SingleCachedNotification_SuccessCallback) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  ASSERT_TRUE(CacheNotification(notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  manager.OnDelegateReady();
  const auto& last_remove_many = delegate.last_remove_many();
  EXPECT_EQ(proto::KeyRepresentation::HOST, last_remove_many.first);
  EXPECT_EQ(base::flat_set<std::string>{"hintkey"}, last_remove_many.second);
  EXPECT_EQ(0U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.CachedNotificationCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.DidOverflow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications."
      "CachedNotificationsHandledSuccessfully",
      true, 1);
}

TEST_F(AndroidPushNotificationManagerJavaTest,
       Cached_SingleNotification_FailedCallback) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(false);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  ASSERT_TRUE(CacheNotification(notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  manager.OnDelegateReady();
  const auto& last_remove_many = delegate.last_remove_many();
  EXPECT_EQ(proto::KeyRepresentation::HOST, last_remove_many.first);
  EXPECT_EQ(base::flat_set<std::string>{"hintkey"}, last_remove_many.second);

  // The callback wasn't run, indicating failure, so the notification should be
  // put back into the Android cache.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications."
      "CachedNotificationsHandledSuccessfully",
      false, 1);
}

TEST_F(AndroidPushNotificationManagerJavaTest, TwoCachedNotifications) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  ASSERT_TRUE(CacheNotification(notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  proto::HintNotificationPayload notification2;
  notification2.set_hint_key("hintkey2");
  notification2.set_key_representation(proto::KeyRepresentation::HOST);
  notification2.set_optimization_type(
      proto::OptimizationType::RESOURCE_LOADING);
  ASSERT_TRUE(CacheNotification(notification2));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::RESOURCE_LOADING));

  manager.OnDelegateReady();
  const auto& last_remove_many = delegate.last_remove_many();
  EXPECT_EQ(proto::KeyRepresentation::HOST, last_remove_many.first);
  EXPECT_EQ(0U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));
  EXPECT_EQ(0U, AndroidNotificationCacheSize(
                    proto::OptimizationType::RESOURCE_LOADING));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.CachedNotificationCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.DidOverflow", false, 1);
  // One sample is logged for each OptimizationType.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications."
      "CachedNotificationsHandledSuccessfully",
      true, 2);
}

TEST_F(AndroidPushNotificationManagerJavaTest, Cached_Overflow_HandledSuccess) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  CauseOverflow(proto::OptimizationType::PERFORMANCE_HINTS);
  ASSERT_TRUE(DidOverflow(proto::OptimizationType::PERFORMANCE_HINTS));

  manager.OnDelegateReady();
  EXPECT_TRUE(delegate.did_call_purge());

  EXPECT_FALSE(DidOverflow(proto::OptimizationType::PERFORMANCE_HINTS));
  EXPECT_EQ(0U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.DidOverflow", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PushNotifications."
      "CachedNotificationsHandledSuccessfully",
      0);
}

TEST_F(AndroidPushNotificationManagerJavaTest, Cached_Overflow_HandledFailure) {
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(false);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  CauseOverflow(proto::OptimizationType::PERFORMANCE_HINTS);
  ASSERT_TRUE(DidOverflow(proto::OptimizationType::PERFORMANCE_HINTS));

  manager.OnDelegateReady();
  EXPECT_TRUE(delegate.did_call_purge());

  EXPECT_TRUE(DidOverflow(proto::OptimizationType::PERFORMANCE_HINTS));
}

TEST_F(AndroidPushNotificationManagerJavaTest, Cached_OverflowPurgesAllTypes) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload supported_notification;
  supported_notification.set_hint_key("other hintkey");
  supported_notification.set_key_representation(proto::KeyRepresentation::HOST);
  supported_notification.set_optimization_type(
      proto::OptimizationType::LINK_PERFORMANCE);
  ASSERT_TRUE(CacheNotification(supported_notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::LINK_PERFORMANCE));

  CauseOverflow(proto::OptimizationType::PERFORMANCE_HINTS);
  ASSERT_TRUE(DidOverflow(proto::OptimizationType::PERFORMANCE_HINTS));

  manager.OnDelegateReady();
  EXPECT_TRUE(delegate.did_call_purge());

  // All types should be purged.
  EXPECT_FALSE(DidOverflow(proto::OptimizationType::PERFORMANCE_HINTS));
  EXPECT_FALSE(DidOverflow(proto::OptimizationType::LINK_PERFORMANCE));
  EXPECT_EQ(0U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));
  EXPECT_EQ(0U, AndroidNotificationCacheSize(
                    proto::OptimizationType::LINK_PERFORMANCE));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.DidOverflow", true, 1);
}

TEST_F(AndroidPushNotificationManagerJavaTest, PushNotification_Success) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);

  manager.OnNewPushNotification(notification);

  // Because there was a delegate, the notification should not be cached.
  EXPECT_EQ(0U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.PushNotificationHandledSuccessfully",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.GotPushNotification", true, 1);
}

TEST_F(AndroidPushNotificationManagerJavaTest, PushNotification_Failure) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(false);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);

  manager.OnNewPushNotification(notification);

  // The notification should be cached because it was not handled successfully.
  EXPECT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.PushNotificationHandledSuccessfully",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.GotPushNotification", true, 1);
}

TEST_F(AndroidPushNotificationManagerJavaTest,
       PushNotificationCachedWhenNoDelegate) {
  base::HistogramTester histogram_tester;
  AndroidPushNotificationManager manager(prefs());

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);

  manager.OnNewPushNotification(notification);

  // Because there was not delegate, the notification should be cached.
  EXPECT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  // But the same notification can be pulled with a delegate.
  TestDelegate delegate;
  manager.SetDelegate(&delegate);
  manager.OnDelegateReady();
  const auto& last_remove_many = delegate.last_remove_many();
  EXPECT_EQ(proto::KeyRepresentation::HOST, last_remove_many.first);
  EXPECT_EQ(base::flat_set<std::string>{"hintkey"}, last_remove_many.second);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.GotPushNotification", true, 1);
}

TEST_F(AndroidPushNotificationManagerJavaTest,
       Cached_MultipleKeyRepresentations) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.SetRunSuccessCallbacks(true);

  AndroidPushNotificationManager manager(prefs());
  manager.SetDelegate(&delegate);

  proto::HintNotificationPayload host_notification;
  host_notification.set_hint_key("host-key.com");
  host_notification.set_key_representation(proto::KeyRepresentation::HOST);
  host_notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  ASSERT_TRUE(CacheNotification(host_notification));
  ASSERT_EQ(1U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  proto::HintNotificationPayload url_notification;
  url_notification.set_hint_key("http://url-key.com/page");
  url_notification.set_key_representation(proto::KeyRepresentation::FULL_URL);
  url_notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  ASSERT_TRUE(CacheNotification(url_notification));
  ASSERT_EQ(2U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

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

  EXPECT_EQ(0U, AndroidNotificationCacheSize(
                    proto::OptimizationType::PERFORMANCE_HINTS));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.CachedNotificationCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications."
      "CachedNotificationsHandledSuccessfully",
      true, 1);
}

TEST_F(AndroidPushNotificationManagerJavaTest, Pushed_URL_SuccessCase) {
  // Pre-populate the store with some hints.
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
  EXPECT_CALL(*observer(), OnNotificationPayload(_, _)).Times(0);

  proto::HintNotificationPayload notification;
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  notification.set_key_representation(proto::KeyRepresentation::FULL_URL);
  notification.set_hint_key(url.spec());

  PushNotificationNative(notification);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_FALSE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(AndroidPushNotificationManagerJavaTest, Pushed_Host_SuccessCase) {
  // Pre-populate the store with some hints.
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
  EXPECT_CALL(*observer(), OnNotificationPayload(_, _)).Times(0);

  proto::HintNotificationPayload notification;
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_hint_key(url.host());

  PushNotificationNative(notification);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(AndroidPushNotificationManagerJavaTest, PushedJava_URL_SuccessCase) {
  // Pre-populate the store with some hints.
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
  EXPECT_CALL(*observer(), OnNotificationPayload(_, _)).Times(0);

  proto::HintNotificationPayload notification;
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  notification.set_key_representation(proto::KeyRepresentation::FULL_URL);
  notification.set_hint_key(url.spec());

  PushNotificationJava(notification);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_FALSE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(AndroidPushNotificationManagerJavaTest, PushedJava_Host_SuccessCase) {
  // Pre-populate the store with some hints.
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
  EXPECT_CALL(*observer(), OnNotificationPayload(_, _)).Times(0);

  proto::HintNotificationPayload notification;
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_hint_key(url.host());

  PushNotificationJava(notification);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(AndroidPushNotificationManagerJavaTest,
       Pushed_KeyRepresentationRequired) {
  // Pre-populate the store with some hints.
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  proto::HintNotificationPayload notification;
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  notification.set_hint_key(url.spec());

  PushNotificationNative(notification);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(AndroidPushNotificationManagerJavaTest,
       Pushed_OptimizationTypeNotRequired) {
  // Pre-populate the store with some hints.
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  proto::HintNotificationPayload notification;
  notification.set_key_representation(proto::KeyRepresentation::FULL_URL);
  notification.set_hint_key(url.spec());

  PushNotificationNative(notification);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_FALSE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(AndroidPushNotificationManagerJavaTest, Pushed_HintKeyRequired) {
  // Pre-populate the store with some hints.
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  proto::HintNotificationPayload notification;
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  notification.set_key_representation(proto::KeyRepresentation::FULL_URL);

  PushNotificationNative(notification);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

// Send non-empty HintNotificationPayload.payload and valid hints data in the
// HintNotificationPayload proto, observer should receive
// the optimization_guide::proto::Any.
TEST_F(AndroidPushNotificationManagerJavaTest, PayloadDispatched_WithHintsKey) {
  base::HistogramTester histogram_tester;
  optimization_guide::proto::Any payload_to_observer;
  EXPECT_CALL(*observer(),
              OnNotificationPayload(proto::OptimizationType::PRICE_TRACKING, _))
      .WillOnce(SaveArg<1>(&payload_to_observer));

  proto::HintNotificationPayload notification;
  notification.set_optimization_type(proto::OptimizationType::PRICE_TRACKING);
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  GURL url("https://host.com/r/cats");
  notification.set_hint_key(url.host());
  optimization_guide::proto::Any* payload = new optimization_guide::proto::Any;
  payload->set_type_url("type_url");
  payload->set_value("value");
  notification.set_allocated_payload(payload);
  PushNotificationNative(notification);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(payload_to_observer.type_url(), "type_url");
  EXPECT_EQ(payload_to_observer.value(), "value");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.ReceivedNotificationType",
      proto::OptimizationType::PRICE_TRACKING, 1);
}

// Send non-empty HintNotificationPayload.payload and empty hints key in the
// HintNotificationPayload proto, observer should not receive
// optimization_guide::proto::Any.
TEST_F(AndroidPushNotificationManagerJavaTest,
       PayloadNotDispatched_InvalidPayload) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*observer(), OnNotificationPayload(_, _)).Times(0);

  // No hints key.
  proto::HintNotificationPayload notification;
  optimization_guide::proto::Any* payload = new optimization_guide::proto::Any;
  notification.set_optimization_type(proto::OptimizationType::PRICE_TRACKING);
  payload->set_type_url("type_url");
  payload->set_value("value");
  notification.set_allocated_payload(payload);
  PushNotificationNative(notification);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.ReceivedNotificationType",
      proto::OptimizationType::PRICE_TRACKING, 0);
}

}  // namespace android
}  // namespace optimization_guide
