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
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/actor_logging.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
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
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#endif

namespace actor {

namespace {

class DecisionWrapper {
 public:
  DecisionWrapper(AggregatedJournal& journal,
                  const GURL& url,
                  TaskId task_id,
                  std::string_view event_name,
                  DecisionCallback callback)
      : callback_(std::move(callback)),
        journal_entry_(
            journal.CreatePendingAsyncEntry(url, task_id, event_name, "")) {}

  void Reject(std::string_view reason) {
    journal_entry_->EndEntry(reason);

    // Some decisions are made asynchronously, so always invoke the callback
    // asynchronously for consistency.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), /*decision=*/false));
  }

  void Accept() {
    journal_entry_->EndEntry("Allow");

    // Some decisions are made asynchronously, so always invoke the callback
    // asynchronously for consistency.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), /*decision=*/true));
  }

 private:
  DecisionCallback callback_;
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry_;
};

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
    std::unique_ptr<DecisionWrapper> decision_wrapper,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue) {
    decision_wrapper->Accept();
  } else {
    std::string result("OptimizationGuideDecision ");
    result +=
        optimization_guide::GetStringForOptimizationGuideDecision(decision);
    decision_wrapper->Reject(result);
  }
}

void MayActOnUrl(const GURL& url,
                 Profile* profile,
                 std::unique_ptr<DecisionWrapper> decision_wrapper) {
  if (net::IsLocalhost(url) || url.IsAboutBlank()) {
    decision_wrapper->Accept();
    return;
  }

  if (!url.SchemeIs(url::kHttpsScheme) || url.HostIsIPAddress()) {
    decision_wrapper->Reject("Wrong scheme");
    return;
  }

  bool is_safe_browsing_enabled = false;
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  is_safe_browsing_enabled =
      safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs());
#endif
  if (!is_safe_browsing_enabled) {
    // We don't want to risk acting on dangerous sites, so we require
    // SafeBrowsing.
    decision_wrapper->Reject("Safebrowsing unavailable");
    return;
  }

  if (base::FeatureList::IsEnabled(kGlicActionAllowlist)) {
    const std::string allowlist_joined = kAllowlist.Get();
    const std::vector<std::string_view> allowlist =
        base::SplitStringPiece(allowlist_joined, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    if (IsHostInAllowList(allowlist, url, /*include_subdomains=*/true)) {
      decision_wrapper->Accept();
      return;
    }

    const std::string allowlist_exact_joined = kAllowlistExact.Get();
    const std::vector<std::string_view> allowlist_exact =
        base::SplitStringPiece(allowlist_exact_joined, ",",
                               base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    if (IsHostInAllowList(allowlist_exact, url, /*include_subdomains=*/false)) {
      decision_wrapper->Accept();
      return;
    }

    if (kAllowlistOnly.Get()) {
      if (allowlist.empty() && allowlist_exact.empty()) {
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
        decision_wrapper->Reject("Allowlist is empty");
      } else {
        decision_wrapper->Reject("URL not in allowlist");
      }
      return;
    }
  }

  auto* lookalike_service = LookalikeUrlServiceFactory::GetForProfile(profile);
  LookalikeUrlService::LookalikeUrlCheckResult lookalike_result =
      lookalike_service->CheckUrlForLookalikes(
          url, lookalike_service->GetLatestEngagedSites(),
          /*stop_checking_on_allowlist_or_ignore=*/true);
  if (lookalike_result.action_type != lookalikes::LookalikeActionType::kNone &&
      lookalike_result.action_type !=
          lookalikes::LookalikeActionType::kRecordMetrics) {
    // Out of caution, do not act on lookalike domains.
    // For now, we just accept the possibility of false positives.
    // Note that this is partially redundant in the case where the lookalike
    // detection shows an interstitial, since we don't act on interstitials.
    // However, it may be that the navigation is allowed and a safety tip is
    // shown instead. We consider that sufficient cause for concern for actor
    // code.
    decision_wrapper->Reject("Lookalike domain");
    return;
  }

  if (auto* optimization_guide_decider =
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
      optimization_guide_decider &&
      base::FeatureList::IsEnabled(kGlicActionUseOptimizationGuide)) {
    optimization_guide_decider->CanApplyOptimization(
        url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
        base::BindOnce(&OnOptimizationGuideDecision,
                       std::move(decision_wrapper)));
    return;
  }

  // Fail closed.
  decision_wrapper->Reject("Fallback");
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
void MayActOnTab(const tabs::TabInterface& tab,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 DecisionCallback callback) {
  content::WebContents& web_contents = *tab.GetContents();

  const GURL& url = web_contents.GetPrimaryMainFrame()->GetLastCommittedURL();
  std::unique_ptr<DecisionWrapper> decision_wrapper =
      std::make_unique<DecisionWrapper>(journal, url, task_id, "MayActOnTab",
                                        std::move(callback));

  if (web_contents.GetPrimaryMainFrame()->IsErrorDocument()) {
    decision_wrapper->Reject("Tab is an error document");
    return;
  }

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing
  // warnings until user interaction. If the current page has a delayed warning,
  // it'll have a user interaction observer attached.
  // Do not act on such a page.
  if (safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
          &web_contents)) {
    decision_wrapper->Reject("Blocked by safebrowsing");
    return;
  }
#endif

  MayActOnUrl(url,
              Profile::FromBrowserContext(web_contents.GetBrowserContext()),
              std::move(decision_wrapper));
}

void MayActOnUrl(const GURL& url,
                 Profile* profile,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 DecisionCallback callback) {
  std::unique_ptr<DecisionWrapper> decision_wrapper =
      std::make_unique<DecisionWrapper>(journal, url, task_id, "MayActOnUrl",
                                        std::move(callback));
  MayActOnUrl(url, profile, std::move(decision_wrapper));
}

}  // namespace actor
