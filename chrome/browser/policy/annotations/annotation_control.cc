// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/annotations/annotation_control.h"

namespace policy {

AnnotationControl::AnnotationControl() = default;
AnnotationControl::AnnotationControl(const AnnotationControl&) = default;
AnnotationControl::~AnnotationControl() = default;

AnnotationControl& AnnotationControl::Add(std::string policy_name,
                                          base::Value value) {
  // Only boolean policies supported currently.
  CHECK(value.is_bool());

  policy_values_[policy_name] = value.GetBool();
  return *this;
}

bool AnnotationControl::IsBlockedByPolicies(
    const policy::PolicyMap& policies) const {
  if (policy_values_.size() == 0) {
    return false;
  }

  for (auto const& [policy_name, policy_value] : policy_values_) {
    const base::Value* value =
        policies.GetValue(policy_name, base::Value::Type::BOOLEAN);
    if (value == nullptr || value->GetBool() != policy_value) {
      return false;
    }
  }
  return true;
}

}  // namespace policy
