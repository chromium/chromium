// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/dom_access_signal_processor.h"

#include "base/check.h"
#include "chrome/browser/safe_browsing/extension_telemetry/dom_access_signal.h"

namespace safe_browsing {

namespace {
// Max number of unique signals stored per extension.
constexpr size_t kMaxAggregatedSignals = 100;
}  // namespace

DOMAccessSignalProcessor::DOMAccessData::DOMAccessData() = default;
DOMAccessSignalProcessor::DOMAccessData::~DOMAccessData() = default;
DOMAccessSignalProcessor::DOMAccessData::DOMAccessData(const DOMAccessData&) =
    default;

DOMAccessSignalProcessor::DOMAccessStoreEntry::DOMAccessStoreEntry() = default;
DOMAccessSignalProcessor::DOMAccessStoreEntry::~DOMAccessStoreEntry() = default;
DOMAccessSignalProcessor::DOMAccessStoreEntry::DOMAccessStoreEntry(
    const DOMAccessStoreEntry&) = default;

DOMAccessSignalProcessor::DOMAccessSignalProcessor()
    : max_aggregated_signals_(kMaxAggregatedSignals) {}
DOMAccessSignalProcessor::~DOMAccessSignalProcessor() = default;

void DOMAccessSignalProcessor::ProcessSignal(const ExtensionSignal& signal) {
  CHECK_EQ(signal.GetType(), ExtensionSignalType::kDOMAccess);
  const auto& dom_access_signal = static_cast<const DOMAccessSignal&>(signal);

  DOMAccessStoreEntry& store_entry =
      dom_access_store_[dom_access_signal.extension_id()];
  DOMAccessDataMap& data_map = store_entry.dom_access_data_map;

  std::string aggregation_key = dom_access_signal.GetAggregationKey();
  auto it = data_map.find(aggregation_key);
  if (it != data_map.end()) {
    it->second.count++;
    if (dom_access_signal.timestamp() > it->second.last_timestamp) {
      it->second.last_timestamp = dom_access_signal.timestamp();
    }
  } else if (data_map.size() < max_aggregated_signals_) {
    DOMAccessData data;
    data.api_name = dom_access_signal.api_name();
    data.url = dom_access_signal.url();
    data.access_type = dom_access_signal.access_type();
    data.last_timestamp = dom_access_signal.timestamp();
    data.count = 1;
    data_map.emplace(std::move(aggregation_key), std::move(data));
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
DOMAccessSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto it = dom_access_store_.find(extension_id);
  if (it == dom_access_store_.end()) {
    return nullptr;
  }

  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  auto* dom_access_info = signal_info->mutable_dom_access_info();

  for (auto& [key, data] : it->second.dom_access_data_map) {
    auto* dom_access = dom_access_info->add_dom_accesses();
    dom_access->set_api_name(std::move(data.api_name));
    dom_access->set_url(std::move(data.url));
    dom_access->set_access_type(data.access_type);
    dom_access->set_timestamp_ms(
        data.last_timestamp.InMillisecondsSinceUnixEpoch());
    dom_access->set_count(data.count);
  }

  dom_access_store_.erase(it);
  return signal_info;
}

bool DOMAccessSignalProcessor::HasDataToReportForTest() const {
  return !dom_access_store_.empty();
}

void DOMAccessSignalProcessor::SetMaxAggregatedSignalsForTest(
    size_t max_aggregated_signals) {
  max_aggregated_signals_ = max_aggregated_signals;
}

}  // namespace safe_browsing
