// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/win/win_signals_decorator.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {
constexpr char kLatencyHistogramVariant[] = "Win";

constexpr wchar_t kCSAgentRegPath[] =
    L"SYSTEM\\CurrentControlSet\\services\\CSAgent\\Sim";

// CU is the registry value containing the customer ID.
constexpr wchar_t kCSCURegKey[] = L"CU";

// AG is the registry value containing the agent ID.
constexpr wchar_t kCSAGRegKey[] = L"AG";

absl::optional<std::string> GetHexStringRegValue(
    const base::win::RegKey& key,
    const std::wstring& reg_key_name) {
  DWORD type = REG_NONE;
  DWORD size = 0;
  auto res = key.ReadValue(reg_key_name.c_str(), nullptr, &size, &type);
  if (res == ERROR_SUCCESS && type == REG_BINARY) {
    std::vector<uint8_t> raw_bytes(size);
    res = key.ReadValue(reg_key_name.c_str(), raw_bytes.data(), &size, &type);

    if (res == ERROR_SUCCESS) {
      return base::HexEncode(raw_bytes.data(), raw_bytes.size());
    }
  }

  return absl::nullopt;
}

}  // namespace

WinSignalsDecorator::WinSignalsDecorator() = default;

WinSignalsDecorator::~WinSignalsDecorator() = default;

void WinSignalsDecorator::Decorate(base::Value::Dict& signals,
                                   base::OnceClosure done_closure) {
  auto start_time = base::TimeTicks::Now();

  base::win::RegKey key;
  auto result = key.Open(HKEY_LOCAL_MACHINE, kCSAgentRegPath,
                         KEY_QUERY_VALUE | KEY_WOW64_64KEY);

  if (result == ERROR_SUCCESS && key.Valid()) {
    base::Value::Dict crowdstrike_info;

    auto customer_id = GetHexStringRegValue(key, kCSCURegKey);
    if (customer_id) {
      crowdstrike_info.Set(device_signals::names::kCustomerId,
                           customer_id.value());
    }

    auto agent_id = GetHexStringRegValue(key, kCSAGRegKey);
    if (agent_id) {
      crowdstrike_info.Set(device_signals::names::kAgentId, agent_id.value());
    }

    if (customer_id || agent_id) {
      signals.Set(device_signals::names::kCrowdStrike,
                  std::move(crowdstrike_info));
    }
  }

  LogSignalsCollectionLatency(kLatencyHistogramVariant, start_time);

  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
