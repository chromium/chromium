// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_overrides_policy_handler.h"

#include <stdlib.h>
#include <iostream>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace first_party_sets {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* test_case = new IcuEnvironment();

DEFINE_PROTO_FUZZER(const json_proto::JsonValue& json_value) {
  json_proto::JsonProtoConverter converter;
  std::string native_input = converter.Convert(json_value);
  FirstPartySetsOverridesPolicyHandler fps_handler(
      policy::key::kFirstPartySetsOverrides, policy::GetChromeSchema());
  FirstPartySetsOverridesPolicyHandler rws_handler(
      policy::key::kRelatedWebsiteSetsOverrides, policy::GetChromeSchema());

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kFirstPartySetsOverrides,
                 policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                 policy::PolicyScope::POLICY_SCOPE_MACHINE,
                 policy::PolicySource::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 base::JSONReader::Read(native_input),
                 /*external_data_fetcher=*/nullptr);
  policy_map.Set(policy::key::kRelatedWebsiteSetsOverrides,
                 policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                 policy::PolicyScope::POLICY_SCOPE_MACHINE,
                 policy::PolicySource::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 base::JSONReader::Read(native_input),
                 /*external_data_fetcher=*/nullptr);
  policy::PolicyErrorMap errors;
  fps_handler.CheckPolicySettings(policy_map, &errors);
  rws_handler.CheckPolicySettings(policy_map, &errors);
}

}  // namespace first_party_sets
