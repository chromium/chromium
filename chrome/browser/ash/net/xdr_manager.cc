// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/xdr_manager.h"

#include <memory>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "components/policy/policy_constants.h"

namespace ash {

XdrManager::XdrManager(policy::PolicyService* policy_service) {
  // Register callback for when DeviceReportXDREvents changes.
  policy_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      policy_service,
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  policy_registrar_->Observe(policy::key::kDeviceReportXDREvents,
                             base::BindRepeating(&XdrManager::OnXdrPolicyChange,
                                                 base::Unretained(this)));
  // Get and set initial XDR policy.
  auto* report_xdr_events_value =
      policy_service
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .GetValue(policy::key::kDeviceReportXDREvents,
                    base::Value::Type::BOOLEAN);
  report_xdr_events_enabled_ =
      report_xdr_events_value && report_xdr_events_value->GetBool();

  SetNetworkMetadataStoreXdrValue();
}

XdrManager::~XdrManager() = default;

bool XdrManager::AreXdrPoliciesEnabled() {
  return report_xdr_events_enabled_;
}

void XdrManager::OnXdrPolicyChange(const base::Value* previous,
                                   const base::Value* current) {
  report_xdr_events_enabled_ = current && current->GetBool();

  SetNetworkMetadataStoreXdrValue();
}

void XdrManager::SetNetworkMetadataStoreXdrValue() {
  NetworkHandler::Get()->network_metadata_store()->SetReportXdrEventsEnabled(
      report_xdr_events_enabled_);
}
}  // namespace ash
