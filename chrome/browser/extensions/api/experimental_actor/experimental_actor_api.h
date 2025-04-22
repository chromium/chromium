// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_ACTOR_EXPERIMENTAL_ACTOR_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_ACTOR_EXPERIMENTAL_ACTOR_API_H_

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

// Starts an actor task.
class ExperimentalActorStartTaskFunction : public ExperimentalActorApiFunction {
 public:
  ExperimentalActorStartTaskFunction();

  ExperimentalActorStartTaskFunction(
      const ExperimentalActorStartTaskFunction&) = delete;
  ExperimentalActorStartTaskFunction& operator=(
      const ExperimentalActorStartTaskFunction&) = delete;

 protected:
  ~ExperimentalActorStartTaskFunction() override;
  ResponseAction Run() override;
  void OnTaskStarted(
      optimization_guide::proto::BrowserStartTaskResult response);

  DECLARE_EXTENSION_FUNCTION("experimentalActor.startTask",
                             EXPERIMENTALACTOR_STARTTASK)
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
  void OnTaskStopped(bool success);

  DECLARE_EXTENSION_FUNCTION("experimentalActor.stopTask",
                             EXPERIMENTALACTOR_STOPTASK)
};

// Executes an actor action.
class ExperimentalActorExecuteActionFunction
    : public ExperimentalActorApiFunction {
 public:
  ExperimentalActorExecuteActionFunction();

  ExperimentalActorExecuteActionFunction(
      const ExperimentalActorExecuteActionFunction&) = delete;
  ExperimentalActorExecuteActionFunction& operator=(
      const ExperimentalActorExecuteActionFunction&) = delete;

 protected:
  ~ExperimentalActorExecuteActionFunction() override;
  ResponseAction Run() override;
  void OnResponseReceived(
      optimization_guide::proto::BrowserActionResult response);

  DECLARE_EXTENSION_FUNCTION("experimentalActor.executeAction",
                             EXPERIMENTALACTOR_EXECUTEACTION)
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_ACTOR_EXPERIMENTAL_ACTOR_API_H_
