// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_step.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

// HaTS product-specific string data (PSD) keys and values attached to every
// CUJ survey. Shared by CriticalUserJourneyService and the HaTS survey configs
// so the allowlisted field names cannot drift. PSD keys and values are shown
// verbatim to users, so they use plain English per
// go/privacy-guidelines-hats-in-chrome.
inline constexpr char kCujJourneyOutcomeKey[] = "Journey outcome";
inline constexpr char kCujJourneyOutcomeCompleted[] = "completed";
inline constexpr char kCujJourneyOutcomeAbandoned[] = "abandoned";
inline constexpr char kCujJourneyNameKey[] = "Journey name";
inline constexpr char kCujEntryStepKey[] = "Entry step";
inline constexpr char kCujJourneyDurationKey[] = "Journey duration in seconds";

// The default time allowed between reaching a step and reaching the next one.
inline constexpr base::TimeDelta kDefaultCujStepTimeout = base::Minutes(2);

// Returns a coarse duration bucket string ("<2s", "2s-5s", "5s-15s",
// "15s-60s", ">60s") for HaTS product-specific string data.
std::string GetCujDurationBucket(base::TimeDelta duration);

// Helper used to define alternative paths (branches) within a journey step.
struct Branch {
  Branch(ui::ElementIdentifier id,
         ui::InteractionSequence::StepType type,
         int metric_id,
         ui::InteractionSequence::ContextMode context_mode =
             ui::InteractionSequence::ContextMode::kInitial);
  Branch(ui::CustomElementEventType event_type,
         int metric_id,
         ui::InteractionSequence::ContextMode context_mode =
             ui::InteractionSequence::ContextMode::kInitial);

  template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
  Branch(ui::ElementIdentifier id,
         ui::InteractionSequence::StepType type,
         T metric_id,
         ui::InteractionSequence::ContextMode context_mode =
             ui::InteractionSequence::ContextMode::kInitial)
      : Branch(id, type, static_cast<int>(metric_id), context_mode) {}

  template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
  Branch(ui::CustomElementEventType event_type,
         T metric_id,
         ui::InteractionSequence::ContextMode context_mode =
             ui::InteractionSequence::ContextMode::kInitial)
      : Branch(event_type, static_cast<int>(metric_id), context_mode) {}

  // Sets the interaction sequence context mode for this branch step.
  Branch& SetContextMode(ui::InteractionSequence::ContextMode mode) {
    context_mode = mode;
    return *this;
  }

  // Marks this branch as an exit branch: reaching it ends the journey with
  // JourneyResult::kAbandoned instead of kCompleted, and any HaTS survey is
  // launched with `kCujJourneyOutcomeKey` set to `kCujJourneyOutcomeAbandoned`.
  //
  // If `min_dwell` is non-zero and the branch is reached sooner than
  // that after the previous step, the journey ends with
  // JourneyResult::kAbandonedBelowDwell and no survey is launched. If
  // `max_dwell` is non-zero and the branch is reached later than that, the
  // journey ends with JourneyResult::kAbandonedAboveDwell and no survey is
  // launched.
  //
  // Exit branches are only valid in the final step of a journey (enforced by
  // CriticalUserJourney::Builder::Build()). The underlying InteractionSequence
  // cannot be reset from within a step callback, so an exit branch in an
  // earlier step could not stop the remaining steps from running.
  Branch& SetExitBranch(base::TimeDelta min_dwell = base::TimeDelta(),
                        base::TimeDelta max_dwell = base::TimeDelta()) {
    CHECK_GE(min_dwell, base::TimeDelta());
    CHECK_GE(max_dwell, base::TimeDelta());
    CHECK(max_dwell.is_zero() || max_dwell > min_dwell);
    is_exit_branch = true;
    min_dwell_duration = min_dwell;
    max_dwell_duration = max_dwell;
    return *this;
  }

  // Tags this branch with a plain-English name for the UI surface that started
  // the journey (for example, "app_menu" or "toolbar_button"). Use this when a
  // journey's first step is an `AddAnyOf()` group with multiple UI entry
  // points and HaTS survey analysis needs to distinguish which entry point the
  // user took.
  //
  // Sent as the `kCujEntryStepKey` ("Entry step") field in HaTS
  // product-specific data (PSD), which is shown verbatim to users in the
  // survey privacy disclosure. Ignored on branches after the first step.
  Branch& SetHatsEntryPoint(std::string_view entry_point) {
    hats_entry_point = std::string(entry_point);
    return *this;
  }

  // Overrides how long the journey waits for the next step after this branch
  // is reached (default `kDefaultCujStepTimeout`).
  Branch& SetTimeout(base::TimeDelta timeout) {
    CHECK_GT(timeout, base::TimeDelta());
    time_out_duration = timeout;
    return *this;
  }

  ~Branch();
  Branch(const Branch&);
  Branch& operator=(const Branch&);
  Branch(Branch&&) noexcept;
  Branch& operator=(Branch&&) noexcept;

  ui::ElementIdentifier id;
  ui::InteractionSequence::StepType type;
  ui::CustomElementEventType custom_event_type;
  int metric_id;
  bool is_exit_branch = false;
  base::TimeDelta min_dwell_duration = base::TimeDelta();
  base::TimeDelta max_dwell_duration = base::TimeDelta();
  base::TimeDelta time_out_duration = kDefaultCujStepTimeout;
  // Plain-English UI entry-point label sent as `kCujEntryStepKey` in HaTS
  // product-specific data (PSD) when this branch starts the journey.
  std::string hats_entry_point = kCujDefaultEntryStep;
  ui::InteractionSequence::ContextMode context_mode =
      ui::InteractionSequence::ContextMode::kInitial;
};

// Configuration parameters for triggering a HaTS survey upon journey
// completion.
struct HatsParams {
  HatsParams();
  HatsParams(const HatsParams&);
  HatsParams& operator=(const HatsParams&);
  ~HatsParams();

  std::string trigger;
  base::RepeatingClosure success_callback;
  base::RepeatingClosure failure_callback;
  std::map<std::string, bool> product_specific_bits_data;
  std::map<std::string, std::string> product_specific_string_data;
  std::optional<std::string> supplied_trigger_id;
};

// Defines a sequence of user-UI interactions representing a critical task.
// Used to track progress, completion, and drop-off rates for key user
// workflows.
class CriticalUserJourney {
 public:
  class Builder {
   public:
    // Requires a base::Feature. The journey name will be `feature->name`.
    explicit Builder(const base::Feature* feature);
    ~Builder();

    Builder& AddStep(
        std::variant<ui::ElementIdentifier, ui::CustomElementEventType> event,
        ui::InteractionSequence::StepType type,
        int metric_id,
        ui::InteractionSequence::ContextMode context_mode =
            ui::InteractionSequence::ContextMode::kInitial);

    template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
    Builder& AddStep(
        std::variant<ui::ElementIdentifier, ui::CustomElementEventType> event,
        ui::InteractionSequence::StepType type,
        T metric_id,
        ui::InteractionSequence::ContextMode context_mode =
            ui::InteractionSequence::ContextMode::kInitial) {
      return AddStep(event, type, static_cast<int>(metric_id), context_mode);
    }

    Builder& AddAnyOf(const std::vector<Branch>& branches);

    // Overrides how long the journey waits for the next step after the most
    // recently added step is reached (default `kDefaultCujStepTimeout`). Use
    // for surfaces that stay open longer than the default, such as Reading
    // Mode. When the most recent step is an `AddAnyOf()` group, the timeout
    // applies to every branch.
    Builder& SetStepTimeout(base::TimeDelta timeout);

    Builder& AddCustomCompletionCallback(base::RepeatingClosure callback);
    Builder& LaunchHatsSurveyOnCompletion(HatsParams params);

    std::unique_ptr<CriticalUserJourney> Build();

   private:
    const raw_ptr<const base::Feature> feature_;
    std::vector<std::unique_ptr<CriticalUserJourneyStep>> steps_;
    base::RepeatingClosure completion_callback_;
    std::optional<HatsParams> hats_params_;
  };

  CriticalUserJourney(
      const base::Feature* feature,
      std::vector<std::unique_ptr<CriticalUserJourneyStep>> steps,
      base::RepeatingClosure completion_callback,
      std::optional<HatsParams> hats_params);
  ~CriticalUserJourney();

  // The name is automatically provided by the feature.
  const char* name() const { return feature_->name; }

  // Returns whether this specific journey's feature flag is enabled.
  bool IsEnabled() const { return base::FeatureList::IsEnabled(*feature_); }

  const base::Feature* feature() const { return feature_; }

  const std::vector<std::unique_ptr<CriticalUserJourneyStep>>& steps() const {
    return steps_;
  }
  base::RepeatingClosure completion_callback() const {
    return completion_callback_;
  }
  const std::optional<HatsParams>& hats_params() const { return hats_params_; }

 private:
  const raw_ptr<const base::Feature> feature_;
  std::vector<std::unique_ptr<CriticalUserJourneyStep>> steps_;
  base::RepeatingClosure completion_callback_;
  std::optional<HatsParams> hats_params_;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_H_
