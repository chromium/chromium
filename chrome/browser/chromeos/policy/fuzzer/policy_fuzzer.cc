// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"
#include "chrome/browser/chromeos/policy/fuzzer/policy_fuzzer.pb.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace policy {

DEFINE_PROTO_FUZZER(const PolicyFuzzerProto& proto) {
  if (!proto.has_chrome_device_settings())
    return;

  const enterprise_management::ChromeDeviceSettingsProto&
      chrome_device_settings = proto.chrome_device_settings();

  base::WeakPtr<ExternalDataManager> data_manager;
  PolicyMap policy_map;
  DecodeDevicePolicy(chrome_device_settings, data_manager, &policy_map);

  for (const auto& it : policy_map) {
    const std::string& policy_name = it.first;
    const PolicyMap::Entry& entry = it.second;
    CHECK(entry.value) << "Policy " << policy_name << " has an empty value";
    CHECK_EQ(entry.scope, POLICY_SCOPE_MACHINE)
        << "Policy " << policy_name << " has not machine scope";
  }
}

}  // namespace policy
