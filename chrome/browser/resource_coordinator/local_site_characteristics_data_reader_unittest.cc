// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_reader.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

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
    OnReadSiteCharacteristicsFromDB(std::move(origin), callback);
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

void InitializeSiteDataProto(SiteDataProto* site_characteristics) {
  DCHECK(site_characteristics);
  site_characteristics->set_last_loaded(42);

  SiteDataFeatureProto used_feature_proto;
  used_feature_proto.set_observation_duration(0U);
  used_feature_proto.set_use_timestamp(1U);

  site_characteristics->mutable_updates_favicon_in_background()->CopyFrom(
      used_feature_proto);
  site_characteristics->mutable_updates_title_in_background()->CopyFrom(
      used_feature_proto);
  site_characteristics->mutable_uses_audio_in_background()->CopyFrom(
      used_feature_proto);
  site_characteristics->mutable_uses_notifications_in_background()->CopyFrom(
      used_feature_proto);

  DCHECK(site_characteristics->IsInitialized());
}

}  // namespace

class LocalSiteCharacteristicsDataReaderTest : public ::testing::Test {
 protected:
  // The constructors needs to call 'new' directly rather than using the
  // base::MakeRefCounted helper function because the constructor of
  // LocalSiteCharacteristicsDataImpl is protected and not visible to
  // base::MakeRefCounted.
  LocalSiteCharacteristicsDataReaderTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    test_impl_ =
        base::WrapRefCounted(new internal::LocalSiteCharacteristicsDataImpl(
            url::Origin::Create(GURL("foo.com")), &delegate_, &database_));
    test_impl_->NotifySiteLoaded();
    test_impl_->NotifyLoadedSiteBackgrounded();
    LocalSiteCharacteristicsDataReader* reader =
        new LocalSiteCharacteristicsDataReader(test_impl_.get());
    reader_ = base::WrapUnique(reader);
  }

  ~LocalSiteCharacteristicsDataReaderTest() override {
    test_impl_->NotifySiteUnloaded(
        performance_manager::TabVisibility::kBackground);
  }

  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;

  // The mock delegate used by the LocalSiteCharacteristicsDataImpl objects
  // created by this class, NiceMock is used to avoid having to set expectations
  // in test cases that don't care about this.
  ::testing::NiceMock<
      testing::MockLocalSiteCharacteristicsDataImplOnDestroyDelegate>
      delegate_;

  // The LocalSiteCharacteristicsDataImpl object used in these tests.
  scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> test_impl_;

  // A LocalSiteCharacteristicsDataReader object associated with the origin used
  // to create this object.
  std::unique_ptr<LocalSiteCharacteristicsDataReader> reader_;

  testing::NoopLocalSiteCharacteristicsDatabase database_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataReaderTest);
};

TEST_F(LocalSiteCharacteristicsDataReaderTest, TestAccessors) {
  // Initially we have no information about any of the features.
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UsesNotificationsInBackground());

  // Simulates a title update event, make sure it gets reported directly.
  test_impl_->NotifyUpdatesTitleInBackground();

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader_->UpdatesTitleInBackground());

  // Advance the clock by a large amount of time, enough for the unused features
  // observation windows to expire.
  test_clock_.Advance(base::TimeDelta::FromDays(31));

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader_->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader_->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader_->UsesAudioInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader_->UsesNotificationsInBackground());
}

TEST_F(LocalSiteCharacteristicsDataReaderTest,
       FreeingReaderDoesntCauseWriteOperation) {
  const url::Origin kOrigin = url::Origin::Create(GURL("foo.com"));
  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase> database;

  // Override the read callback to simulate a successful read from the
  // database.
  SiteDataProto proto = {};
  InitializeSiteDataProto(&proto);
  auto read_from_db_mock_impl =
      [&](const url::Origin& origin,
          LocalSiteCharacteristicsDatabase::
              ReadSiteCharacteristicsFromDBCallback& callback) {
        std::move(callback).Run(base::Optional<SiteDataProto>(proto));
      };

  EXPECT_CALL(database, OnReadSiteCharacteristicsFromDB(
                            ::testing::Property(&url::Origin::Serialize,
                                                kOrigin.Serialize()),
                            ::testing::_))
      .WillOnce(::testing::Invoke(read_from_db_mock_impl));

  std::unique_ptr<LocalSiteCharacteristicsDataReader> reader =
      base::WrapUnique(new LocalSiteCharacteristicsDataReader(
          base::WrapRefCounted(new internal::LocalSiteCharacteristicsDataImpl(
              kOrigin, &delegate_, &database))));
  ::testing::Mock::VerifyAndClear(&database);

  EXPECT_TRUE(reader->impl_for_testing()->fully_initialized_for_testing());

  // Resetting the reader shouldn't cause any write operation to the database.
  EXPECT_CALL(database,
              WriteSiteCharacteristicsIntoDB(::testing::_, ::testing::_))
      .Times(0);
  reader.reset();
  ::testing::Mock::VerifyAndClear(&database);
}

TEST_F(LocalSiteCharacteristicsDataReaderTest, OnDataLoadedCallbackInvoked) {
  const url::Origin kOrigin = url::Origin::Create(GURL("foo.com"));
  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase> database;

  // Create the impl.
  EXPECT_CALL(database, OnReadSiteCharacteristicsFromDB(
                            ::testing::Property(&url::Origin::Serialize,
                                                kOrigin.Serialize()),
                            ::testing::_));
  scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl =
      base::WrapRefCounted(new internal::LocalSiteCharacteristicsDataImpl(
          kOrigin, &delegate_, &database));

  // Create the reader.
  std::unique_ptr<LocalSiteCharacteristicsDataReader> reader =
      base::WrapUnique(new LocalSiteCharacteristicsDataReader(impl));
  EXPECT_FALSE(reader->DataLoaded());

  // Register a data ready closure.
  bool on_data_loaded = false;
  reader->RegisterDataLoadedCallback(base::BindLambdaForTesting(
      [&on_data_loaded]() { on_data_loaded = true; }));

  // Transition the impl to fully initialized, which should cause the callbacks
  // to fire.
  EXPECT_FALSE(impl->DataLoaded());
  EXPECT_FALSE(on_data_loaded);
  impl->TransitionToFullyInitialized();
  EXPECT_TRUE(impl->DataLoaded());
  EXPECT_TRUE(on_data_loaded);
}

TEST_F(LocalSiteCharacteristicsDataReaderTest,
       DestroyingReaderCancelsPendingCallbacks) {
  const url::Origin kOrigin = url::Origin::Create(GURL("foo.com"));
  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase> database;

  // Create the impl.
  EXPECT_CALL(database, OnReadSiteCharacteristicsFromDB(
                            ::testing::Property(&url::Origin::Serialize,
                                                kOrigin.Serialize()),
                            ::testing::_));
  scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl =
      base::WrapRefCounted(new internal::LocalSiteCharacteristicsDataImpl(
          kOrigin, &delegate_, &database));

  // Create the reader.
  std::unique_ptr<LocalSiteCharacteristicsDataReader> reader =
      base::WrapUnique(new LocalSiteCharacteristicsDataReader(impl));
  EXPECT_FALSE(reader->DataLoaded());

  // Register a data ready closure.
  reader->RegisterDataLoadedCallback(
      base::MakeExpectedNotRunClosure(FROM_HERE));

  // Reset the reader.
  reader.reset();

  // Transition the impl to fully initialized, which should cause the callbacks
  // to fire. The reader's callback should *not* be invoked.
  EXPECT_FALSE(impl->DataLoaded());
  impl->TransitionToFullyInitialized();
  EXPECT_TRUE(impl->DataLoaded());
}

}  // namespace resource_coordinator
