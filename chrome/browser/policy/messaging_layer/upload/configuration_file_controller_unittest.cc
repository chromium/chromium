// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/configuration_file_controller.h"
#include <cstdint>

#include "base/task/thread_pool.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"

using ::base::test::EqualsProto;
using ::testing::HasSubstr;

namespace reporting {

static constexpr int kBaseChromeVersion = 50;
static constexpr int kConfigFileVersion = 11111;
static constexpr char kSignature[] = "XXXXXXXXXXXXXX";
static constexpr uint8_t kBadSignature[] = {
    0xD1, 0x9E, 0x7D, 0x77, 0xD4, 0x8D, 0xC9, 0xAC, 0x27, 0x54, 0x40,
    0xC2, 0x8A, 0xDE, 0xE1, 0xEE, 0xCE, 0xED, 0x15, 0xC9, 0x9C, 0x87,
    0xB6, 0xB0, 0x32, 0xC1, 0x37, 0x3D, 0x06, 0x05, 0x5C, 0xFD, 0x63,
    0x17, 0x54, 0x62, 0x53, 0xBC, 0x79, 0xAD, 0x82, 0xC3, 0xF1, 0xBC,
    0xB6, 0x0A, 0xC9, 0x2A, 0x42, 0xD0, 0xFC, 0x00, 0xAD, 0xE6, 0x84,
    0x29, 0xBC, 0x7F, 0x10, 0xC3, 0x19, 0x85, 0xA2, 0x0E};
static_assert(std::size(kBadSignature) == kSignatureSize);

class ConfigurationFileControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    controller_provider_ = ConfigurationFileController::CreateForTesting(
        update_config_in_missive_event_.repeating_cb(),
        ListOfBlockedDestinations(), kBaseChromeVersion);
    scoped_feature_list_.InitAndEnableFeature(kShouldRequestConfigurationFile);
  }
  void TearDown() override {}

  void EnableSignatureTestFlag() {
    scoped_feature_list_.Reset();
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitFromCommandLine(
        "ReportingConfigurationFileTestSignature, "
        "ShouldRequestConfigurationFile",
        "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  void ControllerWithList(ListOfBlockedDestinations list) {
    controller_provider_ = ConfigurationFileController::CreateForTesting(
        update_config_in_missive_event_.repeating_cb(), std::move(list),
        kBaseChromeVersion);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});

  std::unique_ptr<ConfigurationFileController> controller_provider_;

  test::TestEvent<ListOfBlockedDestinations> update_config_in_missive_event_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ConfigurationFileControllerTest, EmptyConfigFileDoesNotCallMissive) {
  ConfigFile test_config_file;
  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  EXPECT_EQ(handle_configuration_file_result, 0);
}

TEST_F(ConfigurationFileControllerTest, VersionMinusOneDoesNotCallMissive) {
  ConfigFile test_config_file;
  test_config_file.set_version(-1);
  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  EXPECT_EQ(handle_configuration_file_result, 0);
}

TEST_F(ConfigurationFileControllerTest,
       EmptyBlockedEventConfigsUpdatesVersionButDoesNotCallMissive) {
  EnableSignatureTestFlag();
  ConfigFile test_config_file;
  test_config_file.set_config_file_signature(kSignature);
  test_config_file.set_version(kConfigFileVersion);

  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  EXPECT_EQ(handle_configuration_file_result, kConfigFileVersion);
}

TEST_F(ConfigurationFileControllerTest,
       ConfigFileExperimentDisabledDoesNothing) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(kShouldRequestConfigurationFile);
  ConfigFile test_config_file;
  auto* const current_config = test_config_file.add_blocked_event_configs();
  current_config->set_destination(Destination::DLP_EVENTS);
  test_config_file.set_config_file_signature(kSignature);
  test_config_file.set_version(kConfigFileVersion);

  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  // We expect -1 when the experiment is disabled.
  EXPECT_EQ(handle_configuration_file_result, kFeatureDisabled);
}

TEST_F(ConfigurationFileControllerTest,
       TestFlagCallsMissiveWithSingleDestination) {
  EnableSignatureTestFlag();
  ConfigFile test_config_file;

  // Populate one blocked destination.
  test_config_file.set_version(kConfigFileVersion);
  test_config_file.add_blocked_event_configs()->set_destination(
      Destination::DLP_EVENTS);
  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);

  // Create list to compare against.
  ListOfBlockedDestinations list;
  list.add_destinations(Destination::DLP_EVENTS);

  const auto result = update_config_in_missive_event_.result();
  EXPECT_THAT(result, EqualsProto(list));
  EXPECT_EQ(handle_configuration_file_result, kConfigFileVersion);
}

TEST_F(ConfigurationFileControllerTest,
       TestFlagCallsMissiveWithMultipleDestinations) {
  EnableSignatureTestFlag();
  ConfigFile test_config_file;

  // Populate one blocked destination.
  test_config_file.set_version(kConfigFileVersion);
  test_config_file.add_blocked_event_configs()->set_destination(
      Destination::DLP_EVENTS);
  test_config_file.add_blocked_event_configs()->set_destination(
      Destination::LOGIN_LOGOUT_EVENTS);
  test_config_file.add_blocked_event_configs()->set_destination(
      Destination::OS_EVENTS);
  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);

  // Create list to compare against.
  ListOfBlockedDestinations list;
  list.add_destinations(Destination::DLP_EVENTS);
  list.add_destinations(Destination::LOGIN_LOGOUT_EVENTS);
  list.add_destinations(Destination::OS_EVENTS);

  const auto result = update_config_in_missive_event_.result();
  EXPECT_THAT(result, EqualsProto(list));
  EXPECT_EQ(handle_configuration_file_result, kConfigFileVersion);
}

TEST_F(ConfigurationFileControllerTest,
       TestFlagWithMultipleDestinationsNotInRange) {
  EnableSignatureTestFlag();
  ConfigFile test_config_file;

  // Populate two blocked destination that are not in the range to be blocked.
  test_config_file.set_version(kConfigFileVersion);
  auto* dlp_destination = test_config_file.add_blocked_event_configs();
  dlp_destination->set_destination(Destination::DLP_EVENTS);
  dlp_destination->set_minimum_release_version(kBaseChromeVersion + 2);
  auto* login_destination = test_config_file.add_blocked_event_configs();
  login_destination->set_destination(Destination::LOGIN_LOGOUT_EVENTS);
  login_destination->set_minimum_release_version(kBaseChromeVersion + 5);
  login_destination->set_maximum_release_version(kBaseChromeVersion + 10);

  // Call the public method.
  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);

  // Verify it doesn't call missive but returns the correct version.
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  EXPECT_EQ(handle_configuration_file_result, kConfigFileVersion);
}

TEST_F(ConfigurationFileControllerTest,
       TestFlagWithMultipleDestinationsOneInRange) {
  EnableSignatureTestFlag();
  ConfigFile test_config_file;

  // Populate two blocked destination one in range and the other one out of
  // range.
  test_config_file.set_version(kConfigFileVersion);
  auto* const dlp_destination = test_config_file.add_blocked_event_configs();
  dlp_destination->set_destination(Destination::DLP_EVENTS);
  dlp_destination->set_minimum_release_version(kBaseChromeVersion);
  auto* const login_destination = test_config_file.add_blocked_event_configs();
  login_destination->set_destination(Destination::LOGIN_LOGOUT_EVENTS);
  login_destination->set_minimum_release_version(kBaseChromeVersion + 5);
  login_destination->set_maximum_release_version(kBaseChromeVersion + 10);

  // Call the public method.
  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  // Create list to compare against.
  ListOfBlockedDestinations list;
  list.add_destinations(Destination::DLP_EVENTS);

  const auto result = update_config_in_missive_event_.result();
  EXPECT_THAT(result, EqualsProto(list));
  EXPECT_EQ(handle_configuration_file_result, kConfigFileVersion);
}

// TODO(b/302544753): Add test to verify that the `ConfigurationFileController`
// calls missive when provided with a valid prod signature. Adding this after
// server MVP implementation.

TEST_F(ConfigurationFileControllerTest, BadProdSignatureSendsUMA) {
  base::HistogramTester histogram_tester;
  ConfigFile test_config_file;
  test_config_file.set_version(kConfigFileVersion);
  test_config_file.add_blocked_event_configs()->set_destination(
      Destination::DLP_EVENTS);
  test_config_file.add_blocked_event_configs()->set_destination(
      Destination::OS_EVENTS);
  test_config_file.set_config_file_signature(std::string(
      reinterpret_cast<const char*>(kBadSignature), kSignatureSize));

  const int handle_result =
      controller_provider_->HandleConfigurationFile(test_config_file);

  histogram_tester.ExpectTotalCount(
      "Browser.ERP.ConfigFileSignatureVerificationError", 1);
  histogram_tester.ExpectBucketCount(
      "Browser.ERP.ConfigFileSignatureVerificationError",
      error::INVALID_ARGUMENT, 1);
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  EXPECT_EQ(handle_result, kConfigurationFileCorrupted);
}

TEST_F(ConfigurationFileControllerTest, NoConfigFileSignatureSendsUMA) {
  base::HistogramTester histogram_tester;
  ConfigFile test_config_file;
  auto* const current_config = test_config_file.add_blocked_event_configs();
  current_config->set_destination(Destination::DLP_EVENTS);
  test_config_file.set_version(kConfigFileVersion);

  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  histogram_tester.ExpectTotalCount(
      "Browser.ERP.ConfigFileSignatureVerificationError", 1);
  histogram_tester.ExpectBucketCount(
      "Browser.ERP.ConfigFileSignatureVerificationError",
      error::INVALID_ARGUMENT, 1);
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  EXPECT_EQ(handle_configuration_file_result, kConfigurationFileCorrupted);
}

TEST_F(ConfigurationFileControllerTest, BadConfigFileSignatureSizeSendsUMA) {
  base::HistogramTester histogram_tester;
  ConfigFile test_config_file;
  auto* const current_config = test_config_file.add_blocked_event_configs();
  current_config->set_destination(Destination::DLP_EVENTS);
  test_config_file.set_config_file_signature(kSignature);
  test_config_file.set_version(kConfigFileVersion);

  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  histogram_tester.ExpectTotalCount(
      "Browser.ERP.ConfigFileSignatureVerificationError", 1);
  histogram_tester.ExpectBucketCount(
      "Browser.ERP.ConfigFileSignatureVerificationError",
      error::FAILED_PRECONDITION, 1);
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  EXPECT_EQ(handle_configuration_file_result, kConfigurationFileCorrupted);
}

TEST_F(ConfigurationFileControllerTest,
       PreviousDestinationListEqualDoesntCallMissive) {
  EnableSignatureTestFlag();
  ListOfBlockedDestinations list;
  list.add_destinations(Destination::LOCK_UNLOCK_EVENTS);
  ControllerWithList(list);

  ConfigFile test_config_file;
  test_config_file.add_blocked_event_configs()->set_destination(
      Destination::LOCK_UNLOCK_EVENTS);
  test_config_file.set_version(kConfigFileVersion);

  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  EXPECT_TRUE(update_config_in_missive_event_.no_result());
  EXPECT_EQ(handle_configuration_file_result, kConfigFileVersion);
}

TEST_F(ConfigurationFileControllerTest,
       PreviousDestinationListDifferentCallsMissive) {
  EnableSignatureTestFlag();
  ListOfBlockedDestinations initial_list;
  initial_list.add_destinations(Destination::OS_EVENTS);
  ControllerWithList(initial_list);

  ConfigFile test_config_file;
  test_config_file.add_blocked_event_configs()->set_destination(
      Destination::LOCK_UNLOCK_EVENTS);
  test_config_file.set_version(kConfigFileVersion);

  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);
  // Create list to compare against.
  ListOfBlockedDestinations list;
  list.add_destinations(Destination::LOCK_UNLOCK_EVENTS);

  const auto result = update_config_in_missive_event_.result();
  EXPECT_THAT(result, EqualsProto(list));
  EXPECT_EQ(handle_configuration_file_result, kConfigFileVersion);
}

TEST_F(ConfigurationFileControllerTest,
       PreviousDestinationListPopulatedNewDestinationListEmptyCallsMissive) {
  EnableSignatureTestFlag();
  ListOfBlockedDestinations initial_list;
  initial_list.add_destinations(Destination::OS_EVENTS);
  ControllerWithList(initial_list);

  ConfigFile test_config_file;
  test_config_file.set_version(kConfigFileVersion);

  const int handle_configuration_file_result =
      controller_provider_->HandleConfigurationFile(test_config_file);

  const auto result = update_config_in_missive_event_.result();
  EXPECT_THAT(result, EqualsProto(ListOfBlockedDestinations()));
  EXPECT_EQ(handle_configuration_file_result, kConfigFileVersion);
}

}  // namespace reporting
