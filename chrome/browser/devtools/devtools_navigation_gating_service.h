// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_SERVICE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/types.h"

class DevToolsNavigationGatingRuleManager;
class DevToolsNavigationGatingServiceFactory;
class GURL;

namespace content {
class BrowserContext;
}

namespace origin_gating {
class OriginGatingRegistration;
class OriginGatingService;
}  // namespace origin_gating

// Per-profile KeyedService that gates DevTools-controlled navigations. Each
// instance owns its own `OriginGatingChecker` registration with the profile's
// `OriginGatingService`, while the (profile-independent) rules are shared via
// the global `DevToolsNavigationGatingRuleManager`.
class DevToolsNavigationGatingService
    : public KeyedService,
      public origin_gating::OriginGatingChecker::Delegate {
 public:
  // Returns the instance for `context`, creating one if needed.
  static DevToolsNavigationGatingService* GetForBrowserContext(
      content::BrowserContext* context);

  // `rule_manager` must outlive the returned service.
  static std::unique_ptr<DevToolsNavigationGatingService> CreateForTesting(
      origin_gating::OriginGatingService& origin_gating_service,
      const DevToolsNavigationGatingRuleManager& rule_manager);

  DevToolsNavigationGatingService(
      base::PassKey<DevToolsNavigationGatingServiceFactory>,
      origin_gating::OriginGatingService& origin_gating_service);

  DevToolsNavigationGatingService(const DevToolsNavigationGatingService&) =
      delete;
  DevToolsNavigationGatingService& operator=(
      const DevToolsNavigationGatingService&) = delete;
  DevToolsNavigationGatingService(DevToolsNavigationGatingService&&) = delete;
  DevToolsNavigationGatingService& operator=(
      DevToolsNavigationGatingService&&) = delete;

  ~DevToolsNavigationGatingService() override;

  // Checks asynchronously whether navigation to `url` is allowed.
  void IsNavigationAllowed(const GURL& url,
                           base::OnceCallback<void(bool)> callback);

  // origin_gating::OriginGatingChecker::Delegate:
  void DoesOriginRequireUserConfirmation(
      origin_gating::GatingDecisionContext* context,
      const origin_gating::GateableEvent& event,
      DoesOriginRequireUserConfirmationCallback callback) const override;
  void EvaluateEnterprisePolicy(
      const GURL& destination,
      EvaluateEnterprisePolicyCallback callback) const override;
  void OnNoVerdict(origin_gating::GatingDecisionContext* context,
                   const origin_gating::GateableEvent& event,
                   bool requires_user_confirmation,
                   base::OnceCallback<void(NoVerdictResult)> callback) override;

 private:
  DevToolsNavigationGatingService(
      origin_gating::OriginGatingService& origin_gating_service,
      const DevToolsNavigationGatingRuleManager& rule_manager);

  std::unique_ptr<origin_gating::OriginGatingRegistration>
      origin_gating_registration_;
  base::WeakPtrFactory<DevToolsNavigationGatingService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_SERVICE_H_
