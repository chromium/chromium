// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/site_policy.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace actor {

namespace {

class DecisionWrapper {
 public:
  DecisionWrapper(AggregatedJournal& journal,
                  const GURL& url,
                  TaskId task_id,
                  std::string_view event_name,
                  DecisionCallbackWithReason callback)
      : callback_(std::move(callback)),
        journal_entry_(
            journal.CreatePendingAsyncEntry(url,
                                            task_id,
                                            MakeBrowserTrackUUID(task_id),
                                            event_name,
                                            {})) {}

  void Reject(std::string_view reason, MayActOnUrlBlockReason block_reason) {
    journal_entry_->EndEntry(JournalDetailsBuilder().AddError(reason).Build());

    // Some decisions are made asynchronously, so always invoke the callback
    // asynchronously for consistency.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), block_reason));
  }

  void Accept() {
    journal_entry_->EndEntry(
        JournalDetailsBuilder().Add("result", "Allow").Build());

    // Some decisions are made asynchronously, so always invoke the callback
    // asynchronously for consistency.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), MayActOnUrlBlockReason::kAllowed));
  }

 private:
  DecisionCallbackWithReason callback_;
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry_;
};

// Whether to continue with the action or navigation based on the optimization
// guide decision. Since we want to not block navigation in case the service has
// an issue, we only stop the actor when the decision is kFalse, explicitly
// indicating optimization guide suggests we block the URL.
bool ShouldContinueFromOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision) {
  return decision != optimization_guide::OptimizationGuideDecision::kFalse;
}

void MayActOnUrlInternal(const GURL& url,
                         NoVerdictContinuation resolve_no_verdict,
                         std::unique_ptr<DecisionWrapper> decision_wrapper) {
  CHECK(resolve_no_verdict);
  std::move(resolve_no_verdict)
      .Run(url,
           base::BindOnce(
               [](std::unique_ptr<DecisionWrapper> wrapper,
                  base::expected<void, MayActOnUrlBlockResult> block_result) {
                 if (block_result.has_value()) {
                   wrapper->Accept();
                 } else {
                   wrapper->Reject(block_result.error().reason,
                                   block_result.error().reason_code);
                 }
               },
               std::move(decision_wrapper)));
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
                 NoVerdictContinuation resolve_no_verdict,
                 DecisionCallbackWithReason callback) {
  content::WebContents& web_contents = *tab.GetContents();

  const GURL& url = web_contents.GetPrimaryMainFrame()->GetLastCommittedURL();
  std::unique_ptr<DecisionWrapper> decision_wrapper =
      std::make_unique<DecisionWrapper>(journal, url, task_id, "MayActOnTab",
                                        std::move(callback));

  MayActOnUrlInternal(url, std::move(resolve_no_verdict),
                      std::move(decision_wrapper));
}

void MayActOnUrl(const GURL& url,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 NoVerdictContinuation resolve_no_verdict,
                 DecisionCallbackWithReason callback) {
  std::unique_ptr<DecisionWrapper> decision_wrapper =
      std::make_unique<DecisionWrapper>(journal, url, task_id, "MayActOnUrl",
                                        std::move(callback));
  MayActOnUrlInternal(url, std::move(resolve_no_verdict),
                      std::move(decision_wrapper));
}

base::expected<void, DecisionCallback>
MaybeCheckOptimizationGuideForSensitiveUrl(const GURL& url,
                                           Profile* profile,
                                           DecisionCallback callback) {
  // Check that the optimization guide component has loaded. It could be
  // missing, for example, if the user has very recently installed chrome and
  // the component updater has not yet run. We don't want to reject every URL,
  // so we check for this and fail open.
  if (!base::FeatureList::IsEnabled(kGlicActionUseOptimizationGuide) ||
      !optimization_guide::OptimizationHintsComponentUpdateListener::
           GetInstance()
               ->hints_component_info()
               .has_value()) {
    return base::unexpected(std::move(callback));
  }

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_decider) {
    return base::unexpected(std::move(callback));
  }

  optimization_guide_decider->CanApplyOptimization(
      url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
      base::BindOnce([](optimization_guide::OptimizationGuideDecision decision,
                        const optimization_guide::OptimizationMetadata&) {
        return ShouldContinueFromOptimizationGuideDecision(decision);
      }).Then(std::move(callback)));
  return base::ok();
}

mojom::ActionResultCode BlockReasonToResultCode(MayActOnUrlBlockReason reason,
                                                bool for_navigation) {
  using mojom::ActionResultCode;

  const ActionResultCode generic_block_code =
      for_navigation ? ActionResultCode::kTriggeredNavigationBlocked
                     : ActionResultCode::kUrlBlocked;

  auto maybe_granular = [generic_block_code](
                            mojom::ActionResultCode specific_code) {
    return base::FeatureList::IsEnabled(kGlicGranularBlockingActionResultCodes)
               ? specific_code
               : generic_block_code;
  };

  switch (reason) {
    case MayActOnUrlBlockReason::kAllowed:
      return ActionResultCode::kOk;
    case MayActOnUrlBlockReason::kExternalProtocol: {
      if (base::FeatureList::IsEnabled(kGlicExternalProtocolActionResultCode)) {
        return ActionResultCode::kExternalProtocolNavigationBlocked;
      }
      return generic_block_code;
    }
    case MayActOnUrlBlockReason::kLookalikeDomain:
    case MayActOnUrlBlockReason::kBlockedByStaticList:
      return maybe_granular(ActionResultCode::kActionsBlockedForSiteRisk);
    case MayActOnUrlBlockReason::kSafeBrowsing:
      return maybe_granular(
          ActionResultCode::kActionsBlockedSafeBrowsingDisabled);
    case MayActOnUrlBlockReason::kEnterprisePolicy:
      return maybe_granular(
          ActionResultCode::kActionsBlockedByEnterprisePolicy);
    case MayActOnUrlBlockReason::kWrongScheme:
      return maybe_granular(ActionResultCode::kActionsBlockedForScheme);
    case MayActOnUrlBlockReason::kTabIsErrorDocument:
      return maybe_granular(ActionResultCode::kActionsBlockedOnErrorPage);
    case MayActOnUrlBlockReason::kIpAddress:
    case MayActOnUrlBlockReason::kOptimizationGuideBlock:
    case MayActOnUrlBlockReason::kUrlNotInAllowlist:
    case MayActOnUrlBlockReason::kBlockedByContainerConfig:
      return generic_block_code;
  }
}

}  // namespace actor
