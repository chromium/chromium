// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_RULE_MANAGER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_RULE_MANAGER_H_

#include <memory>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/types.h"

class GURL;

// Manages configuration rules for DevTools navigation gating.
// The configuration is loaded globally from command-line switches as a
// singleton.
class DevToolsNavigationGatingRuleManager
    : public origin_gating::OriginGatingChecker::Delegate {
 public:
  // Returns the global singleton instance.
  static DevToolsNavigationGatingRuleManager& Get();

  // Creates an instance for testing purposes with a custom JSON configuration.
  static DevToolsNavigationGatingRuleManager CreateForTesting(
      std::string_view rules_json);

  DevToolsNavigationGatingRuleManager(
      const DevToolsNavigationGatingRuleManager&) = delete;
  DevToolsNavigationGatingRuleManager& operator=(
      const DevToolsNavigationGatingRuleManager&) = delete;
  DevToolsNavigationGatingRuleManager(DevToolsNavigationGatingRuleManager&&) =
      delete;
  DevToolsNavigationGatingRuleManager& operator=(
      DevToolsNavigationGatingRuleManager&&) = delete;

  ~DevToolsNavigationGatingRuleManager() override;

  // Checks asynchronously whether navigation to `url` is allowed.
  void IsNavigationAllowed(const GURL& url,
                           base::OnceCallback<void(bool)> callback);

  // Returns true if this manager has any policies that might block a
  // navigation.
  bool MayBlockNavigation() const;

  // origin_gating::OriginGatingChecker::Delegate implementation:
  void DoesOriginRequireUserConfirmation(
      origin_gating::GatingDecisionContext* context,
      origin_gating::GateableEvent event,
      const GURL& source,
      const GURL& destination,
      DoesOriginRequireUserConfirmationCallback callback) const override;
  void EvaluateEnterprisePolicy(
      const GURL& destination,
      EvaluateEnterprisePolicyCallback callback) const override;
  void OnNoVerdict(origin_gating::GatingDecisionContext* context,
                   origin_gating::GateableEvent event,
                   const GURL& source,
                   const GURL& destination,
                   bool requires_user_confirmation,
                   base::OnceCallback<void(NoVerdictResult)> callback) override;

 private:
  friend class base::NoDestructor<DevToolsNavigationGatingRuleManager>;

  explicit DevToolsNavigationGatingRuleManager(std::string_view rules_json);

  origin_gating::Decision EvaluateRules(
      origin_gating::GatingDecisionContext* context,
      const GURL& source,
      const GURL& destination) const;

  content_settings::HostIndexedContentSettings rules_;
  bool has_allowlist_ = false;
  origin_gating::OriginGatingChecker origin_gating_checker_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_RULE_MANAGER_H_
