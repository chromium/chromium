// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ANNOTATIONS_ANNOTATION_CONTROL_H_
#define CHROME_BROWSER_POLICY_ANNOTATIONS_ANNOTATION_CONTROL_H_

#include <map>
#include <string>

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

// AnnotationControl defines the policy values that should block a network
// annotation. Policies values here are considered all-or-nothing, meaning that
// ALL policies must match the specified values for `IsBlockedByPolicies()` to
// return true.
class AnnotationControl {
 public:
  AnnotationControl();
  AnnotationControl(const AnnotationControl&);
  ~AnnotationControl();

  // Add a policy with a corresponding value to the set of required policy
  // values to block the annotation.
  // Note: Currently only supports boolean value policies.
  AnnotationControl& Add(std::string policy_name, base::Value value);

  // Given current policy values, return whether the annotation should be
  // blocked. Note that a return value of false does NOT necessarily mean that
  // the annotation is enabled, since some functionality may be disabled by
  // default or disabled by user settings. This instead returns whether the
  // network annotation is EXPLICITLY disabled by policy values.
  bool IsBlockedByPolicies(const policy::PolicyMap& policies) const;

 private:
  std::map<std::string, bool> policy_values_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ANNOTATIONS_ANNOTATION_CONTROL_H_
