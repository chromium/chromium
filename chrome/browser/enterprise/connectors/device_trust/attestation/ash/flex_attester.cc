// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/flex_attester.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"

namespace enterprise_connectors {

FlexAttester::FlexAttester(Profile* profile) : profile_(profile) {
  CHECK(profile_);
}
FlexAttester::~FlexAttester() = default;

void FlexAttester::DecorateKeyInfo(const std::set<DTCPolicyLevel>& levels,
                                   KeyInfo& key_info,
                                   base::OnceClosure done_closure) {
  key_info.set_device_id(ash::InstallAttributes::Get()->GetDeviceId());
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    // Add signals that are available on enrolled devices (ENTERPRISE_MACHINE
    // flow)
    key_info.set_domain(ash::InstallAttributes::Get()->GetDomain());
    key_info.set_customer_id(g_browser_process->platform_part()
                                 ->browser_policy_connector_ash()
                                 ->GetObfuscatedCustomerID());
  } else {
    // Add signals that are available on un-enrolled devices
    // (DEVICE_TRUST_CONNECTOR flow)
    auto* user = ash::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user) {
      key_info.set_domain(user->GetAccountId().GetUserEmail());
    }
  }
  std::move(done_closure).Run();
}

void FlexAttester::SignResponse(const std::set<DTCPolicyLevel>& levels,
                                const std::string& challenge_response,
                                SignedData& signed_data,
                                base::OnceClosure done_closure) {
  // No signature for Flex devices.
  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
