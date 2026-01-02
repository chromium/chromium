// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_ACTOR_EXPERIMENTAL_ACTOR_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_ACTOR_EXPERIMENTAL_ACTOR_API_H_

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Base class for all experimental actor functions. See idl for more details.
class ExperimentalActorApiFunction : public ExtensionFunction {
 public:
  ExperimentalActorApiFunction();

  ExperimentalActorApiFunction(const ExperimentalActorApiFunction&) = delete;
  ExperimentalActorApiFunction& operator=(const ExperimentalActorApiFunction&) =
      delete;

 protected:
  ~ExperimentalActorApiFunction() override;
  bool PreRunValidation(std::string* error) override;
};

// Stops an actor task.
class ExperimentalActorStopTaskFunction : public ExperimentalActorApiFunction {
 public:
  ExperimentalActorStopTaskFunction();

  ExperimentalActorStopTaskFunction(const ExperimentalActorStopTaskFunction&) =
      delete;
  ExperimentalActorStopTaskFunction& operator=(
      const ExperimentalActorStopTaskFunction&) = delete;

 protected:
  ~ExperimentalActorStopTaskFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("experimentalActor.stopTask",
                             EXPERIMENTALACTOR_STOPTASK)
};

class ExperimentalActorCreateTaskFunction
    : public ExperimentalActorApiFunction {
 public:
  ExperimentalActorCreateTaskFunction();
  ExperimentalActorCreateTaskFunction(
      const ExperimentalActorCreateTaskFunction&) = delete;
  ExperimentalActorCreateTaskFunction& operator=(
      const ExperimentalActorCreateTaskFunction&) = delete;

 protected:
  ~ExperimentalActorCreateTaskFunction() override;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("experimentalActor.createTask",
                             EXPERIMENTALACTOR_CREATETASK)
};

class ExperimentalActorPerformActionsFunction
    : public ExperimentalActorApiFunction {
 public:
  ExperimentalActorPerformActionsFunction();
  ExperimentalActorPerformActionsFunction(
      const ExperimentalActorPerformActionsFunction&) = delete;
  ExperimentalActorPerformActionsFunction& operator=(
      const ExperimentalActorPerformActionsFunction&) = delete;

 protected:
  ~ExperimentalActorPerformActionsFunction() override;
  ResponseAction Run() override;
  void OnActionsFinished(
      actor::TaskId task_id,
      base::TimeTicks start_time,
      bool skip_async_observation_information,
      actor::mojom::ActionResultCode result_code,
      std::optional<size_t> index_of_failed_action,
      std::vector<actor::ActionResultWithLatencyInfo> action_results);
  void OnObservationResult(
      base::TimeTicks start_time,
      actor::mojom::ActionResultCode result_code,
      std::optional<size_t> index_of_failed_action,
      std::vector<actor::ActionResultWithLatencyInfo> action_results,
      actor::TaskId task_id,
      bool skip_async_observation_information,
      std::unique_ptr<optimization_guide::proto::ActionsResult> response,
      std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>
          journal_entry);
  DECLARE_EXTENSION_FUNCTION("experimentalActor.performActions",
                             EXPERIMENTALACTOR_PERFORMACTIONS)
};

class ExperimentalActorRequestTabObservationFunction
    : public ExperimentalActorApiFunction {
 public:
  ExperimentalActorRequestTabObservationFunction();
  ExperimentalActorRequestTabObservationFunction(
      const ExperimentalActorRequestTabObservationFunction&) = delete;
  ExperimentalActorRequestTabObservationFunction& operator=(
      const ExperimentalActorRequestTabObservationFunction&) = delete;

 protected:
  ~ExperimentalActorRequestTabObservationFunction() override;
  ResponseAction Run() override;
  void OnObservationFinished(
      actor::ActorKeyedService::TabObservationResult observation_result);
  DECLARE_EXTENSION_FUNCTION("experimentalActor.requestTabObservation",
                             EXPERIMENTALACTOR_REQUESTTABOBSERVATION)
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_ACTOR_EXPERIMENTAL_ACTOR_API_H_
