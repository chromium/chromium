// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/containers/stack.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/action_tracker_for_metrics.h"
#include "chrome/browser/actor/actor_critical_action_logger.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/execution_engine_dispatcher.h"
#include "chrome/browser/actor/javascript_dialog_interceptor.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/browser/actor/tools/password_manager/attempt_login/attempt_login_tool.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_impl.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_service_factory.h"
#include "chrome/browser/origin_gating/origin_gating_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_metrics.h"
#include "components/actor/core/actor_switches.h"
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
#include "components/origin_gating/core/decision.h"
#include "components/origin_gating/core/decision_attribution.h"
#include "components/origin_gating/core/decision_source.h"
#include "components/origin_gating/core/gateable_event.h"
#include "components/origin_gating/core/gating_decision.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_service_impl.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/safe_browsing/buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/event_dispatcher.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/interstitials/user_interaction_observer.h"
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
  kRequireHttpsOrHttpOrNtp,
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
    case ActorCustomPredicate::kRequireHttpsOrHttpOrNtp:
      return "actor_require_https_or_http_or_ntp";
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
  explicit OriginGatingDecisionContext(bool is_dry_run = false)
      : is_dry_run(is_dry_run) {}
  ~OriginGatingDecisionContext() override = default;

  // Whether the destination origin is considered sensitive. Nullopt until
  // optimization guide has been queried.
  std::optional<bool> destination_is_sensitive;
  // When true, the evaluation must not prompt the user or mutate the origin
  // gating cache.
  const bool is_dry_run = false;
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
      base::WeakPtr<content::WebContents> web_contents,
      bool is_dry_run = false)
      : OriginGatingDecisionContext(is_dry_run),
        web_contents(std::move(web_contents)) {}
  ~PageActionGatingContext() override = default;

  base::WeakPtr<content::WebContents> web_contents;
};

// Blocks acting on a tab whose primary main frame is showing an error document.
origin_gating::Decision BlockTabErrorDocument(
    origin_gating::GatingDecisionContext* context,
    const GateableEvent& event) {
  content::WebContents* web_contents =
      static_cast<const PageActionGatingContext*>(context)->web_contents.get();
  if (web_contents && web_contents->GetPrimaryMainFrame()->IsErrorDocument()) {
    return origin_gating::Decision::kBlocked;
  }
  return origin_gating::Decision::kNoDecision;
}

// Allow HTTP/HTTPS schemes, as well as NTP origins.
origin_gating::Decision RequireHttpsOrHttpOrNtp(
    origin_gating::GatingDecisionContext* context,
    const GateableEvent& event) {
  const GURL& destination = event.destination();
  if (destination.SchemeIsHTTPOrHTTPS()) {
    return origin_gating::Decision::kNoDecision;
  }
  if (search::IsWebUINewTabPageOrigin(destination)) {
    return origin_gating::Decision::kNoDecision;
  }
  return origin_gating::Decision::kBlocked;
}

// Blocks acting on a tab that has a pending SafeBrowsing delayed warning. The
// SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing warnings
// until user interaction; such a page has a user interaction observer attached.
origin_gating::Decision BlockSafeBrowsingWarningIfSafetyChecksEnabled(
    origin_gating::GatingDecisionContext* context,
    const GateableEvent& event) {
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
    const GateableEvent& event) {
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
      base::BindRepeating(
          [](origin_gating::GatingDecisionContext*, const GateableEvent& event,
             base::OnceCallback<void(origin_gating::Decision)> callback) {
            const GURL& destination_url = event.destination();
            const GURL& effective_source =
                event.source() && !event.source()->is_empty() ? *event.source()
                                                              : destination_url;
            SafetyListManager::GetInstance()->Find(
                effective_source, destination_url,
                base::BindOnce([](SafetyListManager::Decision decision) {
                  switch (decision) {
                    case SafetyListManager::Decision::kNone:
                      return origin_gating::Decision::kNoDecision;
                    case SafetyListManager::Decision::kAllow:
                      return origin_gating::Decision::kAllowed;
                    case SafetyListManager::Decision::kBlock:
                      return origin_gating::Decision::kBlocked;
                  }
                }).Then(std::move(callback)));
          }),
      ActorCustomPredicate::kSafetyList);
}

bool IsUrlSensitiveFromCommandLine(const GURL& url) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kActorSensitiveSites)) {
    return false;
  }

  net::SchemefulSite target_site(url);
  if (target_site.opaque()) {
    return false;
  }

  return std::ranges::any_of(
      base::SplitStringPiece(
          command_line->GetSwitchValueASCII(switches::kActorSensitiveSites),
          ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY),
      [&](std::string_view entry) {
        return target_site == net::SchemefulSite(GURL(entry));
      });
}

// Returns whether the given `url` is considered non-sensitive. Caches the
// result in `context`, for future queries.
void IsNonSensitiveUrl(Profile* profile,
                       origin_gating::GatingDecisionContext* context,
                       const GURL& url,
                       base::OnceCallback<void(bool)> callback) {
  CHECK_NE(context, nullptr);
  auto* decision_context = static_cast<OriginGatingDecisionContext*>(context);

  const bool is_sensitive_localhost =
      base::FeatureList::IsEnabled(kGlicActorLocalhostIsSensitive) &&
      net::IsLocalhost(url);
  if (is_sensitive_localhost || IsUrlSensitiveFromCommandLine(url)) {
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
    const GateableEvent& event,
    base::OnceCallback<void(origin_gating::Decision)> callback) {
  IsNonSensitiveUrl(profile, context, event.destination(),
                    base::BindOnce([](bool not_sensitive) {
                      return not_sensitive
                                 ? origin_gating::Decision::kNoDecision
                                 : origin_gating::Decision::kBlocked;
                    }).Then(std::move(callback)));
}

void BlockSensitiveUrlWhenNavigationGatingDisabled(
    Profile* profile,
    origin_gating::GatingDecisionContext* context,
    const GateableEvent& event,
    base::OnceCallback<void(origin_gating::Decision)> callback) {
  if (IsNavigationGatingEnabled()) {
    std::move(callback).Run(origin_gating::Decision::kNoDecision);
    return;
  }

  BlockSensitiveUrl(profile, context, event, std::move(callback));
}

origin_gating::Decision BlockLookalikeUrl(
    Profile* profile,
    origin_gating::GatingDecisionContext* context,
    const GateableEvent& event) {
  auto* lookalike_service = LookalikeUrlServiceFactory::GetForProfile(profile);
  LookalikeUrlService::LookalikeUrlCheckResult lookalike_result =
      lookalike_service->CheckUrlForLookalikes(
          event.destination(), lookalike_service->GetLatestEngagedSites(),
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
    const GateableEvent& event) {
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
    const GateableEvent& event) {
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
    std::vector<ToolResultWithLatencyInfo> action_results,
    TabObservationStrategy observation_strategy) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(action_results),
                                std::move(observation_strategy)));
}

struct BaseDispatcherToolResult : public ToolResultData {
  explicit BaseDispatcherToolResult(
      std::vector<ToolResultWithLatencyInfo> results)
      : results(std::move(results)) {}
  BaseDispatcherToolResult(const BaseDispatcherToolResult&) = default;
  BaseDispatcherToolResult& operator=(const BaseDispatcherToolResult&) = delete;
  BaseDispatcherToolResult(BaseDispatcherToolResult&&) noexcept = default;
  BaseDispatcherToolResult& operator=(BaseDispatcherToolResult&&) noexcept =
      default;
  ~BaseDispatcherToolResult() override = default;

  // ToolResultData:
  base::DictValue ToDict() const override { NOTREACHED(); }
  std::unique_ptr<ToolResultData> Clone() const override {
    return std::make_unique<BaseDispatcherToolResult>(*this);
  }

  std::vector<ToolResultWithLatencyInfo> results;
};

class BaseDispatcher : public ExecutionEngineDispatcher {
 public:
  explicit BaseDispatcher(std::vector<std::unique_ptr<ToolRequest>> requests)
      : requests_(std::move(requests)) {
    CHECK(!requests_.empty());
    for (const auto& request : requests_) {
      CHECK(request);
    }
  }
  ~BaseDispatcher() override = default;

  // ExecutionEngineDispatcher:
  NextAction GetNext(ToolDelegate& tool_delegate) override {
    CHECK(!awaiting_tool_completion_);
    if ((!results_.empty() && !results_.back().tool_result.Ok()) ||
        next_index_ >= requests_.size()) {
      CHECK(!results_.empty());
      const base::TimeTicks start = results_.front().start_time;
      const base::TimeTicks end = std::max(start, results_.back().end_time);
      // Always return success as any errors will be provided in the vector of
      // results inside the ToolResult's data.
      return ToolResultWithLatencyInfo(
          start, end,
          ToolResult::Success(BaseDispatcherToolResult(std::move(results_))));
    }
    std::unique_ptr<ToolRequest> next = std::move(requests_[next_index_++]);
    last_request_was_followup_ = next->IsFollowup();
    awaiting_tool_completion_ = true;
    return next;
  }

  void OnToolCompleted(ToolResultWithLatencyInfo result) override {
    CHECK(awaiting_tool_completion_);
    awaiting_tool_completion_ = false;
    if (last_request_was_followup_) {
      CHECK(!results_.empty());
      MergeFollowupResult(results_.back(), std::move(result));
      last_request_was_followup_ = false;
    } else {
      results_.push_back(std::move(result));
    }
  }

  base::span<const std::unique_ptr<ToolRequest>> PeekRemainingRequests() const {
    return base::span(requests_).subspan(next_index_);
  }

  // TODO(b/555808355): Remove the notion of a followup action once composite
  // tools have been implemented.
  void EnqueueFollowupAction(std::unique_ptr<ToolRequest> action) {
    requests_.insert(requests_.begin() + next_index_, std::move(action));
  }

 private:
  std::vector<std::unique_ptr<ToolRequest>> requests_;
  size_t next_index_ = 0;
  bool awaiting_tool_completion_ = false;
  bool last_request_was_followup_ = false;
  std::vector<ToolResultWithLatencyInfo> results_;
};

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

// Returns `url_to_commit` if its origin/precursor already matches
// `origin_to_commit`'s tuple or precursor (preserving path/query for URL-level
// policy checks), or if `origin_to_commit` has no tuple/precursor (e.g.
// browser-initiated about:blank).  Otherwise (e.g. renderer-initiated
// about:blank or about:srcdoc that inherited a tuple/precursor origin), returns
// the precursor/tuple origin's GURL so gating evaluates the true security
// principal rather than "about:blank".
GURL GetEffectiveUrlForGating(const GURL& url_to_commit,
                              const url::Origin& origin_to_commit) {
  const url::Origin effective_origin =
      OriginOrPrecursorIfOpaque(origin_to_commit);
  if (effective_origin.opaque()) {
    return url_to_commit;
  }
  if (OriginOrPrecursorIfOpaque(url::Origin::Create(url_to_commit)) ==
      effective_origin) {
    return url_to_commit;
  }
  return effective_origin.GetURL();
}

ExecutionEngine::GatingDecision MapGatingDecisionToEngineDecision(
    const origin_gating::GatingDecision& decision) {
  switch (decision.attribution.type()) {
    case origin_gating::DecisionAttribution::Type::kDecisionSource:
      switch (decision.attribution.Source()) {
        case DecisionSource::kAllowSameOrigin:
          return ExecutionEngine::GatingDecision::kAllowSameOrigin;
        case DecisionSource::kBlockByTaskPolicyConfig:
          return ExecutionEngine::GatingDecision::kBlockByContainerConfig;
        case DecisionSource::kAllowByTaskPolicyConfig:
          return ExecutionEngine::GatingDecision::kAllowByContainerConfig;
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
        case ActorCustomPredicate::kRequireHttpsOrHttpOrNtp:
          // Unreachable because `MapGatingDecisionToEngineDecision` is only
          // called for navigation response events, while
          // `kRequireHttpsOrHttpOrNtp` only gates navigation requests.
          NOTREACHED();
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
        case DecisionSource::kBlockByTaskPolicyConfig:
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
        case origin_gating::DecisionSource::kAllowByTaskPolicyConfig:
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
        case ActorCustomPredicate::kRequireHttpsOrHttpOrNtp:
          return ProfileIOData::IsHandledURL(url)
                     ? MayActOnUrlBlockReason::kWrongScheme
                     : MayActOnUrlBlockReason::kExternalProtocol;
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
    GateableEvent::Type event_type,
    std::unique_ptr<origin_gating::GatingDecisionContext> context,
    origin_gating::GatingDecision decision) {
  journal_entry->EndEntry(
      JournalDetailsBuilder()
          .Add("origin", url::Origin::Create(url).Serialize())
          .Add("event", origin_gating::GateableEventTypeToString(event_type))
          .Add("decision", decision.is_allowed ? "allowed" : "blocked")
          .Add("attribution", DecisionAttributionToString(decision.attribution))
          .Build());

  return MapGatingDecisionToBlockReason(decision, url);
}

origin_gating::OriginGatingConfiguration::CacheScope
GetOriginGatingCacheScope() {
  using CacheScope = origin_gating::OriginGatingConfiguration::CacheScope;
  return kGlicNavigationGatingUseSiteNotOrigin.Get() ? CacheScope::kSite
                                                     : CacheScope::kOrigin;
}

struct WouldBeAllowed {};
struct WouldRequireUserConfirmation {};
struct WouldBlock {
  MayActOnUrlBlockReason reason;
};

// Outcome of dry-run page-action gating for a single origin, ordered from least
// to most restrictive so that `index()` increases with restrictiveness.
using SubframeDryRunResult =
    std::variant<WouldBeAllowed, WouldRequireUserConfirmation, WouldBlock>;

// Records the most restrictive of `results`, since that is what would apply to
// the action if every frame were gated.
void RecordMostRestrictiveSubframeDryRunResult(
    std::vector<SubframeDryRunResult> results) {
  CHECK(!results.empty());
  const SubframeDryRunResult& most_restrictive =
      *std::ranges::max_element(results, {}, &SubframeDryRunResult::index);
  std::visit(
      absl::Overload{
          [](WouldBeAllowed) {
            RecordSubframeGatingDryRunOutcome(
                SubframeGatingDryRunOutcome::kWouldBeAllowed);
          },
          [](WouldRequireUserConfirmation) {
            RecordSubframeGatingDryRunOutcome(
                SubframeGatingDryRunOutcome::kWouldRequireUserConfirmation);
          },
          [](WouldBlock block) {
            RecordSubframeGatingDryRunOutcome(
                SubframeGatingDryRunOutcome::kWouldBlock);
            RecordSubframeGatingDryRunBlockReason(block.reason);
          },
      },
      most_restrictive);
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

std::vector<origin_gating::PredicateConfiguration> CreateOriginGatingPredicates(
    ActorTask& task,
    ExecutionEngine::AllowedSchemes allowed_schemes) {
  std::vector<origin_gating::PredicateConfiguration> predicates({
      {CustomPredicate(base::BindRepeating(&BlockTabErrorDocument),
                       ActorCustomPredicate::kTabErrorDocument),
       {GateableEvent::kPageAction}},
      {CustomPredicate(
           base::BindRepeating(&BlockSafeBrowsingWarningIfSafetyChecksEnabled),
           ActorCustomPredicate::kTabSafeBrowsingObserver),
       {GateableEvent::kPageAction}},
      // If localhost should be treated as sensitive, only
      // auto-allow for navigation requests.
      {DecisionSource::kAllowHttpLocalhost,
       base::FeatureList::IsEnabled(kGlicActorLocalhostIsSensitive)
           ? GateableEventSet{GateableEvent::kNavigationRequest}
           : kRequestsAndPageActions},
      {DecisionSource::kAllowAboutBlank, kRequestsAndPageActions},
  });

  if (allowed_schemes ==
      ExecutionEngine::AllowedSchemes::kRequireHttpsOrHttpOrNtp) {
    predicates.push_back(
        {CustomPredicate(base::BindRepeating(&RequireHttpsOrHttpOrNtp),
                         ActorCustomPredicate::kRequireHttpsOrHttpOrNtp),
         {GateableEvent::kNavigationRequest}});
  } else {
    // Allow insecure HTTP for navigation requests, as in
    // practice sites may have HTTP links that will get upgraded.
    // Rejecting HTTP URLs before this can happen would be too
    // serious of an impediment.
    predicates.push_back({DecisionSource::kRequireHttpsOrHttp,
                          {GateableEvent::kNavigationRequest}});
  }

  predicates.append_range(std::initializer_list<
                          origin_gating::PredicateConfiguration>{
      {DecisionSource::kRequireHttpsOrLocalhost, {GateableEvent::kPageAction}},
      {DecisionSource::kForbidNonLocalhostIpAddress, kRequestsAndPageActions},
      {CustomPredicate(base::BindRepeating(&AllowIfSafetyChecksDisabled),
                       ActorCustomPredicate::kSafetyChecksDisabled),
       origin_gating::GateableEventSet::All()},
      {CustomPredicate(
           base::BindRepeating(&BlockIfSafeBrowsingDisabled, task.GetProfile()),
           ActorCustomPredicate::kSafeBrowsing),
       kRequestsAndPageActions},
      {CustomPredicate(base::BindRepeating(&BlockDangerousMimeType),
                       ActorCustomPredicate::kDangerousMimeType),
       {GateableEvent::kNavigationResponse}},
      {DecisionSource::kEnterprisePolicy,
       {GateableEvent::kNavigationResponse, GateableEvent::kPageAction}},
      {CustomPredicate(
           base::BindRepeating(&BlockLookalikeUrl, task.GetProfile()),
           ActorCustomPredicate::kLookalikeUrl),
       kRequestsAndPageActions},
      {DecisionSource::kBlockByTaskPolicyConfig,
       {GateableEvent::kNavigationResponse, GateableEvent::kPageAction}},
      {CreateSafetyListPredicate(),
       {GateableEvent::kNavigationResponse, GateableEvent::kPageAction}},
      {DecisionSource::kCacheWithUserConfirmation, GateableEventSet::All()},
      {DecisionSource::kAllowSameOrigin, {GateableEvent::kNavigationResponse}},
      {CustomPredicate(
           base::BindRepeating(&BlockSensitiveUrlWhenNavigationGatingDisabled,
                               task.GetProfile()),
           ActorCustomPredicate::kSensitiveUrl),
       {GateableEvent::kNavigationRequest}},
      {DecisionSource::kAllowByTaskPolicyConfig,
       {GateableEvent::kNavigationResponse, GateableEvent::kPageAction}},
      {DecisionSource::kCacheWithoutUserConfirmation,
       {GateableEvent::kNavigationResponse}},
  });

  return predicates;
}

// Protected constructor without pass key to allow subclassing.
ExecutionEngine::ExecutionEngine(ActorTask& owner_task,
                                 AllowedSchemes allowed_schemes)
    : ExecutionEngine(base::PassKey<ExecutionEngine>(),
                      owner_task,
                      allowed_schemes) {}

ExecutionEngine::ExecutionEngine(base::PassKey<ExecutionEngine>,
                                 ActorTask& owner_task,
                                 AllowedSchemes allowed_schemes)
    : task_(owner_task),
      allowed_schemes_(allowed_schemes),
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
              task_->id())) {
  TRACE_EVENT0("actor", "ExecutionEngine::ExecutionEngine");
  origin_gating_registration_ =
      GetOriginGatingService().CreateAndRegisterChecker(
          weak_ptr_factory_.GetWeakPtr(),
          origin_gating::OriginGatingConfiguration(
              CreateOriginGatingPredicates(*task_, allowed_schemes_),
              GetOriginGatingCacheScope()));
}

// static
std::unique_ptr<ExecutionEngine> ExecutionEngine::Create(
    ActorTask& owner_task,
    AllowedSchemes allowed_schemes) {
  if (!GetFactoryFunctionForTesting().is_null()) {
    return GetFactoryFunctionForTesting().Run(owner_task);
  }

  return std::make_unique<ExecutionEngine>(base::PassKey<ExecutionEngine>(),
                                           owner_task, allowed_schemes);
}

ExecutionEngine::~ExecutionEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  origin_gating::OriginGatingCache::SizeMetrics metrics =
      GetOriginGatingChecker().GetCacheSizeMetrics();
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
           {State::kStartAction, State::kToolCreateAndVerify,
            State::kComplete}},
          {State::kToolCreateAndVerify,
           {State::kStartAction, State::kUiPreInvoke, State::kComplete}},
          {State::kUiPreInvoke,
           {State::kStartAction, State::kToolInvoke, State::kComplete}},
          {State::kToolInvoke,
           {State::kStartAction, State::kUiPostInvoke, State::kComplete}},
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
  const GURL destination_url = GetEffectiveUrlForGating(
      navigation_handle.GetURL(),
      url::Origin::Resolve(
          navigation_handle.GetURL(),
          navigation_handle.GetInitiatorOrigin().value_or(url::Origin())));
  const url::Origin destination_origin = url::Origin::Create(destination_url);
  GateableEvent event(origin_gating::NavigationResponseEvent{
      .source = source_origin.GetURL(), .destination = destination_url});
  auto wrapped_callback = TrackPendingNavigation(
      pending_navigation_cancellations_, std::move(callback),
      /*block_reason_if_dropped=*/MayActOnUrlBlockReason::kTaskCancelled);
  GateableEvent::Type event_type = event.type();
  GetOriginGatingChecker().ComputeGatingDecision(
      std::make_unique<NavigationResponseContext>(
          GetPrimaryMainFrame(navigation_handle)->GetPageUkmSourceId(),
          navigation_handle.IsInPrerenderedMainFrame(), std::move(timer),
          ExtractMimeType(navigation_handle)),
      std::move(event),
      base::BindOnce(
          &ExecutionEngine::OnComputedGatingDecision, GetWeakPtr(),
          std::move(wrapped_callback),
          journal_->CreatePendingAsyncEntry(destination_url, task_->id(),
                                            MakeBrowserTrackUUID(task_->id()),
                                            "OriginGatingDecision", {}),
          source_origin, destination_origin, state_,
          navigation_handle.GetInitiatorOrigin(), event_type));
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
    GateableEvent::Type event_type,
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
          .Add("event", origin_gating::GateableEventTypeToString(event_type))
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
    const GateableEvent& event,
    DoesOriginRequireUserConfirmationCallback callback) const {
  // Navigation requests never prompt the user.
  if (event.is_navigation_request()) {
    std::move(callback).Run(/*requires_user_confirmation=*/false);
    return;
  }

  IsNonSensitiveUrl(task_->GetProfile(), context, event.destination(),
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
    const GateableEvent& event,
    bool requires_user_confirmation,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Navigation requests fail open.
  if (event.is_navigation_request()) {
    std::move(callback).Run(
        {.is_allowed = true, .did_prompt_user = false, .bypass_cache = true});
    return;
  }

  auto* decision_context = static_cast<OriginGatingDecisionContext*>(context);
  CHECK(decision_context);
  if (decision_context->is_dry_run) {
    std::move(callback).Run({.is_allowed = !requires_user_confirmation,
                             .did_prompt_user = false,
                             .bypass_cache = true});
    return;
  }

  NavigationResponseContext* navigation_response_context =
      event.is_navigation_response()
          ? static_cast<NavigationResponseContext*>(context)
          : nullptr;
  url::Origin destination_origin = url::Origin::Create(event.destination());
  if (navigation_response_context && navigation_response_context->skip_prompt) {
    std::move(callback).Run({.is_allowed = false, .did_prompt_user = false});
    return;
  }

  if (!requires_user_confirmation) {
    if (event.is_page_action()) {
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
                     GetActSequenceWeakPtr(), destination, ukm_source_id,
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
                     GetActSequenceWeakPtr(), destination,
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

  CancelOngoingAct(takeover_response_code);

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
  return state() == State::kToolInvoke && !is_settling_pending_observation_ &&
         GetInProgressToolRequest().RequiresOpeningWebContents();
}

base::WeakPtr<ExecutionEngine> ExecutionEngine::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ExecutionEngine::CancelOngoingAct(mojom::ActionResultCode reason) {
  TRACE_EVENT0("actor", "ExecutionEngine::CancelOngoingAct");
  deferred_finish_tool_invoke_.Reset();
  is_settling_pending_observation_ = false;
  CHECK(tool_controller_);
  tool_controller_->Cancel();

  if (!IsExecutingActCall()) {
    return;
  }

  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeTicks start =
      current_request_start_time_.is_null() ? now : current_request_start_time_;

  ToolResultWithLatencyInfo cancel_result(start, std::max(start, now),
                                          MakeResult(reason));
  MaybeRecordActionResultCodeAfterSkippedWait(cancel_result.tool_result);
  // We might have already completed the dispatcher but be waiting on a pending
  // observation. ToolController::Cancel above will have reset that observation,
  // so re-call CompleteActCall with the cancel result added.
  if (completed_results_.has_value()) {
    CHECK(!executing_request_);
    std::vector<ToolResultWithLatencyInfo> results =
        std::move(*completed_results_);
    completed_results_.reset();
    // TODO(b/555808355): This preserves existing behavior, but is inconsistent
    // because we overwrite an existing result if it's a failure and append if
    // all results succeeded. We should always overwrite the last result.
    if (!results.empty() && !results.back().tool_result.Ok()) {
      results.back() = std::move(cancel_result);
    } else {
      results.push_back(std::move(cancel_result));
    }
    CompleteActCall(std::move(results));
    return;
  }

  // Unwind any nested dispatchers so cancellation terminates on the base
  // dispatcher rather than allowing a composite dispatcher to recover.
  while (IsExecutingNestedDispatcher()) {
    dispatchers_.pop();
  }
  CHECK(IsExecutingBaseDispatcher());
  CurrentDispatcher().OnToolCompleted(std::move(cancel_result));
  ProcessNextFromDispatcher();
}

void ExecutionEngine::PauseOngoingAct() {
  TRACE_EVENT0("actor", "ExecutionEngine::PauseOngoingAct");
  if (tool_controller_) {
    tool_controller_->Pause();
  }
}

void ExecutionEngine::FailCurrentTool(mojom::ActionResultCode reason) {
  TRACE_EVENT0("actor", "ExecutionEngine::FailCurrentTool");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(reason, mojom::ActionResultCode::kOk);
  if (state_ != State::kToolInvoke || is_settling_pending_observation_) {
    return;
  }

  external_tool_failure_reason_ = reason;
}

void ExecutionEngine::Act(std::vector<std::unique_ptr<ToolRequest>>&& requests,
                          base::OnceClosure on_tools_accepted,
                          ActorTask::ActCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::Act");
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(!requests.empty());
  CHECK(deferred_finish_tool_invoke_.is_null());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(task_->GetState(), ActorTask::State::kActing);

  {
    JournalDetailsBuilder journal_details;
    for (size_t i = 0; i < requests.size(); ++i) {
      journal_details.Add(absl::StrFormat("Actions[%d]", i),
                          requests[i]->JournalEvent());
    }
    journal_->Log(GURL::EmptyGURL(), task_->id(), "ExecutionEngine::Act",
                  std::move(journal_details).Build());
  }

  if (IsExecutingActCall()) {
    journal_->Log(
        requests[0]->GetURLForJournal(), task_->id(), "Act Failed",
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

  if (on_tools_accepted) {
    // `on_tools_accepted` may stop or pause the task, or destroy it along
    // with this engine. It runs before this engine takes `requests`, so doing
    // that doesn't need to cancel requests that haven't started. In all those
    // cases, `requests` must not start.
    base::WeakPtr<ExecutionEngine> weak_this = weak_ptr_factory_.GetWeakPtr();
    std::move(on_tools_accepted).Run();
    if (!weak_this) {
      return;
    }
    if (task_->GetState() != ActorTask::State::kActing) {
      PostTaskForActCallback(
          std::move(callback),
          MakeResultVector(task_->IsUnderUserControl()
                               ? mojom::ActionResultCode::kTaskPaused
                               : mojom::ActionResultCode::kTaskWentAway),
          TabObservationStrategy());
      return;
    }
  }

  act_callback_ = std::move(callback);
  observation_strategy_ = TabObservationStrategy();

  dispatchers_.push(std::make_unique<BaseDispatcher>(std::move(requests)));
  ProcessNextFromDispatcher();
}

ExecutionEngineDispatcher& ExecutionEngine::CurrentDispatcher() {
  CHECK(!dispatchers_.empty());
  return *dispatchers_.top();
}

const ExecutionEngineDispatcher& ExecutionEngine::CurrentDispatcher() const {
  CHECK(!dispatchers_.empty());
  return *dispatchers_.top();
}

bool ExecutionEngine::IsExecutingBaseDispatcher() const {
  return dispatchers_.size() == 1;
}

bool ExecutionEngine::IsExecutingNestedDispatcher() const {
  return dispatchers_.size() > 1;
}

void ExecutionEngine::ProcessNextFromDispatcher() {
  executing_request_.reset();
  external_tool_failure_reason_.reset();

  ExecutionEngineDispatcher::NextAction next =
      CurrentDispatcher().GetNext(*this);
  if (auto* result = std::get_if<ToolResultWithLatencyInfo>(&next)) {
    dispatchers_.pop();
    if (dispatchers_.empty()) {
      CHECK(result->tool_result.data());
      CompleteActCall(
          result->tool_result.data()->As<BaseDispatcherToolResult>()->results);
      return;
    }

    CurrentDispatcher().OnToolCompleted(std::move(*result));
    ProcessNextFromDispatcher();
    return;
  }

  executing_request_ = std::move(std::get<std::unique_ptr<ToolRequest>>(next));
  CHECK(executing_request_);

  if (IsNavigationGatingEnabled() && kGlicAllowImplicitToolOriginGrants.Get()) {
    if (std::optional<url::Origin> maybe_origin =
            executing_request_->AssociatedOriginGrant();
        maybe_origin) {
      GetOriginGatingChecker().AllowNavigationTo(maybe_origin.value(),
                                                 /*is_user_confirmed=*/false);
    }
  }

  if (std::unique_ptr<ExecutionEngineDispatcher> dispatcher =
          executing_request_->CreateDispatcher()) {
    dispatchers_.push(std::move(dispatcher));
    ProcessNextFromDispatcher();
    return;
  }

  StartExecutingNewRequest();
}

void ExecutionEngine::StartExecutingNewRequest() {
  TRACE_EVENT0("actor", "ExecutionEngine::StartExecutingNewRequest");
  CHECK(executing_request_);

  // TODO(b/555808355): Consider how observation deferral should work with
  // followup/composite tools.
  if (tool_controller_->HasPendingObservation() &&
      (GetInProgressToolRequest().IsFollowup() ||
       IsExecutingNestedDispatcher() ||
       tool_controller_->ShouldSettlePendingObservationBefore(
           GetInProgressToolRequest()))) {
    tool_controller_->SettlePendingObservation(base::BindOnce(
        &ExecutionEngine::StartExecutingNewRequest, GetActSequenceWeakPtr()));
    return;
  }
  // Captured at ToolRequest start: by the time the ToolRequest finishes, the
  // pending wait may have been settled (early termination) or run (bundle end).
  tool_request_started_after_skipped_wait_ =
      tool_controller_->HasPendingStabilization();

  SetState(State::kStartAction);
  if (!GetInProgressToolRequest().IsFollowup()) {
    current_request_start_time_ = base::TimeTicks::Now();
  }

  // TODO(b/467984847): ActorTask::AddTab isn't the best way to track a crashed
  // tab here. We should refactor this to be more explicit.
  if (ActorSurface* actor_surface =
          GetSurfaceForValidation(GetInProgressToolRequest());
      actor_surface &&
      base::FeatureList::IsEnabled(kActorReloadCrashedTabBeforeAct)) {
    content::WebContents* contents = actor_surface->GetWebContents();
    CHECK(contents);
    if (contents->IsCrashed() &&
        GetInProgressToolRequest().ActsOnWebContents()) {
      GetJournal().Log(
          contents->GetLastCommittedURL(), task_->id(),
          "ExecutionEngine::StartExecutingNewRequest",
          JournalDetailsBuilder().AddError("Renderer crashed").Build());
      if (GetInProgressToolRequest().GetActorSurfaceHandle().is_null()) {
        task_->ObserveActorSurfaceOnce(
            GetInProgressToolRequest().GetActorSurfaceForValidation());
      } else {
        CHECK(task_->IsActingOnActorSurface(actor_surface->GetHandle()));
      }
      ReportResultAndStartNext(
          ToolResult::FromDeprecatedActionResult(MakeResult(
              mojom::ActionResultCode::kRendererCrashed,
              /*requires_page_stabilization=*/false, "Renderer crashed.")));
      return;
    }
  }

  if (GetInProgressToolRequest().RequiresUrlCheckInCurrentTab()) {
    SafetyChecksForExecutingTool();
  } else {
    InvokeCurrentToolRequest();
  }
}

void ExecutionEngine::ReportResultAndStartNext(
    ToolResult tool_result,
    std::vector<InterceptedJavaScriptDialog> javascript_dialogs) {
  CHECK(executing_request_);

  MaybeRecordActionResultCodeAfterSkippedWait(tool_result);

  // Clear the tool controller so the next request starts with a fresh state.
  tool_controller_->ResetActiveTool();

  const base::TimeTicks end_time = base::TimeTicks::Now();
  const base::TimeTicks start_time = current_request_start_time_.is_null()
                                         ? end_time
                                         : current_request_start_time_;
  std::string tool_name(executing_request_->Name());

  CurrentDispatcher().OnToolCompleted(ToolResultWithLatencyInfo(
      start_time, std::max(start_time, end_time), std::move(tool_result),
      tool_name, std::move(javascript_dialogs)));
  ProcessNextFromDispatcher();
}

void ExecutionEngine::SafetyChecksForExecutingTool() {
  TRACE_EVENT0("actor", "ExecutionEngine::SafetyChecksForExecutingTool");
  ActorSurface* actor_surface =
      GetSurfaceForValidation(GetInProgressToolRequest());

  if (!actor_surface) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("The tab is no longer present")
                      .Build());
    ReportResultAndStartNext(ToolResult::FromDeprecatedActionResult(
        MakeResult(mojom::ActionResultCode::kTabWentAway,
                   /*requires_page_stabilization=*/false,
                   "The tab is no longer present.")));
    return;
  }

  // TODO(mcnee): Add UMA for the outcomes.
  content::WebContents& web_contents = *(actor_surface->GetWebContents());
  content::RenderFrameHost* main_frame = web_contents.GetPrimaryMainFrame();
  const GURL url = GetEffectiveUrlForGating(
      main_frame->GetLastCommittedURL(), main_frame->GetLastCommittedOrigin());
  GetOriginGatingChecker().ComputeGatingDecision(
      std::make_unique<PageActionGatingContext>(web_contents.GetWeakPtr()),
      GateableEvent(origin_gating::PageActionEvent{
          .destination = url,
          .tool = GetClientToolForRequest(GetInProgressToolRequest())}),
      base::BindOnce(&ResolveGatingDecision,
                     journal_->CreatePendingAsyncEntry(
                         url, task_->id(), MakeBrowserTrackUUID(task_->id()),
                         "OriginGatingDecision", {}),
                     url, GateableEvent::kPageAction)
          .Then(base::BindOnce([](MayActOnUrlBlockReason block_reason) {
            return BlockReasonToResultCode(block_reason,
                                           /*for_navigation=*/false);
          }))
          .Then(base::BindOnce(&ExecutionEngine::DidFinishAsyncSafetyChecks,
                               GetActSequenceWeakPtr(),
                               main_frame->GetLastCommittedOrigin())));
}

void ExecutionEngine::DidFinishAsyncSafetyChecks(
    const url::Origin& committed_origin,
    mojom::ActionResultCode result_code) {
  TRACE_EVENT0("actor", "ExecutionEngine::DidFinishAsyncSafetyChecks");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(executing_request_);

  ActorSurface* actor_surface =
      GetSurfaceForValidation(GetInProgressToolRequest());
  if (!actor_surface) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("The tab is no longer present")
                      .Build());

    ReportResultAndStartNext(ToolResult::FromDeprecatedActionResult(
        MakeResult(mojom::ActionResultCode::kTabWentAway,
                   /*requires_page_stabilization=*/false,
                   "The tab is no longer present.")));
    return;
  }

  auto ensure_surface_observed = [&]() {
    const ToolRequest& action = GetInProgressToolRequest();
    if (action.GetActorSurfaceHandle().is_null()) {
      task_->ObserveActorSurfaceOnce(action.GetActorSurfaceForValidation());
    } else {
      CHECK(task_->IsActingOnActorSurface(action.GetActorSurfaceHandle()));
    }
  };

  TaskId task_id = task_->id();
  if (!committed_origin.IsSameOriginWith(actor_surface->GetWebContents()
                                             ->GetPrimaryMainFrame()
                                             ->GetLastCommittedOrigin())) {
    // A cross-origin navigation occurred before we got permission. The result
    // is no longer applicable. For now just fail.
    // TODO(mcnee): Handle this gracefully.
    journal_->Log(GetInProgressToolRequest().GetURLForJournal(), task_id,
                  "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("Acting after cross-origin navigation occurred")
                      .Build());
    ensure_surface_observed();
    ReportResultAndStartNext(ToolResult::FromDeprecatedActionResult(
        MakeResult(mojom::ActionResultCode::kCrossOriginNavigation,
                   /*requires_page_stabilization=*/false,
                   "Acting after cross-origin navigation occurred")));
    return;
  }

  if (!IsOk(result_code)) {
    journal_->Log(
        GetInProgressToolRequest().GetURLForJournal(), task_id, "Act Failed",
        JournalDetailsBuilder().AddError("URL blocked for actions").Build());
    ensure_surface_observed();
    ReportResultAndStartNext(ToolResult::FromDeprecatedActionResult(MakeResult(
        result_code,
        /*requires_page_stabilization=*/false, "URL blocked for actions")));
    return;
  }

  InvokeCurrentToolRequest();
}

void ExecutionEngine::InvokeCurrentToolRequest() {
  TRACE_EVENT0("actor", "ExecutionEngine::InvokeCurrentToolRequest");
  CHECK_EQ(state_, State::kStartAction, base::NotFatalUntil::M161);
  CHECK(executing_request_);
  CHECK(tool_controller_);

  SetState(State::kToolCreateAndVerify);
  tool_controller_->CreateToolAndValidate(
      GetInProgressToolRequest(),
      base::BindOnce(&ExecutionEngine::PostToolCreate,
                     GetActSequenceWeakPtr()));
}

void ExecutionEngine::PostToolCreate(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::PostToolCreate");
  if (!IsOk(*result)) {
    ReportResultAndStartNext(
        ToolResult::FromDeprecatedActionResult(std::move(result)));
    return;
  }
  ActorTaskDelegate* delegate = task_->delegate().get();
  if (!delegate) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ExecutionEngine::PostDelegateToolValidation,
                                  GetActSequenceWeakPtr(),
                                  /*journal_entry=*/nullptr, MakeOkResult()));
    return;
  }
  const ToolRequest& action = GetInProgressToolRequest();
  auto journal_entry = journal_->CreatePendingAsyncEntry(
      action.GetURLForJournal(), task_->id(), MakeBrowserTrackUUID(task_->id()),
      "DidToolValidation",
      JournalDetailsBuilder().Add("tool", action.JournalEvent()).Build());
  delegate->DidToolValidation(
      task_->id(), action,
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&ExecutionEngine::PostDelegateToolValidation,
                         GetActSequenceWeakPtr(), std::move(journal_entry))));
}

void ExecutionEngine::PostDelegateToolValidation(
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::PostDelegateToolValidation");
  CHECK_EQ(state_, State::kToolCreateAndVerify, base::NotFatalUntil::M161);
  if (journal_entry) {
    journal_entry->EndEntry(
        JournalDetailsBuilder().Add("result", ToDebugString(*result)).Build());
  }
  if (!IsOk(*result)) {
    tool_controller_->Cancel();
    ReportResultAndStartNext(
        ToolResult::FromDeprecatedActionResult(std::move(result)));
    return;
  }
  SetState(State::kUiPreInvoke);
  GetUiEventDispatcher().OnPreTool(
      GetInProgressToolRequest(),
      base::BindOnce(&ExecutionEngine::FinishedUiPreInvoke,
                     GetActSequenceWeakPtr()));
}

void ExecutionEngine::FinishedUiPreInvoke(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedUiPreInvoke");
  CHECK_EQ(state_, State::kUiPreInvoke, base::NotFatalUntil::M161);
  if (!IsOk(*result)) {
    ReportResultAndStartNext(
        ToolResult::FromDeprecatedActionResult(std::move(result)));
    return;
  }

  // Cache the navigation ID before invoking the tool to ensure we log the
  // critical action under the correct pre-navigation page context.
  pre_invoke_navigation_id_ = 0;
  const ToolRequest& current_action = GetInProgressToolRequest();
  ActorSurface* actor_surface = GetSurfaceForValidation(current_action);
  content::WebContents* contents =
      actor_surface ? actor_surface->GetWebContents() : nullptr;
  if (contents && contents->GetPrimaryMainFrame()) {
    pre_invoke_navigation_id_ =
        contents->GetPrimaryMainFrame()->GetNavigationId();
  }

  // Discard any dialogs buffered between actions (for example while reflecting
  // or during an earlier action that failed validation) so this action only
  // reports dialogs fired during its own invocation.
  if (content::WebContents* dialog_contents =
          GetWebContentsForDialogAttachment(current_action)) {
    JavaScriptDialogInterceptor::ClearInterceptedDialogs(*dialog_contents);
  }

  SetState(State::kToolInvoke);
  tool_controller_->Invoke(base::BindOnce(&ExecutionEngine::FinishedToolInvoke,
                                          GetActSequenceWeakPtr()),
                           GetDeferredObservationConfig());

  if (tool_invoke_callback_for_testing_) {
    std::move(tool_invoke_callback_for_testing_).Run();
  }
}

void ExecutionEngine::FinishedToolInvoke(ToolResult tool_result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedToolInvoke");
  CHECK_EQ(state_, State::kToolInvoke, base::NotFatalUntil::M161);
  // TODO(crbug.com/574534362): Remove once all tools migrate off
  // mojom::ActionResult.
  CHECK(tool_result.deprecated_action_result());

  // Drain dialogs intercepted during this tool invocation before any deferral
  // so dialogs fired while waiting on the user are not attributed to the
  // already-finished tool.
  std::vector<InterceptedJavaScriptDialog> javascript_dialogs =
      TakeInterceptedJavaScriptDialogs(GetInProgressToolRequest());

  if (tool_invoke_complete_callback_for_testing_) {
    std::move(tool_invoke_complete_callback_for_testing_).Run();
  }

  FinishToolInvokeWithDialogs(std::move(tool_result),
                              std::move(javascript_dialogs));
}

void ExecutionEngine::FinishToolInvokeWithDialogs(
    ToolResult tool_result,
    std::vector<InterceptedJavaScriptDialog> javascript_dialogs) {
  CHECK_EQ(state_, State::kToolInvoke, base::NotFatalUntil::M161);

  // If the task is waiting on user input, defer returning a result for it. This
  // prevents the actor state from proceeding and also allows UI to insert an
  // `external_tool_failure_reason` to the action.
  if (base::FeatureList::IsEnabled(kGlicDeferActUntilUninterrupted) &&
      task_->GetState() == ActorTask::State::kWaitingOnUser) {
    CHECK(deferred_finish_tool_invoke_.is_null());
    deferred_finish_tool_invoke_ = base::BindOnce(
        &ExecutionEngine::FinishToolInvokeWithDialogs, base::Unretained(this),
        std::move(tool_result), std::move(javascript_dialogs));
    return;
  }

  const ToolRequest& current_action = GetInProgressToolRequest();

  // The current action errored out. Stop the chain.
  std::optional<mojom::ActionResultCode> external_tool_failure_reason;
  std::swap(external_tool_failure_reason, external_tool_failure_reason_);
  if (external_tool_failure_reason) {
    ReportResultAndStartNext(ToolResult::FromDeprecatedActionResult(
                                 MakeResult(*external_tool_failure_reason)),
                             std::move(javascript_dialogs));
    return;
  }

  const mojom::ActionResultPtr& result = tool_result.deprecated_action_result();
  if (!tool_result.Ok()) {
    ReportResultAndStartNext(std::move(tool_result),
                             std::move(javascript_dialogs));
    return;
  }

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
  // Page actions that defer their stabilization wait to a later action in a
  // coalesced bundle don't run their own wait, so don't record per-action
  // PageStabilization for them. A pending observation at this point means the
  // action that just finished deferred its wait.
  const bool deferred_page_stabilization =
      base::FeatureList::IsEnabled(kActorStabilizeOncePerBundle) &&
      current_action.AsPageToolRequest() &&
      tool_controller_->HasPendingObservation();
  RecordToolTimings(
      current_action.Name(), end_time - current_request_start_time_,
      deferred_page_stabilization
          ? std::nullopt
          : std::make_optional(end_time - *result->execution_end_time));

  if (ActorSurfaceHandle actor_surface_handle =
          GetInProgressToolRequest().GetActorSurfaceForValidation();
      !actor_surface_handle.is_null()) {
    observation_strategy_.VoteForScreenshot(
        actor_surface_handle,
        static_cast<ScreenshotPolicy>(result->screenshot_policy));
    observation_strategy_.VoteForPageContentExtraction(
        actor_surface_handle,
        static_cast<PageContentExtractionPolicy>(result->page_content_policy));
  }

  ToolResultWithLatencyInfo execute_result(
      current_request_start_time_, end_time, std::move(tool_result),
      current_action.Name(), std::move(javascript_dialogs));

  SetState(State::kUiPostInvoke);
  GetUiEventDispatcher().OnPostTool(
      GetInProgressToolRequest(),
      base::BindOnce(&ExecutionEngine::FinishedUiPostInvoke,
                     GetActSequenceWeakPtr(), std::move(execute_result)));
}

void ExecutionEngine::FinishedUiPostInvoke(
    ToolResultWithLatencyInfo execute_result,
    mojom::ActionResultPtr ui_result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedUiPostInvoke");
  CHECK_EQ(state_, State::kUiPostInvoke, base::NotFatalUntil::M161);
  CHECK(executing_request_);
  CHECK(deferred_finish_tool_invoke_.is_null());

  if (!IsOk(*ui_result)) {
    MergeToolResult(
        execute_result.tool_result,
        ToolResult::FromDeprecatedActionResult(std::move(ui_result)));
    execute_result.end_time = base::TimeTicks::Now();
  }

  MaybeRecordActionResultCodeAfterSkippedWait(execute_result.tool_result);
  CurrentDispatcher().OnToolCompleted(std::move(execute_result));
  ProcessNextFromDispatcher();
}

void ExecutionEngine::MaybeRecordActionResultCodeAfterSkippedWait(
    const ToolResult& tool_result) {
  if (!std::exchange(tool_request_started_after_skipped_wait_, false) ||
      !base::FeatureList::IsEnabled(kActorStabilizeOncePerBundle)) {
    return;
  }
  const mojom::ActionResultPtr& action_result =
      tool_result.deprecated_action_result();
  RecordActionResultCodeAfterSkippedWait(
      action_result ? action_result->code : mojom::ActionResultCode::kOk);
}

void ExecutionEngine::CompleteActCall(
    std::vector<ToolResultWithLatencyInfo> results) {
  TRACE_EVENT0("actor", "ExecutionEngine::CompleteActCall");
  CHECK(dispatchers_.empty());
  CHECK(!executing_request_);
  CHECK(act_callback_);

  completed_results_ = std::move(results);
  if (tool_controller_ && tool_controller_->HasPendingObservation()) {
    is_settling_pending_observation_ = true;
    tool_controller_->SettlePendingObservation(base::BindOnce(
        &ExecutionEngine::RetryCompleteActCallWhenObservationSettled,
        GetActSequenceWeakPtr()));
    return;
  }
  is_settling_pending_observation_ = false;

  mojom::ActionResultCode result_code = mojom::ActionResultCode::kOk;
  std::optional<size_t> failed_index;
  ExtractErrorResult(*completed_results_, result_code, failed_index);
  const bool is_ok = !failed_index.has_value();

  SetState(State::kComplete);

  act_sequence_ended_callbacks_.Notify(is_ok);

  if (!is_ok) {
    const std::string error_debug_string =
        ToDebugString(*(*completed_results_)[*failed_index]
                           .tool_result.deprecated_action_result());
    journal_->Log(GURL(), task_->id(), "Act Failed",
                  JournalDetailsBuilder().AddError(error_debug_string).Build());
  }

  RecordActionResultCode(result_code);
  observation_strategy_.Lock();
  PostTaskForActCallback(std::move(act_callback_),
                         std::move(*completed_results_),
                         std::move(observation_strategy_));

  completed_results_.reset();
  act_sequence_weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExecutionEngine::RetryCompleteActCallWhenObservationSettled() {
  CHECK(completed_results_.has_value());
  std::vector<ToolResultWithLatencyInfo> results =
      std::move(*completed_results_);
  completed_results_.reset();
  CompleteActCall(std::move(results));
}

base::WeakPtr<ExecutionEngine> ExecutionEngine::GetActSequenceWeakPtr() {
  return act_sequence_weak_ptr_factory_.GetWeakPtr();
}

bool ExecutionEngine::IsExecutingActCall() const {
  return !dispatchers_.empty() || completed_results_.has_value();
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
  GetOriginGatingChecker().ComputeGatingDecision(
      std::make_unique<OriginGatingDecisionContext>(),
      GateableEvent(origin_gating::NavigationRequestEvent{.source = GURL(),
                                                          .destination = url}),
      base::BindOnce(&ResolveGatingDecision,
                     journal_->CreatePendingAsyncEntry(
                         url, task_->id(), MakeBrowserTrackUUID(task_->id()),
                         "OriginGatingDecision", {}),
                     url, GateableEvent::kNavigationRequest)
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
                       GetActSequenceWeakPtr(), origin,
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
  // TODO(b/555808355): Followups are going to be replaced by dispatchers.
  // Until they're removed we'll preserve the existing behavior by allowing
  // followup only in the base dispatcher (i.e. the initially passed-in vector
  // of ToolRequests).
  CHECK(IsExecutingBaseDispatcher());
  action->SetAsFollowup(base::PassKey<ExecutionEngine>());
  // Follow-ups act on behalf of the in-progress action, so they inherit its
  // observation delay behavior.
  action->SetSkipObservationDelay(
      GetInProgressToolRequest().ShouldSkipObservationDelay());
  static_cast<BaseDispatcher&>(CurrentDispatcher())
      .EnqueueFollowupAction(std::move(action));
}

void ExecutionEngine::AddActorSurface(
    ActorSurfaceHandle actor_surface_handle,
    bool stop_task_on_detach,
    base::OnceCallback<void(mojom::ActionResultPtr)> callback) {
  task_->AddActorSurface(actor_surface_handle, stop_task_on_detach,
                         std::move(callback));
}

bool ExecutionEngine::HasActorSurface(ActorSurfaceHandle actor_surface_handle) {
  return task_->HasActorSurface(actor_surface_handle);
}

void ExecutionEngine::RemoveActorSurface(
    ActorSurfaceHandle actor_surface_handle) {
  task_->RemoveActorSurface(actor_surface_handle);
}

base::WeakPtr<actor_login::ActionSequenceDelegate>
ExecutionEngine::GetActionSequenceDelegate() {
  return GetActSequenceWeakPtr();
}

void ExecutionEngine::OnActionTargetFrameResolved(
    RenderFrameHost& target_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(kGlicActorSubframeGatingDryRun)) {
    return;
  }
  // Only measure actions that are subject to page-action gating on the main
  // frame (see `StartExecutingNewRequest`).
  if (!GetInProgressToolRequest().RequiresUrlCheckInCurrentTab()) {
    return;
  }

  // Page actions are only gated on the main frame's origin (see
  // `SafetyChecksForExecutingTool`). Walk up to the main frame, collecting the
  // distinct origins of the frames between the target and the main frame.
  std::set<url::Origin> origins;
  RenderFrameHost* main_frame = &target_frame;
  while (RenderFrameHost* parent = main_frame->GetParentOrOuterDocument()) {
    origins.insert(
        OriginOrPrecursorIfOpaque(main_frame->GetLastCommittedOrigin()));
    main_frame = parent;
  }
  if (main_frame == &target_frame) {
    RecordPageActionTargetFrameType(PageActionTargetFrameType::kMainFrame);
    // The dry run wouldn't add anything on top of the existing main-frame
    // checks, so skip it.
    return;
  }

  origins.erase(
      OriginOrPrecursorIfOpaque(main_frame->GetLastCommittedOrigin()));
  if (origins.empty()) {
    RecordPageActionTargetFrameType(
        PageActionTargetFrameType::kSameOriginSubframe);
    // Every subframe in the chain is same-origin with the main frame, so the
    // dry run wouldn't add anything on top of the main-frame check.
    return;
  }
  RecordPageActionTargetFrameType(
      PageActionTargetFrameType::kCrossOriginSubframe);

  // The dry run is for metrics only, so keep it off the action's critical path.
  // Bound to `GetWeakPtr()` rather than the action sequence so the outcome is
  // still recorded if the action sequence ends first.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &ExecutionEngine::RunSubframeGatingDryRun, GetWeakPtr(),
                     WebContents::FromRenderFrameHost(main_frame)->GetWeakPtr(),
                     GetClientToolForRequest(GetInProgressToolRequest()),
                     std::move(origins)));
}

void ExecutionEngine::RunSubframeGatingDryRun(
    base::WeakPtr<WebContents> web_contents,
    origin_gating::ClientTool tool,
    std::set<url::Origin> origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents) {
    return;
  }

  auto barrier = base::BarrierCallback<SubframeDryRunResult>(
      origins.size(),
      base::BindOnce(&RecordMostRestrictiveSubframeDryRunResult));
  for (const url::Origin& origin : origins) {
    GURL url = origin.GetURL();
    GetOriginGatingChecker().ComputeGatingDecision(
        std::make_unique<PageActionGatingContext>(web_contents,
                                                  /*is_dry_run=*/true),
        GateableEvent(
            origin_gating::PageActionEvent{.destination = url, .tool = tool}),
        base::BindOnce(
            [](const GURL& url,
               std::unique_ptr<origin_gating::GatingDecisionContext>,
               origin_gating::GatingDecision decision) -> SubframeDryRunResult {
              if (decision.is_allowed) {
                return WouldBeAllowed{};
              }
              if (decision.attribution == DecisionSource::kNoVerdict) {
                // A disallowed dry run without a verdict means the destination
                // requires user confirmation, so a live evaluation's
                // `OnNoVerdict` would prompt the user.
                return WouldRequireUserConfirmation{};
              }
              return WouldBlock{
                  .reason = MapGatingDecisionToBlockReason(decision, url),
              };
            },
            url)
            .Then(barrier));
  }
}

base::CallbackListSubscription ExecutionEngine::RegisterActionSequenceEnded(
    base::OnceCallback<void(bool)> callback) {
  return act_sequence_ended_callbacks_.Add(std::move(callback));
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
  GetOriginGatingChecker().AllowNavigationTo(added_writable_mainframe_origins);
}

void ExecutionEngine::SetActorLoginService(
    std::unique_ptr<actor_login::ActorLoginService> actor_login_service) {
  actor_login_service_ = std::move(actor_login_service);
}

const ToolRequest& ExecutionEngine::GetInProgressToolRequest() const {
  CHECK(executing_request_);
  return *executing_request_;
}

ActorSurface* ExecutionEngine::GetSurfaceForValidation(
    const ToolRequest& request) {
  return task_->actor_keyed_service().GetSurfaceRegistry().Get(
      request.GetActorSurfaceForValidation());
}

content::WebContents* ExecutionEngine::GetWebContentsForDialogAttachment(
    const ToolRequest& request) {
  ActorSurfaceHandle surface_handle = request.GetActorSurfaceForValidation();
  if (surface_handle.is_null()) {
    surface_handle = task_->GetLastActuatedActorSurface();
  }
  return GetWebContentsIfSurfaceAlive(
      task_->actor_keyed_service().GetSurfaceRegistry().Get(surface_handle));
}

std::vector<InterceptedJavaScriptDialog>
ExecutionEngine::TakeInterceptedJavaScriptDialogs(const ToolRequest& request) {
  content::WebContents* web_contents =
      GetWebContentsForDialogAttachment(request);
  if (!web_contents) {
    return {};
  }
  return JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents);
}

std::optional<ObservationDelayController::PageStabilityConfig>
ExecutionEngine::GetDeferredObservationConfig() const {
  if (!base::FeatureList::IsEnabled(kActorStabilizeOncePerBundle)) {
    return std::nullopt;
  }

  const ToolRequest& current_action = GetInProgressToolRequest();
  const PageToolRequest* current_page_request =
      current_action.AsPageToolRequest();
  if (!current_page_request || current_action.IsFollowup() ||
      IsExecutingNestedDispatcher() ||
      current_action.ShouldSkipObservationDelay()) {
    return std::nullopt;
  }

  // TODO(b/555808355): To preserve current behavior, DeferredObservations will
  // only be supported on the base dispatcher (i.e. not on a composite tool).
  // This matches current behavior that breaks up observation deferral across
  // followup actions. Re-consider once we understand the interaction better.
  CHECK(IsExecutingBaseDispatcher());

  const ActorSurfaceHandle actor_surface =
      current_page_request->GetActorSurfaceHandle();
  content::RenderFrameHost* target_frame = FindTargetLocalRootFrame(
      actor_surface, current_page_request->GetTarget());
  if (!target_frame) {
    return std::nullopt;
  }
  auto is_in_bundle = [&](const ToolRequest& request) {
    const PageToolRequest* page_request = request.AsPageToolRequest();
    return page_request && !request.IsFollowup() &&
           !request.ShouldSkipObservationDelay() &&
           page_request->GetActorSurfaceHandle() == actor_surface &&
           FindTargetLocalRootFrame(actor_surface, page_request->GetTarget()) ==
               target_frame;
  };

  base::span<const std::unique_ptr<ToolRequest>> remaining =
      static_cast<const BaseDispatcher&>(CurrentDispatcher())
          .PeekRemainingRequests();

  if (remaining.empty() || !is_in_bundle(*remaining[0])) {
    return std::nullopt;
  }

  ObservationDelayController::PageStabilityConfig config =
      current_action.GetObservationPageStabilityConfig();
  for (const std::unique_ptr<ToolRequest>& req : remaining) {
    if (!is_in_bundle(*req)) {
      break;
    }
    config.supports_paint_stability &=
        req->GetObservationPageStabilityConfig().supports_paint_stability;
  }
  return config;
}

ui::UiEventDispatcher& ExecutionEngine::GetUiEventDispatcher() {
  return task_->ui_event_dispatcher();
}

origin_gating::OriginGatingService& ExecutionEngine::GetOriginGatingService()
    const {
  return CHECK_DEREF(
      origin_gating::OriginGatingServiceFactory::GetForBrowserContext(
          task_->GetProfile()));
}

origin_gating::OriginGatingChecker&
ExecutionEngine::GetOriginGatingCheckerInternal() const {
  CHECK(origin_gating_registration_);
  return CHECK_DEREF(origin_gating_registration_->service().GetChecker(
      origin_gating_registration_->id()));
}

const origin_gating::OriginGatingChecker&
ExecutionEngine::GetOriginGatingChecker() const {
  return GetOriginGatingCheckerInternal();
}

origin_gating::OriginGatingChecker& ExecutionEngine::GetOriginGatingChecker() {
  return GetOriginGatingCheckerInternal();
}

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s) {
  return o << ExecutionEngine::StateToString(s);
}

}  // namespace actor
