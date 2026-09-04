// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/types/id_type.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/action_tracker_for_metrics.h"
#include "chrome/browser/actor/actor_critical_action_logger.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/attempt_login_tool.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_impl.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_metrics.h"
#include "components/actor/core/actor_util.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/safety_list_manager.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/autofill/core/browser/actor/actor_form_filling_service_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/types.h"
#include "components/password_manager/core/browser/actor_login/actor_login_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_service_impl.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/safe_browsing/buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/event_dispatcher.h"
#include "url/origin.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#endif

using content::RenderFrameHost;
using content::WebContents;
using optimization_guide::DocumentIdentifierUserData;
using optimization_guide::proto::Action;
using optimization_guide::proto::Actions;
using optimization_guide::proto::ActionTarget;
using optimization_guide::proto::AnnotatedPageContent;
using origin_gating::CustomPredicate;
using origin_gating::DecisionSource;
using origin_gating::GateableEvent;
using origin_gating::GateableEventSet;
using tabs::TabInterface;

namespace actor {

// Individual custom predicates that the actor framework supports in addition to
// those provided by the origin_gating framework.
enum class ActorCustomPredicate {
  kSafetyList,
  kSensitiveUrl,
  kLookalikeUrl,
  kSafeBrowsing,
  kSafetyChecksDisabled,
  kTabErrorDocument,
  kTabSafeBrowsingObserver,
  kDangerousMimeType,
};

}  // namespace actor

template <>
const origin_gating::CustomPredicateDomain origin_gating::
    CustomPredicateDomain::kInstance<actor::ActorCustomPredicate>{};

namespace actor {
namespace {

constexpr std::string_view ActorCustomPredicateToString(
    ActorCustomPredicate predicate) {
  switch (predicate) {
    case ActorCustomPredicate::kSafetyList:
      return "actor_safety_list_check";
    case ActorCustomPredicate::kSensitiveUrl:
      return "actor_sensitive_url_check";
    case ActorCustomPredicate::kLookalikeUrl:
      return "actor_lookalike_url_check";
    case ActorCustomPredicate::kSafeBrowsing:
      return "actor_safe_browsing_enabled_check";
    case ActorCustomPredicate::kSafetyChecksDisabled:
      return "actor_safety_checks_disabled";
    case ActorCustomPredicate::kTabErrorDocument:
      return "actor_tab_error_document_check";
    case ActorCustomPredicate::kTabSafeBrowsingObserver:
      return "actor_tab_safe_browsing_observer_check";
    case ActorCustomPredicate::kDangerousMimeType:
      return "actor_dangerous_mime_type_check";
  }
  NOTREACHED();
}

std::string DecisionAttributionToString(
    const origin_gating::DecisionAttribution& decision_attribution) {
  switch (decision_attribution.type()) {
    case origin_gating::DecisionAttribution::Type::kDecisionSource:
      return origin_gating::DecisionSourceToString(
          decision_attribution.Source());
    case origin_gating::DecisionAttribution::Type::kCustomPredicate:
      return std::string(ActorCustomPredicateToString(
          decision_attribution.CustomPredicateId<ActorCustomPredicate>()));
  }
  NOTREACHED();
}

constexpr GateableEventSet kRequestsAndPageActions = {
    GateableEvent::kNavigationRequest, GateableEvent::kPageAction};

// Splits a navigation gating callback, storing one split in
// `pending_cancellations` (to invoke with `block_reason_if_dropped` when
// pending navigations are cancelled) and another in a ScopedClosureRunner (to
// invoke with `block_reason_if_dropped` if the callback is dropped before
// execution).  Returns a wrapped callback that disarms both upon normal
// invocation.
ExecutionEngine::NavigationDecisionCallback TrackPendingNavigation(
    base::OnceCallbackList<void()>& pending_cancellations,
    ExecutionEngine::NavigationDecisionCallback callback,
    MayActOnUrlBlockReason block_reason_if_dropped) {
  auto [cancel_1, temp] = base::SplitOnceCallback(std::move(callback));
  auto [cancel_2, wrapped] = base::SplitOnceCallback(std::move(temp));

  auto runner =
      base::MakeRefCounted<base::RefCountedData<base::ScopedClosureRunner>>(
          base::ScopedClosureRunner(
              base::BindOnce(std::move(cancel_2), block_reason_if_dropped)));

  base::CallbackListSubscription subscription =
      pending_cancellations.Add(base::BindOnce(
          [](scoped_refptr<base::RefCountedData<base::ScopedClosureRunner>>
                 runner,
             ExecutionEngine::NavigationDecisionCallback cancel_cb,
             MayActOnUrlBlockReason arg) {
            runner->data.ReplaceClosure(base::DoNothing());
            std::move(cancel_cb).Run(arg);
          },
          runner, std::move(cancel_1), block_reason_if_dropped));

  return base::BindOnce(
             [](scoped_refptr<base::RefCountedData<base::ScopedClosureRunner>>
                    runner,
                base::CallbackListSubscription sub,
                MayActOnUrlBlockReason arg) {
               runner->data.ReplaceClosure(base::DoNothing());
               return arg;
             },
             std::move(runner), std::move(subscription))
      .Then(std::move(wrapped));
}

struct OriginGatingDecisionContext
    : public origin_gating::GatingDecisionContext {
  // Whether the destination origin is considered sensitive. Nullopt until
  // optimization guide has been queried.
  std::optional<bool> destination_is_sensitive;
};

struct NavigationResponseContext : public OriginGatingDecisionContext {
  NavigationResponseContext(ukm::SourceId ukm_id,
                            bool skip,
                            base::ScopedUmaHistogramTimer gating_timer,
                            std::optional<std::string> response_mime_type)
      : ukm_source_id(ukm_id),
        skip_prompt(skip),
        timer(std::move(gating_timer)),
        response_mime_type(std::move(response_mime_type)) {}
  ~NavigationResponseContext() override = default;

  ukm::SourceId ukm_source_id;
  bool skip_prompt;
  base::ScopedUmaHistogramTimer timer;
  std::optional<std::string> response_mime_type;
};

// Context for page-action gating. Carries the tab's WebContents so that the
// tab-specific predicates (error document, SafeBrowsing observer) can inspect
// it.
struct PageActionGatingContext : public OriginGatingDecisionContext {
  explicit PageActionGatingContext(
      base::WeakPtr<content::WebContents> web_contents)
      : web_contents(std::move(web_contents)) {}
  ~PageActionGatingContext() override = default;

  base::WeakPtr<content::WebContents> web_contents;
};

// Blocks acting on a tab whose primary main frame is showing an error document.
origin_gating::Decision BlockTabErrorDocument(
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination) {
  content::WebContents* web_contents =
      static_cast<const PageActionGatingContext*>(context)->web_contents.get();
  if (web_contents && web_contents->GetPrimaryMainFrame()->IsErrorDocument()) {
    return origin_gating::Decision::kBlocked;
  }
  return origin_gating::Decision::kNoDecision;
}

// Blocks acting on a tab that has a pending SafeBrowsing delayed warning. The
// SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing warnings
// until user interaction; such a page has a user interaction observer attached.
origin_gating::Decision BlockSafeBrowsingWarningIfSafetyChecksEnabled(
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  content::WebContents* web_contents =
      static_cast<const PageActionGatingContext*>(context)->web_contents.get();
  if (web_contents &&
      safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
          web_contents) &&
      !IsActorSafetyCheckDisabled()) {
    return origin_gating::Decision::kBlocked;
  }
#endif
  return origin_gating::Decision::kNoDecision;
}

bool IsDangerousMimeType(std::string_view mime_type) {
  static constexpr auto kBlockedTabularTypes =
      base::MakeFixedFlatSet<std::string_view>({
          "text/csv",
          "text/comma-separated-values",
          "text/tsv",
          "text/tab-separated-values",
      });
  return kBlockedTabularTypes.contains(mime_type) ||
         blink::IsJSONMimeType(mime_type) || blink::IsXMLMimeType(mime_type) ||
         blink::IsSupportedJavascriptMimeType(mime_type);
}

// Blocks navigation responses that have dangerous MIME types (e.g. JSON, XML,
// JavaScript, CSV).
origin_gating::Decision BlockDangerousMimeType(
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination) {
  if (!base::FeatureList::IsEnabled(
          kGlicBlockNavigationToDangerousContentTypes) ||
      !context) {
    return origin_gating::Decision::kNoDecision;
  }
  const auto* response_context =
      static_cast<const NavigationResponseContext*>(context);
  return response_context->response_mime_type.transform(&IsDangerousMimeType)
                 .value_or(false)
             ? origin_gating::Decision::kBlocked
             : origin_gating::Decision::kNoDecision;
}

// Extracts the MIME type from a navigation response payload.
std::optional<std::string> ExtractMimeType(
    content::NavigationHandle& navigation_handle) {
  const net::HttpResponseHeaders* response_headers =
      navigation_handle.GetResponseHeaders();
  if (!response_headers) {
    return std::nullopt;
  }
  std::string mime_type;
  return response_headers->GetMimeType(&mime_type)
             ? std::make_optional(mime_type)
             : std::nullopt;
}

CustomPredicate CreateSafetyListPredicate() {
  return CustomPredicate(
      base::BindRepeating([](origin_gating::GatingDecisionContext*,
                             const GURL& source_url,
                             const GURL& destination_url) {
        const GURL& effective_source =
            source_url.is_empty() ? destination_url : source_url;
        switch (SafetyListManager::GetInstance()->Find(effective_source,
                                                       destination_url)) {
          case SafetyListManager::Decision::kNone:
            return origin_gating::Decision::kNoDecision;
          case SafetyListManager::Decision::kAllow:
            return origin_gating::Decision::kAllowed;
          case SafetyListManager::Decision::kBlock:
            return origin_gating::Decision::kBlocked;
        }
      }),
      ActorCustomPredicate::kSafetyList);
}

// Returns whether the given `url` is considered non-sensitive. Caches the
// result in `context`, for future queries.
void IsNonSensitiveUrl(Profile* profile,
                       origin_gating::GatingDecisionContext* context,
                       const GURL& url,
                       base::OnceCallback<void(bool)> callback) {
  CHECK_NE(context, nullptr);
  auto* decision_context = static_cast<OriginGatingDecisionContext*>(context);

  if (base::FeatureList::IsEnabled(kGlicActorLocalhostIsSensitive) &&
      net::IsLocalhost(url)) {
    decision_context->destination_is_sensitive = true;
    std::move(callback).Run(/*not_sensitive=*/false);
    return;
  }

  if (decision_context->destination_is_sensitive.has_value()) {
    std::move(callback).Run(
        !decision_context->destination_is_sensitive.value());
    return;
  }

  base::expected<void, base::OnceCallback<void(bool)>> sensitive_check_result =
      MaybeCheckOptimizationGuideForSensitiveUrl(
          url, profile,
          base::BindOnce(
              [](OriginGatingDecisionContext* decision_context,
                 bool not_sensitive) {
                decision_context->destination_is_sensitive = !not_sensitive;
                return not_sensitive;
              },
              // Passing `decision_context` as a raw pointer is safe here
              // because the pointee is owned by `callback`, and won't be freed
              // until after `callback` executes.
              decision_context)
              .Then(std::move(callback)));
  if (!sensitive_check_result.has_value()) {
    // Optimization guide is unavailable; assume the URL is non-sensitive.
    std::move(sensitive_check_result).error().Run(/*not_sensitive=*/true);
  }
}

void BlockSensitiveUrl(
    Profile* profile,
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination,
    base::OnceCallback<void(origin_gating::Decision)> callback) {
  IsNonSensitiveUrl(profile, context, destination,
                    base::BindOnce([](bool not_sensitive) {
                      return not_sensitive
                                 ? origin_gating::Decision::kNoDecision
                                 : origin_gating::Decision::kBlocked;
                    }).Then(std::move(callback)));
}

void BlockSensitiveUrlWhenNavigationGatingDisabled(
    Profile* profile,
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination,
    base::OnceCallback<void(origin_gating::Decision)> callback) {
  if (IsNavigationGatingEnabled()) {
    std::move(callback).Run(origin_gating::Decision::kNoDecision);
    return;
  }

  BlockSensitiveUrl(profile, context, source, destination, std::move(callback));
}

origin_gating::Decision BlockLookalikeUrl(
    Profile* profile,
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination) {
  auto* lookalike_service = LookalikeUrlServiceFactory::GetForProfile(profile);
  LookalikeUrlService::LookalikeUrlCheckResult lookalike_result =
      lookalike_service->CheckUrlForLookalikes(
          destination, lookalike_service->GetLatestEngagedSites(),
          /*stop_checking_on_allowlist_or_ignore=*/true);
  // Out of caution, do not act on lookalike domains.
  // For now, we just accept the possibility of false positives.
  // Note that this is partially redundant in the case where the lookalike
  // detection shows an interstitial, since we don't act on interstitials.
  // However, it may be that the navigation is allowed and a safety tip is
  // shown instead. We consider that sufficient cause for concern for actor
  // code.
  if (lookalike_result.action_type != lookalikes::LookalikeActionType::kNone &&
      lookalike_result.action_type !=
          lookalikes::LookalikeActionType::kRecordMetrics) {
    return origin_gating::Decision::kBlocked;
  }
  return origin_gating::Decision::kNoDecision;
}

origin_gating::Decision BlockIfSafeBrowsingDisabled(
    Profile* profile,
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination) {
  bool is_safe_browsing_enabled = false;
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  is_safe_browsing_enabled =
      safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs());
#endif
  // We don't want to risk acting on dangerous sites, so we require
  // SafeBrowsing.
  return is_safe_browsing_enabled ? origin_gating::Decision::kNoDecision
                                  : origin_gating::Decision::kBlocked;
}

origin_gating::Decision AllowIfSafetyChecksDisabled(
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination) {
  return IsActorSafetyCheckDisabled() ? origin_gating::Decision::kAllowed
                                      : origin_gating::Decision::kNoDecision;
}

static constexpr std::string_view kPermissionGrantedHistogram =
    "Actor.NavigationGating.PermissionGranted";

BASE_FEATURE(kActorReloadCrashedTabBeforeAct, base::FEATURE_ENABLED_BY_DEFAULT);

RenderFrameHost* GetPrimaryMainFrame(
    content::NavigationHandle& navigation_handle) {
  return navigation_handle.GetWebContents()->GetPrimaryMainFrame();
}

void PostTaskForActCallback(
    ActorTask::ActCallback callback,
    std::vector<ActionResultWithLatencyInfo> action_results,
    TabObservationStrategy observation_strategy) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(action_results),
                                std::move(observation_strategy)));
}

// When operating on an opaque site, we choose to use the precursor's origin
// when judging whether a user confirmation should be triggered or not. We are
// effectively using `url::Origin::Create(rfh.GetLastCommittedUrl())` in lieu of
// `rfh.GetLastCommittedOrigin()` for this "security" purpose, contrary to the
// guidance here (docs/security/origin-vs-url.md).
//
// This is an intentional decision since it relates to user confirmations and it
// would be confusing to ask the user to distinguish between opaque domains.
url::Origin OriginOrPrecursorIfOpaque(const url::Origin& origin) {
  if (!origin.opaque()) {
    return origin;
  }

  return url::Origin::Create(
      origin.GetTupleOrPrecursorTupleIfOpaque().GetURL());
}

ExecutionEngine::GatingDecision MapGatingDecisionToEngineDecision(
    const origin_gating::GatingDecision& decision) {
  switch (decision.attribution.type()) {
    case origin_gating::DecisionAttribution::Type::kDecisionSource:
      switch (decision.attribution.Source()) {
        case DecisionSource::kAllowSameOrigin:
          return ExecutionEngine::GatingDecision::kAllowSameOrigin;
        case DecisionSource::kActorContainerConfig:
          return decision.is_allowed
                     ? ExecutionEngine::GatingDecision::kAllowByContainerConfig
                     : ExecutionEngine::GatingDecision::kBlockByContainerConfig;
        case DecisionSource::kEnterprisePolicy:
          return decision.is_allowed
                     ? ExecutionEngine::GatingDecision::kAllowByStaticList
                     : ExecutionEngine::GatingDecision::kBlockByStaticList;
        case DecisionSource::kCacheWithUserConfirmation:
        case DecisionSource::kCacheWithoutUserConfirmation:
        case DecisionSource::kNoVerdict:
          return ExecutionEngine::GatingDecision::kNeedsAsyncCheck;
        case DecisionSource::kAllowHttpLocalhost:
        case DecisionSource::kAllowAboutBlank:
        case DecisionSource::kForbidNonLocalhostIpAddress:
        case DecisionSource::kRequireHttpsOrLocalhost:
        case DecisionSource::kRequireHttpsOrHttp:
          NOTREACHED();
      }
    case origin_gating::DecisionAttribution::Type::kCustomPredicate:
      switch (decision.attribution.CustomPredicateId<ActorCustomPredicate>()) {
        case ActorCustomPredicate::kSafetyList:
          return decision.is_allowed
                     ? ExecutionEngine::GatingDecision::kAllowByStaticList
                     : ExecutionEngine::GatingDecision::kBlockByStaticList;
        case ActorCustomPredicate::kDangerousMimeType:
          return ExecutionEngine::GatingDecision::kBlockByDangerousMimeType;
        case ActorCustomPredicate::kSensitiveUrl:
          return ExecutionEngine::GatingDecision::kNeedsAsyncCheck;
        case ActorCustomPredicate::kLookalikeUrl:
          return ExecutionEngine::GatingDecision::kBlockByLookalikeUrl;
        case ActorCustomPredicate::kSafeBrowsing:
          return ExecutionEngine::GatingDecision::kBlockBySafeBrowsing;
        case ActorCustomPredicate::kSafetyChecksDisabled:
          return ExecutionEngine::GatingDecision::kAllowBySafetyChecksDisabled;
        case ActorCustomPredicate::kTabErrorDocument:
          return ExecutionEngine::GatingDecision::kBlockByTabErrorDocument;
        case ActorCustomPredicate::kTabSafeBrowsingObserver:
          return ExecutionEngine::GatingDecision::
              kBlockByTabSafeBrowsingObserver;
      }
      NOTREACHED();
  }
}

MayActOnUrlBlockReason MapGatingDecisionToBlockReason(
    const origin_gating::GatingDecision& decision,
    const GURL& url) {
  if (decision.is_allowed) {
    return MayActOnUrlBlockReason::kAllowed;
  }
  switch (decision.attribution.type()) {
    case origin_gating::DecisionAttribution::Type::kDecisionSource:
      switch (decision.attribution.Source()) {
        case DecisionSource::kEnterprisePolicy:
          return MayActOnUrlBlockReason::kEnterprisePolicy;
        case DecisionSource::kForbidNonLocalhostIpAddress:
          return MayActOnUrlBlockReason::kIpAddress;
        case DecisionSource::kRequireHttpsOrLocalhost:
        case DecisionSource::kRequireHttpsOrHttp:
          return ProfileIOData::IsHandledURL(url)
                     ? MayActOnUrlBlockReason::kWrongScheme
                     : MayActOnUrlBlockReason::kExternalProtocol;
        case DecisionSource::kActorContainerConfig:
          return MayActOnUrlBlockReason::kBlockedByContainerConfig;
        case DecisionSource::kNoVerdict:
          // `OnNoVerdict` allows navigation requests to proceed, and only
          // blocks actions if the URL was sensitive and the user refused the
          // prompt.
          return MayActOnUrlBlockReason::kOptimizationGuideBlock;
        case origin_gating::DecisionSource::kAllowSameOrigin:
        case origin_gating::DecisionSource::kAllowHttpLocalhost:
        case origin_gating::DecisionSource::kAllowAboutBlank:
        case origin_gating::DecisionSource::kCacheWithUserConfirmation:
        case origin_gating::DecisionSource::kCacheWithoutUserConfirmation:
          // Unreachable since these predicates allow the event, but
          // `decision.is_allowed` is false.
          NOTREACHED();
      }
    case origin_gating::DecisionAttribution::Type::kCustomPredicate:
      switch (decision.attribution.CustomPredicateId<ActorCustomPredicate>()) {
        case ActorCustomPredicate::kSafetyList:
          return MayActOnUrlBlockReason::kBlockedByStaticList;
        case ActorCustomPredicate::kDangerousMimeType:
          return MayActOnUrlBlockReason::kDangerousMimeType;
        case ActorCustomPredicate::kSensitiveUrl:
          return MayActOnUrlBlockReason::kOptimizationGuideBlock;
        case ActorCustomPredicate::kLookalikeUrl:
          return MayActOnUrlBlockReason::kLookalikeDomain;
        case ActorCustomPredicate::kSafeBrowsing:
          return MayActOnUrlBlockReason::kSafeBrowsing;
        case ActorCustomPredicate::kTabErrorDocument:
          return MayActOnUrlBlockReason::kTabIsErrorDocument;
        case ActorCustomPredicate::kTabSafeBrowsingObserver:
          return MayActOnUrlBlockReason::kSafeBrowsing;
        case ActorCustomPredicate::kSafetyChecksDisabled:
          // Unreachable since this predicate allows the event, but
          // `decision.is_allowed` is false.
          NOTREACHED();
      }
      NOTREACHED();
  }
}

// Resolves the gating decision and logs it to the journal.
MayActOnUrlBlockReason ResolveGatingDecision(
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    const GURL& url,
    GateableEvent event,
    std::unique_ptr<origin_gating::GatingDecisionContext> context,
    origin_gating::GatingDecision decision) {
  journal_entry->EndEntry(
      JournalDetailsBuilder()
          .Add("origin", url::Origin::Create(url).Serialize())
          .Add("event", origin_gating::GateableEventToString(event))
          .Add("decision", decision.is_allowed ? "allowed" : "blocked")
          .Add("attribution", DecisionAttributionToString(decision.attribution))
          .Build());

  return MapGatingDecisionToBlockReason(decision, url);
}

void OnNavigationConfirmationDecisionInBackground(
    ExecutionEngine::State state,
    ukm::SourceId ukm_source_id,
    base::ScopedUmaHistogramTimer timer,
    webui::mojom::NavigationConfirmationResponsePtr response) {
  switch (response->result->which()) {
    case webui::mojom::ConfirmationRequestResult::Tag::kPermissionGranted: {
      bool permission_granted = response->result->get_permission_granted();
      base::UmaHistogramBoolean(kPermissionGrantedHistogram,
                                permission_granted);
      ukm::builders::Actor_OriginGating builder(ukm_source_id);
      builder
          .SetServerConfirmationResult(static_cast<int64_t>(
              permission_granted
                  ? ExecutionEngine::ActorServerConfirmationResult::kAccepted
                  : ExecutionEngine::ActorServerConfirmationResult::kRejected))
          .SetEngineState(static_cast<int64_t>(state));
      builder.Record(ukm::UkmRecorder::Get());
      return;
    }
    case webui::mojom::ConfirmationRequestResult::Tag::kErrorReason:
      return;
  }
  NOTREACHED();
}

}  // namespace

ToolDelegate::CredentialWithPermission::CredentialWithPermission() = default;
ToolDelegate::CredentialWithPermission::CredentialWithPermission(
    const actor_login::Credential& credential,
    webui::mojom::UserGrantedPermissionDuration permission_duration)
    : credential(credential), permission_duration(permission_duration) {}
ToolDelegate::CredentialWithPermission::CredentialWithPermission(
    const CredentialWithPermission&) = default;
ToolDelegate::CredentialWithPermission::CredentialWithPermission(
    CredentialWithPermission&&) = default;
ToolDelegate::CredentialWithPermission&
ToolDelegate::CredentialWithPermission::operator=(
    const CredentialWithPermission&) = default;
ToolDelegate::CredentialWithPermission&
ToolDelegate::CredentialWithPermission::operator=(CredentialWithPermission&&) =
    default;
ToolDelegate::CredentialWithPermission::~CredentialWithPermission() = default;

// static
ExecutionEngine::FactoryFunction&
ExecutionEngine::GetFactoryFunctionForTesting() {
  static base::NoDestructor<FactoryFunction> callback;
  return *callback;
}

// Protected constructor without pass key to allow subclassing.
ExecutionEngine::ExecutionEngine(ActorTask& owner_task)
    : ExecutionEngine(
          base::PassKey<ExecutionEngine>(),
          owner_task,
          ui::NewUiEventDispatcher(
              owner_task.actor_keyed_service().GetActorUiStateManager())) {}

ExecutionEngine::ExecutionEngine(
    base::PassKey<ExecutionEngine>,
    ActorTask& owner_task,
    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher)
    : task_(owner_task),
      journal_(task_->actor_keyed_service().GetJournal().GetSafeRef()),
      tool_controller_(std::make_unique<ToolController>(*task_, *this)),
      actor_login_service_(
          std::make_unique<actor_login::ActorLoginServiceImpl>()),
      actor_form_filling_service_(
          std::make_unique<autofill::ActorFormFillingServiceImpl>(journal_,
                                                                  task_->id())),
      actor_one_time_token_filling_service_(
          std::make_unique<autofill::ActorOneTimeTokenFillingServiceImpl>(
              task_->GetProfile(),
              journal_,
              task_->id())),
      ui_event_dispatcher_(std::move(ui_event_dispatcher)),
      origin_gating_checker_(
          *this,
          origin_gating::OriginGatingConfiguration(
              {
                  {CustomPredicate(base::BindRepeating(&BlockTabErrorDocument),
                                   ActorCustomPredicate::kTabErrorDocument),
                   {GateableEvent::kPageAction}},
                  {CustomPredicate(
                       base::BindRepeating(
                           &BlockSafeBrowsingWarningIfSafetyChecksEnabled),
                       ActorCustomPredicate::kTabSafeBrowsingObserver),
                   {GateableEvent::kPageAction}},
                  // If localhost should be treated as sensitive, only
                  // auto-allow for navigation requests.
                  {DecisionSource::kAllowHttpLocalhost,
                   base::FeatureList::IsEnabled(kGlicActorLocalhostIsSensitive)
                       ? GateableEventSet{GateableEvent::kNavigationRequest}
                       : kRequestsAndPageActions},
                  {DecisionSource::kAllowAboutBlank, kRequestsAndPageActions},
                  // Allow insecure HTTP for navigation requests, as in
                  // practice sites may have HTTP links that will get upgraded.
                  // Rejecting HTTP URLs before this can happen would be too
                  // serious of an impediment.
                  {DecisionSource::kRequireHttpsOrHttp,
                   {GateableEvent::kNavigationRequest}},
                  {DecisionSource::kRequireHttpsOrLocalhost,
                   {GateableEvent::kPageAction}},
                  {DecisionSource::kForbidNonLocalhostIpAddress,
                   kRequestsAndPageActions},
                  {CustomPredicate(
                       base::BindRepeating(&AllowIfSafetyChecksDisabled),
                       ActorCustomPredicate::kSafetyChecksDisabled),
                   origin_gating::GateableEventSet::All()},
                  {CustomPredicate(
                       base::BindRepeating(&BlockIfSafeBrowsingDisabled,
                                           task_->GetProfile()),
                       ActorCustomPredicate::kSafeBrowsing),
                   kRequestsAndPageActions},
                  {CustomPredicate(base::BindRepeating(&BlockDangerousMimeType),
                                   ActorCustomPredicate::kDangerousMimeType),
                   {GateableEvent::kNavigationResponse}},
                  {DecisionSource::kEnterprisePolicy,
                   {GateableEvent::kNavigationResponse,
                    GateableEvent::kPageAction}},
                  {CustomPredicate(base::BindRepeating(&BlockLookalikeUrl,
                                                       task_->GetProfile()),
                                   ActorCustomPredicate::kLookalikeUrl),
                   kRequestsAndPageActions},
                  {DecisionSource::kActorContainerConfig,
                   {GateableEvent::kNavigationResponse,
                    GateableEvent::kPageAction}},
                  {CreateSafetyListPredicate(),
                   {GateableEvent::kNavigationResponse,
                    GateableEvent::kPageAction}},
                  {DecisionSource::kCacheWithUserConfirmation,
                   GateableEventSet::All()},
                  {DecisionSource::kAllowSameOrigin,
                   {GateableEvent::kNavigationResponse}},
                  {CustomPredicate(
                       base::BindRepeating(
                           &BlockSensitiveUrlWhenNavigationGatingDisabled,
                           task_->GetProfile()),
                       ActorCustomPredicate::kSensitiveUrl),
                   {GateableEvent::kNavigationRequest}},
                  {DecisionSource::kCacheWithoutUserConfirmation,
                   {GateableEvent::kNavigationResponse}},
              },
              kGlicNavigationGatingUseSiteNotOrigin.Get())),
      dark_launch_origin_gating_cache_(
          kGlicNavigationGatingUseSiteNotOrigin.Get()) {
  TRACE_EVENT0("actor", "ExecutionEngine::ExecutionEngine");
}

// static
std::unique_ptr<ExecutionEngine> ExecutionEngine::Create(
    ActorTask& owner_task,
    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher) {
  if (!GetFactoryFunctionForTesting().is_null()) {
    return GetFactoryFunctionForTesting().Run(owner_task);
  }

  return std::make_unique<ExecutionEngine>(base::PassKey<ExecutionEngine>(),
                                           owner_task,
                                           std::move(ui_event_dispatcher));
}

// static
std::unique_ptr<ExecutionEngine> ExecutionEngine::CreateForTesting(
    ActorTask& owner_task,
    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher) {
  return std::make_unique<ExecutionEngine>(base::PassKey<ExecutionEngine>(),
                                           owner_task,
                                           std::move(ui_event_dispatcher));
}

ExecutionEngine::~ExecutionEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  origin_gating::OriginGatingCache::SizeMetrics metrics =
      origin_gating_cache().GetSizeMetrics();
  RecordActorNavigationGatingListSize(metrics.allow_list_size,
                                      metrics.confirmed_list_size);

  RunUserTakeoverCallbackIfExists(/*should_cancel=*/true);
  CancelPendingNavigations();
}

void ExecutionEngine::SetState(State state) {
  TRACE_EVENT0("actor", "ExecutionEngine::SetState");
  journal_->Log(GURL(), task_->id(), "ExecutionEngine::StateChange",
                JournalDetailsBuilder()
                    .Add("current_state", StateToString(state_))
                    .Add("new_state", StateToString(state))
                    .Build());

#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          {State::kInit, {State::kStartAction, State::kComplete}},
          {State::kStartAction,
           {State::kToolCreateAndVerify, State::kComplete}},
          {State::kToolCreateAndVerify,
           {State::kUiPreInvoke, State::kComplete}},
          {State::kUiPreInvoke, {State::kToolInvoke, State::kComplete}},
          {State::kToolInvoke, {State::kUiPostInvoke, State::kComplete}},
          {State::kUiPostInvoke, {State::kComplete, State::kStartAction}},
          {State::kComplete, {State::kStartAction}},
      }));
  DCHECK_STATE_TRANSITION(transitions, state_, state);
#endif  // DCHECK_IS_ON()
  observers_.Notify(&StateObserver::OnStateChanged, state_, state);
  state_ = state;
}

std::string ExecutionEngine::StateToString(State state) {
  switch (state) {
    case State::kInit:
      return "INIT";
    case State::kStartAction:
      return "START_ACTION";
    case State::kToolCreateAndVerify:
      return "CREATE_AND_VERIFY";
    case State::kUiPreInvoke:
      return "UI_PRE_INVOKE";
    case State::kToolInvoke:
      return "TOOL_INVOKE";
    case State::kUiPostInvoke:
      return "UI_POST_INVOKE";
    case State::kComplete:
      return "COMPLETE";
  }
}

void ExecutionEngine::ShouldNavigationCommit(
    content::NavigationHandle& navigation_handle,
    ExecutionEngine::NavigationDecisionCallback callback) {
  if (!IsNavigationGatingEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), MayActOnUrlBlockReason::kAllowed));
    return;
  }

  CHECK(navigation_handle.GetNavigatingFrameType() ==
            content::FrameType::kPrimaryMainFrame ||
        navigation_handle.GetNavigatingFrameType() ==
            content::FrameType::kPrerenderMainFrame);
  CHECK(!navigation_handle.HasCommitted());

  base::ScopedUmaHistogramTimer timer(
      "Actor.NavigationGating.TimeElapsedForGating2");

  const url::Origin source_origin = OriginOrPrecursorIfOpaque(
      GetPrimaryMainFrame(navigation_handle)->GetLastCommittedOrigin());
  auto event = GateableEvent::kNavigationResponse;
  auto wrapped_callback = TrackPendingNavigation(
      pending_navigation_cancellations_, std::move(callback),
      /*block_reason_if_dropped=*/MayActOnUrlBlockReason::kTaskCancelled);
  origin_gating_checker_.ComputeGatingDecision(
      std::make_unique<NavigationResponseContext>(
          GetPrimaryMainFrame(navigation_handle)->GetPageUkmSourceId(),
          navigation_handle.IsInPrerenderedMainFrame(), std::move(timer),
          ExtractMimeType(navigation_handle)),
      event, source_origin.GetURL(), navigation_handle.GetURL(),
      base::BindOnce(
          &ExecutionEngine::OnComputedGatingDecision, GetWeakPtr(),
          std::move(wrapped_callback),
          journal_->CreatePendingAsyncEntry(
              navigation_handle.GetURL(), task_->id(),
              MakeBrowserTrackUUID(task_->id()), "OriginGatingDecision", {}),
          source_origin, url::Origin::Create(navigation_handle.GetURL()),
          state_, navigation_handle.GetInitiatorOrigin(), event));
}

void ExecutionEngine::CancelPendingNavigations() {
  TRACE_EVENT0("actor", "ExecutionEngine::CancelPendingNavigations");
  pending_navigation_cancellations_.Notify();
}

void ExecutionEngine::OnComputedGatingDecision(
    NavigationDecisionCallback callback,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    const url::Origin& source_origin,
    const url::Origin& destination_origin,
    State initial_state,
    std::optional<url::Origin> initiator,
    GateableEvent event,
    std::unique_ptr<origin_gating::GatingDecisionContext> context,
    origin_gating::GatingDecision decision) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogNavigationGating(source_origin, initiator, destination_origin,
                      /*applied_gate=*/!decision.is_allowed ||
                          decision.attribution == DecisionSource::kNoVerdict);

  RecordNavigationGatingDecision(MapGatingDecisionToEngineDecision(decision));

  const auto* response_context =
      static_cast<NavigationResponseContext*>(context.get());
  CHECK(response_context);
  if (decision.attribution == DecisionSource::kCacheWithoutUserConfirmation ||
      decision.attribution == DecisionSource::kCacheWithUserConfirmation) {
    ukm::builders::Actor_OriginGating(response_context->ukm_source_id)
        .SetServerConfirmationResult(static_cast<int64_t>(
            ExecutionEngine::ActorServerConfirmationResult::kNotRequired))
        .SetEngineState(static_cast<int64_t>(initial_state))
        .Record(ukm::UkmRecorder::Get());
  }

  journal_entry->EndEntry(
      JournalDetailsBuilder()
          .Add("source_origin", source_origin.Serialize())
          .Add("destination_origin", destination_origin.Serialize())
          .Add("initiator_origin",
               initiator.transform(&url::Origin::Serialize).value_or("none"))
          .Add("event", origin_gating::GateableEventToString(event))
          .Add("decision", decision.is_allowed ? "allowed" : "blocked")
          .Add("attribution", DecisionAttributionToString(decision.attribution))
          .Add("mime_type",
               response_context->response_mime_type.value_or("null"))
          .Build());

  std::move(callback).Run(
      MapGatingDecisionToBlockReason(decision, destination_origin.GetURL()));
}

void ExecutionEngine::LogNavigationGating(
    const url::Origin& source,
    base::optional_ref<const url::Origin> initiator,
    const url::Origin& destination,
    bool applied_gate) const {
  base::UmaHistogramBoolean("Actor.NavigationGating.AppliedGate", applied_gate);

  base::UmaHistogramBoolean("Actor.NavigationGating.SameOriginSource",
                            source.IsSameOriginWith(destination));
  base::UmaHistogramBoolean(
      "Actor.NavigationGating.SameSiteSource",
      net::SchemefulSite::IsSameSite(source, destination));
  if (initiator) {
    base::UmaHistogramBoolean("Actor.NavigationGating.SameOriginInitiator",
                              initiator->IsSameOriginWith(destination));
    base::UmaHistogramBoolean(
        "Actor.NavigationGating.SameSiteInitiator",
        net::SchemefulSite::IsSameSite(*initiator, destination));
  }
}

void ExecutionEngine::DoesOriginRequireUserConfirmation(
    origin_gating::GatingDecisionContext* context,
    GateableEvent event,
    const GURL& source,
    const GURL& destination,
    DoesOriginRequireUserConfirmationCallback callback) const {
  // Navigation requests never prompt the user.
  if (event == GateableEvent::kNavigationRequest) {
    std::move(callback).Run(/*requires_user_confirmation=*/false);
    return;
  }

  IsNonSensitiveUrl(task_->GetProfile(), context, destination,
                    base::BindOnce([](bool not_sensitive) {
                      return !not_sensitive;
                    }).Then(std::move(callback)));
}

void ExecutionEngine::EvaluateEnterprisePolicy(
    const GURL& destination,
    EvaluateEnterprisePolicyCallback callback) const {
  origin_gating::Decision decision;
  switch (GetEnterprisePolicyChecker().Evaluate(destination)) {
    case EnterprisePolicyChecker::UrlBlockReason::kNotBlocked:
      decision = origin_gating::Decision::kNoDecision;
      break;
    case EnterprisePolicyChecker::UrlBlockReason::kExplicitlyAllowed:
      decision = origin_gating::Decision::kAllowed;
      break;
    case EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked:
      decision = origin_gating::Decision::kBlocked;
      break;
  }
  std::move(callback).Run({.decision = decision, .bypass_cache = true});
}

void ExecutionEngine::OnNoVerdict(
    origin_gating::GatingDecisionContext* context,
    GateableEvent event,
    const GURL& source,
    const GURL& destination,
    bool requires_user_confirmation,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Navigation requests fail open.
  if (event == GateableEvent::kNavigationRequest) {
    std::move(callback).Run(
        {.is_allowed = true, .did_prompt_user = false, .bypass_cache = true});
    return;
  }

  NavigationResponseContext* navigation_response_context =
      event == GateableEvent::kNavigationResponse
          ? static_cast<NavigationResponseContext*>(context)
          : nullptr;
  url::Origin destination_origin = url::Origin::Create(destination);
  if (navigation_response_context && navigation_response_context->skip_prompt) {
    std::move(callback).Run({.is_allowed = false, .did_prompt_user = false});
    return;
  }

  if (!requires_user_confirmation) {
    if (event == GateableEvent::kPageAction) {
      std::move(callback).Run({.is_allowed = true, .did_prompt_user = false});
      return;
    }
    CHECK(navigation_response_context);
    HandleNavigationToNewOrigin(
        destination_origin, navigation_response_context->ukm_source_id,
        std::move(navigation_response_context->timer), std::move(callback));
    return;
  }

  SendUserConfirmationDialogRequest(
      destination_origin,
      /*for_sensitive_origin=*/true,
      navigation_response_context
          ? std::optional(std::move(navigation_response_context->timer))
          : std::nullopt,
      std::move(callback));
}

void ExecutionEngine::HandleNavigationToNewOrigin(
    const url::Origin& destination,
    ukm::SourceId ukm_source_id,
    base::ScopedUmaHistogramTimer timer,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  if (!kGlicConfirmNavigationToNewOrigins.Get()) {
    if (kGlicConfirmNavigationToNewOriginsDarkLaunch.Get() &&
        !dark_launch_origin_gating_cache_.IsNavigationAllowed(url::Origin(),
                                                              destination)) {
      SendNavigationConfirmationRequest(
          destination,
          base::BindOnce(&OnNavigationConfirmationDecisionInBackground, state_,
                         ukm_source_id,
                         base::ScopedUmaHistogramTimer(
                             "Actor.NavigationGating."
                             "DarkLaunchConfirmationRequestLatency")));
      // Navigation is auto-approved, so add to origin checker allowlist to skip
      // future checks and metrics.
      dark_launch_origin_gating_cache_.AllowNavigationTo(
          destination,
          /*is_user_confirmed=*/false);
    }
    std::move(callback).Run({.is_allowed = true, .did_prompt_user = false});
    return;
  }
  if (kGlicPromptUserForNavigationToNewOrigins.Get()) {
    SendUserConfirmationDialogRequest(destination,
                                      /*for_sensitive_origin=*/false,
                                      std::move(timer), std::move(callback));
    return;
  }

  SendNavigationConfirmationRequest(
      destination,
      base::BindOnce(&ExecutionEngine::OnNavigationConfirmationDecision,
                     GetActionSequenceWeakPtr(), destination, ukm_source_id,
                     std::move(timer), state_,
                     base::BindOnce([](bool permission_granted) {
                       return NoVerdictResult{
                           .is_allowed = permission_granted,
                           .did_prompt_user = false,
                       };
                     }).Then(std::move(callback))));
}

void ExecutionEngine::SendNavigationConfirmationRequest(
    const url::Origin& destination,
    NavigationConfirmationCallback callback) {
  if (!task_->delegate()) {
    auto response = webui::mojom::NavigationConfirmationResponse::New();
    response->result =
        webui::mojom::ConfirmationRequestResult::NewPermissionGranted(false);
    std::move(callback).Run(std::move(response));
    return;
  }
  task_->delegate()->RequestToConfirmNavigation(task_->id(), destination,
                                                std::move(callback));
}

void ExecutionEngine::OnNavigationConfirmationDecision(
    const url::Origin& destination,
    ukm::SourceId ukm_source_id,
    base::ScopedUmaHistogramTimer timer,
    State engine_state,
    base::OnceCallback<void(bool)> callback,
    webui::mojom::NavigationConfirmationResponsePtr response) {
  switch (response->result->which()) {
    case webui::mojom::ConfirmationRequestResult::Tag::kPermissionGranted: {
      bool permission_granted = response->result->get_permission_granted();
      // TODO(dylancutler): Separate Actor.NavigationGating.PermissionGranted
      // into separate histograms for different confirmation types.
      base::UmaHistogramBoolean(kPermissionGrantedHistogram,
                                permission_granted);
      ukm::builders::Actor_OriginGating builder(ukm_source_id);
      builder
          .SetServerConfirmationResult(static_cast<int64_t>(
              permission_granted
                  ? ExecutionEngine::ActorServerConfirmationResult::kAccepted
                  : ExecutionEngine::ActorServerConfirmationResult::kRejected))
          .SetEngineState(static_cast<int64_t>(engine_state));
      builder.Record(ukm::UkmRecorder::Get());
      std::move(callback).Run(permission_granted);
      return;
    }
    case webui::mojom::ConfirmationRequestResult::Tag::kErrorReason:
      // TODO(crbug.com/450302860): Add UMA metrics for logging frequency of
      // different failure modes.
      std::move(callback).Run(/*may_continue=*/false);
      return;
  }
  NOTREACHED();
}

void ExecutionEngine::SendUserConfirmationDialogRequest(
    const url::Origin& destination,
    bool for_sensitive_origin,
    std::optional<base::ScopedUmaHistogramTimer> timer,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  if (!task_->delegate()) {
    std::move(callback).Run({.is_allowed = false, .did_prompt_user = false});
    return;
  }

  journal_->Log(GURL::EmptyGURL(), task_->id(),
                "SendUserConfirmationDialogRequest", {});

  task_->delegate()->RequestToShowUserConfirmationDialog(
      task_->id(), destination, for_sensitive_origin,
      base::BindOnce(&ExecutionEngine::OnPromptUserToConfirmNavigationDecision,
                     GetActionSequenceWeakPtr(), destination,
                     base::BindOnce([](bool permission_granted) {
                       return NoVerdictResult{
                           .is_allowed = permission_granted,
                           .did_prompt_user = true,
                       };
                     }).Then(std::move(callback))));
}

void ExecutionEngine::OnPromptUserToConfirmNavigationDecision(
    const url::Origin& destination,
    base::OnceCallback<void(bool)> callback,
    webui::mojom::UserConfirmationDialogResponsePtr response) {
  switch (response->result->which()) {
    case webui::mojom::ConfirmationRequestResult::Tag::kPermissionGranted: {
      bool permission_granted = response->result->get_permission_granted();
      base::UmaHistogramBoolean(kPermissionGrantedHistogram,
                                permission_granted);
      std::move(callback).Run(permission_granted);
      return;
    }
    case webui::mojom::ConfirmationRequestResult::Tag::kErrorReason:
      // TODO(crbug.com/450302860): Add UMA metrics for logging frequency of
      // different failure modes.
      std::move(callback).Run(/*may_continue=*/false);
      return;
  }
  NOTREACHED();
}

void ExecutionEngine::UserTakeover(
    mojom::ActionResultCode takeover_response_code,
    base::OnceCallback<void(bool)> callback) {
  if (takeover_response_code == mojom::ActionResultCode::kFilePickerTriggered) {
    RecordDownloadSaveAsDialogTriggered(true);
  }

  CancelOngoingActions(takeover_response_code);

  // Cancel any existing user takeover callback
  RunUserTakeoverCallbackIfExists(/*should_cancel=*/true);

  user_takeover_callback_ = std::move(callback);
}

void ExecutionEngine::RunUserTakeoverCallbackIfExists(bool should_cancel) {
  if (user_takeover_callback_.is_null()) {
    return;
  }

  std::move(user_takeover_callback_).Run(should_cancel);
}

void ExecutionEngine::AddObserver(StateObserver* observer) {
  observers_.AddObserver(observer);
}

void ExecutionEngine::RemoveObserver(StateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ExecutionEngine::DidUninterruptTask() {
  if (deferred_finish_tool_invoke_) {
    std::move(deferred_finish_tool_invoke_).Run();
  }
}

bool ExecutionEngine::TabsCanOpenNewWebContents() const {
  return state() == State::kToolInvoke &&
         GetInProgressAction().RequiresOpeningWebContents();
}

base::WeakPtr<ExecutionEngine> ExecutionEngine::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ExecutionEngine::CancelOngoingActions(mojom::ActionResultCode reason) {
  TRACE_EVENT0("actor", "ExecutionEngine::CancelOngoingActions");
  deferred_finish_tool_invoke_.Reset();
  if (tool_controller_) {
    tool_controller_->Cancel();
  }
  if (!action_sequence_.empty()) {
    CompleteActions(MakeResult(reason), /*action_index=*/std::nullopt);
  }
}

void ExecutionEngine::PauseOngoingActions() {
  TRACE_EVENT0("actor", "ExecutionEngine::PauseOngoingActions");
  if (tool_controller_) {
    tool_controller_->Pause();
  }
}

void ExecutionEngine::FailCurrentTool(mojom::ActionResultCode reason) {
  TRACE_EVENT0("actor", "ExecutionEngine::FailCurrentTool");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(reason, mojom::ActionResultCode::kOk);
  if (state_ != State::kToolInvoke) {
    return;
  }

  external_tool_failure_reason_ = reason;
}

void ExecutionEngine::Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
                          ActorTask::ActCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::Act");
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(!actions.empty());
  CHECK(deferred_finish_tool_invoke_.is_null());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(task_->GetState(), ActorTask::State::kActing);

  {
    JournalDetailsBuilder journal_details;
    for (size_t i = 0; i < actions.size(); ++i) {
      journal_details.Add(absl::StrFormat("Actions[%d]", i),
                          actions[i]->JournalEvent());
    }
    journal_->Log(GURL::EmptyGURL(), task_->id(), "ExecutionEngine::Act",
                  std::move(journal_details).Build());
  }

  if (!action_sequence_.empty()) {
    journal_->Log(
        actions[0]->GetURLForJournal(), task_->id(), "Act Failed",
        JournalDetailsBuilder()
            .AddError(
                "Unable to perform action: task already has action in progress")
            .Build());
    PostTaskForActCallback(
        std::move(callback),
        MakeResultVector(
            mojom::ActionResultCode::kExecutionEngineExistingAction),
        TabObservationStrategy());
    return;
  }

  act_callback_ = std::move(callback);
  next_action_index_ = 0;
  observation_strategy_ = TabObservationStrategy();

  absl::flat_hash_set<int32_t> acting_tab_handles;

  action_sequence_ = std::move(actions);
  for (const std::unique_ptr<ToolRequest>& action : action_sequence_) {
    CHECK(action);
    if (action->GetTabHandle() != tabs::TabHandle::Null()) {
      acting_tab_handles.insert(action->GetTabHandle().raw_value());
    }
    if (IsNavigationGatingEnabled() &&
        kGlicAllowImplicitToolOriginGrants.Get()) {
      if (std::optional<url::Origin> maybe_origin =
              action->AssociatedOriginGrant();
          maybe_origin) {
        origin_gating_checker_.AllowNavigationTo(maybe_origin.value(),
                                                 /*is_user_confirmed=*/false);
      }
    }
  }

  KickOffNextAction();
}

void ExecutionEngine::KickOffNextAction() {
  TRACE_EVENT0("actor", "ExecutionEngine::KickOffNextAction");
  DCHECK(state_ == State::kInit || state_ == State::kUiPostInvoke ||
         state_ == State::kComplete)
      << "Current state is " << StateToString(state_);
  CHECK_LT(next_action_index_, action_sequence_.size());

  SetState(State::kStartAction);
  if (!GetNextAction().IsFollowup()) {
    action_start_time_ = base::TimeTicks::Now();
  }

  // TODO(b/467984847): ActorTask::AddTab isn't the best way to track a crashed
  // tab here. We should refactor this to be more explicit.
  if (tabs::TabInterface* tab = GetNextAction().GetTabHandle().Get();
      tab && base::FeatureList::IsEnabled(kActorReloadCrashedTabBeforeAct)) {
    content::WebContents* contents = tab->GetContents();
    CHECK(contents);
    if (contents->IsCrashed()) {
      GetJournal().Log(
          contents->GetLastCommittedURL(), task_->id(),
          "ExecutionEngine::KickOffNextAction",
          JournalDetailsBuilder().AddError("Renderer crashed").Build());
      task_->AddTab(GetNextAction().GetTabHandle(),
                    /*stop_task_on_detach=*/true, base::DoNothing());
      CompleteActions(MakeResult(mojom::ActionResultCode::kRendererCrashed,
                                 /*requires_page_stabilization=*/false,
                                 "Renderer crashed."),
                      next_action_index_);
      return;
    }
  }

  if (GetNextAction().RequiresUrlCheckInCurrentTab()) {
    SafetyChecksForNextAction();
  } else {
    ExecuteNextAction();
  }
}

void ExecutionEngine::SafetyChecksForNextAction() {
  TRACE_EVENT0("actor", "ExecutionEngine::SafetyChecksForNextAction");
  tabs::TabInterface* tab = GetNextAction().GetTabForValidation().Get();

  if (!tab) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("The tab is no longer present")
                      .Build());
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               /*requires_page_stabilization=*/false,
                               "The tab is no longer present."),
                    next_action_index_);
    return;
  }

  // Asynchronously check if we can act on the tab. NOTE that the check uses
  // `GetLastCommittedURL()` from the tab. For opaque origins, this means that
  // we'll get the precursor URL. For this reason, we previously added the
  // precursor to `origin_gating_cache()` to ensure the optimization guide
  // sensitive origin check would be skipped as expected.

  // TODO(mcnee): Add UMA for the outcomes.
  content::WebContents& web_contents = *(tab->GetContents());
  const GURL& url = web_contents.GetPrimaryMainFrame()->GetLastCommittedURL();
  auto event = GateableEvent::kPageAction;
  origin_gating_checker_.ComputeGatingDecision(
      std::make_unique<PageActionGatingContext>(web_contents.GetWeakPtr()),
      event, /*source=*/GURL(), url,
      base::BindOnce(&ResolveGatingDecision,
                     journal_->CreatePendingAsyncEntry(
                         url, task_->id(), MakeBrowserTrackUUID(task_->id()),
                         "OriginGatingDecision", {}),
                     url, event)
          .Then(base::BindOnce([](MayActOnUrlBlockReason block_reason) {
            return BlockReasonToResultCode(block_reason,
                                           /*for_navigation=*/false);
          }))
          .Then(base::BindOnce(&ExecutionEngine::DidFinishAsyncSafetyChecks,
                               GetActionSequenceWeakPtr(),
                               tab->GetContents()
                                   ->GetPrimaryMainFrame()
                                   ->GetLastCommittedOrigin())));
}

void ExecutionEngine::DidFinishAsyncSafetyChecks(
    const url::Origin& evaluated_origin,
    mojom::ActionResultCode result_code) {
  TRACE_EVENT0("actor", "ExecutionEngine::DidFinishAsyncSafetyChecks");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!action_sequence_.empty());

  tabs::TabInterface* tab = GetNextAction().GetTabForValidation().Get();
  if (!tab) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("The tab is no longer present")
                      .Build());

    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               /*requires_page_stabilization=*/false,
                               "The tab is no longer present."),
                    next_action_index_);
    return;
  }

  TaskId task_id = task_->id();
  if (!evaluated_origin.IsSameOriginWith(tab->GetContents()
                                             ->GetPrimaryMainFrame()
                                             ->GetLastCommittedOrigin())) {
    // A cross-origin navigation occurred before we got permission. The result
    // is no longer applicable. For now just fail.
    // TODO(mcnee): Handle this gracefully.
    journal_->Log(GetNextAction().GetURLForJournal(), task_id, "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("Acting after cross-origin navigation occurred")
                      .Build());
    FailedOnTabBeforeToolCreation();
    CompleteActions(MakeResult(mojom::ActionResultCode::kCrossOriginNavigation,
                               /*requires_page_stabilization=*/false,
                               "Acting after cross-origin navigation occurred"),
                    next_action_index_);
    return;
  }

  if (!IsOk(result_code)) {
    journal_->Log(
        GetNextAction().GetURLForJournal(), task_id, "Act Failed",
        JournalDetailsBuilder().AddError("URL blocked for actions").Build());
    FailedOnTabBeforeToolCreation();
    CompleteActions(MakeResult(result_code,
                               /*requires_page_stabilization=*/false,
                               "URL blocked for actions"),
                    next_action_index_);
    return;
  }

  ExecuteNextAction();
}

void ExecutionEngine::FailedOnTabBeforeToolCreation() {
  journal_->Log(
      GetNextAction().GetURLForJournal(), task_->id(), "Act Failed",
      JournalDetailsBuilder()
          .Add("tabId", GetNextAction().GetTabForValidation().raw_value())
          .AddError("Associating tab for failed action")
          .Build());
  tabs::TabHandle actuation_tab = GetNextAction().GetTabHandle();
  if (actuation_tab != tabs::TabHandle::Null()) {
    task_->AddTab(actuation_tab, /*stop_task_on_detach=*/true,
                  base::DoNothing());
  }
}

void ExecutionEngine::ExecuteNextAction() {
  TRACE_EVENT0("actor", "ExecutionEngine::ExecuteNextAction");
  DCHECK_EQ(state_, State::kStartAction);
  CHECK(!action_sequence_.empty());
  CHECK(tool_controller_);

  ++next_action_index_;

  SetState(State::kToolCreateAndVerify);
  tool_controller_->CreateToolAndValidate(
      GetInProgressAction(), base::BindOnce(&ExecutionEngine::PostToolCreate,
                                            GetActionSequenceWeakPtr()));
}

void ExecutionEngine::PostToolCreate(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::PostToolCreate");
  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }
  SetState(State::kUiPreInvoke);
  ui_event_dispatcher_->OnPreTool(
      GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::FinishedUiPreInvoke,
                     GetActionSequenceWeakPtr()));
}

void ExecutionEngine::FinishedUiPreInvoke(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedUiPreInvoke");
  DCHECK_EQ(state_, State::kUiPreInvoke);
  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  // Cache the navigation ID before invoking the tool to ensure we log the
  // critical action under the correct pre-navigation page context.
  pre_invoke_navigation_id_ = 0;
  const ToolRequest& current_action = GetInProgressAction();
  tabs::TabInterface* tab = current_action.GetTabForValidation().Get();
  if (tab && tab->GetContents() && tab->GetContents()->GetPrimaryMainFrame()) {
    pre_invoke_navigation_id_ =
        tab->GetContents()->GetPrimaryMainFrame()->GetNavigationId();
  }

  SetState(State::kToolInvoke);
  tool_controller_->Invoke(base::BindOnce(&ExecutionEngine::FinishedToolInvoke,
                                          GetActionSequenceWeakPtr()));
}

void ExecutionEngine::FinishedToolInvoke(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedToolInvoke");
  DCHECK_EQ(state_, State::kToolInvoke);

  if (tool_invoke_complete_callback_for_testing_) {
    std::move(tool_invoke_complete_callback_for_testing_).Run();
  }

  // If the task is waiting on user input, defer returning a result for it. This
  // prevents the actor state from proceeding and also allows UI to insert an
  // `external_tool_failure_reason` to the action.
  if (base::FeatureList::IsEnabled(kGlicDeferActUntilUninterrupted) &&
      task_->GetState() == ActorTask::State::kWaitingOnUser) {
    CHECK(deferred_finish_tool_invoke_.is_null());
    deferred_finish_tool_invoke_ =
        base::BindOnce(&ExecutionEngine::FinishedToolInvoke,
                       base::Unretained(this), std::move(result));
    return;
  }

  // The current action errored out. Stop the chain.
  std::optional<mojom::ActionResultCode> external_tool_failure_reason;
  std::swap(external_tool_failure_reason, external_tool_failure_reason_);
  if (external_tool_failure_reason) {
    CompleteActions(MakeResult(*external_tool_failure_reason),
                    InProgressActionIndex());
    return;
  }

  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  const ToolRequest& current_action = GetInProgressAction();

  ActorCriticalActionLogger::MaybeLogAction(*task_, &GetProfile(),
                                            current_action, *result,
                                            pre_invoke_navigation_id_);
  pre_invoke_navigation_id_ = 0;

  // TODO(bokan): If tool completion is deferred due to interruption (e.g.
  // waiting on a user to confirm an action) the recorded tool metrics will look
  // inflated. This is a problem even if we record the metrics at the start of
  // this function (before deferring) because presumably the tool itself waits
  // on the cause of an interruption (and may reach here due to timeout or other
  // reason). Ideally we'd split metrics based on whether or not an
  // interruption was involved. Will file bug.
  CHECK(result->execution_end_time);
  base::TimeTicks end_time = base::TimeTicks::Now();
  RecordToolTimings(GetInProgressAction().Name(), end_time - action_start_time_,
                    end_time - *result->execution_end_time);

  if (GetInProgressAction().GetTabForValidation() != tabs::TabHandle::Null()) {
    observation_strategy_.VoteForScreenshot(
        GetInProgressAction().GetTabForValidation(),
        static_cast<ScreenshotPolicy>(result->screenshot_policy));
    observation_strategy_.VoteForPageContentExtraction(
        GetInProgressAction().GetTabForValidation(),
        static_cast<PageContentExtractionPolicy>(result->page_content_policy));
  }

  if (GetInProgressAction().IsFollowup()) {
    CHECK(!action_results_.empty());
    ActionResultWithLatencyInfo& action_result = action_results_.back();
    action_result.result = std::move(result);
    action_result.end_time = end_time;
  } else {
    action_results_.emplace_back(action_start_time_, end_time,
                                 std::move(result));
  }

  SetState(State::kUiPostInvoke);
  ui_event_dispatcher_->OnPostTool(
      GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::FinishedUiPostInvoke,
                     GetActionSequenceWeakPtr()));
}

void ExecutionEngine::FinishedUiPostInvoke(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedUiPostInvoke");
  DCHECK_EQ(state_, State::kUiPostInvoke);
  CHECK(!action_sequence_.empty());
  CHECK(deferred_finish_tool_invoke_.is_null());

  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  if (next_action_index_ >= action_sequence_.size()) {
    CompleteActions(MakeOkResult(), std::nullopt);
    return;
  }

  KickOffNextAction();
}

void ExecutionEngine::CompleteActions(mojom::ActionResultPtr result,
                                      std::optional<size_t> action_index) {
  TRACE_EVENT0("actor", "ExecutionEngine::CompleteActions");
  CHECK(!action_sequence_.empty());
  CHECK(act_callback_);

  // If we have not yet appended the action_results for the failed index,
  // append it now.
  if (action_index) {
    size_t result_index = GetResultIndexForAction(*action_index);
    if (action_results_.size() == result_index) {
      action_results_.emplace_back(action_start_time_, base::TimeTicks::Now(),
                                   result->Clone());
    } else if (action_results_.size() > result_index &&
               (!IsOk(*result) ||
                action_sequence_[*action_index]->IsFollowup())) {
      // If we already have a result for this action, and the new result is an
      // error, overwrite it. This can happen if a tool invocation succeeds
      // but a subsequent UI post-invoke stage fails, or for follow up tools
      // that fail.
      ActionResultWithLatencyInfo& action_result =
          action_results_[result_index];
      action_result.result = result->Clone();
      action_result.end_time = base::TimeTicks::Now();
    }
  } else if (!IsOk(*result)) {
    // If it's a general error, we still want it in action_results.
    action_results_.emplace_back(action_start_time_, base::TimeTicks::Now(),
                                 result->Clone());
  }

  SetState(State::kComplete);

  action_sequence_ended_callbacks_.Notify(IsOk(*result));

  if (!IsOk(*result)) {
    GURL url;
    if (action_index) {
      url = action_sequence_[*action_index]->GetURLForJournal();
    }
    journal_->Log(
        url, task_->id(), "Act Failed",
        JournalDetailsBuilder().AddError(ToDebugString(*result)).Build());
  }

  RecordActionResultCode(result->code);
  observation_strategy_.Lock();
  PostTaskForActCallback(std::move(act_callback_), std::move(action_results_),
                         std::move(observation_strategy_));

  action_sequence_.clear();
  next_action_index_ = 0;
  actions_weak_ptr_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<ExecutionEngine> ExecutionEngine::GetActionSequenceWeakPtr() {
  return actions_weak_ptr_factory_.GetWeakPtr();
}

bool ExecutionEngine::HasActionSequence() const {
  return !action_sequence_.empty();
}

favicon::FaviconService* ExecutionEngine::GetFaviconService() {
  return FaviconServiceFactory::GetForProfile(
      task_->GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
}

const EnterprisePolicyChecker& ExecutionEngine::GetEnterprisePolicyChecker()
    const {
  return task_->policy_checker();
}

void ExecutionEngine::IsAcceptableNavigationDestination(
    const GURL& url,
    DecisionCallbackWithReason callback) {
  auto event = GateableEvent::kNavigationRequest;
  origin_gating_checker_.ComputeGatingDecision(
      std::make_unique<OriginGatingDecisionContext>(), event, /*source=*/GURL(),
      url,
      base::BindOnce(&ResolveGatingDecision,
                     journal_->CreatePendingAsyncEntry(
                         url, task_->id(), MakeBrowserTrackUUID(task_->id()),
                         "OriginGatingDecision", {}),
                     url, event)
          .Then(std::move(callback)));
}

Profile& ExecutionEngine::GetProfile() {
  return *task_->GetProfile();
}

AggregatedJournal& ExecutionEngine::GetJournal() {
  return *journal_;
}

actor_login::ActorLoginService& ExecutionEngine::GetActorLoginService() {
  return *actor_login_service_;
}

autofill::ActorFormFillingService&
ExecutionEngine::GetActorFormFillingService() {
  return *actor_form_filling_service_;
}

autofill::ActorOneTimeTokenFillingService&
ExecutionEngine::GetActorOneTimeTokenFillingService() {
  return *actor_one_time_token_filling_service_;
}

void ExecutionEngine::PromptToSelectCredential(
    const std::vector<actor_login::Credential>& credentials,
    const base::flat_map<std::string, gfx::Image>& icons,
    ToolDelegate::CredentialSelectedCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::PromptToSelectCredential");
  CHECK(!credentials.empty());

  if (!task_->delegate()) {
    // TODO(crbug.com/427817882): Explicit error reason (kNewLonginAttempt).
    std::move(callback).Run(/*selected_credential=*/webui::mojom::
                                SelectCredentialDialogResponse::New());
    return;
  }
  task_->delegate()->RequestToShowCredentialSelectionDialog(
      task_->id(), icons, credentials, std::move(callback));
}

void ExecutionEngine::SetUserSelectedCredential(
    const ToolDelegate::CredentialWithPermission& credential_with_permission,
    base::OnceClosure affiliations_fetched) {
  url::Origin origin = credential_with_permission.credential.request_origin;
  user_selected_credentials_[origin] = credential_with_permission;

  affiliations::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(task_->GetProfile());
  // Fetch strongly affiliated domains, in order to be able to reuse the
  // permission for sites that do not have the exact same origin but are
  // strongly affiliated.
  if (affiliation_service) {
    affiliation_service->GetAffiliationsAndBranding(
        affiliations::FacetURI::FromPotentiallyInvalidSpec(
            origin.GetURL().GetWithEmptyPath().spec()),
        base::BindOnce(&ExecutionEngine::OnAffiliationsReceived,
                       GetActionSequenceWeakPtr(), origin,
                       std::move(affiliations_fetched)));
  } else {
    std::move(affiliations_fetched).Run();
  }
}

void ExecutionEngine::OnAffiliationsReceived(
    const url::Origin& source_origin,
    base::OnceClosure affiliations_fetched,
    const std::vector<affiliations::Facet>& results,
    bool success) {
  if (success) {
    for (const auto& facet : results) {
      // Iterate through results to find Web facets (format:
      // https://<host>[:<port>]) required for actor login. Android facets are
      // ignored.
      if (!facet.uri.IsValidWebFacetURI()) {
        continue;
      }

      GURL url(facet.uri.canonical_spec());
      url::Origin affiliated_origin = url::Origin::Create(url);
      if (!affiliated_origin.IsSameOriginWith(source_origin)) {
        affiliated_origin_map_[affiliated_origin] = source_origin;
      }
    }
  }
  std::move(affiliations_fetched).Run();
}

const std::optional<ToolDelegate::CredentialWithPermission>
ExecutionEngine::GetUserSelectedCredential(
    const url::Origin& request_origin) const {
  // Try exact match first.
  auto it = user_selected_credentials_.find(request_origin);
  if (it != user_selected_credentials_.end()) {
    return it->second;
  }

  // Check if the current origin is affiliated with a previously encountered
  // one within the current task.
  auto aff_it = affiliated_origin_map_.find(request_origin);
  if (aff_it != affiliated_origin_map_.end()) {
    auto original_cred_it = user_selected_credentials_.find(aff_it->second);
    if (original_cred_it != user_selected_credentials_.end()) {
      return original_cred_it->second;
    }
  }

  return std::nullopt;
}

void ExecutionEngine::RequestToShowAutofillSuggestions(
    std::vector<autofill::ActorFormFillingRequest> requests,
    base::WeakPtr<AutofillSelectionDialogEventHandler> event_handler,
    ExecutionEngine::AutofillSuggestionSelectedCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::RequestToShowAutofillSuggestions");
  CHECK(!requests.empty());

  if (!task_->delegate()) {
    std::move(callback).Run(
        webui::mojom::SelectAutofillSuggestionsDialogResponse::New(
            task_->id().value(),
            webui::mojom::SelectAutofillSuggestionsDialogResult::NewErrorReason(
                webui::mojom::SelectAutofillSuggestionsDialogErrorReason::
                    kNoActorTaskDelegate)));
    return;
  }
  task_->delegate()->RequestToShowAutofillSuggestionsDialog(
      task_->id(), std::move(requests), std::move(event_handler),
      std::move(callback));
  task_->action_tracker_for_metrics().OnAutofillAttentionDialogPresented();
}

void ExecutionEngine::RequestToShowGmailOtpOptInDialog(
    ToolDelegate::GmailOtpOptInCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::RequestToShowGmailOtpOptInDialog");
  if (!task_->delegate()) {
    auto result = webui::mojom::GmailOtpOptInResult::NewErrorReason(
        webui::mojom::GmailOtpErrorReason::kRequestPromiseNoSubscriber);
    std::move(callback).Run(std::move(result));
    return;
  }
  task_->delegate()->RequestToShowGmailOtpOptInDialog(task_->id(),
                                                      std::move(callback));
}

void ExecutionEngine::RequestToShowGmailOtpConfirmationDialog(
    const std::string& verification_code,
    GmailOtpConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("actor",
               "ExecutionEngine::RequestToShowGmailOtpConfirmationDialog");
  if (!task_->delegate()) {
    auto result = webui::mojom::GmailOtpConfirmationResult::NewErrorReason(
        webui::mojom::GmailOtpErrorReason::kRequestPromiseNoSubscriber);
    std::move(callback).Run(std::move(result));
    return;
  }
  task_->delegate()->RequestToShowGmailOtpConfirmationDialog(
      task_->id(), verification_code, std::move(callback));
}

void ExecutionEngine::InterruptFromTool() {
  InterruptFromTool(/*retain_user_control=*/false);
}

void ExecutionEngine::InterruptFromTool(bool retain_user_control) {
  task_->Interrupt(retain_user_control);
}

void ExecutionEngine::UninterruptFromTool() {
  task_->Uninterrupt(ActorTask::State::kActing);
}

void ExecutionEngine::EnqueueFollowupAction(
    std::unique_ptr<ToolRequest> action) {
  action->SetAsFollowup(base::PassKey<ExecutionEngine>());
  action_sequence_.insert(action_sequence_.begin() + next_action_index_,
                          std::move(action));
}

void ExecutionEngine::AddTab(
    tabs::TabHandle tab_handle,
    bool stop_task_on_detach,
    base::OnceCallback<void(mojom::ActionResultPtr)> callback) {
  task_->AddTab(tab_handle, stop_task_on_detach, std::move(callback));
}

bool ExecutionEngine::HasTab(tabs::TabHandle tab_handle) {
  return task_->HasTab(tab_handle);
}

void ExecutionEngine::RemoveTab(tabs::TabHandle tab_handle) {
  task_->RemoveTab(tab_handle);
}

base::WeakPtr<actor_login::ActionSequenceDelegate>
ExecutionEngine::GetActionSequenceDelegate() {
  return GetActionSequenceWeakPtr();
}

base::CallbackListSubscription ExecutionEngine::RegisterActionSequenceEnded(
    base::OnceCallback<void(bool)> callback) {
  return action_sequence_ended_callbacks_.Add(std::move(callback));
}

void ExecutionEngine::OnFederatedLoginOutcome(
    actor_login::LoginStatusResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojom::ActionResultCode code = actor_login::LoginResultToActorResult(result);
  if (!IsOk(code)) {
    FailCurrentTool(code);
  }
}

void ExecutionEngine::AddWritableMainframeOrigins(
    const absl::flat_hash_set<url::Origin>& added_writable_mainframe_origins) {
  if (!IsNavigationGatingEnabled()) {
    return;
  }
  origin_gating_checker_.AllowNavigationTo(added_writable_mainframe_origins);
}

void ExecutionEngine::SetActorLoginService(
    std::unique_ptr<actor_login::ActorLoginService> actor_login_service) {
  actor_login_service_ = std::move(actor_login_service);
}

const ToolRequest& ExecutionEngine::GetNextAction() const {
  CHECK_LT(next_action_index_, action_sequence_.size());
  return *action_sequence_.at(next_action_index_).get();
}

size_t ExecutionEngine::InProgressActionIndex() const {
  CHECK(state_ == State::kUiPreInvoke || state_ == State::kToolInvoke ||
        state_ == State::kUiPostInvoke || state_ == State::kToolCreateAndVerify)
      << "Current state is " << StateToString(state_);
  CHECK_GT(next_action_index_, 0ul);
  return next_action_index_ - 1;
}

const ToolRequest& ExecutionEngine::GetInProgressAction() const {
  return *action_sequence_.at(InProgressActionIndex()).get();
}

size_t ExecutionEngine::GetResultIndexForAction(size_t action_index) const {
  CHECK_LT(action_index, action_sequence_.size());
  size_t original_count = std::count_if(
      action_sequence_.begin(), action_sequence_.begin() + action_index + 1,
      [](const std::unique_ptr<ToolRequest>& action) {
        return !action->IsFollowup();
      });

  // At least the first action could not be a follow up.
  CHECK_GT(original_count, 0ul);

  return original_count - 1;
}

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s) {
  return o << ExecutionEngine::StateToString(s);
}

}  // namespace actor
