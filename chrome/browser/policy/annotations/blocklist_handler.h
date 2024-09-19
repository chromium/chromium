// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ANNOTATIONS_BLOCKLIST_HANDLER_H_
#define CHROME_BROWSER_POLICY_ANNOTATIONS_BLOCKLIST_HANDLER_H_

#include "chrome/browser/policy/annotations/annotation_control.h"
#include "chrome/browser/policy/annotations/annotation_control_provider.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

// Policy handler that determines which network annotations should be disabled
// based on current policy values. When all policies for a specific network
// annotation are set to disabled, then the annotation id (hash code) is added
// to the `kNetworkAnnotationBlocklist` pref.
//
// Note: The `kNetworkAnnotationBlocklist` pref is currently only used for
// monitoring/reporting purposes to determine potential incorrect policy to
// annotation mappings. No network requests are blocked at this time.
class NetworkAnnotationBlocklistHandler : public ConfigurationPolicyHandler {
 public:
  NetworkAnnotationBlocklistHandler();
  NetworkAnnotationBlocklistHandler(const NetworkAnnotationBlocklistHandler&) =
      delete;
  NetworkAnnotationBlocklistHandler& operator=(
      const NetworkAnnotationBlocklistHandler&) = delete;
  ~NetworkAnnotationBlocklistHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  AnnotationControlProvider annotation_control_provider_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ANNOTATIONS_BLOCKLIST_HANDLER_H_
