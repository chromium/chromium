// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/auto_enrollment_client_impl.h"

#include <stdint.h>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace em = enterprise_management;
namespace psm_rlwe = private_membership::rlwe;

namespace policy {

namespace {

const char kStateKey[] = "state_key";
const char kStateKeyHash[] =
    "\xde\x74\xcd\xf0\x03\x36\x8c\x21\x79\xba\xb1\x5a\xc4\x32\xee\xd6"
    "\xb3\x4a\x5e\xff\x73\x7e\x92\xd9\xf8\x6e\x72\x44\xd0\x97\xc3\xe6";
const char kDisabledMessage[] = "This device has been disabled.";

const char kSerialNumber[] = "SN123456";
const char kBrandCode[] = "AABC";
const char kInitialEnrollmentIdHash[] = "\x30\x18\xb7\x0f\x76\x09\xc5\xc7";

const int kInitialEnrollmentIdHashLength = 8;

const bool kNotWithLicense = false;
const bool kWithLicense = true;

// This is modulus power value used in initial enrollment to detect that the
// server is outdated and does not support initial enrollment. See the
// |DetectOutdatedServer| test case.
const int kInitialEnrollmentModulusPowerOutdatedServer = 14;

// Start and limit powers for the hash dance clients.
const int kPowerStart = 4;
const int kPowerLimit = 8;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;

// Number of test cases exist in cros_test_data.binarypb file, which is part of
// private_membership third_party library.
const int kNumberOfPrivateSetMembershipTestCases = 10;

// Invalid test case index which acts as a dummy value when the private set
// membership is disabled.
const int kInvalidPrivateSetMembershipTestCaseIndex = -1;

// PrivateSetMembership regression tests maximum file size which is 4MB.
const size_t kMaxFileSizeInBytes = 4 * 1024 * 1024;

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto) {
  if (!out_proto) {
    return false;
  }

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_content,
                                         kMaxFileSizeInBytes)) {
    return false;
  }

  return out_proto->ParseFromString(file_content);
}

enum class AutoEnrollmentProtocol { kFRE = 0, kInitialEnrollment = 1 };

enum class PrivateSetMembershipState { kEnabled = 0, kDisabled = 1 };

// Holds the state of the AutoEnrollmentClientImplTest and its subclass i.e.
// PrivateSetMembershipHelperTest. It will be used to run their tests with
// different values.
struct AutoEnrollmentClientImplTestState final {
  AutoEnrollmentClientImplTestState(
      AutoEnrollmentProtocol auto_enrollment_protocol,
      PrivateSetMembershipState private_set_membership_state)
      : auto_enrollment_protocol(auto_enrollment_protocol),
        private_set_membership_state(private_set_membership_state) {}

  AutoEnrollmentProtocol auto_enrollment_protocol;
  PrivateSetMembershipState private_set_membership_state;
};

// The integer parameter represents the index of private set membership test
// case.
class AutoEnrollmentClientImplTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<AutoEnrollmentClientImplTestState, int>> {
 protected:
  AutoEnrollmentClientImplTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()),
        local_state_(scoped_testing_local_state_.Get()),
        state_(AUTO_ENROLLMENT_STATE_PENDING) {}

  void SetUpCommandLine(base::CommandLine* command_line) const {
    // Disable private set membership switch when its protocol state param is
    // kDisabled.
    if (GetPrivateSetMembershipState() ==
        PrivateSetMembershipState::kDisabled) {
      command_line->AppendSwitchASCII(
          chromeos::switches::kEnterpriseEnablePrivateSetMembership,
          chromeos::AutoEnrollmentController::kEnablePsmNever);
    }
  }

  void SetUp() override {
    SetUpCommandLine(base::CommandLine::ForCurrentProcess());
    CreateClient(kPowerStart, kPowerLimit);
    ASSERT_FALSE(local_state_->GetUserPref(prefs::kShouldAutoEnroll));
    ASSERT_FALSE(local_state_->GetUserPref(prefs::kAutoEnrollmentPowerLimit));
  }

  void TearDown() override {
    // Flush any deletion tasks.
    base::RunLoop().RunUntilIdle();
  }

  AutoEnrollmentProtocol GetAutoEnrollmentProtocol() const {
    return std::get<0>(GetParam()).auto_enrollment_protocol;
  }

  PrivateSetMembershipState GetPrivateSetMembershipState() const {
    return std::get<0>(GetParam()).private_set_membership_state;
  }

  int GetPrivateSetMembershipTestCaseIndex() const {
    return std::get<1>(GetParam());
  }

  void CreateClient(int power_initial, int power_limit) {
    state_ = AUTO_ENROLLMENT_STATE_PENDING;
    service_.reset(new MockDeviceManagementService());
    service_->ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();

    auto progress_callback =
        base::BindRepeating(&AutoEnrollmentClientImplTest::ProgressCallback,
                            base::Unretained(this));
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
      client_ = AutoEnrollmentClientImpl::FactoryImpl().CreateForFRE(
          progress_callback, service_.get(), local_state_,
          shared_url_loader_factory_, kStateKey, power_initial, power_limit);
    } else {
      client_ =
          AutoEnrollmentClientImpl::FactoryImpl().CreateForInitialEnrollment(
              progress_callback, service_.get(), local_state_,
              shared_url_loader_factory_, kSerialNumber, kBrandCode,
              power_initial, power_limit,
              kInitialEnrollmentModulusPowerOutdatedServer);
    }
  }

  void ProgressCallback(AutoEnrollmentState state) { state_ = state; }

  void ServerWillFail(int net_error, int response_code) {
    em::DeviceManagementResponse dummy_response;
    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(DoAll(
            service_->CaptureJobType(&failed_job_type_),
            service_->CaptureRequest(&last_request_),
            service_->StartJobAsync(net_error, response_code, dummy_response)))
        .RetiresOnSaturation();
  }

  void ServerWillReply(int64_t modulus, bool with_hashes, bool with_id_hash) {
    em::DeviceManagementResponse response;
    em::DeviceAutoEnrollmentResponse* enrollment_response =
        response.mutable_auto_enrollment_response();
    if (modulus >= 0)
      enrollment_response->set_expected_modulus(modulus);
    if (with_hashes) {
      for (int i = 0; i < 10; ++i) {
        std::string state_key = base::StringPrintf("state_key %d", i);
        std::string hash_full = crypto::SHA256HashString(state_key);
        std::string hash =
            GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE
                ? hash_full
                : hash_full.substr(0, kInitialEnrollmentIdHashLength);
        enrollment_response->mutable_hashes()->Add()->assign(hash);
      }
    }
    if (with_id_hash) {
      if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
        enrollment_response->mutable_hashes()->Add()->assign(
            kStateKeyHash, crypto::kSHA256Length);
      } else {
        enrollment_response->mutable_hashes()->Add()->assign(
            kInitialEnrollmentIdHash, kInitialEnrollmentIdHashLength);
      }
    }

    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(
            DoAll(service_->CaptureJobType(&auto_enrollment_job_type_),
                  service_->CaptureRequest(&last_request_),
                  service_->StartJobAsync(
                      net::OK, DeviceManagementService::kSuccess, response)))
        .RetiresOnSaturation();
  }

  em::DeviceInitialEnrollmentStateResponse::InitialEnrollmentMode
  MapRestoreModeToInitialEnrollmentMode(
      em::DeviceStateRetrievalResponse::RestoreMode restore_mode) {
    using DeviceStateRetrieval = em::DeviceStateRetrievalResponse;
    using DeviceInitialEnrollmentState =
        em::DeviceInitialEnrollmentStateResponse;

    switch (restore_mode) {
      case DeviceStateRetrieval::RESTORE_MODE_NONE:
        return DeviceInitialEnrollmentState::INITIAL_ENROLLMENT_MODE_NONE;
      case DeviceStateRetrieval::RESTORE_MODE_REENROLLMENT_REQUESTED:
        return DeviceInitialEnrollmentState::INITIAL_ENROLLMENT_MODE_NONE;
      case DeviceStateRetrieval::RESTORE_MODE_REENROLLMENT_ENFORCED:
        return DeviceInitialEnrollmentState::
            INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED;
      case DeviceStateRetrieval::RESTORE_MODE_DISABLED:
        return DeviceInitialEnrollmentState::INITIAL_ENROLLMENT_MODE_DISABLED;
      case DeviceStateRetrieval::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH:
        return DeviceInitialEnrollmentState::
            INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED;
    }
  }

  std::string MapDeviceRestoreStateToDeviceInitialState(
      const std::string& restore_state) const {
    if (restore_state == kDeviceStateRestoreModeReEnrollmentEnforced)
      return kDeviceStateInitialModeEnrollmentEnforced;
    if (restore_state == kDeviceStateRestoreModeReEnrollmentZeroTouch)
      return kDeviceStateInitialModeEnrollmentZeroTouch;
    NOTREACHED();
    return "";
  }

  void ServerWillSendState(
      const std::string& management_domain,
      em::DeviceStateRetrievalResponse::RestoreMode restore_mode,
      const std::string& device_disabled_message,
      bool is_license_packaged_with_device) {
    if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
      ServerWillSendStateForFRE(management_domain, restore_mode,
                                device_disabled_message, base::nullopt);
    } else {
      ServerWillSendStateForInitialEnrollment(
          management_domain, is_license_packaged_with_device,
          MapRestoreModeToInitialEnrollmentMode(restore_mode));
    }
  }

  DeviceManagementService::JobConfiguration::JobType
  GetStateRetrievalJobType() {
    return GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE
               ? DeviceManagementService::JobConfiguration::
                     TYPE_DEVICE_STATE_RETRIEVAL
               : DeviceManagementService::JobConfiguration::
                     TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL;
  }

  void ServerWillSendStateForFRE(
      const std::string& management_domain,
      em::DeviceStateRetrievalResponse::RestoreMode restore_mode,
      const std::string& device_disabled_message,
      base::Optional<em::DeviceInitialEnrollmentStateResponse>
          initial_state_response) {
    em::DeviceManagementResponse response;
    em::DeviceStateRetrievalResponse* state_response =
        response.mutable_device_state_retrieval_response();
    state_response->set_restore_mode(restore_mode);
    if (!management_domain.empty())
      state_response->set_management_domain(management_domain);
    state_response->mutable_disabled_state()->set_message(
        device_disabled_message);

    ASSERT_TRUE(!initial_state_response ||
                restore_mode ==
                    em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE);
    if (initial_state_response) {
      state_response->mutable_initial_state_response()->MergeFrom(
          *initial_state_response);
    }

    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(
            DoAll(service_->CaptureJobType(&state_retrieval_job_type_),
                  service_->CaptureRequest(&last_request_),
                  service_->StartJobAsync(
                      net::OK, DeviceManagementService::kSuccess, response)))
        .RetiresOnSaturation();
  }

  void ServerWillSendStateForInitialEnrollment(
      const std::string& management_domain,
      bool is_license_packaged_with_device,
      em::DeviceInitialEnrollmentStateResponse::InitialEnrollmentMode
          initial_enrollment_mode) {
    em::DeviceManagementResponse response;
    em::DeviceInitialEnrollmentStateResponse* state_response =
        response.mutable_device_initial_enrollment_state_response();
    state_response->set_initial_enrollment_mode(initial_enrollment_mode);
    if (!management_domain.empty())
      state_response->set_management_domain(management_domain);
    state_response->set_is_license_packaged_with_device(
        is_license_packaged_with_device);
    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(
            DoAll(service_->CaptureJobType(&state_retrieval_job_type_),
                  service_->CaptureRequest(&last_request_),
                  service_->StartJobAsync(
                      net::OK, DeviceManagementService::kSuccess, response)))
        .RetiresOnSaturation();
  }

  DeviceManagementService::JobConfiguration::JobType
  GetExpectedStateRetrievalJobType() {
    return GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE
               ? DeviceManagementService::JobConfiguration::
                     TYPE_DEVICE_STATE_RETRIEVAL
               : DeviceManagementService::JobConfiguration::
                     TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL;
  }

  void ServerWillReplyAsync(DeviceManagementService::JobControl** job) {
    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(DoAll(service_->CaptureJobType(&last_async_job_type_),
                        service_->StartJobFullControl(job)));
  }

  bool HasCachedDecision() {
    return local_state_->GetUserPref(prefs::kShouldAutoEnroll);
  }

  void VerifyCachedResult(bool should_enroll, int power_limit) {
    base::Value value_should_enroll(should_enroll);
    base::Value value_power_limit(power_limit);
    EXPECT_EQ(value_should_enroll,
              *local_state_->GetUserPref(prefs::kShouldAutoEnroll));
    EXPECT_EQ(value_power_limit,
              *local_state_->GetUserPref(prefs::kAutoEnrollmentPowerLimit));
  }

  bool HasServerBackedState() {
    return local_state_->GetUserPref(prefs::kServerBackedDeviceState);
  }

  void VerifyServerBackedState(const std::string& expected_management_domain,
                               const std::string& expected_restore_mode,
                               const std::string& expected_disabled_message,
                               bool expected_is_license_packaged_with_device) {
    if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
      VerifyServerBackedStateForFRE(expected_management_domain,
                                    expected_restore_mode,
                                    expected_disabled_message);
    } else {
      VerifyServerBackedStateForInitialEnrollment(
          expected_management_domain, expected_restore_mode,
          expected_is_license_packaged_with_device);
    }
  }

  void VerifyServerBackedStateForAll(
      const std::string& expected_management_domain,
      const std::string& expected_restore_mode,
      const base::DictionaryValue** local_state_dict) {
    const base::Value* state =
        local_state_->GetUserPref(prefs::kServerBackedDeviceState);
    ASSERT_TRUE(state);
    const base::DictionaryValue* state_dict = nullptr;
    ASSERT_TRUE(state->GetAsDictionary(&state_dict));
    *local_state_dict = state_dict;

    std::string actual_management_domain;
    if (expected_management_domain.empty()) {
      EXPECT_FALSE(state_dict->GetString(kDeviceStateManagementDomain,
                                         &actual_management_domain));
    } else {
      EXPECT_TRUE(state_dict->GetString(kDeviceStateManagementDomain,
                                        &actual_management_domain));
      EXPECT_EQ(expected_management_domain, actual_management_domain);
    }

    if (!expected_restore_mode.empty()) {
      std::string actual_restore_mode;
      EXPECT_TRUE(
          state_dict->GetString(kDeviceStateMode, &actual_restore_mode));
    } else {
      EXPECT_FALSE(state_dict->HasKey(kDeviceStateMode));
    }
  }

  void VerifyServerBackedStateForFRE(
      const std::string& expected_management_domain,
      const std::string& expected_restore_mode,
      const std::string& expected_disabled_message) {
    const base::DictionaryValue* state_dict;
    VerifyServerBackedStateForAll(expected_management_domain,
                                  expected_restore_mode, &state_dict);

    if (!expected_restore_mode.empty()) {
      std::string actual_restore_mode;
      EXPECT_TRUE(
          state_dict->GetString(kDeviceStateMode, &actual_restore_mode));
      EXPECT_EQ(GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE
                    ? expected_restore_mode
                    : MapDeviceRestoreStateToDeviceInitialState(
                          expected_restore_mode),
                actual_restore_mode);
    }

    std::string actual_disabled_message;
    EXPECT_TRUE(state_dict->GetString(kDeviceStateDisabledMessage,
                                      &actual_disabled_message));
    EXPECT_EQ(expected_disabled_message, actual_disabled_message);

    EXPECT_FALSE(state_dict->FindBoolPath(kDeviceStatePackagedLicense));
  }

  void VerifyServerBackedStateForInitialEnrollment(
      const std::string& expected_management_domain,
      const std::string& expected_restore_mode,
      bool expected_is_license_packaged_with_device) {
    const base::DictionaryValue* state_dict;
    VerifyServerBackedStateForAll(expected_management_domain,
                                  expected_restore_mode, &state_dict);

    std::string actual_disabled_message;
    EXPECT_FALSE(state_dict->GetString(kDeviceStateDisabledMessage,
                                       &actual_disabled_message));

    base::Optional<bool> actual_is_license_packaged_with_device;
    actual_is_license_packaged_with_device =
        state_dict->FindBoolPath(kDeviceStatePackagedLicense);
    if (actual_is_license_packaged_with_device) {
      EXPECT_EQ(expected_is_license_packaged_with_device,
                actual_is_license_packaged_with_device.value());
    } else {
      EXPECT_FALSE(expected_is_license_packaged_with_device);
    }
  }

  const em::DeviceAutoEnrollmentRequest& auto_enrollment_request() {
    return last_request_.auto_enrollment_request();
  }

  // Returns |client_| as |AutoEnrollmentClientImpl*|. This is fine because this
  // test only creates |client_| using |AutoEnrollmentClientImpl::FactoryImpl|.
  AutoEnrollmentClientImpl* client() {
    return static_cast<AutoEnrollmentClientImpl*>(client_.get());
  }

  // Releases |client_| and returns the pointer as |AutoEnrollmentClientImpl*|.
  // This is fine because this test only creates |client_| using
  // |AutoEnrollmentClientImpl::FactoryImpl|.
  AutoEnrollmentClientImpl* release_client() {
    return static_cast<AutoEnrollmentClientImpl*>(client_.release());
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState scoped_testing_local_state_;
  TestingPrefServiceSimple* local_state_;
  std::unique_ptr<MockDeviceManagementService> service_;
  em::DeviceManagementRequest last_request_;
  AutoEnrollmentState state_;
  DeviceManagementService::JobConfiguration::JobType failed_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType last_async_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType auto_enrollment_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType state_retrieval_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;

 private:
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<AutoEnrollmentClient> client_;
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentClientImplTest);
};

TEST_P(AutoEnrollmentClientImplTest, NetworkFailure) {
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, EmptyReply) {
  ServerWillReply(-1, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  VerifyCachedResult(false, kPowerLimit);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ClientUploadsRightBits) {
  ServerWillReply(-1, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);

  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(16, auto_enrollment_request().modulus());
  if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
    EXPECT_EQ(kStateKeyHash[31] & 0xf, auto_enrollment_request().remainder());
  } else {
    EXPECT_EQ(kInitialEnrollmentIdHash[7] & 0xf,
              auto_enrollment_request().remainder());
  }
  VerifyCachedResult(false, kPowerLimit);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForMoreThenFail) {
  InSequence sequence;
  ServerWillReply(32, false, false);
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(failed_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForMoreThenEvenMore) {
  InSequence sequence;
  ServerWillReply(32, false, false);
  ServerWillReply(64, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForLess) {
  InSequence sequence;
  ServerWillReply(8, false, false);
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, AskForSame) {
  InSequence sequence;
  ServerWillReply(16, false, false);
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, AskForSameTwice) {
  InSequence sequence;
  ServerWillReply(16, false, false);
  ServerWillReply(16, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForTooMuch) {
  ServerWillReply(512, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, DetectOutdatedServer) {
  CreateClient(0, kInitialEnrollmentModulusPowerOutdatedServer + 1);
  InSequence sequence;
  ServerWillReply(1 << kInitialEnrollmentModulusPowerOutdatedServer, false,
                  false);

  if (GetAutoEnrollmentProtocol() ==
      AutoEnrollmentProtocol::kInitialEnrollment) {
    // For initial enrollment, a modulus power higher or equal to
    // |kInitialEnrollmentModulusPowerOutdatedServer| means that the client will
    // detect the server as outdated and will skip enrollment.
    client()->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(auto_enrollment_job_type_,
              DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    EXPECT_TRUE(HasCachedDecision());
    EXPECT_FALSE(HasServerBackedState());
  } else {
    // For FRE, such a detection does not exist. The client will do the second
    // round and upload bits of its device identifier hash.
    ServerWillReply(-1, false, false);
    client()->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(auto_enrollment_job_type_,
              DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    EXPECT_TRUE(HasCachedDecision());
    EXPECT_FALSE(HasServerBackedState());
  }
}

TEST_P(AutoEnrollmentClientImplTest, AskNonPowerOf2) {
  InSequence sequence;
  ServerWillReply(100, false, false);
  ServerWillReply(-1, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(128, auto_enrollment_request().modulus());
  if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
    EXPECT_EQ(kStateKeyHash[31] & 0x7f, auto_enrollment_request().remainder());
  } else {
    EXPECT_EQ(kInitialEnrollmentIdHash[7] & 0x7f,
              auto_enrollment_request().remainder());
  }
  VerifyCachedResult(false, kPowerLimit);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ConsumerDevice) {
  ServerWillReply(-1, true, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  VerifyCachedResult(false, kPowerLimit);
  EXPECT_FALSE(HasServerBackedState());

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
}

TEST_P(AutoEnrollmentClientImplTest, ForcedReEnrollment) {
  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
}

TEST_P(AutoEnrollmentClientImplTest, ForcedEnrollmentZeroTouch) {
  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH,
      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentZeroTouch,
                          kDisabledMessage, kNotWithLicense);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
}

TEST_P(AutoEnrollmentClientImplTest, RequestedReEnrollment) {
  // Requesting re-enrollment is currently not supported in the
  // initial-enrollment exchange.
  if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kInitialEnrollment)
    return;

  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED,
      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentRequested,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, DeviceDisabled) {
  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState("example.com",
                      em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED,
                      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_DISABLED);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedState("example.com", kDeviceStateModeDisabled,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, NoReEnrollment) {
  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState(std::string(),
                      em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
                      std::string(), kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedState(std::string(), std::string(), std::string(),
                          kNotWithLicense);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
}

TEST_P(AutoEnrollmentClientImplTest, NoBitsUploaded) {
  CreateClient(0, 0);
  ServerWillReply(-1, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(1, auto_enrollment_request().modulus());
  EXPECT_EQ(0, auto_enrollment_request().remainder());
  VerifyCachedResult(false, 0);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ManyBitsUploaded) {
  int64_t bottom62 = GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE
                         ? INT64_C(0x386e7244d097c3e6)
                         : INT64_C(0x3018b70f7609c5c7);
  for (int i = 0; i <= 62; ++i) {
    CreateClient(i, i);
    ServerWillReply(-1, false, false);
    client()->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(auto_enrollment_job_type_,
              DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    EXPECT_TRUE(auto_enrollment_request().has_remainder());
    EXPECT_TRUE(auto_enrollment_request().has_modulus());
    EXPECT_EQ(INT64_C(1) << i, auto_enrollment_request().modulus());
    EXPECT_EQ(bottom62 % (INT64_C(1) << i),
              auto_enrollment_request().remainder());
    VerifyCachedResult(false, i);
    EXPECT_FALSE(HasServerBackedState());
  }
}

TEST_P(AutoEnrollmentClientImplTest, MoreThan32BitsUploaded) {
  // Skip for initial enrollment, because the outdated server detection would
  // kick in when more than |kInitialEnrollmentModulusPowerOutdatedServer| bits
  // are requested.
  if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kInitialEnrollment)
    return;

  CreateClient(10, 37);
  InSequence sequence;
  ServerWillReply(INT64_C(1) << 37, false, false);
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(true, 37);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, ReuseCachedDecision) {
  // No bucket download requests should be issued.
  EXPECT_CALL(*service_, StartJob(_)).Times(0);
  local_state_->SetUserPref(prefs::kShouldAutoEnroll,
                            std::make_unique<base::Value>(true));
  local_state_->SetUserPref(prefs::kAutoEnrollmentPowerLimit,
                            std::make_unique<base::Value>(8));

  // Note that device state will be retrieved every time, regardless of any
  // cached information. This is intentional, the idea is that device state on
  // the server may change.
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, RetryIfPowerLargerThanCached) {
  local_state_->SetUserPref(prefs::kShouldAutoEnroll,
                            std::make_unique<base::Value>(false));
  local_state_->SetUserPref(prefs::kAutoEnrollmentPowerLimit,
                            std::make_unique<base::Value>(8));
  CreateClient(5, 10);

  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, NetworkChangeRetryAfterErrors) {
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  // Don't invoke the callback if there was a network failure.
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());

  // The client doesn't retry if no new connection became available.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());

  // Retry once the network is back.
  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense);
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  EXPECT_TRUE(HasCachedDecision());
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);

  // Subsequent network changes don't trigger retries.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  EXPECT_TRUE(HasCachedDecision());
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, CancelAndDeleteSoonWithPendingRequest) {
  DeviceManagementService::JobControl* job = nullptr;
  ServerWillReplyAsync(&job);
  EXPECT_FALSE(job);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(job);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // Cancel while a request is in flight.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());

  // The client cleans itself up once a reply is received.
  service_->DoURLCompletion(&job, net::OK,
                            DeviceManagementService::kServiceUnavailable,
                            em::DeviceManagementResponse());
  EXPECT_EQ(nullptr, job);
  // The DeleteSoon task has been posted:
  EXPECT_FALSE(base::CurrentThread::Get()->IsIdleForTesting());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);
}

TEST_P(AutoEnrollmentClientImplTest, NetworkChangedAfterCancelAndDeleteSoon) {
  DeviceManagementService::JobControl* job = nullptr;
  ServerWillReplyAsync(&job);
  EXPECT_FALSE(job);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(job);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // Cancel while a request is in flight.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  AutoEnrollmentClientImpl* client = release_client();
  client->CancelAndDeleteSoon();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());

  // Network change events are ignored while a request is pending.
  client->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // The client cleans itself up once a reply is received.
  service_->DoURLCompletion(&job, net::OK,
                            DeviceManagementService::kServiceUnavailable,
                            em::DeviceManagementResponse());
  EXPECT_EQ(nullptr, job);
  // The DeleteSoon task has been posted:
  EXPECT_FALSE(base::CurrentThread::Get()->IsIdleForTesting());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // Network changes that have been posted before are also ignored:
  client->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);
}

TEST_P(AutoEnrollmentClientImplTest, CancelAndDeleteSoonAfterCompletion) {
  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);

  // The client will delete itself immediately if there are no pending
  // requests.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_P(AutoEnrollmentClientImplTest, CancelAndDeleteSoonAfterNetworkFailure) {
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);

  // The client will delete itself immediately if there are no pending
  // requests.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_P(AutoEnrollmentClientImplTest, NetworkFailureThenRequireUpdatedModulus) {
  // This test verifies that if the first request fails due to a network
  // problem then the second request will correctly handle an updated
  // modulus request from the server.

  ServerWillFail(net::ERR_FAILED, DeviceManagementService::kSuccess);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  // Callback should signal the connection error.
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
  Mock::VerifyAndClearExpectations(service_.get());

  InSequence sequence;
  // The default client uploads 4 bits. Make the server ask for 5.
  ServerWillReply(1 << 5, false, false);
  // Then reply with a valid response and include the hash.
  ServerWillReply(-1, true, true);
  // State download triggers.
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense);

  // Trigger a network change event.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  EXPECT_TRUE(HasCachedDecision());
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);
  Mock::VerifyAndClearExpectations(service_.get());
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
}

// Private set membership is disabed to test only FRE case extensively instead.
// That is necessary as both protocols are running in sequential order starting
// off with private set membership.
INSTANTIATE_TEST_SUITE_P(
    FRE,
    AutoEnrollmentClientImplTest,
    testing::Combine(
        testing::Values(AutoEnrollmentClientImplTestState(
            AutoEnrollmentProtocol::kFRE,
            PrivateSetMembershipState::kDisabled)),
        testing::Values(kInvalidPrivateSetMembershipTestCaseIndex)));

// Private set membership is disabed to test only initial enrollment case
// extensively instead. That is necessary as both protocols are running in
// sequential order starting off with private set membership.
INSTANTIATE_TEST_SUITE_P(
    InitialEnrollment,
    AutoEnrollmentClientImplTest,
    testing::Combine(
        testing::Values(AutoEnrollmentClientImplTestState(
            AutoEnrollmentProtocol::kInitialEnrollment,
            PrivateSetMembershipState::kDisabled)),
        testing::Values(kInvalidPrivateSetMembershipTestCaseIndex)));

using AutoEnrollmentClientImplFREToInitialEnrollmentTest =
    AutoEnrollmentClientImplTest;

TEST_P(AutoEnrollmentClientImplFREToInitialEnrollmentTest,
       NoReEnrollmentInitialEnrollmentLicensePackaging) {
  InSequence sequence;
  ServerWillReply(-1, true, true);
  em::DeviceInitialEnrollmentStateResponse initial_state_response;
  initial_state_response.set_is_license_packaged_with_device(kWithLicense);
  ServerWillSendStateForFRE(
      std::string(), em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      std::string(),
      base::Optional<em::DeviceInitialEnrollmentStateResponse>(
          initial_state_response));
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedStateForInitialEnrollment(std::string(), std::string(),
                                              kWithLicense);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
}

TEST_P(AutoEnrollmentClientImplFREToInitialEnrollmentTest,
       NoReEnrollmentInitialEnrollmentZeroTouch) {
  InSequence sequence;
  ServerWillReply(-1, true, true);
  em::DeviceInitialEnrollmentStateResponse initial_state_response;
  initial_state_response.set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  initial_state_response.set_management_domain("example.com");
  ServerWillSendStateForFRE(
      std::string(), em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      std::string(),
      base::Optional<em::DeviceInitialEnrollmentStateResponse>(
          initial_state_response));
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedStateForInitialEnrollment(
      "example.com", kDeviceStateInitialModeEnrollmentZeroTouch,
      kNotWithLicense);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
}

TEST_P(AutoEnrollmentClientImplFREToInitialEnrollmentTest,
       NoReEnrollmentInitialEnrollmentGuaranteed) {
  InSequence sequence;
  ServerWillReply(-1, true, true);
  em::DeviceInitialEnrollmentStateResponse initial_state_response;
  initial_state_response.set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED);
  initial_state_response.set_management_domain("example.com");
  ServerWillSendStateForFRE(
      std::string(), em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      std::string(),
      base::Optional<em::DeviceInitialEnrollmentStateResponse>(
          initial_state_response));
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(true, kPowerLimit);
  VerifyServerBackedStateForInitialEnrollment(
      "example.com", kDeviceStateInitialModeEnrollmentEnforced,
      kNotWithLicense);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
}

// Private set membership is disabed to test only switching from FRE to initial
// enrollment case extensively instead. That is necessary as both protocols are
// running in sequential order starting off with private set membership.
INSTANTIATE_TEST_SUITE_P(
    FREToInitialEnrollment,
    AutoEnrollmentClientImplFREToInitialEnrollmentTest,
    testing::Combine(
        testing::Values(AutoEnrollmentClientImplTestState(
            AutoEnrollmentProtocol::kFRE,
            PrivateSetMembershipState::kDisabled)),
        testing::Values(kInvalidPrivateSetMembershipTestCaseIndex)));

// This class is used to test any private set membership related test cases
// only. Therefore, the PrivateSetMembershipState param has to be kEnabled.
class PrivateSetMembershipHelperTest : public AutoEnrollmentClientImplTest {
 protected:
  // Indicates the state of the private set membership protocol.
  enum class StateDiscoveryResult {
    // Failed.
    kFailure = 0,
    // Succeeded, the result was that the server does not have any state for the
    // device.
    kSuccessNoServerSideState = 1,
    // Succeeded, the result was that the server does have a state for the
    // device.
    kSuccessHasServerSideState = 2,
  };

  PrivateSetMembershipHelperTest() {}
  ~PrivateSetMembershipHelperTest() {
    // Flush any deletion tasks.
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    // Verify that PrivateSetMembershipState has value kEnabled, then enable
    // private set membership switch
    // prefs::kEnterpriseEnablePrivateSetMembership.
    ASSERT_EQ(GetPrivateSetMembershipState(),
              PrivateSetMembershipState::kEnabled);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        chromeos::switches::kEnterpriseEnablePrivateSetMembership,
        chromeos::AutoEnrollmentController::kEnablePsmAlways);

    // Verify that private set membership state pref has not been set before.
    ASSERT_EQ(local_state_->GetUserPref(prefs::kShouldRetrieveDeviceState),
              nullptr);

    // Set up the base class AutoEnrollmentClientImplTest after the private set
    // membership has been enabled.
    AutoEnrollmentClientImplTest::SetUp();

    // Create private set membership test case, and its corresponding RLWE
    // client.
    CreatePrivateSetMembershipTestCase();
    SetPrivateSetMembershipRlweClient();
  }

  void CreatePrivateSetMembershipTestCase() {
    // Verify that private set membership is enabled, and the test case index is
    // valid.
    EXPECT_TRUE(
        chromeos::AutoEnrollmentController::IsPrivateSetMembershipEnabled());
    ASSERT_GE(GetPrivateSetMembershipTestCaseIndex(), 0);

    // Retrieve the private set membership test case.
    base::FilePath src_root_dir;
    EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
    const base::FilePath kPrivateSetMembershipTestDataPath =
        src_root_dir.AppendASCII("third_party")
            .AppendASCII("private_membership")
            .AppendASCII("src")
            .AppendASCII("internal")
            .AppendASCII("testing")
            .AppendASCII("regression_test_data")
            .AppendASCII("cros_test_data.binarypb");
    EXPECT_TRUE(base::PathExists(kPrivateSetMembershipTestDataPath));
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData test_data;
    EXPECT_TRUE(
        ParseProtoFromFile(kPrivateSetMembershipTestDataPath, &test_data));
    EXPECT_EQ(test_data.test_cases_size(),
              kNumberOfPrivateSetMembershipTestCases);
    private_set_membership_test_case_ =
        test_data.test_cases(GetPrivateSetMembershipTestCaseIndex());
  }

  void SetPrivateSetMembershipRlweClient() {
    auto rlwe_client_or_status =
        psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
            private_set_membership_test_case_.use_case(),
            {private_set_membership_test_case_.plaintext_id()},
            private_set_membership_test_case_.ec_cipher_key(),
            private_set_membership_test_case_.seed());
    ASSERT_OK(rlwe_client_or_status.status());

    auto rlwe_client = std::move(rlwe_client_or_status.value());
    client()->SetPrivateSetMembershipRlweClientForTesting(
        std::move(rlwe_client),
        private_set_membership_test_case_.plaintext_id());
  }

  void ServerWillReplyWithPrivateSetMembershipOprfResponse() {
    em::DeviceManagementResponse response =
        GetPrivateSetMembershipOprfResponse();

    ServerWillReplyForPrivateSetMembership(
        net::OK, DeviceManagementService::kSuccess, response);
  }

  void ServerWillReplyWithPrivateSetMembershipQueryResponse() {
    em::DeviceManagementResponse response =
        GetPrivateSetMembershipQueryResponse();

    ServerWillReplyForPrivateSetMembership(
        net::OK, DeviceManagementService::kSuccess, response);
  }

  void ServerWillReplyWithEmptyPrivateSetMembershipResponse() {
    em::DeviceManagementResponse dummy_response;
    ServerWillReplyForPrivateSetMembership(
        net::OK, DeviceManagementService::kSuccess, dummy_response);
  }

  void ServerWillFailForPrivateSetMembership(int net_error, int response_code) {
    em::DeviceManagementResponse dummy_response;
    ServerWillReplyForPrivateSetMembership(net_error, response_code,
                                           dummy_response);
  }

  // Mocks the server reply and captures the job type in
  // |private_set_membership_last_job_type_|, and the request in
  // |private_set_membership_last_request_|.
  void ServerWillReplyForPrivateSetMembership(
      int net_error,
      int response_code,
      const em::DeviceManagementResponse& response) {
    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(DoAll(
            service_->CaptureJobType(&private_set_membership_last_job_type_),
            service_->CaptureRequest(&private_set_membership_last_request_),
            service_->StartJobAsync(net_error, response_code, response)))
        .RetiresOnSaturation();
  }

  // Holds the full control of the given job in |job| and captures the job type
  // in |private_set_membership_last_job_type_|, and its request in
  // |private_set_membership_last_request_|.
  void ServerWillReplyAsyncForPrivateSetMembership(
      DeviceManagementService::JobControl** job) {
    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(DoAll(
            service_->CaptureJobType(&private_set_membership_last_job_type_),
            service_->CaptureRequest(&private_set_membership_last_request_),
            service_->StartJobFullControl(job)));
  }

  void ServerReplyForPrivateSetMembershipAsyncJobWithOprfResponse(
      DeviceManagementService::JobControl** job) {
    em::DeviceManagementResponse response =
        GetPrivateSetMembershipOprfResponse();

    ServerReplyForAsyncJob(job, net::OK, DeviceManagementService::kSuccess,
                           response);
  }

  void ServerReplyForPrivateSetMembershipAsyncJobWithQueryResponse(
      DeviceManagementService::JobControl** job) {
    em::DeviceManagementResponse response =
        GetPrivateSetMembershipQueryResponse();

    ServerReplyForAsyncJob(job, net::OK, DeviceManagementService::kSuccess,
                           response);
  }

  void ServerFailsForAsyncJob(DeviceManagementService::JobControl** job) {
    em::DeviceManagementResponse dummy_response;
    ServerReplyForAsyncJob(job, net::OK,
                           DeviceManagementService::kServiceUnavailable,
                           dummy_response);
  }

  void ServerRepliesEmptyResponseForAsyncJob(
      DeviceManagementService::JobControl** job) {
    em::DeviceManagementResponse dummy_response;
    ServerReplyForAsyncJob(job, net::OK, DeviceManagementService::kSuccess,
                           dummy_response);
  }

  // Mocks the server reply for the full controlled job |job|.
  void ServerReplyForAsyncJob(
      DeviceManagementService::JobControl** job,
      int net_error,
      int response_code,
      const enterprise_management::DeviceManagementResponse& response) {
    service_->DoURLCompletion(job, net_error, response_code, response);
  }

  const em::PrivateSetMembershipRequest& private_set_membership_request()
      const {
    return private_set_membership_last_request_
        .private_set_membership_request();
  }

  StateDiscoveryResult GetStateDiscoveryResult() const {
    const base::Value* device_state_value =
        local_state_->GetUserPref(prefs::kShouldRetrieveDeviceState);
    if (!device_state_value)
      return StateDiscoveryResult::kFailure;
    return device_state_value->GetBool()
               ? StateDiscoveryResult::kSuccessHasServerSideState
               : StateDiscoveryResult::kSuccessNoServerSideState;
  }

  // Returns the expected membership result for the current private set
  // membership test case.
  bool GetExpectedMembershipResult() const {
    return private_set_membership_test_case_.is_positive_membership_expected();
  }

  // Expects a sample for kUMAPrivateSetMembershipRequestStatus to be recorded
  // once with value |status|.
  void ExpectPrivateSetMembershipHistograms(
      const std::vector<PrivateSetMembershipStatus> status_list,
      bool success_time_recorded) const {
    for (PrivateSetMembershipStatus status : status_list) {
      histogram_tester_.ExpectBucketCount(kUMAPrivateSetMembershipRequestStatus,
                                          status, /*expected_count=*/1);
    }
    histogram_tester_.ExpectTotalCount(kUMAPrivateSetMembershipSuccessTime,
                                       success_time_recorded ? 1 : 0);
  }

  // Expects a sample for kUMAPrivateSetMembershipSuccessTime to be recorded
  // |count| times.
  void ExpectPrivateSetMembershipSuccessTimeRecorded(int count) const {
    histogram_tester_.ExpectTotalCount(kUMAPrivateSetMembershipSuccessTime,
                                       count);
  }

  // Expects a sample for kUMAPrivateSetMembershipHashDanceComparison to be
  // recorded once with value |comparison|.
  void ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      PrivateSetMembershipHashDanceComparison comparison) const {
    histogram_tester_.ExpectUniqueSample(
        kUMAPrivateSetMembershipHashDanceComparison, comparison,
        /*expected_count=*/1);
  }

  void VerifyPrivateSetMembershipLastRequestJobType() const {
    EXPECT_EQ(DeviceManagementService::JobConfiguration::
                  TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
              private_set_membership_last_job_type_);
  }

  void VerifyPrivateSetMembershipRlweOprfRequest() const {
    EXPECT_EQ(private_set_membership_test_case_.expected_oprf_request()
                  .SerializeAsString(),
              private_set_membership_request()
                  .rlwe_request()
                  .oprf_request()
                  .SerializeAsString());
  }

  void VerifyPrivateSetMembershipRlweQueryRequest() const {
    EXPECT_EQ(private_set_membership_test_case_.expected_query_request()
                  .SerializeAsString(),
              private_set_membership_request()
                  .rlwe_request()
                  .query_request()
                  .SerializeAsString());
  }

  // Disallow copy constructor and assignment operator.
  PrivateSetMembershipHelperTest(const PrivateSetMembershipHelperTest&) =
      delete;
  PrivateSetMembershipHelperTest& operator=(
      const PrivateSetMembershipHelperTest&) = delete;

  base::HistogramTester histogram_tester_;

 private:
  em::DeviceManagementResponse GetPrivateSetMembershipOprfResponse() const {
    em::DeviceManagementResponse response;
    em::PrivateSetMembershipResponse* private_set_membership_response =
        response.mutable_private_set_membership_response();

    *private_set_membership_response->mutable_rlwe_response()
         ->mutable_oprf_response() =
        private_set_membership_test_case_.oprf_response();
    return response;
  }

  em::DeviceManagementResponse GetPrivateSetMembershipQueryResponse() const {
    em::DeviceManagementResponse response;
    em::PrivateSetMembershipResponse* private_set_membership_response =
        response.mutable_private_set_membership_response();

    *private_set_membership_response->mutable_rlwe_response()
         ->mutable_query_response() =
        private_set_membership_test_case_.query_response();
    return response;
  }

  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase
      private_set_membership_test_case_;
  DeviceManagementService::JobConfiguration::JobType
      private_set_membership_last_job_type_ =
          DeviceManagementService::JobConfiguration::TYPE_INVALID;
  em::DeviceManagementRequest private_set_membership_last_request_;
};

TEST_P(PrivateSetMembershipHelperTest, MembershipRetrievedSuccessfully) {
  InSequence sequence;
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillReplyWithPrivateSetMembershipQueryResponse();

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();

  // TODO(crbug.com/1143634) Remove all usages of RunUntilIdle for all PSM
  // tests, after removing support of Hash dance from client side.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetStateDiscoveryResult(),
            GetExpectedMembershipResult()
                ? StateDiscoveryResult::kSuccessHasServerSideState
                : StateDiscoveryResult::kSuccessNoServerSideState);
  ExpectPrivateSetMembershipHistograms(
      {PrivateSetMembershipStatus::kAttempt,
       PrivateSetMembershipStatus::kSuccessfulDetermination},
      /*success_time_recorded=*/true);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
}

// TODO(crbug/1113798) Fix the empty RLWE query response processing, once it's
// already fixed on the library.
TEST_P(PrivateSetMembershipHelperTest, DISABLED_EmptyRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillReplyWithEmptyPrivateSetMembershipResponse();

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
}

TEST_P(PrivateSetMembershipHelperTest, EmptyRlweOprfResponse) {
  InSequence sequence;
  ServerWillReplyWithEmptyPrivateSetMembershipResponse();

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipRlweOprfRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
}

TEST_P(PrivateSetMembershipHelperTest, ConnectionErrorForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillFailForPrivateSetMembership(net::ERR_FAILED,
                                        DeviceManagementService::kSuccess);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
}

TEST_P(PrivateSetMembershipHelperTest, ConnectionErrorForRlweOprfResponse) {
  InSequence sequence;
  ServerWillFailForPrivateSetMembership(net::ERR_FAILED,
                                        DeviceManagementService::kSuccess);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipRlweOprfRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
}

TEST_P(PrivateSetMembershipHelperTest, NetworkFailureForRlweOprfResponse) {
  InSequence sequence;
  ServerWillFailForPrivateSetMembership(
      net::OK, DeviceManagementService::kServiceUnavailable);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipLastRequestJobType();
}

TEST_P(PrivateSetMembershipHelperTest, NetworkFailureForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillFailForPrivateSetMembership(net::OK, net::ERR_CONNECTION_CLOSED);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
}

TEST_P(PrivateSetMembershipHelperTest,
       RetryLogicAfterMembershipSuccessfullyRetrieved) {
  InSequence sequence;
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillReplyWithPrivateSetMembershipQueryResponse();

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  const StateDiscoveryResult expected_state_result =
      GetExpectedMembershipResult()
          ? StateDiscoveryResult::kSuccessHasServerSideState
          : StateDiscoveryResult::kSuccessNoServerSideState;
  EXPECT_EQ(GetStateDiscoveryResult(), expected_state_result);

  // Verify that none of the private set membership requests have been sent
  // again. And its cached membership result hasn't changed.

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Retry();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetStateDiscoveryResult(), expected_state_result);
  ExpectPrivateSetMembershipHistograms(
      {PrivateSetMembershipStatus::kAttempt,
       PrivateSetMembershipStatus::kSuccessfulDetermination},
      /*success_time_recorded=*/true);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
}

TEST_P(PrivateSetMembershipHelperTest,
       RetryLogicAfterNetworkFailureForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillFailForPrivateSetMembership(net::OK, net::ERR_CONNECTION_CLOSED);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  const StateDiscoveryResult expected_state_result =
      StateDiscoveryResult::kFailure;
  EXPECT_EQ(GetStateDiscoveryResult(), expected_state_result);

  // Verify that none of the private set membership requests have been sent
  // again. And its cached membership result hasn't changed.

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Retry();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetStateDiscoveryResult(), expected_state_result);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
}

INSTANTIATE_TEST_SUITE_P(
    PrivateSetMembership,
    PrivateSetMembershipHelperTest,
    testing::Combine(testing::Values(AutoEnrollmentClientImplTestState(
                         AutoEnrollmentProtocol::kInitialEnrollment,
                         PrivateSetMembershipState::kEnabled)),
                     ::testing::Range(0,
                                      kNumberOfPrivateSetMembershipTestCases)));

using PrivateSetMembershipHelperAndHashDanceTest =
    PrivateSetMembershipHelperTest;

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       PrivateSetMembershipRlweQueryFailedAndHashDanceSucceeded) {
  InSequence sequence;

  // Fail for private set membership RLWE query request.
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillFailForPrivateSetMembership(net::OK, net::ERR_CONNECTION_CLOSED);

  // Succeed for both DeviceAutoEnrollmentRequest and
  // DeviceStateRetrievalRequest. And the result of DeviceAutoEnrollmentRequest
  // is positive.
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kWithLicense);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify failure of private set membership protocol.
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();

  // Verify Hash dance result.
  VerifyCachedResult(true, kPowerLimit);

  // Verify recorded comparison value between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      PrivateSetMembershipHashDanceComparison::kPSMErrorHashDanceSuccess);

  // Verify device state result.
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kWithLicense);
}

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       PrivateSetMembershipRlweOprfFailedAndHashDanceSucceeded) {
  InSequence sequence;

  // Fail for private set membership RLWE OPRF request.
  ServerWillFailForPrivateSetMembership(
      net::OK, DeviceManagementService::kServiceUnavailable);

  // Succeed for both DeviceAutoEnrollmentRequest and
  // DeviceStateRetrievalRequest. And the result of DeviceAutoEnrollmentRequest
  // is positive.
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kWithLicense);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify failure of private set membership protocol.
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipLastRequestJobType();

  // Verify Hash dance result.
  VerifyCachedResult(true, kPowerLimit);

  // Verify recorded comparison between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      PrivateSetMembershipHashDanceComparison::kPSMErrorHashDanceSuccess);

  // Verify device state result.
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kWithLicense);
}

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       PrivateSetMembershipSucceedAndHashDanceSucceed) {
  InSequence sequence;

  // Succeed for both private set membership RLWE requests.
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillReplyWithPrivateSetMembershipQueryResponse();

  // Succeed for both DeviceAutoEnrollmentRequest and
  // DeviceStateRetrievalRequest. And the result of DeviceAutoEnrollmentRequest
  // is positive.
  const bool kExpectedHashDanceResult = true;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true,
                  /*with_id_hash=*/kExpectedHashDanceResult);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kWithLicense);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify private set membership result.
  EXPECT_EQ(GetStateDiscoveryResult(),
            GetExpectedMembershipResult()
                ? StateDiscoveryResult::kSuccessHasServerSideState
                : StateDiscoveryResult::kSuccessNoServerSideState);
  ExpectPrivateSetMembershipHistograms(
      {PrivateSetMembershipStatus::kAttempt,
       PrivateSetMembershipStatus::kSuccessfulDetermination},
      /*success_time_recorded=*/true);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();

  // Verify Hash dance result.
  VerifyCachedResult(kExpectedHashDanceResult, kPowerLimit);

  // Verify recorded comparison between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      (GetExpectedMembershipResult() == kExpectedHashDanceResult)
          ? PrivateSetMembershipHashDanceComparison::kEqualResults
          : PrivateSetMembershipHashDanceComparison::kDifferentResults);

  // Verify device state result.
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kWithLicense);
}

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       PrivateSetMembershipSucceedAndHashDanceSucceedForNoEnrollment) {
  InSequence sequence;

  // Succeed for both private set membership RLWE requests.
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillReplyWithPrivateSetMembershipQueryResponse();

  // Succeed with a negative result for DeviceAutoEnrollmentRequest i.e. Hash
  // dance request.
  const bool kExpectedHashDanceResult = false;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true,
                  /*with_id_hash=*/kExpectedHashDanceResult);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify private set membership result.
  EXPECT_EQ(GetStateDiscoveryResult(),
            GetExpectedMembershipResult()
                ? StateDiscoveryResult::kSuccessHasServerSideState
                : StateDiscoveryResult::kSuccessNoServerSideState);
  ExpectPrivateSetMembershipHistograms(
      {PrivateSetMembershipStatus::kAttempt,
       PrivateSetMembershipStatus::kSuccessfulDetermination},
      /*success_time_recorded=*/true);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();

  // Verify Hash dance result.
  VerifyCachedResult(kExpectedHashDanceResult, kPowerLimit);

  // Verify recorded comparison between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      (GetExpectedMembershipResult() == kExpectedHashDanceResult)
          ? PrivateSetMembershipHashDanceComparison::kEqualResults
          : PrivateSetMembershipHashDanceComparison::kDifferentResults);

  // Verify that no enrollment has been done, and no state has been retrieved.
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       PrivateSetMembershipRlweOprfFailedAndHashDanceFailed) {
  InSequence sequence;

  // Fail for private set membership RLWE OPRF request.
  ServerWillFailForPrivateSetMembership(
      net::OK, DeviceManagementService::kServiceUnavailable);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify failure of private set membership protocol.
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipLastRequestJobType();

  // Verify failure of Hash dance by inexistence of its cached decision.
  EXPECT_FALSE(HasCachedDecision());

  // Verify recorded comparison between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      PrivateSetMembershipHashDanceComparison::kBothError);

  // Verify that no enrollment has been done, and no state has been retrieved.
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       RetryLogicAfterPrivateSetMembershipSucceededAndHashDanceSucceeded) {
  InSequence sequence;

  // Succeed for both private set membership RLWE requests.
  ServerWillReplyWithPrivateSetMembershipOprfResponse();
  ServerWillReplyWithPrivateSetMembershipQueryResponse();

  // Succeed for both DeviceAutoEnrollmentRequest and
  // DeviceStateRetrievalRequest. And the result of DeviceAutoEnrollmentRequest
  // is positive.
  const bool kExpectedHashDanceResult = true;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true,
                  /*with_id_hash=*/kExpectedHashDanceResult);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kWithLicense);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify private set membership result.
  const StateDiscoveryResult expected_psm_state_result =
      GetExpectedMembershipResult()
          ? StateDiscoveryResult::kSuccessHasServerSideState
          : StateDiscoveryResult::kSuccessNoServerSideState;
  EXPECT_EQ(GetStateDiscoveryResult(), expected_psm_state_result);

  // Verify Hash dance result.
  VerifyCachedResult(kExpectedHashDanceResult, kPowerLimit);

  // Verify device state result.
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kWithLicense);

  // Trigger AutoEnrollmentClientImpl retry.
  client()->Retry();
  base::RunLoop().RunUntilIdle();

  // Verify that private set membership cached decision hasn't changed, and no
  // new requests have been sent.
  EXPECT_EQ(GetStateDiscoveryResult(), expected_psm_state_result);
  ExpectPrivateSetMembershipHistograms(
      {PrivateSetMembershipStatus::kAttempt,
       PrivateSetMembershipStatus::kSuccessfulDetermination},
      /*success_time_recorded=*/true);
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();

  // Verify that Hash dance cached decision hasn't changed, and no new request
  // has been sent.
  VerifyCachedResult(kExpectedHashDanceResult, kPowerLimit);
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);

  // Verify recorded comparison between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      (GetExpectedMembershipResult() == kExpectedHashDanceResult)
          ? PrivateSetMembershipHashDanceComparison::kEqualResults
          : PrivateSetMembershipHashDanceComparison::kDifferentResults);
}

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       RetryLogicAfterPrivateSetMembershipRlweOprfFailedAndHashDanceFailed) {
  InSequence sequence;

  // Fail for private set membership RLWE OPRF request.
  ServerWillFailForPrivateSetMembership(
      net::OK, DeviceManagementService::kServiceUnavailable);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify failure of private set membership protocol.
  const StateDiscoveryResult expected_psm_state_result =
      StateDiscoveryResult::kFailure;
  EXPECT_EQ(GetStateDiscoveryResult(), expected_psm_state_result);

  // Verify failure of Hash dance by inexistence of its cached decision.
  EXPECT_FALSE(HasCachedDecision());

  // Verify that no enrollment has been done, and no state has been retrieved.
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasServerBackedState());

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  // Trigger AutoEnrollmentClientImpl retry.
  client()->Retry();
  base::RunLoop().RunUntilIdle();

  // Verify that private set membership cached decision hasn't changed, and no
  // new requests have been sent.
  EXPECT_EQ(GetStateDiscoveryResult(), expected_psm_state_result);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);
  VerifyPrivateSetMembershipRlweOprfRequest();
  VerifyPrivateSetMembershipLastRequestJobType();

  // Verify inexistence of Hash dance cached decision, and its new request
  // has failed again.
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_INVALID);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);

  // Verify recorded comparison between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      PrivateSetMembershipHashDanceComparison::kBothError);
}

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       RetryWhileWaitingForPrivateSetMembershipOprfResponseAndHashDanceFails) {
  InSequence sequence;

  DeviceManagementService::JobControl* psm_rlwe_oprf_job = nullptr;
  DeviceManagementService::JobControl* hash_dance_job = nullptr;

  // Expect two requests and capture them, in order, when available in
  // |psm_rlwe_oprf_job| and |hash_dance_job|.
  ServerWillReplyAsyncForPrivateSetMembership(&psm_rlwe_oprf_job);
  ServerWillReplyAsync(&hash_dance_job);

  // Expect none of the jobs have been captured.
  EXPECT_FALSE(psm_rlwe_oprf_job);
  EXPECT_FALSE(hash_dance_job);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify the only job that has been captured is the PSM RLWE OPRF request.
  VerifyPrivateSetMembershipRlweOprfRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
  ASSERT_TRUE(psm_rlwe_oprf_job);
  EXPECT_FALSE(hash_dance_job);

  // Trigger RetryStep.
  client()->Retry();

  // Verify hash dance job has not been triggered after RetryStep.
  EXPECT_FALSE(hash_dance_job);

  // Fail for private set membership RLWE OPRF request.
  ServerFailsForAsyncJob(&psm_rlwe_oprf_job);

  // Verify failure of private set membership protocol.
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  ExpectPrivateSetMembershipHistograms({PrivateSetMembershipStatus::kAttempt,
                                        PrivateSetMembershipStatus::kError},
                                       /*success_time_recorded=*/false);

  // Verify hash dance job has been captured.
  ASSERT_TRUE(hash_dance_job);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            last_async_job_type_);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request.
  ServerFailsForAsyncJob(&hash_dance_job);

  // Verify failure of Hash dance by inexistence of its cached decision.
  EXPECT_FALSE(HasCachedDecision());

  // Verify that no enrollment has been done, and no state has been retrieved.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasServerBackedState());

  // Verify recorded comparison between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      PrivateSetMembershipHashDanceComparison::kBothError);

  // Verify both jobs have finished.
  EXPECT_EQ(hash_dance_job, nullptr);
  EXPECT_EQ(psm_rlwe_oprf_job, nullptr);
}

TEST_P(PrivateSetMembershipHelperAndHashDanceTest,
       RetryWhileWaitingForPrivateSetMembershipQueryResponseAndHashDanceFails) {
  InSequence sequence;

  const base::TimeDelta kOneSecondTimeDelta = base::TimeDelta::FromSeconds(1);

  DeviceManagementService::JobControl* psm_rlwe_oprf_job = nullptr;
  DeviceManagementService::JobControl* psm_rlwe_query_job = nullptr;
  DeviceManagementService::JobControl* hash_dance_job = nullptr;

  // Expect three requests and capture them, in order, when available in
  // |psm_rlwe_oprf_job|, |psm_rlwe_query_job|, and |hash_dance_job|.
  ServerWillReplyAsyncForPrivateSetMembership(&psm_rlwe_oprf_job);
  ServerWillReplyAsyncForPrivateSetMembership(&psm_rlwe_query_job);
  ServerWillReplyAsync(&hash_dance_job);

  // Expect none of the jobs have been captured.
  EXPECT_FALSE(psm_rlwe_oprf_job);
  EXPECT_FALSE(psm_rlwe_query_job);
  EXPECT_FALSE(hash_dance_job);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify the only job that has been captured is the PSM RLWE OPRF request.
  VerifyPrivateSetMembershipRlweOprfRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
  ASSERT_TRUE(psm_rlwe_oprf_job);
  EXPECT_FALSE(psm_rlwe_query_job);
  EXPECT_FALSE(hash_dance_job);

  // Reply with PSM RLWE OPRF response.
  ServerReplyForPrivateSetMembershipAsyncJobWithOprfResponse(
      &psm_rlwe_oprf_job);

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  // Verify the only job that has been captured is the PSM RLWE Query request.
  VerifyPrivateSetMembershipRlweQueryRequest();
  VerifyPrivateSetMembershipLastRequestJobType();
  ASSERT_TRUE(psm_rlwe_query_job);
  EXPECT_FALSE(hash_dance_job);

  // Trigger RetryStep.
  client()->Retry();

  // Verify hash dance job has not been triggered after RetryStep.
  EXPECT_FALSE(hash_dance_job);

  // Reply with PSM RLWE Query response.
  ServerReplyForPrivateSetMembershipAsyncJobWithQueryResponse(
      &psm_rlwe_query_job);

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  // Verify private set membership result.
  EXPECT_EQ(GetStateDiscoveryResult(),
            GetExpectedMembershipResult()
                ? StateDiscoveryResult::kSuccessHasServerSideState
                : StateDiscoveryResult::kSuccessNoServerSideState);
  ExpectPrivateSetMembershipHistograms(
      {PrivateSetMembershipStatus::kAttempt,
       PrivateSetMembershipStatus::kSuccessfulDetermination},
      /*success_time_recorded=*/true);

  // Verify hash dance job has been captured.
  ASSERT_TRUE(hash_dance_job);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            last_async_job_type_);

  // Fail for DeviceAutoEnrollmentRequest i.e. hash dance request by sending
  // an empty response.
  ServerRepliesEmptyResponseForAsyncJob(&hash_dance_job);

  // Verify Hash dance execution time histogram was recorded correctly.
  const std::string kUMAHashDanceProtocolTimeStr = kUMAHashDanceProtocolTime;
  histogram_tester_.ExpectUniqueTimeSample(
      kUMAHashDanceProtocolTimeStr + kUMAHashDanceSuffixInitialEnrollment,
      kOneSecondTimeDelta, /*expected_count=*/1);

  // Verify failure of Hash dance by inexistence of its cached decision.
  EXPECT_FALSE(HasCachedDecision());

  // Verify that no enrollment has been done, and no state has been retrieved.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasServerBackedState());

  // Verify recorded comparison between private set membership and Hash
  // dance.
  ExpectPrivateSetMembershipHashDanceComparisonRecorded(
      PrivateSetMembershipHashDanceComparison::kPSMSuccessHashDanceError);

  // Verify all jobs have finished.
  EXPECT_EQ(hash_dance_job, nullptr);
  EXPECT_EQ(psm_rlwe_oprf_job, nullptr);
  EXPECT_EQ(psm_rlwe_query_job, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    PrivateSetMembershipAndHashDance,
    PrivateSetMembershipHelperAndHashDanceTest,
    testing::Combine(testing::Values(AutoEnrollmentClientImplTestState(
                         AutoEnrollmentProtocol::kInitialEnrollment,
                         PrivateSetMembershipState::kEnabled)),
                     ::testing::Range(0,
                                      kNumberOfPrivateSetMembershipTestCases)));
}  // namespace
}  // namespace policy
