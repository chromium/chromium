// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/device_command_fetch_crd_availability_info_job.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/fake_cros_network_config.h"
#include "chrome/browser/ash/policy/remote_commands/user_session_type_test_util.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

using base::test::DictionaryHasValue;
using base::test::ParseJson;
using base::test::ParseJsonDict;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::OncSource;
using enterprise_management::CrdSessionAvailability;
using enterprise_management::RemoteCommand;
using test::SessionTypeToString;
using test::TestSessionType;
using testing::Not;

constexpr int64_t kUniqueID = 111222333444;
constexpr TestSessionType kAnySessionType = TestSessionType::kGuestSession;

RemoteCommand GenerateCommandProto() {
  RemoteCommand command_proto;
  command_proto.set_type(RemoteCommand::FETCH_CRD_AVAILABILITY_INFO);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(0);
  command_proto.set_payload("");
  return command_proto;
}

struct Result {
  RemoteCommandJob::Status status;
  std::string payload;
};

test::NetworkBuilder CreateNetwork(NetworkType type = NetworkType::kWiFi) {
  return test::NetworkBuilder(type);
}

base::Value::List ToList(std::vector<CrdSessionType> input) {
  base::Value::List result;
  for (CrdSessionType type : input) {
    result.Append(static_cast<int>(type));
  }
  return result;
}

MATCHER_P(ListContains, expected_type, "") {
  base::Value expected_value(expected_type);
  base::Value::List* list = arg;

  if (!list) {
    *result_listener << "List is null";
    return false;
  }

  for (const base::Value& element : *list) {
    if (element == expected_value) {
      return true;
    }
  }

  *result_listener << "Actual list content: " << list->DebugString();
  return false;
}

}  // namespace

class DeviceCommandFetchCrdAvailabilityInfoJobTest
    : public ash::DeviceSettingsTestBase {
 public:
  DeviceCommandFetchCrdAvailabilityInfoJobTest()
      // We use `TimeSource::MOCK_TIME` because both `SetDeviceIdleTime` and
      // `GetDeviceIdleTime` use `base::TimeTicks::Now()`, and our tests rely on
      // this being the same time twice.
      : ash::DeviceSettingsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // `ash::DeviceSettingsTestBase` implementation:
  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());

    user_activity_detector_ = ui::UserActivityDetector::Get();
    web_kiosk_app_manager_ = std::make_unique<ash::WebKioskAppManager>();
    kiosk_chrome_app_manager_ = std::make_unique<ash::KioskChromeAppManager>();
  }

  void TearDown() override {
    kiosk_chrome_app_manager_.reset();
    web_kiosk_app_manager_.reset();
    DeviceSettingsTestBase::TearDown();
  }

  Result CreateAndRunJob() {
    DeviceCommandFetchCrdAvailabilityInfoJob job;

    bool initialized = job.Init(base::TimeTicks::Now(), GenerateCommandProto(),
                                enterprise_management::SignedData());
    if (!initialized) {
      ADD_FAILURE() << "Failed to initialize job";
      return Result{};
    }

    base::test::TestFuture<void> done_signal_;
    bool launched = job.Run(base::Time::Now(), base::TimeTicks::Now(),
                            done_signal_.GetCallback());
    if (!launched) {
      ADD_FAILURE() << "Failed to launch job";
      return Result{};
    }
    EXPECT_TRUE(done_signal_.Wait());

    std::string response_payload =
        job.GetResultPayload() ? *job.GetResultPayload() : "{}";
    return Result{job.status(), response_payload};
  }

  void SetDeviceIdleTime(int idle_time_in_sec) {
    user_activity_detector_->set_last_activity_time_for_test(
        base::TimeTicks::Now() - base::Seconds(idle_time_in_sec));
  }

  void SetLastDeviceActivityTime(base::TimeTicks value) {
    user_activity_detector_->set_last_activity_time_for_test(value);
  }

  ash::FakeChromeUserManager& user_manager() { return *user_manager_; }

  test::FakeCrosNetworkConfig& fake_cros_network_config() {
    return fake_cros_network_config_;
  }

  void AddActiveManagedNetwork() {
    fake_cros_network_config().AddActiveNetwork(
        CreateNetwork(NetworkType::kWiFi)
            .SetOncSource(OncSource::kDevicePolicy));
  }

  void StartSessionOfType(TestSessionType user_session_type) {
    test::StartSessionOfType(user_session_type, user_manager());
  }

  void EnablePref(const char* pref_name) {
    profile_manager_.local_state()->Get()->SetBoolean(pref_name, true);
  }

  void DisablePref(const char* pref_name) {
    profile_manager_.local_state()->Get()->SetBoolean(pref_name, false);
  }

 private:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_{std::make_unique<ash::FakeChromeUserManager>()};

  std::unique_ptr<ash::WebKioskAppManager> web_kiosk_app_manager_;
  std::unique_ptr<ash::KioskChromeAppManager> kiosk_chrome_app_manager_;

  // Automatically installed as a singleton upon creation.
  raw_ptr<ui::UserActivityDetector> user_activity_detector_;

  test::ScopedFakeCrosNetworkConfig fake_cros_network_config_;

  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

// Fixture for tests parameterized over the possible session types
// (`TestSessionType`).
class DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType
    : public DeviceCommandFetchCrdAvailabilityInfoJobTest,
      public ::testing::WithParamInterface<TestSessionType> {
 public:
  CrdSessionAvailability GetExpectedRemoteAccessAvailabilityFor(
      TestSessionType session_type) {
    switch (session_type) {
      case TestSessionType::kNoSession:
        return CrdSessionAvailability::AVAILABLE;

      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
        return CrdSessionAvailability::
            UNAVAILABLE_UNSUPPORTED_USER_SESSION_TYPE;
    }
  }

  CrdSessionAvailability GetExpectedRemoteSupportAvailabilityFor(
      TestSessionType session_type) {
    switch (session_type) {
      case TestSessionType::kNoSession:
      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
        return CrdSessionAvailability::
            UNAVAILABLE_UNSUPPORTED_USER_SESSION_TYPE;

      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
        return CrdSessionAvailability::AVAILABLE;
    }
  }
};

TEST_F(DeviceCommandFetchCrdAvailabilityInfoJobTest, GetType) {
  DeviceCommandFetchCrdAvailabilityInfoJob job;
  EXPECT_EQ(job.GetType(), RemoteCommand::FETCH_CRD_AVAILABILITY_INFO);
}

TEST_F(DeviceCommandFetchCrdAvailabilityInfoJobTest,
       ShouldReturnDeviceIdleTime) {
  const int device_idle_time_in_sec = 8;

  SetDeviceIdleTime(device_idle_time_in_sec);
  Result result = CreateAndRunJob();

  EXPECT_THAT(ParseJsonDict(result.payload),
              DictionaryHasValue("deviceIdleTimeInSeconds",
                                 base::Value(device_idle_time_in_sec)));
}

TEST_F(DeviceCommandFetchCrdAvailabilityInfoJobTest,
       ShouldReturnMaxIntDeviceIdleTimeIfThereWasNoActivitySinceBoot) {
  base::TimeTicks never;
  ASSERT_TRUE(never.is_null());
  SetLastDeviceActivityTime(never);

  Result result = CreateAndRunJob();

  EXPECT_THAT(
      ParseJsonDict(result.payload),
      DictionaryHasValue("deviceIdleTimeInSeconds",
                         base::Value(std::numeric_limits<int32_t>::max())));
}

TEST_F(DeviceCommandFetchCrdAvailabilityInfoJobTest,
       ShouldReturnIsInManagedEnvironmentTrue) {
  fake_cros_network_config().SetActiveNetworks(
      {CreateNetwork().SetOncSource(OncSource::kDevicePolicy)});

  Result result = CreateAndRunJob();

  EXPECT_THAT(ParseJsonDict(result.payload),
              DictionaryHasValue("isInManagedEnvironment", base::Value(true)));
}

TEST_F(DeviceCommandFetchCrdAvailabilityInfoJobTest,
       ShouldReturnIsInManagedEnvironmentFalse) {
  fake_cros_network_config().SetActiveNetworks(
      {CreateNetwork().SetOncSource(OncSource::kNone)});

  Result result = CreateAndRunJob();

  EXPECT_THAT(ParseJsonDict(result.payload),
              DictionaryHasValue("isInManagedEnvironment", base::Value(false)));
}

TEST_F(DeviceCommandFetchCrdAvailabilityInfoJobTest,
       ShouldRespectDisabledByPolicy) {
  StartSessionOfType(kAnySessionType);

  DisablePref(prefs::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections);

  Result result = CreateAndRunJob();

  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteAccessAvailability"),
            CrdSessionAvailability::UNAVAILABLE_DISABLED_BY_POLICY);
  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteSupportAvailability"),
            CrdSessionAvailability::UNAVAILABLE_DISABLED_BY_POLICY);
}

TEST_P(DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
       ShouldReturnUserSessionType) {
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();

  UserSessionType expected = [&]() {
    switch (session_type) {
      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
        return UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION;
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
        return UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION;
      case TestSessionType::kManagedGuestSession:
        return UserSessionType::MANAGED_GUEST_SESSION;
      case TestSessionType::kGuestSession:
        return UserSessionType::GUEST_SESSION;
      case TestSessionType::kAffiliatedUserSession:
        return UserSessionType::AFFILIATED_USER_SESSION;
      case TestSessionType::kUnaffiliatedUserSession:
        return UserSessionType::UNAFFILIATED_USER_SESSION;
      case TestSessionType::kNoSession:
        return UserSessionType::NO_SESSION;
    }
  }();

  EXPECT_THAT(ParseJsonDict(result.payload),
              DictionaryHasValue("userSessionType",
                                 base::Value(static_cast<int>(expected))));
}

TEST_P(DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
       ShouldReturnSupportedCrdSessionTypes) {
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  StartSessionOfType(session_type);

  AddActiveManagedNetwork();
  Result result = CreateAndRunJob();

  const base::Value::List expected = [&]() {
    switch (session_type) {
      case TestSessionType::kNoSession:
        return ToList({CrdSessionType::REMOTE_ACCESS_SESSION});

      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
        return ToList({CrdSessionType::REMOTE_SUPPORT_SESSION});

      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
        return ToList({});
    }
  }();

  const base::Value::List actual = ParseJsonDict(result.payload)
                                       .EnsureList("supportedCrdSessionTypes")
                                       ->Clone();

  EXPECT_EQ(actual, expected);
}

TEST_P(DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
       ShouldNotSupportRemoteAccessWithoutManagedNetworks) {
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  fake_cros_network_config().SetActiveNetworks(
      {CreateNetwork().SetOncSource(OncSource::kNone)});

  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();

  EXPECT_THAT(
      ParseJsonDict(result.payload).FindList("supportedCrdSessionTypes"),
      Not(ListContains(
          static_cast<int>(CrdSessionType::REMOTE_ACCESS_SESSION))));

  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteAccessAvailability"),
            CrdSessionAvailability::UNAVAILABLE_UNMANAGED_ENVIRONMENT);
}

TEST_P(DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
       ShouldReturnRemoteSupportAvailability) {
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();

  const CrdSessionAvailability expected = [&]() {
    switch (session_type) {
      case TestSessionType::kNoSession:
      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
        return CrdSessionAvailability::
            UNAVAILABLE_UNSUPPORTED_USER_SESSION_TYPE;

      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
        return CrdSessionAvailability::AVAILABLE;
    }
  }();

  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteSupportAvailability"),
            expected);
}

TEST_P(
    DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
    DeviceRemoteAccessPolicyShouldNotEffectRemoteSupportAvailabilityIfDisabled) {
  DisablePref(prefs::kDeviceAllowEnterpriseRemoteAccessConnections);
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();

  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteSupportAvailability"),
            GetExpectedRemoteSupportAvailabilityFor(GetParam()));
}

TEST_P(
    DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
    DeviceRemoteAccessPolicyShouldNotEffectRemoteSupportAvailabilityIfEnabled) {
  EnablePref(prefs::kDeviceAllowEnterpriseRemoteAccessConnections);
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();

  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteSupportAvailability"),
            GetExpectedRemoteSupportAvailabilityFor(GetParam()));
}

TEST_P(DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
       ShouldRespectDisabledByDeviceRemoteAccessPolicy) {
  DisablePref(prefs::kDeviceAllowEnterpriseRemoteAccessConnections);
  TestSessionType session_type = GetParam();
  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();

  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteAccessAvailability"),
            CrdSessionAvailability::UNAVAILABLE_DISABLED_BY_POLICY);
}

TEST_P(DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
       ShouldReturnRemoteAccessAvailability) {
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  AddActiveManagedNetwork();
  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();

  const CrdSessionAvailability expected = [&]() {
    switch (session_type) {
      case TestSessionType::kNoSession:
        return CrdSessionAvailability::AVAILABLE;

      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
        return CrdSessionAvailability::
            UNAVAILABLE_UNSUPPORTED_USER_SESSION_TYPE;
    }
  }();

  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteAccessAvailability"),
            expected);
}

TEST_P(DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
       ShouldReturnRemoteAccessAvailabilityWhenPolicyIsNotSet) {
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  AddActiveManagedNetwork();
  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();
  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteAccessAvailability"),
            GetExpectedRemoteAccessAvailabilityFor(GetParam()));
}

TEST_P(DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
       ShouldReturnRemoteAccessAvailabilityWhenPolicyIsEnabled) {
  EnablePref(prefs::kDeviceAllowEnterpriseRemoteAccessConnections);
  TestSessionType session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(session_type)));

  AddActiveManagedNetwork();
  StartSessionOfType(session_type);

  Result result = CreateAndRunJob();
  EXPECT_EQ(ParseJsonDict(result.payload).FindInt("remoteAccessAvailability"),
            GetExpectedRemoteAccessAvailabilityFor(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceCommandFetchCrdAvailabilityInfoJobTestParameterizedOverSessionType,
    ::testing::Values(TestSessionType::kManuallyLaunchedWebKioskSession,
                      TestSessionType::kManuallyLaunchedKioskSession,
                      TestSessionType::kAutoLaunchedWebKioskSession,
                      TestSessionType::kAutoLaunchedKioskSession,
                      TestSessionType::kManagedGuestSession,
                      TestSessionType::kGuestSession,
                      TestSessionType::kAffiliatedUserSession,
                      TestSessionType::kUnaffiliatedUserSession,
                      TestSessionType::kNoSession));

}  // namespace policy
