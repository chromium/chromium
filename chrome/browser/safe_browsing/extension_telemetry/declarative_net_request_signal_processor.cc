// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal_processor.h"

#include "base/check_op.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// Used to limit the number of rules stored for each extension.
constexpr size_t kMaxRules = 50;

}  // namespace

DeclarativeNetRequestSignalProcessor::DeclarativeNetRequestStoreEntry::
    DeclarativeNetRequestStoreEntry() = default;
DeclarativeNetRequestSignalProcessor::DeclarativeNetRequestStoreEntry::
    ~DeclarativeNetRequestStoreEntry() = default;
DeclarativeNetRequestSignalProcessor::DeclarativeNetRequestStoreEntry::
    DeclarativeNetRequestStoreEntry(
        const DeclarativeNetRequestStoreEntry& src) = default;
DeclarativeNetRequestSignalProcessor::DeclarativeNetRequestSignalProcessor()
    : max_rules_(kMaxRules) {}
DeclarativeNetRequestSignalProcessor::~DeclarativeNetRequestSignalProcessor() =
    default;

void DeclarativeNetRequestSignalProcessor::ProcessSignal(
    const ExtensionSignal& signal) {
  CHECK_EQ(ExtensionSignalType::kDeclarativeNetRequest, signal.GetType());
  const auto& dnr_signal =
      static_cast<const DeclarativeNetRequestSignal&>(signal);

  for (auto& rule : dnr_signal.rules()) {
    if (rule.action.type != extensions::api::declarative_net_request::
                                RuleActionType::kRedirect &&
        rule.action.type != extensions::api::declarative_net_request::
                                RuleActionType::kModifyHeaders) {
      continue;
    }

    // Retrieve store entry for extension. If this is the first signal for an
    // extension, a new entry is created in the store.
    DeclarativeNetRequestStoreEntry& store_entry =
        dnr_store_[dnr_signal.extension_id()];
    base::flat_set<std::string>& store_entry_rules = store_entry.rules;

    // Convert and save rule to store if `max_rules_` limit is not reached.
    // Once limit is reached, only increment if rule is unique.
    std::string rule_string = rule.ToValue().DebugString();
    if (store_entry_rules.size() < max_rules_) {
      store_entry_rules.insert(std::move(rule_string));
    } else if (!store_entry_rules.contains(rule_string)) {
      store_entry.max_exceeded_rules_count++;
    }
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
DeclarativeNetRequestSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto dnr_store_it = dnr_store_.find(extension_id);
  if (dnr_store_it == dnr_store_.end()) {
    return nullptr;
  }

  // Create the signal info protobuf.
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  ExtensionTelemetryReportRequest_SignalInfo_DeclarativeNetRequestInfo*
      dnr_info = signal_info->mutable_declarative_net_request_info();

  dnr_info->mutable_rules()->Add(dnr_store_it->second.rules.begin(),
                                 dnr_store_it->second.rules.end());
  dnr_info->set_max_exceeded_rules_count(
      dnr_store_it->second.max_exceeded_rules_count);

  // Clear data in store.
  dnr_store_.erase(dnr_store_it);
  return signal_info;
}

bool DeclarativeNetRequestSignalProcessor::HasDataToReportForTest() const {
  return !dnr_store_.empty();
}

void DeclarativeNetRequestSignalProcessor::SetMaxRulesForTest(
    size_t max_rules) {
  max_rules_ = max_rules;
}

}  // namespace safe_browsing
