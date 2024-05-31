// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/proactive_nudge_tracker.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/common/signatures.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace compose {
namespace {
using segmentation_platform::processing::ProcessedValue;

scoped_refptr<segmentation_platform::InputContext> PopulateInputContextForField(
    const ProactiveNudgeTracker::Signals& signals) {
  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();

  input_context->metadata_args.emplace(
      "field_max_length",
      ProcessedValue::FromFloat(signals.field.max_length()));
  input_context->metadata_args.emplace(
      "field_width_pixels",
      ProcessedValue::FromFloat(signals.field.bounds().width()));
  input_context->metadata_args.emplace(
      "field_height_pixels",
      ProcessedValue::FromFloat(signals.field.bounds().height()));
  input_context->metadata_args.emplace(
      "field_form_control_type", ProcessedValue::FromFloat(base::to_underlying(
                                     signals.field.form_control_type())));
  input_context->metadata_args.emplace(
      "total_field_count",
      ProcessedValue::FromFloat(signals.form.fields.size()));

  int multiline_field_count = 0;
  for (const auto& f : signals.form.fields) {
    if (f.form_control_type() == autofill::FormControlType::kTextArea ||
        f.form_control_type() == autofill::FormControlType::kContentEditable) {
      ++multiline_field_count;
    }
  }
  input_context->metadata_args.emplace(
      "multiline_field_count",
      ProcessedValue::FromFloat(multiline_field_count));
  input_context->metadata_args.emplace(
      "time_spent_on_page",
      ProcessedValue::FromFloat(
          (base::TimeTicks::Now() - signals.page_change_time).InSecondsF()));

  input_context->metadata_args.emplace(
      "field_signature",
      ProcessedValue(autofill::HashFieldSignature(
          autofill::CalculateFieldSignatureForField(signals.field))));
  input_context->metadata_args.emplace(
      "form_signature", ProcessedValue(autofill::HashFormSignature(
                            autofill::CalculateFormSignature(signals.form))));

  input_context->metadata_args.emplace("page_url",
                                       ProcessedValue(signals.page_url));
  input_context->metadata_args.emplace(
      "origin", ProcessedValue(signals.page_origin.GetURL()));

  input_context->metadata_args.emplace(
      "field_value", ProcessedValue(base::UTF16ToUTF8(signals.field.value())));
  input_context->metadata_args.emplace(
      "field_selected_text",
      ProcessedValue(base::UTF16ToUTF8(signals.field.selected_text())));
  input_context->metadata_args.emplace(
      "field_placeholder_text",
      ProcessedValue(base::UTF16ToUTF8(signals.field.placeholder())));
  input_context->metadata_args.emplace(
      "field_label", ProcessedValue(base::UTF16ToUTF8(signals.field.label())));
  input_context->metadata_args.emplace(
      "field_aria_label",
      ProcessedValue(base::UTF16ToUTF8(signals.field.aria_label())));
  input_context->metadata_args.emplace(
      "field_aria_description",
      ProcessedValue(base::UTF16ToUTF8(signals.field.aria_description())));
  return input_context;
}

}  // namespace

ProactiveNudgeTracker::Signals::Signals() = default;
ProactiveNudgeTracker::Signals::~Signals() = default;
ProactiveNudgeTracker::Signals::Signals(Signals&&) = default;
ProactiveNudgeTracker::Signals& ProactiveNudgeTracker::Signals::operator=(
    Signals&&) = default;

// Tracks user engagement as a result of the nudge.
class ProactiveNudgeTracker::EngagementTracker {
 public:
  EngagementTracker(
      autofill::FieldGlobalId field_global_id,
      segmentation_platform::TrainingRequestId training_request_id,
      ProactiveNudgeTracker* nudge_tracker)
      : field_global_id_(field_global_id),
        training_request_id_(training_request_id),
        nudge_tracker_(nudge_tracker) {}
  EngagementTracker(const EngagementTracker&) = delete;
  EngagementTracker& operator=(const EngagementTracker&) = delete;

  ~EngagementTracker() {
    ReportIfFirst(ProactiveNudgeDerivedEngagement::kIgnored);
  }

  void ComposeSessionCompleted(ComposeSessionCloseReason session_close_reason,
                               const compose::ComposeSessionEvents& events) {
    if (events.inserted_results) {
      ReportIfFirst(
          ProactiveNudgeDerivedEngagement::kAcceptedComposeSuggestion);
    } else if (events.compose_count > 0) {
      ReportIfFirst(
          ProactiveNudgeDerivedEngagement::kGeneratedComposeSuggestion);
    } else {
      ReportIfFirst(ProactiveNudgeDerivedEngagement::kOpenedComposeMinimalUse);
    }
  }

  void UserDisabledNudge(bool single_site_only) {
    if (single_site_only) {
      ReportIfFirst(
          ProactiveNudgeDerivedEngagement::kNudgeDisabledOnSingleSite);
    } else {
      ReportIfFirst(ProactiveNudgeDerivedEngagement::kNudgeDisabledOnAllSites);
    }
  }

 private:
  void ReportIfFirst(ProactiveNudgeDerivedEngagement engagement) {
    if (reported_) {
      return;
    }
    nudge_tracker_->CollectTrainingData(training_request_id_, engagement);
    reported_ = true;
  }

  bool reported_ = false;
  autofill::FieldGlobalId field_global_id_;
  segmentation_platform::TrainingRequestId training_request_id_;
  raw_ptr<ProactiveNudgeTracker> nudge_tracker_;
};

ProactiveNudgeTracker::State::State() = default;
ProactiveNudgeTracker::State::~State() = default;

float ProactiveNudgeTracker::Delegate::SegmentationFallbackShowResult() {
  return base::RandFloat();
}

float ProactiveNudgeTracker::Delegate::SegmentationForceShowResult() {
  return base::RandFloat();
}

ProactiveNudgeTracker::ProactiveNudgeTracker(
    segmentation_platform::SegmentationPlatformService* segmentation_service,
    Delegate* delegate)
    : segmentation_service_(segmentation_service), delegate_(delegate) {}

void ProactiveNudgeTracker::StartObserving(content::WebContents* web_contents) {
  if (!SegmentationStateIsValid()) {
    // Unable to show proactive nudge if configuration is not consistent.
    // Todo(b/343281445): Use fallback strategy if state is invalid.
    return;
  }
  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);
}

ProactiveNudgeTracker::~ProactiveNudgeTracker() {
  // Destroy all engagement trackers first to ensure CollectTrainingData() is
  // called before other parts of this class are destroyed.
  engagement_trackers_.clear();
}

bool ProactiveNudgeTracker::ProactiveNudgeRequestedForFormField(
    Signals signals) {
  DVLOG(2) << "ProactiveNudgeTracker: ProactiveNudgeRequestedForFormField";
  if (!SegmentationStateIsValid()) {
    // Unable to show proactive nudge if configuration is not consistent.
    return false;
  }
  autofill::FieldGlobalId field_global_id = signals.field.global_id();
  if (compose::GetComposeConfig().proactive_nudge_field_per_navigation &&
      seen_fields_.contains(field_global_id)) {
    DVLOG(2)
        << "ProactiveNudgeTracker: ProactiveNudgeRequestedForFormField, seen.";
    return false;
  }
  if (MatchesCurrentField(signals.field.renderer_form_id(), field_global_id)) {
    DVLOG(2) << "ProactiveNudgeTracker: Init with matching field";
    if (state_->show_state == ShowState::kCanBeShown) {
      state_->show_state = ShowState::kShown;
      seen_fields_.insert(field_global_id);
      return true;
    }
    return false;
  }

  // Reset to UNINITIALIZED, then immediately transition to WAITING.
  ResetState();
  state_ = std::make_unique<State>();
  state_->signals = std::move(signals);

  base::TimeDelta delay = compose::GetComposeConfig().proactive_nudge_delay;
  if (delay == base::Milliseconds(0)) {
    state_->timer_complete = true;
    BeginSegmentationIfRequired();
  } else {
    state_->timer.Start(FROM_HERE,
                        compose::GetComposeConfig().proactive_nudge_delay, this,
                        &ProactiveNudgeTracker::ShowTimerElapsed);
  }

  if (ShouldShow(*state_)) {
    // If the timer is 0-duration and no segmentation result is required, then
    // just transition to Shown state directly before returning true.
    state_->show_state = ShowState::kShown;
    compose::LogComposeProactiveNudgeCtr(
        compose::ComposeProactiveNudgeCtrEvent::kNudgeDisplayed);
    compose::LogComposeProactiveNudgeShowStatus(
        compose::ComposeShowStatus::kShouldShow);
    delegate_->GetPageUkmTracker()->ProactiveNudgeShown();
    delegate_->GetPageUkmTracker()->ComposeProactiveNudgeShouldShow();

    seen_fields_.insert(field_global_id);
    return true;
  }
  return false;
}

bool ProactiveNudgeTracker::ShouldShow(const State& state) {
  if (!state.timer_complete) {
    return false;
  }
  if (!compose::GetComposeConfig().proactive_nudge_segmentation) {
    return true;
  }
  DVLOG(2) << "ProactiveNudgeTracker: ShouldShow "
           << (state.segmentation_result &&
               !state.segmentation_result->ordered_labels.empty() &&
               state.segmentation_result->ordered_labels[0] ==
                   segmentation_platform::kComposePrmotionLabelShow);
  return state.segmentation_result &&
         !state.segmentation_result->ordered_labels.empty() &&
         state.segmentation_result->ordered_labels[0] ==
             segmentation_platform::kComposePrmotionLabelShow;
}

void ProactiveNudgeTracker::FocusChangedInPage() {
  ResetState();
}

void ProactiveNudgeTracker::Clear() {
  engagement_trackers_.clear();
  seen_fields_.clear();
  ResetState();
}

void ProactiveNudgeTracker::OnAfterFocusOnFormField(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field) {
  DVLOG(2) << "ProactiveNudgeTracker: OnAfterFocusOnFormField";
  // If this focus is on the current field, we are (presumably) already focused
  // and this is a no-op. Also, if we are not currently in the WAITING state,
  // this is a no-op.
  if (MatchesCurrentField(form, field) || state_ == nullptr) {
    return;
  }

  // Now we should transition to the UNINITIALIZED state.
  ResetState();
}

void ProactiveNudgeTracker::OnAfterTextFieldDidChange(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field,
    const std::u16string& text_value) {
  // Continue to delay the proactive nudge if the current field is being
  // changed.
  if (state_ && !state_->timer_complete && state_->timer.IsRunning() &&
      MatchesCurrentField(form, field)) {
    state_->timer.Reset();
  }
}

bool ProactiveNudgeTracker::SegmentationStateIsValid() {
  return !compose::GetComposeConfig().proactive_nudge_segmentation ||
         segmentation_service_ != nullptr;
}

void ProactiveNudgeTracker::ResetState() {
  DVLOG(2) << "ProactiveNudgeTracker: ResetState";
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_.reset();
}

void ProactiveNudgeTracker::BeginSegmentationIfRequired() {
  DVLOG(2) << "ProactiveNudgeTracker: BeginSegmentationIfRequired";
  if (!state_ || state_->show_state != ShowState::kWaitingForTimer) {
    return;
  }

  if (!compose::GetComposeConfig().proactive_nudge_segmentation) {
    return;
  }

  state_->show_state = ShowState::kWaitingForSegmentation;
  segmentation_platform::PredictionOptions options;
  options.on_demand_execution = true;
  segmentation_service_->GetClassificationResult(
      segmentation_platform::kComposePromotionKey, options,
      PopulateInputContextForField(state_->signals),
      base::BindOnce(&ProactiveNudgeTracker::GotClassificationResult,
                     weak_ptr_factory_.GetWeakPtr(), state_->AsWeakPtr()));
}

void ProactiveNudgeTracker::ShowTimerElapsed() {
  DVLOG(2) << "ProactiveNudgeTracker: ShowTimerElapsed";
  // If we are not in the WAITING state, this timer is stale, we should ignore
  // it.
  if (!state_ || state_->show_state != ShowState::kWaitingForTimer) {
    return;
  }

  state_->timer_complete = true;

  BeginSegmentationIfRequired();
  MaybeShowProactiveNudge();
}

void ProactiveNudgeTracker::MaybeShowProactiveNudge() {
  DVLOG(2) << "ProactiveNudgeTracker: MaybeShowProactiveNudge ";
  if (!state_ || !ShouldShow(*state_)) {
    return;
  }

  // Transition to the CAN_BE_SHOWN state.

  if (state_->segmentation_result &&
      (state_->segmentation_result_ignored_for_training ||
       compose::GetComposeConfig()
           .proactive_nudge_always_collect_training_data)) {
    engagement_trackers_[state_->signals.field.global_id()] =
        std::make_unique<EngagementTracker>(
            state_->signals.field.global_id(),
            state_->segmentation_result->request_id, this);
  }
  delegate_->ShowProactiveNudge(state_->signals.field.renderer_form_id(),
                                state_->signals.field.global_id());
  state_->show_state = ShowState::kCanBeShown;
}

void ProactiveNudgeTracker::GotClassificationResult(
    base::WeakPtr<State> state,
    const segmentation_platform::ClassificationResult& result) {
  DVLOG(2) << "ProactiveNudgeTracker: GotClassificationResult";
  if (!state || state->show_state != ShowState::kWaitingForSegmentation) {
    return;
  }

  state_->segmentation_result = result;

  switch (result.status) {
    case segmentation_platform::PredictionStatus::kFailed:
    case segmentation_platform::PredictionStatus::kNotReady:
      if (delegate_->SegmentationFallbackShowResult() <
          compose::GetComposeConfig().proactive_nudge_show_probability) {
        // Override default DontShow decision.
        state_->segmentation_result->ordered_labels.emplace_back(
            segmentation_platform::kComposePrmotionLabelShow);
      }
      break;
    case segmentation_platform::PredictionStatus::kSucceeded:
      if (delegate_->SegmentationForceShowResult() <
          compose::GetComposeConfig().proactive_nudge_force_show_probability) {
        state_->segmentation_result->ordered_labels = {
            segmentation_platform::kComposePrmotionLabelShow};
        state_->segmentation_result_ignored_for_training = true;
      }
      break;
  }

  if (state_->segmentation_result->ordered_labels.empty() ||
      state_->segmentation_result->ordered_labels[0] !=
          segmentation_platform::kComposePrmotionLabelShow) {
    // The nudge was blocked by segmentation platform.
    delegate_->GetPageUkmTracker()->ComposeProactiveNudgeShouldShow();
    compose::LogComposeProactiveNudgeShowStatus(
        compose::ComposeShowStatus::
            kProactiveNudgeBlockedBySegmentationPlatform);
  } else {
    MaybeShowProactiveNudge();
  }
}

void ProactiveNudgeTracker::CollectTrainingData(
    const segmentation_platform::TrainingRequestId training_request_id,
    ProactiveNudgeDerivedEngagement engagement) {
  segmentation_platform::TrainingLabels training_labels;
  base::UmaHistogramEnumeration("Compose.ProactiveNudge.DerivedEngagement",
                                engagement);
  training_labels.output_metric =
      std::make_pair("Compose.ProactiveNudge.DerivedEngagement",
                     static_cast<base::HistogramBase::Sample>(engagement));
  segmentation_service_->CollectTrainingData(
      segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_SEGMENTATION_COMPOSE_PROMOTION,
      training_request_id, training_labels, base::DoNothing());
}

bool ProactiveNudgeTracker::MatchesCurrentField(autofill::FormGlobalId form,
                                                autofill::FieldGlobalId field) {
  return state_ && state_->signals.field.renderer_form_id() == form &&
         state_->signals.field.global_id() == field;
}

void ProactiveNudgeTracker::ComposeSessionCompleted(
    autofill::FieldGlobalId field_global_id,
    ComposeSessionCloseReason session_close_reason,
    const compose::ComposeSessionEvents& events) {
  auto iter = engagement_trackers_.find(field_global_id);
  if (iter != engagement_trackers_.end()) {
    iter->second->ComposeSessionCompleted(session_close_reason, events);
    engagement_trackers_.erase(iter);
  }
}

void ProactiveNudgeTracker::OnUserDisabledNudge(bool single_site_only) {
  for (auto& iter : engagement_trackers_) {
    iter.second->UserDisabledNudge(single_site_only);
  }
  engagement_trackers_.clear();
}

}  // namespace compose
