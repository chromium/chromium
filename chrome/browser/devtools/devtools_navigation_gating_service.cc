// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_gating_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "chrome/browser/devtools/devtools_navigation_gating_rule_manager.h"
#include "chrome/browser/devtools/devtools_navigation_gating_service_factory.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/origin_gating_registration.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "url/gurl.h"

namespace {

enum class DevToolsCustomPredicate {
  kDevToolsNavigationGatingRuleset,
};

}  // namespace

template <>
const origin_gating::CustomPredicateDomain
    origin_gating::CustomPredicateDomain::kInstance<DevToolsCustomPredicate>{};

// static
DevToolsNavigationGatingService*
DevToolsNavigationGatingService::GetForBrowserContext(
    content::BrowserContext* context) {
  return DevToolsNavigationGatingServiceFactory::GetForBrowserContext(context);
}

// static
std::unique_ptr<DevToolsNavigationGatingService>
DevToolsNavigationGatingService::CreateForTesting(
    origin_gating::OriginGatingService& origin_gating_service,
    const DevToolsNavigationGatingRuleManager& rule_manager) {
  return base::WrapUnique(
      new DevToolsNavigationGatingService(origin_gating_service, rule_manager));
}

DevToolsNavigationGatingService::DevToolsNavigationGatingService(
    base::PassKey<DevToolsNavigationGatingServiceFactory>,
    origin_gating::OriginGatingService& origin_gating_service)
    : DevToolsNavigationGatingService(
          origin_gating_service,
          DevToolsNavigationGatingRuleManager::Get()) {}

DevToolsNavigationGatingService::DevToolsNavigationGatingService(
    origin_gating::OriginGatingService& origin_gating_service,
    const DevToolsNavigationGatingRuleManager& rule_manager) {
  origin_gating_registration_ = origin_gating_service.CreateAndRegisterChecker(
      weak_ptr_factory_.GetWeakPtr(),
      origin_gating::OriginGatingConfiguration(
          {
              {origin_gating::CustomPredicate(
                   base::IgnoreArgs<origin_gating::GatingDecisionContext*>(
                       base::BindRepeating(
                           &DevToolsNavigationGatingRuleManager::EvaluateRules,
                           // Safe because `rule_manager` is the global
                           // singleton in production and is required to
                           // outlive `this` in tests.
                           base::Unretained(&rule_manager))),
                   DevToolsCustomPredicate::kDevToolsNavigationGatingRuleset),
               origin_gating::GateableEventSet::All()},
          },
          /*use_site_keyed_cache=*/false));
}

DevToolsNavigationGatingService::~DevToolsNavigationGatingService() = default;

void DevToolsNavigationGatingService::IsNavigationAllowed(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  origin_gating::OriginGatingChecker* checker =
      origin_gating_registration_->service().GetChecker(
          origin_gating_registration_->id());
  CHECK(checker);
  checker->ComputeGatingDecision(
      /*context=*/nullptr,
      origin_gating::GateableEvent(origin_gating::NavigationRequestEvent{
          .source = GURL(), .destination = url}),
      base::BindOnce([](std::unique_ptr<origin_gating::GatingDecisionContext>
                            context,
                        origin_gating::GatingDecision decision) {
        return decision.is_allowed;
      }).Then(std::move(callback)));
}

void DevToolsNavigationGatingService::DoesOriginRequireUserConfirmation(
    origin_gating::GatingDecisionContext* context,
    const origin_gating::GateableEvent& event,
    DoesOriginRequireUserConfirmationCallback callback) const {
  NOTREACHED();
}

void DevToolsNavigationGatingService::EvaluateEnterprisePolicy(
    const GURL& destination,
    EvaluateEnterprisePolicyCallback callback) const {
  NOTREACHED();
}

void DevToolsNavigationGatingService::OnNoVerdict(
    origin_gating::GatingDecisionContext* context,
    const origin_gating::GateableEvent& event,
    bool requires_user_confirmation,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  NOTREACHED();
}
