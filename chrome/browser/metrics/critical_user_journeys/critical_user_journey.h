// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_step.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

// Helper used to define alternative paths (branches) within a journey step.
struct Branch {
  Branch(ui::ElementIdentifier id,
         ui::InteractionSequence::StepType type,
         int metric_id);
  Branch(ui::CustomElementEventType event_type, int metric_id);
  ~Branch();

  ui::ElementIdentifier id;
  ui::InteractionSequence::StepType type;
  ui::CustomElementEventType custom_event_type;
  int metric_id;
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
    explicit Builder(std::string name);
    ~Builder();

    Builder& AddStep(
        std::variant<ui::ElementIdentifier, ui::CustomElementEventType> event,
        ui::InteractionSequence::StepType type,
        int metric_id);
    Builder& AddAnyOf(const std::vector<Branch>& branches);
    Builder& AddCustomCompletionCallback(base::RepeatingClosure callback);
    Builder& LaunchHatsSurveyOnCompletion(HatsParams params);

    std::unique_ptr<CriticalUserJourney> Build();

   private:
    std::string name_;
    std::vector<std::unique_ptr<CriticalUserJourneyStep>> steps_;
    base::RepeatingClosure completion_callback_;
    std::optional<HatsParams> hats_params_;
  };

  CriticalUserJourney(
      std::string name,
      std::vector<std::unique_ptr<CriticalUserJourneyStep>> steps,
      base::RepeatingClosure completion_callback,
      std::optional<HatsParams> hats_params);
  ~CriticalUserJourney();

  const std::string& name() const { return name_; }
  const std::vector<std::unique_ptr<CriticalUserJourneyStep>>& steps() const {
    return steps_;
  }
  base::RepeatingClosure completion_callback() const {
    return completion_callback_;
  }
  const std::optional<HatsParams>& hats_params() const { return hats_params_; }

 private:
  std::string name_;
  std::vector<std::unique_ptr<CriticalUserJourneyStep>> steps_;
  base::RepeatingClosure completion_callback_;
  std::optional<HatsParams> hats_params_;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_H_
