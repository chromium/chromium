// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/core/mock_dependency_factory.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "components/device_signals/core/browser/mock_signals_aggregator.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/core/dependency_factory.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace enterprise_connectors {

namespace {

constexpr char kFakeBrowserEnrollmentDomain[] = "fake.domain.google.com";
constexpr char kFakeUserEnrollmentDomain[] = "user.fake.domain.google.com";
constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Browser";

constexpr char kFakeAgentId[] = "some-agent-id";
constexpr char kFakeCustomerId[] = "some-cid";

constexpr int32_t kDisabledSetting = 1;
constexpr int32_t kEnabledSetting = 2;

base::Value::List GetExpectedMacAddresses() {
  base::Value::List mac_addresses;
  mac_addresses.Append("00:00:00:00:00:00");
  return mac_addresses;
}

device_signals::SignalsAggregationRequest CreateExpectedRequest() {
  device_signals::SignalsAggregationRequest request;
  request.signal_names.emplace(device_signals::SignalName::kAgent);
  return request;
}

device_signals::SignalsAggregationResponse CreateFilledResponse() {
  device_signals::CrowdStrikeSignals crowdstrike_signals;
  crowdstrike_signals.agent_id = kFakeAgentId;
  crowdstrike_signals.customer_id = kFakeCustomerId;

  device_signals::AgentSignalsResponse agent_signals;
  agent_signals.crowdstrike_signals = crowdstrike_signals;

  device_signals::SignalsAggregationResponse response;
  response.agent_signals_response = agent_signals;
  return response;
}

void ValidateStaticSignals(const base::Value::Dict& signals) {
  const auto* serial_number =
      signals.FindString(device_signals::names::kSerialNumber);
  ASSERT_TRUE(serial_number);
  EXPECT_EQ(*serial_number, "twirlchange");

  auto screen_lock_secured =
      signals.FindInt(device_signals::names::kScreenLockSecured);
  ASSERT_TRUE(screen_lock_secured);
  EXPECT_EQ(screen_lock_secured.value(), kEnabledSetting);

  auto disk_encrypted = signals.FindInt(device_signals::names::kDiskEncrypted);
  ASSERT_TRUE(disk_encrypted);
  EXPECT_EQ(disk_encrypted.value(), kDisabledSetting);

  const auto* device_host_name =
      signals.FindString(device_signals::names::kDeviceHostName);
  ASSERT_TRUE(device_host_name);
  EXPECT_EQ(*device_host_name, "midnightshift");

  const auto* mac_addresses =
      signals.FindList(device_signals::names::kMacAddresses);
  ASSERT_TRUE(mac_addresses);
  EXPECT_EQ(*mac_addresses, GetExpectedMacAddresses());

  const auto* windows_machine_domain =
      signals.FindString(device_signals::names::kWindowsMachineDomain);
  ASSERT_TRUE(windows_machine_domain);
  EXPECT_EQ(*windows_machine_domain, "MACHINE_DOMAIN");

  const auto* windows_user_domain =
      signals.FindString(device_signals::names::kWindowsUserDomain);
  ASSERT_TRUE(windows_user_domain);
  EXPECT_EQ(*windows_user_domain, "USER_DOMAIN");

  auto secure_boot_enabled =
      signals.FindInt(device_signals::names::kSecureBootEnabled);
  ASSERT_TRUE(secure_boot_enabled);
  EXPECT_EQ(secure_boot_enabled.value(), kEnabledSetting);

  auto signals_context = signals.FindInt(device_signals::names::kTrigger);
  ASSERT_TRUE(signals_context);
  EXPECT_EQ(signals_context.value(),
            static_cast<int32_t>(device_signals::Trigger::kBrowserNavigation));
}

void ValidateCrowdStrikeSignals(const base::Value::Dict& signals) {
  auto* cs_value = signals.Find(device_signals::names::kCrowdStrike);
  ASSERT_TRUE(cs_value);
  ASSERT_TRUE(cs_value->is_dict());

  const auto& cs_value_dict = cs_value->GetDict();

  auto* customer_id =
      cs_value_dict.FindString(device_signals::names::kCustomerId);
  ASSERT_TRUE(customer_id);
  EXPECT_EQ(*customer_id, kFakeCustomerId);

  auto* agent_id = cs_value_dict.FindString(device_signals::names::kAgentId);
  ASSERT_TRUE(agent_id);
  EXPECT_EQ(*agent_id, kFakeAgentId);
}

}  // namespace

class BrowserSignalsDecoratorTest : public testing::Test {
 protected:
  void SetUp() override {
    enterprise_signals::DeviceInfoFetcher::SetForceStubForTesting(
        /*should_force=*/true);

    auto mock_browser_cloud_policy_store =
        std::make_unique<policy::MockCloudPolicyStore>();
    mock_browser_cloud_policy_store_ = mock_browser_cloud_policy_store.get();
    mock_browser_cloud_policy_manager_ =
        std::make_unique<policy::MockCloudPolicyManager>(
            std::move(mock_browser_cloud_policy_store),
            task_environment_.GetMainThreadTaskRunner());

    auto mock_user_cloud_policy_store =
        std::make_unique<policy::MockCloudPolicyStore>();
    mock_user_cloud_policy_store_ = mock_user_cloud_policy_store.get();
    mock_user_cloud_policy_manager_ =
        std::make_unique<policy::MockCloudPolicyManager>(
            std::move(mock_user_cloud_policy_store),
            task_environment_.GetMainThreadTaskRunner());
  }

  void TearDown() override {
    enterprise_signals::DeviceInfoFetcher::SetForceStubForTesting(
        /*should_force=*/false);
  }

  void SetFakeBrowserPolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_managed_by(kFakeBrowserEnrollmentDomain);
    mock_browser_cloud_policy_store_->set_policy_data_for_testing(
        std::move(policy_data));
  }

  void SetFakeUserPolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_managed_by(kFakeUserEnrollmentDomain);
    mock_user_cloud_policy_store_->set_policy_data_for_testing(
        std::move(policy_data));
  }

  std::unique_ptr<enterprise_core::DependencyFactory> CreateDependencyFactory(
      bool valid_manager = true) {
    auto mock_dependency_factory =
        std::make_unique<enterprise_core::test::MockDependencyFactory>();
    EXPECT_CALL(*mock_dependency_factory, GetUserCloudPolicyManager())
        .WillOnce(Return(valid_manager ? mock_user_cloud_policy_manager_.get()
                                       : nullptr));
    return mock_dependency_factory;
  }

  BrowserSignalsDecorator CreateDecorator() {
    return BrowserSignalsDecorator(mock_browser_cloud_policy_manager_.get(),
                                   CreateDependencyFactory(),
                                   &mock_aggregator_);
  }

  void SetUpAggregatorExpectations() {
    EXPECT_CALL(mock_aggregator_, GetSignals(CreateExpectedRequest(), _))
        .WillOnce(Invoke(
            [](const device_signals::SignalsAggregationRequest& request,
               base::OnceCallback<void(
                   device_signals::SignalsAggregationResponse)> callback) {
              std::move(callback).Run(CreateFilledResponse());
            }));
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<policy::MockCloudPolicyManager>
      mock_browser_cloud_policy_manager_;
  std::unique_ptr<policy::MockCloudPolicyManager>
      mock_user_cloud_policy_manager_;
  raw_ptr<policy::MockCloudPolicyStore> mock_browser_cloud_policy_store_;
  raw_ptr<policy::MockCloudPolicyStore> mock_user_cloud_policy_store_;
  StrictMock<device_signals::MockSignalsAggregator> mock_aggregator_;
};

TEST_F(BrowserSignalsDecoratorTest, Decorate_AllSignals) {
  SetFakeBrowserPolicyData();
  SetFakeUserPolicyData();
  SetUpAggregatorExpectations();

  auto decorator = CreateDecorator();
  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  ValidateCrowdStrikeSignals(signals);

  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceEnrollmentDomain),
            kFakeBrowserEnrollmentDomain);
  EXPECT_EQ(*signals.FindString(device_signals::names::kUserEnrollmentDomain),
            kFakeUserEnrollmentDomain);

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_NullAggregator) {
  SetFakeBrowserPolicyData();
  SetFakeUserPolicyData();

  BrowserSignalsDecorator decorator(mock_browser_cloud_policy_manager_.get(),
                                    CreateDependencyFactory(), nullptr);
  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);

  EXPECT_FALSE(signals.contains(device_signals::names::kCrowdStrike));
  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceEnrollmentDomain),
            kFakeBrowserEnrollmentDomain);
  EXPECT_EQ(*signals.FindString(device_signals::names::kUserEnrollmentDomain),
            kFakeUserEnrollmentDomain);

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithoutBrowserPolicyData) {
  SetFakeUserPolicyData();
  SetUpAggregatorExpectations();

  auto decorator = CreateDecorator();
  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  ValidateCrowdStrikeSignals(signals);
  EXPECT_FALSE(
      signals.contains(device_signals::names::kDeviceEnrollmentDomain));
  EXPECT_EQ(*signals.FindString(device_signals::names::kUserEnrollmentDomain),
            kFakeUserEnrollmentDomain);
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_NullBrowserPolicyStore) {
  SetFakeUserPolicyData();
  SetUpAggregatorExpectations();

  BrowserSignalsDecorator decorator(nullptr, CreateDependencyFactory(),
                                    &mock_aggregator_);
  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  ValidateCrowdStrikeSignals(signals);
  EXPECT_FALSE(
      signals.contains(device_signals::names::kDeviceEnrollmentDomain));
  EXPECT_EQ(*signals.FindString(device_signals::names::kUserEnrollmentDomain),
            kFakeUserEnrollmentDomain);
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithoutUserPolicyData) {
  SetFakeBrowserPolicyData();
  SetUpAggregatorExpectations();

  auto decorator = CreateDecorator();
  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  ValidateCrowdStrikeSignals(signals);
  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceEnrollmentDomain),
            kFakeBrowserEnrollmentDomain);
  EXPECT_FALSE(signals.contains(device_signals::names::kUserEnrollmentDomain));
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_NullUserPolicyStore) {
  SetFakeBrowserPolicyData();
  SetUpAggregatorExpectations();

  BrowserSignalsDecorator decorator(
      mock_browser_cloud_policy_manager_.get(),
      CreateDependencyFactory(/*valid_manager=*/false), &mock_aggregator_);
  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  ValidateCrowdStrikeSignals(signals);
  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceEnrollmentDomain),
            kFakeBrowserEnrollmentDomain);
  EXPECT_FALSE(signals.contains(device_signals::names::kUserEnrollmentDomain));
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_NoAgentSignals) {
  SetFakeBrowserPolicyData();
  SetFakeUserPolicyData();

  EXPECT_CALL(mock_aggregator_, GetSignals(CreateExpectedRequest(), _))
      .WillOnce(
          Invoke([](const device_signals::SignalsAggregationRequest& request,
                    base::OnceCallback<void(
                        device_signals::SignalsAggregationResponse)> callback) {
            device_signals::SignalsAggregationResponse empty_response;
            std::move(callback).Run(std::move(empty_response));
          }));

  auto decorator = CreateDecorator();
  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  EXPECT_FALSE(signals.contains(device_signals::names::kCrowdStrike));

  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceEnrollmentDomain),
            kFakeBrowserEnrollmentDomain);
  EXPECT_EQ(*signals.FindString(device_signals::names::kUserEnrollmentDomain),
            kFakeUserEnrollmentDomain);

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

}  // namespace enterprise_connectors
