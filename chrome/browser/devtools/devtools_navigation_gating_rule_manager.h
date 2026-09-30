// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_RULE_MANAGER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_RULE_MANAGER_H_

#include <memory>
#include <string_view>

#include "base/no_destructor.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/origin_gating/core/types.h"

// Manages configuration rules for DevTools navigation gating.
// The configuration is loaded globally from command-line switches as a
// singleton.
class DevToolsNavigationGatingRuleManager {
 public:
  // Returns the global singleton instance.
  static const DevToolsNavigationGatingRuleManager& Get();

  // Creates an instance for testing purposes with a custom JSON configuration.
  static std::unique_ptr<DevToolsNavigationGatingRuleManager> CreateForTesting(
      std::string_view rules_json);

  DevToolsNavigationGatingRuleManager(
      const DevToolsNavigationGatingRuleManager&) = delete;
  DevToolsNavigationGatingRuleManager& operator=(
      const DevToolsNavigationGatingRuleManager&) = delete;
  DevToolsNavigationGatingRuleManager(DevToolsNavigationGatingRuleManager&&) =
      delete;
  DevToolsNavigationGatingRuleManager& operator=(
      DevToolsNavigationGatingRuleManager&&) = delete;

  ~DevToolsNavigationGatingRuleManager();

  // Evaluates `event`'s destination against the configured rules.
  origin_gating::Decision EvaluateRules(
      const origin_gating::GateableEvent& event) const;

  // Returns true if this manager has any policies that might block a
  // navigation.
  bool MayBlockNavigation() const;

 private:
  friend class base::NoDestructor<DevToolsNavigationGatingRuleManager>;

  explicit DevToolsNavigationGatingRuleManager(std::string_view rules_json);

  content_settings::HostIndexedContentSettings rules_;
  bool has_allowlist_ = false;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_RULE_MANAGER_H_
