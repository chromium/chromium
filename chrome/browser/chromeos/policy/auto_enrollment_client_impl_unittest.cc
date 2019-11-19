// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/auto_enrollment_client_impl.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

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

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;

enum class AutoEnrollmentProtocol { kFRE, kInitialEnrollment };

class AutoEnrollmentClientImplTest
    : public testing::Test,
      public ::testing::WithParamInterface<AutoEnrollmentProtocol> {
 protected:
  AutoEnrollmentClientImplTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()),
        local_state_(scoped_testing_local_state_.Get()),
        state_(AUTO_ENROLLMENT_STATE_PENDING) {}

  void SetUp() override {
    CreateClient(4, 8);
    ASSERT_FALSE(local_state_->GetUserPref(prefs::kShouldAutoEnroll));
    ASSERT_FALSE(local_state_->GetUserPref(prefs::kAutoEnrollmentPowerLimit));
  }

  void TearDown() override {
    // Flush any deletion tasks.
    base::RunLoop().RunUntilIdle();
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
    if (GetParam() == AutoEnrollmentProtocol::kFRE) {
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
            GetParam() == AutoEnrollmentProtocol::kFRE
                ? hash_full
                : hash_full.substr(0, kInitialEnrollmentIdHashLength);
        enrollment_response->mutable_hashes()->Add()->assign(hash);
      }
    }
    if (with_id_hash) {
      if (GetParam() == AutoEnrollmentProtocol::kFRE) {
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
        return DeviceInitialEnrollmentState::INITIAL_ENROLLMENT_MODE_NONE;
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
    if (GetParam() == AutoEnrollmentProtocol::kFRE) {
      ServerWillSendFREState(management_domain, restore_mode,
                             device_disabled_message);
    } else {
      ServerWillSendInitialEnrollmentState(
          management_domain, is_license_packaged_with_device,
          MapRestoreModeToInitialEnrollmentMode(restore_mode));
    }
  }

  DeviceManagementService::JobConfiguration::JobType
  GetStateRetrievalJobType() {
    return GetParam() == AutoEnrollmentProtocol::kFRE
               ? DeviceManagementService::JobConfiguration::
                     TYPE_DEVICE_STATE_RETRIEVAL
               : DeviceManagementService::JobConfiguration::
                     TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL;
  }

  void ServerWillSendFREState(
      const std::string& management_domain,
      em::DeviceStateRetrievalResponse::RestoreMode restore_mode,
      const std::string& device_disabled_message) {
    em::DeviceManagementResponse response;
    em::DeviceStateRetrievalResponse* state_response =
        response.mutable_device_state_retrieval_response();
    state_response->set_restore_mode(restore_mode);
    state_response->set_management_domain(management_domain);
    state_response->mutable_disabled_state()->set_message(
        device_disabled_message);

    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(
            DoAll(service_->CaptureJobType(&state_retrieval_job_type_),
                  service_->CaptureRequest(&last_request_),
                  service_->StartJobAsync(
                      net::OK, DeviceManagementService::kSuccess, response)))
        .RetiresOnSaturation();
  }

  void ServerWillSendInitialEnrollmentState(
      const std::string& management_domain,
      bool is_license_packaged_with_device,
      em::DeviceInitialEnrollmentStateResponse::InitialEnrollmentMode
          initial_enrollment_mode) {
    em::DeviceManagementResponse response;
    em::DeviceInitialEnrollmentStateResponse* state_response =
        response.mutable_device_initial_enrollment_state_response();
    state_response->set_initial_enrollment_mode(initial_enrollment_mode);
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
    return GetParam() == AutoEnrollmentProtocol::kFRE
               ? DeviceManagementService::JobConfiguration::
                     TYPE_DEVICE_STATE_RETRIEVAL
               : DeviceManagementService::JobConfiguration::
                     TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL;
  }

  void ServerWillReplyAsync(DeviceManagementService::JobControl** job) {
    EXPECT_CALL(*service_, StartJob(_))
        .WillOnce(service_->StartJobFullControl(job));
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
    const base::Value* state =
        local_state_->GetUserPref(prefs::kServerBackedDeviceState);
    ASSERT_TRUE(state);
    const base::DictionaryValue* state_dict = nullptr;
    ASSERT_TRUE(state->GetAsDictionary(&state_dict));

    std::string actual_management_domain;
    EXPECT_TRUE(state_dict->GetString(kDeviceStateManagementDomain,
                                      &actual_management_domain));
    EXPECT_EQ(expected_management_domain, actual_management_domain);

    if (!expected_restore_mode.empty()) {
      std::string actual_restore_mode;
      EXPECT_TRUE(
          state_dict->GetString(kDeviceStateMode, &actual_restore_mode));
      EXPECT_EQ(GetParam() == AutoEnrollmentProtocol::kFRE
                    ? expected_restore_mode
                    : MapDeviceRestoreStateToDeviceInitialState(
                          expected_restore_mode),
                actual_restore_mode);
    } else {
      EXPECT_FALSE(state_dict->HasKey(kDeviceStateMode));
    }

    std::string actual_disabled_message;
    if (GetParam() == AutoEnrollmentProtocol::kFRE) {
      EXPECT_TRUE(state_dict->GetString(kDeviceStateDisabledMessage,
                                        &actual_disabled_message));
      EXPECT_EQ(expected_disabled_message, actual_disabled_message);
    } else {
      EXPECT_FALSE(state_dict->GetString(kDeviceStateDisabledMessage,
                                         &actual_disabled_message));
    }

    if (GetParam() == AutoEnrollmentProtocol::kFRE) {
      EXPECT_FALSE(state_dict->FindBoolPath(kDeviceStatePackagedLicense));
    } else {
      base::Optional<bool> actual_is_license_packaged_with_device;
      actual_is_license_packaged_with_device =
          state_dict->FindBoolPath(kDeviceStatePackagedLicense);
      EXPECT_TRUE(actual_is_license_packaged_with_device.has_value());
      EXPECT_EQ(expected_is_license_packaged_with_device,
                actual_is_license_packaged_with_device.value());
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

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;
  TestingPrefServiceSimple* local_state_;
  std::unique_ptr<MockDeviceManagementService> service_;
  em::DeviceManagementRequest last_request_;
  AutoEnrollmentState state_;
  DeviceManagementService::JobConfiguration::JobType failed_job_type_ =
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
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_SERVER_ERROR, state_);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, EmptyReply) {
  ServerWillReply(-1, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
  VerifyCachedResult(false, 8);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ClientUploadsRightBits) {
  ServerWillReply(-1, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);

  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(16, auto_enrollment_request().modulus());
  if (GetParam() == AutoEnrollmentProtocol::kFRE) {
    EXPECT_EQ(kStateKeyHash[31] & 0xf, auto_enrollment_request().remainder());
  } else {
    EXPECT_EQ(kInitialEnrollmentIdHash[7] & 0xf,
              auto_enrollment_request().remainder());
  }
  VerifyCachedResult(false, 8);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForMoreThenFail) {
  InSequence sequence;
  ServerWillReply(32, false, false);
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_SERVER_ERROR, state_);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForMoreThenEvenMore) {
  InSequence sequence;
  ServerWillReply(32, false, false);
  ServerWillReply(64, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_SERVER_ERROR, state_);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
  VerifyCachedResult(true, 8);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
  VerifyCachedResult(true, 8);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_SERVER_ERROR, state_);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForTooMuch) {
  ServerWillReply(512, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_SERVER_ERROR, state_);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, DetectOutdatedServer) {
  CreateClient(0, kInitialEnrollmentModulusPowerOutdatedServer + 1);
  InSequence sequence;
  ServerWillReply(1 << kInitialEnrollmentModulusPowerOutdatedServer, false,
                  false);

  if (GetParam() == AutoEnrollmentProtocol::kInitialEnrollment) {
    // For initial enrollment, a modulus power higher or equal to
    // |kInitialEnrollmentModulusPowerOutdatedServer| means that the client will
    // detect the server as outdated and will skip enrollment.
    client()->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
              auto_enrollment_job_type_);
    EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
    EXPECT_TRUE(HasCachedDecision());
    EXPECT_FALSE(HasServerBackedState());
  } else {
    // For FRE, such a detection does not exist. The client will do the second
    // round and upload bits of its device identifier hash.
    ServerWillReply(-1, false, false);
    client()->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
              auto_enrollment_job_type_);
    EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(128, auto_enrollment_request().modulus());
  if (GetParam() == AutoEnrollmentProtocol::kFRE) {
    EXPECT_EQ(kStateKeyHash[31] & 0x7f, auto_enrollment_request().remainder());
  } else {
    EXPECT_EQ(kInitialEnrollmentIdHash[7] & 0x7f,
              auto_enrollment_request().remainder());
  }
  VerifyCachedResult(false, 8);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ConsumerDevice) {
  ServerWillReply(-1, true, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
  VerifyCachedResult(false, 8);
  EXPECT_FALSE(HasServerBackedState());

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
  VerifyCachedResult(true, 8);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH, state_);
  VerifyCachedResult(true, 8);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentZeroTouch,
                          kDisabledMessage, kNotWithLicense);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH, state_);
}

TEST_P(AutoEnrollmentClientImplTest, RequestedReEnrollment) {
  // Requesting re-enrollment is currently not supported in the
  // initial-enrollment exchange.
  if (GetParam() == AutoEnrollmentProtocol::kInitialEnrollment)
    return;

  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED,
      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
  VerifyCachedResult(true, 8);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentRequested,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, DeviceDisabled) {
  // Disabling is currently not supported in the initial-enrollment exchange.
  if (GetParam() == AutoEnrollmentProtocol::kInitialEnrollment)
    return;

  InSequence sequence;
  ServerWillReply(-1, true, true);
  ServerWillSendState("example.com",
                      em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED,
                      kDisabledMessage, kNotWithLicense);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_DISABLED, state_);
  VerifyCachedResult(true, 8);
  VerifyServerBackedState("example.com", kDeviceStateRestoreModeDisabled,
                          kDisabledMessage, kNotWithLicense);
}

TEST_P(AutoEnrollmentClientImplTest, NoBitsUploaded) {
  CreateClient(0, 0);
  ServerWillReply(-1, false, false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(1, auto_enrollment_request().modulus());
  EXPECT_EQ(0, auto_enrollment_request().remainder());
  VerifyCachedResult(false, 0);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ManyBitsUploaded) {
  int64_t bottom62 = GetParam() == AutoEnrollmentProtocol::kFRE
                         ? INT64_C(0x386e7244d097c3e6)
                         : INT64_C(0x3018b70f7609c5c7);
  for (int i = 0; i <= 62; ++i) {
    CreateClient(i, i);
    ServerWillReply(-1, false, false);
    client()->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
              auto_enrollment_job_type_);
    EXPECT_EQ(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
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
  if (GetParam() == AutoEnrollmentProtocol::kInitialEnrollment)
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
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
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
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
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_SERVER_ERROR, state_);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());

  // The client doesn't retry if no new connection became available.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_SERVER_ERROR, state_);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
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
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
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
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_PENDING, state_);

  // Cancel while a request is in flight.
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());

  // The client cleans itself up once a reply is received.
  service_->DoURLCompletion(&job, net::OK,
                            DeviceManagementService::kServiceUnavailable,
                            em::DeviceManagementResponse());
  EXPECT_EQ(nullptr, job);
  // The DeleteSoon task has been posted:
  EXPECT_FALSE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_PENDING, state_);
}

TEST_P(AutoEnrollmentClientImplTest, NetworkChangedAfterCancelAndDeleteSoon) {
  DeviceManagementService::JobControl* job = nullptr;
  ServerWillReplyAsync(&job);
  EXPECT_FALSE(job);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(job);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_PENDING, state_);

  // Cancel while a request is in flight.
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
  AutoEnrollmentClientImpl* client = release_client();
  client->CancelAndDeleteSoon();
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());

  // Network change events are ignored while a request is pending.
  client->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_PENDING, state_);

  // The client cleans itself up once a reply is received.
  service_->DoURLCompletion(&job, net::OK,
                            DeviceManagementService::kServiceUnavailable,
                            em::DeviceManagementResponse());
  EXPECT_EQ(nullptr, job);
  // The DeleteSoon task has been posted:
  EXPECT_FALSE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_PENDING, state_);

  // Network changes that have been posted before are also ignored:
  client->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_PENDING, state_);
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
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);

  // The client will delete itself immediately if there are no pending
  // requests.
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
}

TEST_P(AutoEnrollmentClientImplTest, CancelAndDeleteSoonAfterNetworkFailure) {
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_SERVER_ERROR, state_);

  // The client will delete itself immediately if there are no pending
  // requests.
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
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
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_CONNECTION_ERROR, state_);
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
  EXPECT_EQ(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT, state_);
  EXPECT_TRUE(HasCachedDecision());
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense);
  Mock::VerifyAndClearExpectations(service_.get());
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            auto_enrollment_job_type_);
  EXPECT_EQ(GetExpectedStateRetrievalJobType(), state_retrieval_job_type_);
}

INSTANTIATE_TEST_SUITE_P(FRE,
                         AutoEnrollmentClientImplTest,
                         testing::Values(AutoEnrollmentProtocol::kFRE));
INSTANTIATE_TEST_SUITE_P(
    InitialEnrollment,
    AutoEnrollmentClientImplTest,
    testing::Values(AutoEnrollmentProtocol::kInitialEnrollment));

}  // namespace
}  // namespace policy
