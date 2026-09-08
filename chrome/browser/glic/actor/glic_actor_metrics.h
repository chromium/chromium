// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_METRICS_H_
#define CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_METRICS_H_

#include <string_view>

namespace glic {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(GlicActorTaskIdMismatchMethod)
enum class GlicActorTaskIdMismatchMethod {
  kPerformActions = 0,
  kCancelActions = 1,
  kStopActorTask = 2,
  kPauseActorTask = 3,
  kResumeActorTask = 4,
  kInterruptActorTask = 5,
  kUninterruptActorTask = 6,
  kUpdateActorTaskStepProgress = 7,
  kCreateActorTab = 8,
  kMaxValue = kCreateActorTab,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicActorTaskIdMismatchMethod)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(GlicActorTaskIdMismatchReason)
enum class GlicActorTaskIdMismatchReason {
  // Session has no active task, but client provided a non-null task ID.
  kNoCurrentTask = 0,
  // Session has an active task, but client provided a null or empty task ID.
  kProvidedTaskIdNull = 1,
  // Session has an active task, but client provided a different task ID.
  kTaskIdMismatch = 2,
  // Neither the session nor the client specified a valid task ID.
  kBothNull = 3,
  kMaxValue = kBothNull,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicActorTaskIdMismatchReason)

// Returns the string representation of `method` (matching Mojo method name).
std::string_view ToString(GlicActorTaskIdMismatchMethod method);

// Returns the string representation of `reason`.
std::string_view ToString(GlicActorTaskIdMismatchReason reason);

// Records whether the client-provided task ID matched the session's active task
// ID. Emitted on every validation check.
void RecordTaskIdMatchesCurrent(bool matches);

// Records the method and reason for a task ID mismatch. Emitted only when
// task ID validation fails.
void RecordTaskIdMismatch(GlicActorTaskIdMismatchMethod method,
                          GlicActorTaskIdMismatchReason reason);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_METRICS_H_
