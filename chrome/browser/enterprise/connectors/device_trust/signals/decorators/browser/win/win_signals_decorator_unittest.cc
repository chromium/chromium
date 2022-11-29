// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/win/win_signals_decorator.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

constexpr wchar_t kCSAgentRegPath[] =
    L"SYSTEM\\CurrentControlSet\\services\\CSAgent\\Sim";
constexpr wchar_t kCSCURegKey[] = L"CU";
constexpr wchar_t kCSAGRegKey[] = L"AG";

// Those IDs are Hex values.
constexpr char kFakeCSAgentId[] = "ADEBCA432156ABDC";
constexpr char kFakeCSCustomerId[] = "CABCDEF1234ABCD1234D";

constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Win";

void CreateRegistryKey(const std::wstring& key_path) {
  base::win::RegKey key;
  LONG res = key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_WRITE);
  ASSERT_EQ(res, ERROR_SUCCESS);
}

void SetUpCrowdStrikeInfo(const absl::optional<std::string>& customer_id,
                          const absl::optional<std::string>& agent_id) {
  base::win::RegKey key;
  LONG res = key.Open(HKEY_LOCAL_MACHINE, kCSAgentRegPath, KEY_WRITE);
  ASSERT_EQ(res, ERROR_SUCCESS);

  if (customer_id) {
    res = key.WriteValue(kCSCURegKey, customer_id->data(), customer_id->size(),
                         REG_BINARY);
    ASSERT_EQ(res, ERROR_SUCCESS);
  }

  if (agent_id) {
    res = key.WriteValue(kCSAGRegKey, agent_id->data(), agent_id->size(),
                         REG_BINARY);
    ASSERT_EQ(res, ERROR_SUCCESS);
  }
}

}  // namespace

class WinSignalsDecoratorTest : public testing::Test {
 protected:
  WinSignalsDecoratorTest() {
    registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  WinSignalsDecorator decorator_;
};

TEST_F(WinSignalsDecoratorTest, NoCSRegistry) {
  // HKLM hive was overridden and no value was set, therefore no CS value should
  // be returned.
  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator_.Decorate(signals, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(signals.Find(device_signals::names::kCrowdStrike));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

TEST_F(WinSignalsDecoratorTest, EmptyCSRegistry) {
  CreateRegistryKey(kCSAgentRegPath);

  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator_.Decorate(signals, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(signals.Find(device_signals::names::kCrowdStrike));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

TEST_F(WinSignalsDecoratorTest, SuccessCSCustomerId) {
  CreateRegistryKey(kCSAgentRegPath);

  // Have to Hex-decode the values before storing them.
  std::string decoded_customer_id;
  ASSERT_TRUE(base::HexStringToString(kFakeCSCustomerId, &decoded_customer_id));

  SetUpCrowdStrikeInfo(decoded_customer_id, absl::nullopt);

  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator_.Decorate(signals, run_loop.QuitClosure());
  run_loop.Run();

  auto* cs_value = signals.Find(device_signals::names::kCrowdStrike);
  ASSERT_TRUE(cs_value);
  ASSERT_TRUE(cs_value->is_dict());

  const auto& cs_value_dict = cs_value->GetDict();

  auto* customer_id =
      cs_value_dict.FindString(device_signals::names::kCustomerId);
  ASSERT_TRUE(customer_id);
  EXPECT_EQ(*customer_id, base::ToLowerASCII(kFakeCSCustomerId));

  auto* agent_id = cs_value_dict.FindString(device_signals::names::kAgentId);
  EXPECT_FALSE(agent_id);

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

TEST_F(WinSignalsDecoratorTest, SuccessCSAgentId) {
  CreateRegistryKey(kCSAgentRegPath);

  // Have to Hex-decode the values before storing them.
  std::string decoded_agent_id;
  ASSERT_TRUE(base::HexStringToString(kFakeCSAgentId, &decoded_agent_id));
  SetUpCrowdStrikeInfo(absl::nullopt, decoded_agent_id);

  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator_.Decorate(signals, run_loop.QuitClosure());
  run_loop.Run();

  auto* cs_value = signals.Find(device_signals::names::kCrowdStrike);
  ASSERT_TRUE(cs_value);
  ASSERT_TRUE(cs_value->is_dict());

  const auto& cs_value_dict = cs_value->GetDict();

  auto* customer_id =
      cs_value_dict.FindString(device_signals::names::kCustomerId);
  EXPECT_FALSE(customer_id);

  auto* agent_id = cs_value_dict.FindString(device_signals::names::kAgentId);
  ASSERT_TRUE(agent_id);
  EXPECT_EQ(*agent_id, base::ToLowerASCII(kFakeCSAgentId));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

TEST_F(WinSignalsDecoratorTest, SuccessAllCS) {
  CreateRegistryKey(kCSAgentRegPath);

  // Have to Hex-decode the values before storing them.
  std::string decoded_customer_id;
  ASSERT_TRUE(base::HexStringToString(kFakeCSCustomerId, &decoded_customer_id));

  std::string decoded_agent_id;
  ASSERT_TRUE(base::HexStringToString(kFakeCSAgentId, &decoded_agent_id));
  SetUpCrowdStrikeInfo(decoded_customer_id, decoded_agent_id);

  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator_.Decorate(signals, run_loop.QuitClosure());
  run_loop.Run();

  auto* cs_value = signals.Find(device_signals::names::kCrowdStrike);
  ASSERT_TRUE(cs_value);
  ASSERT_TRUE(cs_value->is_dict());

  const auto& cs_value_dict = cs_value->GetDict();

  auto* customer_id =
      cs_value_dict.FindString(device_signals::names::kCustomerId);
  ASSERT_TRUE(customer_id);
  EXPECT_EQ(*customer_id, base::ToLowerASCII(kFakeCSCustomerId));

  auto* agent_id = cs_value_dict.FindString(device_signals::names::kAgentId);
  ASSERT_TRUE(agent_id);
  EXPECT_EQ(*agent_id, base::ToLowerASCII(kFakeCSAgentId));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

}  // namespace enterprise_connectors
