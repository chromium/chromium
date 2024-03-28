// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_policy_fetcher_win_util.h"

#include <OleCtl.h>

#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"

namespace {

// Returns a string Value from `scoped_bstr`.
base::Value ValueFromScopedBStr(const base::win::ScopedBstr& scoped_bstr) {
  return base::Value(base::AsStringPiece16(
      std::wstring_view(scoped_bstr.Get(), scoped_bstr.Length())));
}

policy::PolicySource GetPolicySource(BSTR source_bstr) {
  constexpr std::wstring_view kCloudSource = L"Device Management";
  constexpr std::wstring_view kDefaultSource = L"Default";
  const auto source =
      std::wstring_view(source_bstr, ::SysStringLen(source_bstr));
  if (source == kCloudSource)
    return policy::POLICY_SOURCE_CLOUD;
  if (source == kDefaultSource)
    return policy::POLICY_SOURCE_ENTERPRISE_DEFAULT;
  DCHECK_EQ(source, std::wstring_view(L"Group Policy"));
  return policy::POLICY_SOURCE_PLATFORM;
}

}  // namespace

std::unique_ptr<policy::PolicyMap::Entry> ConvertPolicyStatusValueToPolicyEntry(
    IPolicyStatusValue* policy,
    const PolicyValueOverrideFunction& value_override_function) {
  DCHECK(policy);

  base::win::ScopedBstr value;
  if (FAILED(policy->get_value(value.Receive())) || value.Length() == 0) {
    return nullptr;
  }

  base::win::ScopedBstr source;
  if (FAILED(policy->get_source(source.Receive())))
    return nullptr;

  auto entry = std::make_unique<policy::PolicyMap::Entry>(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      GetPolicySource(source.Get()),
      value_override_function ? value_override_function.Run(value.Get())
                              : ValueFromScopedBStr(value),
      nullptr);
  VARIANT_BOOL has_conflict = VARIANT_FALSE;
  base::win::ScopedBstr conflict_value;
  base::win::ScopedBstr conflict_source;
  if (SUCCEEDED(policy->get_hasConflict(&has_conflict)) &&
      has_conflict == VARIANT_TRUE &&
      SUCCEEDED(policy->get_conflictValue(conflict_value.Receive())) &&
      SUCCEEDED(policy->get_conflictSource(conflict_source.Receive()))) {
    entry->AddConflictingPolicy(policy::PolicyMap::Entry(
        policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
        GetPolicySource(conflict_source.Get()),
        value_override_function
            ? value_override_function.Run(conflict_value.Get())
            : base::Value(base::AsStringPiece16(conflict_value.Get())),
        nullptr));
  }
  if (entry->source == policy::POLICY_SOURCE_ENTERPRISE_DEFAULT)
    entry->SetIsDefaultValue();
  return entry;
}
