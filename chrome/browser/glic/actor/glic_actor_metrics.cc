// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace glic {

std::string_view ToString(GlicActorTaskIdMismatchMethod method) {
  switch (method) {
    case GlicActorTaskIdMismatchMethod::kPerformActions:
      return "PerformActions";
    case GlicActorTaskIdMismatchMethod::kCancelActions:
      return "CancelActions";
    case GlicActorTaskIdMismatchMethod::kStopActorTask:
      return "StopActorTask";
    case GlicActorTaskIdMismatchMethod::kPauseActorTask:
      return "PauseActorTask";
    case GlicActorTaskIdMismatchMethod::kResumeActorTask:
      return "ResumeActorTask";
    case GlicActorTaskIdMismatchMethod::kInterruptActorTask:
      return "InterruptActorTask";
    case GlicActorTaskIdMismatchMethod::kUninterruptActorTask:
      return "UninterruptActorTask";
    case GlicActorTaskIdMismatchMethod::kUpdateActorTaskStepProgress:
      return "UpdateActorTaskStepProgress";
    case GlicActorTaskIdMismatchMethod::kCreateActorTab:
      return "CreateActorTab";
  }
}

std::string_view ToString(GlicActorTaskIdMismatchReason reason) {
  switch (reason) {
    case GlicActorTaskIdMismatchReason::kNoCurrentTask:
      return "NoCurrentTask";
    case GlicActorTaskIdMismatchReason::kProvidedTaskIdNull:
      return "ProvidedTaskIdNull";
    case GlicActorTaskIdMismatchReason::kTaskIdMismatch:
      return "TaskIdMismatch";
    case GlicActorTaskIdMismatchReason::kBothNull:
      return "BothNull";
  }
}

void RecordTaskIdMatchesCurrent(bool matches) {
  base::UmaHistogramBoolean("Glic.Actor.TaskIdMatchesCurrent", matches);
}

void RecordTaskIdMismatch(GlicActorTaskIdMismatchMethod method,
                          GlicActorTaskIdMismatchReason reason) {
  base::UmaHistogramEnumeration("Glic.Actor.TaskIdMismatch.Method", method);
  base::UmaHistogramEnumeration("Glic.Actor.TaskIdMismatch.Reason", reason);
}

}  // namespace glic
