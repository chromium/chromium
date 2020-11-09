// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/intl_profile_watcher.h"

#include <fuchsia/intl/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::fuchsia::intl::Profile;

namespace base {

namespace {

const char kPrimaryTimeZoneName[] = "Australia/Darwin";
const char kSecondaryTimeZoneName[] = "Africa/Djibouti";

void CopyIdsToTimeZoneIds(
    const std::vector<std::string>& zone_ids,
    std::vector<::fuchsia::intl::TimeZoneId>* time_zone_ids) {
  time_zone_ids->clear();
  for (auto id : zone_ids) {
    ::fuchsia::intl::TimeZoneId time_zone_id;
    time_zone_id.id = id;
    time_zone_ids->push_back(time_zone_id);
  }
}

Profile CreateProfileWithTimeZones(const std::vector<std::string>& zone_ids) {
  Profile profile;
  std::vector<::fuchsia::intl::TimeZoneId> time_zone_ids;
  CopyIdsToTimeZoneIds(zone_ids, &time_zone_ids);
  profile.set_time_zones(time_zone_ids);
  return profile;
}

// Partial fake implementation of a PropertyProvider.
class FakePropertyProvider
    : public ::fuchsia::intl::testing::PropertyProvider_TestBase {
 public:
  explicit FakePropertyProvider(
      fidl::InterfaceRequest<::fuchsia::intl::PropertyProvider>
          provider_request)
      : binding_(this) {
    binding_.Bind(std::move(provider_request));
    DCHECK(binding_.is_bound());
  }
  FakePropertyProvider(const FakePropertyProvider&) = delete;
  FakePropertyProvider& operator=(const FakePropertyProvider&) = delete;
  ~FakePropertyProvider() override = default;

  void Close() { binding_.Close(ZX_ERR_PEER_CLOSED); }
  void SetTimeZones(const std::vector<std::string>& zone_ids) {
    CopyIdsToTimeZoneIds(zone_ids, &time_zone_ids_);
  }
  void NotifyChange() { binding_.events().OnChange(); }

  // PropertyProvider_TestBase implementation.
  void GetProfile(
      ::fuchsia::intl::PropertyProvider::GetProfileCallback callback) override {
    Profile profile;
    profile.set_time_zones(time_zone_ids_);
    callback(std::move(profile));
  }
  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unimplemented function called: " << name;
  }

 private:
  ::fidl::Binding<::fuchsia::intl::PropertyProvider> binding_;

  std::vector<::fuchsia::intl::TimeZoneId> time_zone_ids_;
};

class FakePropertyProviderAsync {
 public:
  explicit FakePropertyProviderAsync(
      fidl::InterfaceRequest<::fuchsia::intl::PropertyProvider>
          provider_request)
      : thread_("Property Provider Thread") {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    CHECK(thread_.StartWithOptions(options));
    property_provider_ = base::SequenceBound<FakePropertyProvider>(
        thread_.task_runner(), std::move(provider_request));
  }
  FakePropertyProviderAsync(const FakePropertyProviderAsync&) = delete;
  FakePropertyProviderAsync& operator=(const FakePropertyProviderAsync&) =
      delete;
  ~FakePropertyProviderAsync() = default;

  void Close() {
    property_provider_.Post(FROM_HERE, &FakePropertyProvider::Close);
  }
  void SetTimeZones(const std::vector<std::string>& zone_ids) {
    property_provider_.Post(FROM_HERE, &FakePropertyProvider::SetTimeZones,
                            zone_ids);
  }
  void NotifyChange() {
    property_provider_.Post(FROM_HERE, &FakePropertyProvider::NotifyChange);
  }

 private:
  base::Thread thread_;
  base::SequenceBound<FakePropertyProvider> property_provider_;
};

}  // namespace

class GetPrimaryTimeZoneIdFromPropertyProviderTest : public testing::Test {
 public:
  GetPrimaryTimeZoneIdFromPropertyProviderTest()
      : property_provider_(property_provider_ptr_.NewRequest()) {}
  GetPrimaryTimeZoneIdFromPropertyProviderTest(
      const GetPrimaryTimeZoneIdFromPropertyProviderTest&) = delete;
  GetPrimaryTimeZoneIdFromPropertyProviderTest& operator=(
      const GetPrimaryTimeZoneIdFromPropertyProviderTest&) = delete;
  ~GetPrimaryTimeZoneIdFromPropertyProviderTest() override = default;

 protected:
  static std::string GetPrimaryTimeZoneIdFromPropertyProvider(
      ::fuchsia::intl::PropertyProviderSyncPtr property_provider) {
    return FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdFromPropertyProvider(
        std::move(property_provider));
  }

  ::fuchsia::intl::PropertyProviderSyncPtr property_provider_ptr_;
  FakePropertyProviderAsync property_provider_;
};

class IntlProfileWatcherTest : public testing::Test {
 public:
  IntlProfileWatcherTest()
      : property_provider_(property_provider_ptr_.NewRequest()) {}
  IntlProfileWatcherTest(const IntlProfileWatcherTest&) = delete;
  IntlProfileWatcherTest& operator=(const IntlProfileWatcherTest&) = delete;
  ~IntlProfileWatcherTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<FuchsiaIntlProfileWatcher> CreateIntlProfileWatcher(
      FuchsiaIntlProfileWatcher::ProfileChangeCallback on_profile_changed) {
    return base::WrapUnique(new FuchsiaIntlProfileWatcher(
        std::move(property_provider_ptr_), std::move(on_profile_changed)));
  }

  ::fuchsia::intl::PropertyProviderPtr property_provider_ptr_;
  FakePropertyProviderAsync property_provider_;

  base::RunLoop run_loop_;
};

// Unit tests are run in an environment where intl is not provided.
// However, this is not exposed by the API.
TEST(IntlServiceNotAvailableTest, FuchsiaIntlProfileWatcher) {
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::RunLoop run_loop;

  base::MockCallback<FuchsiaIntlProfileWatcher::ProfileChangeCallback>
      on_profile_changed;
  EXPECT_CALL(on_profile_changed, Run(testing::_)).Times(0);
  auto watcher =
      std::make_unique<FuchsiaIntlProfileWatcher>(on_profile_changed.Get());
  EXPECT_TRUE(watcher);

  run_loop.RunUntilIdle();
}

TEST_F(GetPrimaryTimeZoneIdFromPropertyProviderTest, RemoteNotBound) {
  // Simulate the service not actually being available.
  property_provider_.Close();
  EXPECT_STREQ("", GetPrimaryTimeZoneIdFromPropertyProvider(
                       std::move(property_provider_ptr_))
                       .c_str());
}

TEST_F(GetPrimaryTimeZoneIdFromPropertyProviderTest, NoZones) {
  EXPECT_STREQ("", GetPrimaryTimeZoneIdFromPropertyProvider(
                       std::move(property_provider_ptr_))
                       .c_str());
}

TEST_F(GetPrimaryTimeZoneIdFromPropertyProviderTest, SingleZone) {
  property_provider_.SetTimeZones({kPrimaryTimeZoneName});
  EXPECT_STREQ(kPrimaryTimeZoneName, GetPrimaryTimeZoneIdFromPropertyProvider(
                                         std::move(property_provider_ptr_))
                                         .c_str());
}

TEST_F(GetPrimaryTimeZoneIdFromPropertyProviderTest, SingleZoneIsEmpty) {
  property_provider_.SetTimeZones({""});
  EXPECT_STREQ("", GetPrimaryTimeZoneIdFromPropertyProvider(
                       std::move(property_provider_ptr_))
                       .c_str());
}

TEST_F(GetPrimaryTimeZoneIdFromPropertyProviderTest, MoreThanOneZone) {
  property_provider_.SetTimeZones(
      {kPrimaryTimeZoneName, kSecondaryTimeZoneName});
  EXPECT_STREQ(kPrimaryTimeZoneName, GetPrimaryTimeZoneIdFromPropertyProvider(
                                         std::move(property_provider_ptr_))
                                         .c_str());
}

TEST_F(IntlProfileWatcherTest, NoZones_NoNotification) {
  base::MockCallback<FuchsiaIntlProfileWatcher::ProfileChangeCallback> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  auto watcher = CreateIntlProfileWatcher(callback.Get());
  run_loop_.RunUntilIdle();
}

TEST_F(IntlProfileWatcherTest, ChangeNotification_AfterInitialization) {
  auto watcher = CreateIntlProfileWatcher(base::BindLambdaForTesting(
      [quit_loop = run_loop_.QuitClosure()](const Profile& profile) {
        EXPECT_EQ(kPrimaryTimeZoneName,
                  FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdFromProfile(
                      profile));
        quit_loop.Run();
      }));

  property_provider_.SetTimeZones({kPrimaryTimeZoneName});
  property_provider_.NotifyChange();

  run_loop_.Run();
}

TEST_F(IntlProfileWatcherTest, ChangeNotification_BeforeInitialization) {
  property_provider_.SetTimeZones({kPrimaryTimeZoneName});
  property_provider_.NotifyChange();

  auto watcher = CreateIntlProfileWatcher(base::BindLambdaForTesting(
      [quit_loop = run_loop_.QuitClosure()](const Profile& profile) {
        EXPECT_EQ(kPrimaryTimeZoneName,
                  FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdFromProfile(
                      profile));
        quit_loop.Run();
      }));

  run_loop_.Run();
}

// Ensure no crash when the peer service cannot be reached during creation.
TEST_F(IntlProfileWatcherTest, ChannelClosedBeforeCreation) {
  base::MockCallback<FuchsiaIntlProfileWatcher::ProfileChangeCallback> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(0);

  property_provider_.Close();

  auto watcher = CreateIntlProfileWatcher(callback.Get());

  property_provider_.NotifyChange();
  run_loop_.RunUntilIdle();
}

// Ensure no crash when the channel is closed after creation.
TEST_F(IntlProfileWatcherTest, ChannelClosedAfterCreation) {
  base::MockCallback<FuchsiaIntlProfileWatcher::ProfileChangeCallback> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(0);

  auto watcher = CreateIntlProfileWatcher(callback.Get());

  property_provider_.Close();

  property_provider_.NotifyChange();
  run_loop_.RunUntilIdle();
}

TEST(IntlProfileWatcherGetPrimaryTimeZoneIdFromProfileTest, NoZones) {
  EXPECT_EQ("", FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdFromProfile(
                    Profile()));
}

TEST(IntlProfileWatcherGetPrimaryTimeZoneIdFromProfileTest, EmptyZonesList) {
  EXPECT_EQ("", FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdFromProfile(
                    CreateProfileWithTimeZones({})));
}

TEST(IntlProfileWatcherGetPrimaryTimeZoneIdFromProfileTest, OneZone) {
  EXPECT_EQ(kPrimaryTimeZoneName,
            FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdFromProfile(
                CreateProfileWithTimeZones({kPrimaryTimeZoneName})));
}

TEST(IntlProfileWatcherGetPrimaryTimeZoneIdFromProfileTest, TwoZones) {
  EXPECT_EQ(kPrimaryTimeZoneName,
            FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdFromProfile(
                CreateProfileWithTimeZones(
                    {kPrimaryTimeZoneName, kSecondaryTimeZoneName})));
}

}  // namespace base
