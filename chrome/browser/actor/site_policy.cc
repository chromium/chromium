// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/site_policy.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/actor_logging.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/safe_browsing/buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#endif

namespace actor {

namespace {

void ResolveDecision(DecisionCallback callback, bool decision) {
  // Some decisions are made asynchronously, so always invoke the callback
  // asynchronously for consistency.
  ACTOR_LOG() << __func__ << ": Decided to " << (decision ? "allow" : "block")
              << " for actions";
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), decision));
}

// Returns true if `url`'s host is in the `allowlist`. If `include_subdomains`
// is true, subdomains also match if the parent domain is in the list.
bool IsHostInAllowList(const std::vector<std::string_view>& allowlist,
                       const GURL& url,
                       bool include_subdomains) {
  if (!include_subdomains) {
    return base::Contains(allowlist, url.host_piece());
  }

  std::string host = url.host();
  while (!host.empty()) {
    if (base::Contains(allowlist, host)) {
      return true;
    }
    host = net::GetSuperdomain(host);
  }
  return false;
}

void OnOptimizationGuideDecision(
    DecisionCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  ACTOR_LOG() << __func__ << ": OptimizationGuideDecision is "
              << optimization_guide::GetStringForOptimizationGuideDecision(
                     decision);
  ResolveDecision(
      std::move(callback),
      decision == optimization_guide::OptimizationGuideDecision::kTrue);
}

void MayActOnUrl(const GURL& url, Profile* profile, DecisionCallback callback) {
  ACTOR_LOG() << __func__ << ": Considering for eligibility \"" << url.spec()
              << "\"";
  if (net::IsLocalhost(url) || url.IsAboutBlank()) {
    ResolveDecision(std::move(callback), true);
    return;
  }

  if (!url.SchemeIs(url::kHttpsScheme) || url.HostIsIPAddress()) {
    ACTOR_LOG() << __func__ << ": Wrong scheme";
    ResolveDecision(std::move(callback), false);
    return;
  }

  if (base::FeatureList::IsEnabled(kGlicActionAllowlist)) {
    const std::string allowlist_joined = kAllowlist.Get();
    const std::vector<std::string_view> allowlist =
        base::SplitStringPiece(allowlist_joined, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    if (IsHostInAllowList(allowlist, url, /*include_subdomains=*/true)) {
      ResolveDecision(std::move(callback), true);
      return;
    }

    const std::string allowlist_exact_joined = kAllowlistExact.Get();
    const std::vector<std::string_view> allowlist_exact =
        base::SplitStringPiece(allowlist_exact_joined, ",",
                               base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    if (IsHostInAllowList(allowlist_exact, url, /*include_subdomains=*/false)) {
      ResolveDecision(std::move(callback), true);
      return;
    }

    if (kAllowlistOnly.Get()) {
      if (allowlist.empty() && allowlist_exact.empty()) {
        ACTOR_LOG() << __func__ << ": Allowlist is empty";
        if (variations::VariationsService* variations_service =
                g_browser_process->variations_service()) {
          if (!variations_service->IsLikelyDogfoodClient()) {
            ACTOR_LOG() << __func__ << ": Non-dogfood client";
          }
          if (variations_service->GetClientFilterableStateForVersion()
                  ->GoogleGroups()
                  .empty()) {
            ACTOR_LOG() << __func__ << ": No Google groups";
          }
        }
      } else {
        ACTOR_LOG() << __func__ << ": URL not in allowlist";
      }
      ResolveDecision(std::move(callback), false);
      return;
    }
  }

  if (auto* optimization_guide_decider =
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
      optimization_guide_decider &&
      base::FeatureList::IsEnabled(kGlicActionUseOptimizationGuide)) {
    optimization_guide_decider->CanApplyOptimization(
        url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
        base::BindOnce(&OnOptimizationGuideDecision, std::move(callback)));
    return;
  }

  // Fail closed.
  ResolveDecision(std::move(callback), false);
}

}  // namespace

void InitActionBlocklist(Profile* profile) {
  if (auto* optimization_guide_decider =
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
      optimization_guide_decider &&
      base::FeatureList::IsEnabled(kGlicActionUseOptimizationGuide)) {
    optimization_guide_decider->RegisterOptimizationTypes(
        {optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK});
  }
}

// TODO(mcnee): Add UMA for the outcomes.
void MayActOnTab(const tabs::TabInterface& tab, DecisionCallback callback) {
  content::WebContents& web_contents = *tab.GetContents();

  if (web_contents.GetPrimaryMainFrame()->IsErrorDocument()) {
    ACTOR_LOG() << __func__ << ": Tab is an error document";
    ResolveDecision(std::move(callback), false);
    return;
  }

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing
  // warnings until user interaction. If the current page has a delayed warning,
  // it'll have a user interaction observer attached.
  // Do not act on such a page.
  if (safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
          &web_contents)) {
    ACTOR_LOG() << __func__ << ": Blocked by safebrowsing";
    ResolveDecision(std::move(callback), false);
    return;
  }
#endif

  const GURL& url = web_contents.GetPrimaryMainFrame()->GetLastCommittedURL();
  MayActOnUrl(url,
              Profile::FromBrowserContext(web_contents.GetBrowserContext()),
              std::move(callback));
}

}  // namespace actor
