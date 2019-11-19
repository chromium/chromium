// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/persistence/site_data/feature_usage.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {
namespace internal {

namespace {

constexpr base::TimeDelta kInitialTimeSinceEpoch =
    base::TimeDelta::FromSeconds(1);

class TestLocalSiteCharacteristicsDataImpl
    : public LocalSiteCharacteristicsDataImpl {
 public:
  using LocalSiteCharacteristicsDataImpl::FeatureObservationDuration;
  using LocalSiteCharacteristicsDataImpl::OnDestroyDelegate;
  using LocalSiteCharacteristicsDataImpl::site_characteristics_for_testing;
  using LocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation;

  explicit TestLocalSiteCharacteristicsDataImpl(
      const url::Origin& origin,
      LocalSiteCharacteristicsDataImpl::OnDestroyDelegate* delegate,
      LocalSiteCharacteristicsDatabase* database)
      : LocalSiteCharacteristicsDataImpl(origin, delegate, database) {}

  base::TimeDelta FeatureObservationTimestamp(
      const SiteDataFeatureProto& feature_proto) {
    return InternalRepresentationToTimeDelta(feature_proto.use_timestamp());
  }

 protected:
  ~TestLocalSiteCharacteristicsDataImpl() override {}
};

class MockLocalSiteCharacteristicsDatabase
    : public testing::NoopLocalSiteCharacteristicsDatabase {
 public:
  MockLocalSiteCharacteristicsDatabase() = default;
  ~MockLocalSiteCharacteristicsDatabase() = default;

  // Note: As move-only parameters (e.g. OnceCallback) aren't supported by mock
  // methods, add On... methods to pass a non-const reference to OnceCallback.
  void ReadSiteCharacteristicsFromDB(
      const url::Origin& origin,
      LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
          callback) override {
    OnReadSiteCharacteristicsFromDB(origin, callback);
  }
  MOCK_METHOD2(OnReadSiteCharacteristicsFromDB,
               void(const url::Origin&,
                    LocalSiteCharacteristicsDatabase::
                        ReadSiteCharacteristicsFromDBCallback&));

  MOCK_METHOD2(WriteSiteCharacteristicsIntoDB,
               void(const url::Origin&, const SiteDataProto&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLocalSiteCharacteristicsDatabase);
};

// Returns a SiteDataFeatureProto that indicates that a feature
// hasn't been used.
SiteDataFeatureProto GetUnusedFeatureProto() {
  SiteDataFeatureProto unused_feature_proto;
  unused_feature_proto.set_observation_duration(1U);
  unused_feature_proto.set_use_timestamp(0U);
  return unused_feature_proto;
}

// Returns a SiteDataFeatureProto that indicates that a feature
// has been used.
SiteDataFeatureProto GetUsedFeatureProto() {
  SiteDataFeatureProto used_feature_proto;
  used_feature_proto.set_observation_duration(0U);
  used_feature_proto.set_use_timestamp(1U);
  return used_feature_proto;
}

}  // namespace

class LocalSiteCharacteristicsDataImplTest : public ::testing::Test {
 public:
  LocalSiteCharacteristicsDataImplTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {}

  void SetUp() override {
    test_clock_.SetNowTicks(base::TimeTicks::UnixEpoch());
    // Advance the test clock by a small delay, as some tests will fail if the
    // current time is equal to Epoch.
    test_clock_.Advance(kInitialTimeSinceEpoch);
  }

 protected:
  scoped_refptr<TestLocalSiteCharacteristicsDataImpl> GetDataImpl(
      const url::Origin& origin,
      LocalSiteCharacteristicsDataImpl::OnDestroyDelegate* destroy_delegate,
      LocalSiteCharacteristicsDatabase* database) {
    return base::MakeRefCounted<TestLocalSiteCharacteristicsDataImpl>(
        origin, destroy_delegate, database);
  }

  // Use a mock database to intercept the initialization callback and save it
  // locally so it can be run later.
  scoped_refptr<TestLocalSiteCharacteristicsDataImpl>
  GetDataImplAndInterceptReadCallback(
      const url::Origin& origin,
      LocalSiteCharacteristicsDataImpl::OnDestroyDelegate* destroy_delegate,
      MockLocalSiteCharacteristicsDatabase* mock_db,
      LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback*
          read_cb) {
    auto read_from_db_mock_impl =
        [&](const url::Origin& origin,
            LocalSiteCharacteristicsDatabase::
                ReadSiteCharacteristicsFromDBCallback& callback) {
          *read_cb = std::move(callback);
        };

    EXPECT_CALL(*mock_db,
                OnReadSiteCharacteristicsFromDB(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(read_from_db_mock_impl));
    auto local_site_data = GetDataImpl(origin, &destroy_delegate_, mock_db);
    ::testing::Mock::VerifyAndClear(mock_db);
    return local_site_data;
  }

  const url::Origin kDummyOrigin = url::Origin::Create(GURL("foo.com"));
  const url::Origin kDummyOrigin2 = url::Origin::Create(GURL("bar.com"));

  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  // Use a NiceMock as there's no need to add expectations in these tests,
  // there's a dedicated test that ensure that the delegate works as expected.
  ::testing::NiceMock<
      testing::MockLocalSiteCharacteristicsDataImplOnDestroyDelegate>
      destroy_delegate_;

  testing::NoopLocalSiteCharacteristicsDatabase database_;
};

TEST_F(LocalSiteCharacteristicsDataImplTest, BasicTestEndToEnd) {
  auto local_site_data =
      GetDataImpl(kDummyOrigin, &destroy_delegate_, &database_);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();

  // Initially the feature usage should be reported as unknown.
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesAudioInBackground());

  // Advance the clock by a time lower than the miniumum observation time for
  // the audio feature.
  test_clock_.Advance(GetStaticSiteCharacteristicsDatabaseParams()
                          .audio_usage_observation_window -
                      base::TimeDelta::FromSeconds(1));

  // The audio feature usage is still unknown as the observation window hasn't
  // expired.
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesAudioInBackground());

  // Report that the audio feature has been used.
  local_site_data->NotifyUsesAudioInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());

  // When a feature is in use it's expected that its recorded observation
  // timestamp is equal to the time delta since Unix Epoch when the observation
  // has been made.
  EXPECT_EQ(local_site_data->FeatureObservationTimestamp(
                local_site_data->site_characteristics_for_testing()
                    .uses_audio_in_background()),
            (test_clock_.NowTicks() - base::TimeTicks::UnixEpoch()));
  EXPECT_EQ(local_site_data->FeatureObservationDuration(
                local_site_data->site_characteristics_for_testing()
                    .uses_audio_in_background()),
            base::TimeDelta());

  // Advance the clock and make sure that notifications feature gets
  // reported as unused.
  test_clock_.Advance(GetStaticSiteCharacteristicsDatabaseParams()
                          .notifications_usage_observation_window);
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            local_site_data->UsesNotificationsInBackground());

  // Observating that a feature has been used after its observation window has
  // expired should still be recorded, the feature should then be reported as
  // used.
  local_site_data->NotifyUsesNotificationsInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesNotificationsInBackground());

  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);
}

TEST_F(LocalSiteCharacteristicsDataImplTest, LastLoadedTime) {
  auto local_site_data =
      GetDataImpl(kDummyOrigin, &destroy_delegate_, &database_);

  // Create a second instance of this object, simulates having several tab
  // owning it.
  auto local_site_data2(local_site_data);

  local_site_data->NotifySiteLoaded();
  base::TimeDelta last_loaded_time =
      local_site_data->last_loaded_time_for_testing();

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  // Loading the site a second time shouldn't change the last loaded time.
  local_site_data2->NotifySiteLoaded();
  EXPECT_EQ(last_loaded_time, local_site_data2->last_loaded_time_for_testing());

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  // Unloading the site shouldn't update the last loaded time as there's still
  // a loaded instance.
  local_site_data2->NotifySiteUnloaded(
      performance_manager::TabVisibility::kForeground);
  EXPECT_EQ(last_loaded_time, local_site_data->last_loaded_time_for_testing());

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kForeground);
  EXPECT_NE(last_loaded_time, local_site_data->last_loaded_time_for_testing());
}

TEST_F(LocalSiteCharacteristicsDataImplTest, GetFeatureUsageForUnloadedSite) {
  auto local_site_data =
      GetDataImpl(kDummyOrigin, &destroy_delegate_, &database_);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data->NotifyUsesAudioInBackground();

  test_clock_.Advance(GetStaticSiteCharacteristicsDatabaseParams()
                          .notifications_usage_observation_window -
                      base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesNotificationsInBackground());

  const base::TimeDelta observation_duration_before_unload =
      local_site_data->FeatureObservationDuration(
          local_site_data->site_characteristics_for_testing()
              .uses_notifications_in_background());

  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);

  // Once unloaded the feature observations should still be accessible.
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesNotificationsInBackground());

  // Advancing the clock shouldn't affect the observation duration for this
  // feature.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(observation_duration_before_unload,
            local_site_data->FeatureObservationDuration(
                local_site_data->site_characteristics_for_testing()
                    .uses_notifications_in_background()));
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesNotificationsInBackground());

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            local_site_data->UsesNotificationsInBackground());

  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);
}

TEST_F(LocalSiteCharacteristicsDataImplTest, AllDurationGetSavedOnUnload) {
  // This test helps making sure that the observation/timestamp fields get saved
  // for all the features being tracked.
  auto local_site_data =
      GetDataImpl(kDummyOrigin, &destroy_delegate_, &database_);

  const base::TimeDelta kInterval = base::TimeDelta::FromSeconds(1);
  const auto kIntervalInternalRepresentation =
      TestLocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
          kInterval);
  const auto kZeroIntervalInternalRepresentation =
      TestLocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
          base::TimeDelta());

  // The internal representation of a zero interval is expected to be equal to
  // zero as the protobuf use variable size integers and so storing zero values
  // is really efficient (uses only one bit).
  EXPECT_EQ(0U, kZeroIntervalInternalRepresentation);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  test_clock_.Advance(kInterval);
  // Makes use of a feature to make sure that the observation timestamps get
  // saved.
  local_site_data->NotifyUsesAudioInBackground();
  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);

  SiteDataProto expected_proto;

  auto expected_last_loaded_time =
      TestLocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
          kInterval + kInitialTimeSinceEpoch);

  expected_proto.set_last_loaded(expected_last_loaded_time);

  // Features that haven't been used should have an observation duration of
  // |kIntervalInternalRepresentation| and an observation timestamp equal to
  // zero.
  SiteDataFeatureProto unused_feature_proto;
  unused_feature_proto.set_observation_duration(
      kIntervalInternalRepresentation);

  expected_proto.mutable_updates_favicon_in_background()->CopyFrom(
      unused_feature_proto);
  expected_proto.mutable_updates_title_in_background()->CopyFrom(
      unused_feature_proto);
  expected_proto.mutable_uses_notifications_in_background()->CopyFrom(
      unused_feature_proto);

  // The audio feature has been used, so its observation duration value should
  // be equal to zero, and its observation timestamp should be equal to the last
  // loaded time in this case (as this feature has been used right before
  // unloading).
  SiteDataFeatureProto used_feature_proto;
  used_feature_proto.set_use_timestamp(expected_last_loaded_time);
  expected_proto.mutable_uses_audio_in_background()->CopyFrom(
      used_feature_proto);

  EXPECT_EQ(
      expected_proto.SerializeAsString(),
      local_site_data->site_characteristics_for_testing().SerializeAsString());
}

// Verify that the OnDestroyDelegate gets notified when a
// LocalSiteCharacteristicsDataImpl object gets destroyed.
TEST_F(LocalSiteCharacteristicsDataImplTest, DestroyNotifiesDelegate) {
  ::testing::StrictMock<
      testing::MockLocalSiteCharacteristicsDataImplOnDestroyDelegate>
      strict_delegate;
  {
    auto local_site_data =
        GetDataImpl(kDummyOrigin, &strict_delegate, &database_);
    EXPECT_CALL(strict_delegate, OnLocalSiteCharacteristicsDataImplDestroyed(
                                     local_site_data.get()));
  }
  ::testing::Mock::VerifyAndClear(&strict_delegate);
}

TEST_F(LocalSiteCharacteristicsDataImplTest,
       OnInitCallbackMergePreviousObservations) {
  // Use a mock database to intercept the initialization callback and save it
  // locally so it can be run later. This simulates an asynchronous
  // initialization of this object and is used to test that the observations
  // made between the time this object has been created and the callback is
  // called get properly merged.
  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase> mock_db;
  LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
      read_cb;

  auto local_site_data = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, &destroy_delegate_, &mock_db, &read_cb);

  // Simulates audio in background usage before the callback gets called.
  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data->NotifyUsesAudioInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesNotificationsInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesTitleInBackground());

  // Unload the site and save the last loaded time to make sure the
  // initialization doesn't overwrite it.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  auto last_loaded = local_site_data->last_loaded_time_for_testing();

  // Add a couple of performance samples.
  local_site_data->NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta::FromMicroseconds(100),
      base::TimeDelta::FromMicroseconds(1000), 2000u);
  local_site_data->NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta::FromMicroseconds(200),
      base::TimeDelta::FromMicroseconds(500), 1000u);

  // Make sure the local performance samples are averaged as expected.
  EXPECT_EQ(2U, local_site_data->load_duration().num_datums());
  EXPECT_EQ(150, local_site_data->load_duration().value());

  EXPECT_EQ(2U, local_site_data->cpu_usage_estimate().num_datums());
  EXPECT_EQ(750.0, local_site_data->cpu_usage_estimate().value());

  EXPECT_EQ(2U, local_site_data->private_footprint_kb_estimate().num_datums());
  EXPECT_EQ(1500.0, local_site_data->private_footprint_kb_estimate().value());

  // This protobuf should have a valid |last_loaded| field and valid observation
  // durations for each features, but the |use_timestamp| field shouldn't have
  // been initialized for the features that haven't been used.
  EXPECT_TRUE(
      local_site_data->site_characteristics_for_testing().has_last_loaded());
  EXPECT_TRUE(local_site_data->site_characteristics_for_testing()
                  .uses_audio_in_background()
                  .has_use_timestamp());
  EXPECT_FALSE(local_site_data->site_characteristics_for_testing()
                   .uses_notifications_in_background()
                   .has_use_timestamp());
  EXPECT_TRUE(local_site_data->site_characteristics_for_testing()
                  .uses_notifications_in_background()
                  .has_observation_duration());
  EXPECT_FALSE(local_site_data->site_characteristics_for_testing()
                   .has_load_time_estimates());

  // Initialize a fake protobuf that indicates that this site updates its title
  // while in background and set a fake last loaded time (this should be
  // overriden once the callback runs).
  base::Optional<SiteDataProto> test_proto = SiteDataProto();
  SiteDataFeatureProto unused_feature_proto = GetUnusedFeatureProto();
  test_proto->mutable_updates_title_in_background()->CopyFrom(
      GetUsedFeatureProto());
  test_proto->mutable_updates_favicon_in_background()->CopyFrom(
      unused_feature_proto);
  test_proto->mutable_uses_audio_in_background()->CopyFrom(
      unused_feature_proto);
  test_proto->mutable_uses_notifications_in_background()->CopyFrom(
      unused_feature_proto);
  test_proto->set_last_loaded(42);

  // Set the previously saved performance averages.
  auto* estimates = test_proto->mutable_load_time_estimates();
  estimates->set_avg_load_duration_us(50);
  estimates->set_avg_cpu_usage_us(250);
  estimates->set_avg_footprint_kb(500);

  // Run the callback to indicate that the initialization has completed.
  std::move(read_cb).Run(test_proto);

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesNotificationsInBackground());
  EXPECT_EQ(last_loaded, local_site_data->last_loaded_time_for_testing());

  // Make sure the local performance samples have been updated with the previous
  // averages.
  EXPECT_EQ(3U, local_site_data->load_duration().num_datums());
  EXPECT_EQ(137.5, local_site_data->load_duration().value());

  EXPECT_EQ(3U, local_site_data->cpu_usage_estimate().num_datums());
  EXPECT_EQ(562.5, local_site_data->cpu_usage_estimate().value());

  EXPECT_EQ(3U, local_site_data->private_footprint_kb_estimate().num_datums());
  EXPECT_EQ(1125, local_site_data->private_footprint_kb_estimate().value());


  // Verify that the in-memory data is flushed to the protobuffer on write.
  EXPECT_CALL(mock_db,
              WriteSiteCharacteristicsIntoDB(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const url::Origin& origin, const SiteDataProto& proto) {
            ASSERT_TRUE(proto.has_load_time_estimates());
            const auto& estimates = proto.load_time_estimates();
            ASSERT_TRUE(estimates.has_avg_load_duration_us());
            EXPECT_EQ(137.5, estimates.avg_load_duration_us());
            ASSERT_TRUE(estimates.has_avg_cpu_usage_us());
            EXPECT_EQ(562.5, estimates.avg_cpu_usage_us());
            ASSERT_TRUE(estimates.has_avg_footprint_kb());
            EXPECT_EQ(1125, estimates.avg_footprint_kb());
          }));

  local_site_data = nullptr;
  ::testing::Mock::VerifyAndClear(&mock_db);
}

TEST_F(LocalSiteCharacteristicsDataImplTest, LateAsyncReadDoesntEraseData) {
  // Ensure that no historical data get lost if an asynchronous read from the
  // database finishes after the last reference to a
  // LocalSiteCharacteristicsDataImpl gets destroyed.

  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase> mock_db;
  LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
      read_cb;

  auto local_site_data_writer = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, &destroy_delegate_, &mock_db, &read_cb);

  local_site_data_writer->NotifySiteLoaded();
  local_site_data_writer->NotifyLoadedSiteBackgrounded();
  local_site_data_writer->NotifyUsesAudioInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data_writer->UsesAudioInBackground());

  local_site_data_writer->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);

  // Releasing |local_site_data_writer| should cause this object to get
  // destroyed but there shouldn't be any write operation as the read hasn't
  // completed.
  EXPECT_CALL(destroy_delegate_,
              OnLocalSiteCharacteristicsDataImplDestroyed(::testing::_));
  EXPECT_CALL(mock_db,
              WriteSiteCharacteristicsIntoDB(::testing::_, ::testing::_))
      .Times(0);
  local_site_data_writer = nullptr;
  ::testing::Mock::VerifyAndClear(&destroy_delegate_);
  ::testing::Mock::VerifyAndClear(&mock_db);

  EXPECT_TRUE(read_cb.IsCancelled());
}

TEST_F(LocalSiteCharacteristicsDataImplTest,
       LateAsyncReadDoesntBypassClearEvent) {
  ::testing::NiceMock<MockLocalSiteCharacteristicsDatabase> mock_db;
  LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
      read_cb;

  auto local_site_data = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, &destroy_delegate_, &mock_db, &read_cb);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data->NotifyUsesAudioInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);

  // TODO(sebmarchand): Test that data is cleared here.
  local_site_data->ClearObservationsAndInvalidateReadOperation();

  EXPECT_TRUE(read_cb.IsCancelled());
}

TEST_F(LocalSiteCharacteristicsDataImplTest, BackgroundedCountTests) {
  auto local_site_data =
      GetDataImpl(kDummyOrigin, &destroy_delegate_, &database_);

  // By default the tabs are expected to be foregrounded.
  EXPECT_EQ(0U, local_site_data->loaded_tabs_in_background_count_for_testing());

  local_site_data->NotifySiteLoaded();
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  local_site_data->NotifyLoadedSiteBackgrounded();

  auto background_session_begin =
      local_site_data->background_session_begin_for_testing();
  EXPECT_EQ(test_clock_.NowTicks(), background_session_begin);

  EXPECT_EQ(1U, local_site_data->loaded_tabs_in_background_count_for_testing());

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  // Add a second instance of this object, this one pretending to be in
  // foreground.
  auto local_site_data_copy(local_site_data);
  local_site_data_copy->NotifySiteLoaded();
  EXPECT_EQ(1U, local_site_data->loaded_tabs_in_background_count_for_testing());

  EXPECT_EQ(background_session_begin,
            local_site_data->background_session_begin_for_testing());

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  local_site_data->NotifyLoadedSiteForegrounded();
  EXPECT_EQ(0U, local_site_data->loaded_tabs_in_background_count_for_testing());

  auto expected_observation_duration =
      test_clock_.NowTicks() - background_session_begin;

  auto observed_observation_duration =
      local_site_data->FeatureObservationDuration(
          local_site_data->site_characteristics_for_testing()
              .uses_notifications_in_background());

  EXPECT_EQ(expected_observation_duration, observed_observation_duration);

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  local_site_data->NotifyLoadedSiteBackgrounded();
  EXPECT_EQ(1U, local_site_data->loaded_tabs_in_background_count_for_testing());
  background_session_begin =
      local_site_data->background_session_begin_for_testing();
  EXPECT_EQ(test_clock_.NowTicks(), background_session_begin);

  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);
  local_site_data_copy->NotifySiteUnloaded(
      performance_manager::TabVisibility::kForeground);
}

TEST_F(LocalSiteCharacteristicsDataImplTest,
       OptionalFieldsNotPopulatedWhenClean) {
  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase> mock_db;
  LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
      read_cb;

  auto local_site_data = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, &destroy_delegate_, &mock_db, &read_cb);

  EXPECT_EQ(0u, local_site_data->cpu_usage_estimate().num_datums());
  EXPECT_EQ(0u, local_site_data->private_footprint_kb_estimate().num_datums());

  base::Optional<SiteDataProto> test_proto = SiteDataProto();

  // Run the callback to indicate that the initialization has completed.
  std::move(read_cb).Run(test_proto);

  // There still should be no perf data.
  EXPECT_EQ(0u, local_site_data->cpu_usage_estimate().num_datums());
  EXPECT_EQ(0u, local_site_data->private_footprint_kb_estimate().num_datums());

  // Dirty the record to force a write.
  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data->NotifyUsesAudioInBackground();

  // Verify that the saved protobuffer isn't populated with the perf fields.
  EXPECT_CALL(mock_db,
              WriteSiteCharacteristicsIntoDB(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const url::Origin& origin, const SiteDataProto& proto) {
            ASSERT_FALSE(proto.has_load_time_estimates());
          }));

  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);
  local_site_data = nullptr;
  ::testing::Mock::VerifyAndClear(&mock_db);
}

TEST_F(LocalSiteCharacteristicsDataImplTest,
       FlushingStateToProtoDoesntAffectData) {
  // Create 2 DataImpl object and do the same operations on them, ensures that
  // calling FlushStateToProto doesn't affect the data that gets recorded.

  auto local_site_data =
      GetDataImpl(kDummyOrigin, &destroy_delegate_, &database_);
  auto local_site_data_ref =
      GetDataImpl(kDummyOrigin2, &destroy_delegate_, &database_);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data_ref->NotifySiteLoaded();
  local_site_data_ref->NotifyLoadedSiteBackgrounded();

  test_clock_.Advance(base::TimeDelta::FromSeconds(15));
  local_site_data->FlushStateToProto();
  test_clock_.Advance(base::TimeDelta::FromSeconds(15));

  local_site_data->NotifyUsesAudioInBackground();
  local_site_data_ref->NotifyUsesAudioInBackground();

  local_site_data->FlushStateToProto();

  EXPECT_EQ(local_site_data->FeatureObservationTimestamp(
                local_site_data->site_characteristics_for_testing()
                    .uses_audio_in_background()),
            local_site_data_ref->FeatureObservationTimestamp(
                local_site_data_ref->site_characteristics_for_testing()
                    .uses_audio_in_background()));

  EXPECT_EQ(local_site_data->FeatureObservationDuration(
                local_site_data->site_characteristics_for_testing()
                    .updates_title_in_background()),
            local_site_data_ref->FeatureObservationDuration(
                local_site_data_ref->site_characteristics_for_testing()
                    .updates_title_in_background()));

  local_site_data->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);
  local_site_data_ref->NotifySiteUnloaded(
      performance_manager::TabVisibility::kBackground);
}

TEST_F(LocalSiteCharacteristicsDataImplTest, DataLoadedCallbackInvoked) {
  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase> mock_db;
  LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
      read_cb;

  auto local_site_data = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, &destroy_delegate_, &mock_db, &read_cb);

  EXPECT_FALSE(local_site_data->DataLoaded());

  bool callback_invoked = false;
  local_site_data->RegisterDataLoadedCallback(
      base::BindLambdaForTesting([&]() { callback_invoked = true; }));

  // Run the callback to indicate that the initialization has completed.
  base::Optional<SiteDataProto> test_proto = SiteDataProto();
  std::move(read_cb).Run(test_proto);

  EXPECT_TRUE(callback_invoked);
  EXPECT_TRUE(local_site_data->DataLoaded());
}

}  // namespace internal
}  // namespace resource_coordinator
