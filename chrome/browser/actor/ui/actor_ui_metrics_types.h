// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_TYPES_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_TYPES_H_

namespace actor::ui {

// LINT.IfChange(ActorUiTabControllerError)
// These enum values are persisted to logs.  Do not renumber or reuse numeric
// values.

enum class ActorUiTabControllerError {
  kRequestedForNonExistentTab = 0,
  kCallbackError = 1,
  kMaxValue = kCallbackError,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorUiTabControllerError)

// LINT.IfChange(ComputedTargetResult)
// These enum values are persisted to logs.  Do not renumber or reuse numeric
// values.
enum class ComputedTargetResult {
  kSuccess = 0,
  kMissingActorTabData = 1,
  kMissingAnnotatedPageContent = 2,
  kTargetNotResolvedInApc = 3,
  kMaxValue = kTargetNotResolvedInApc,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ComputedTargetResult)

// LINT.IfChange(RendererResolvedTargetResult)
// These enum values are persisted to logs.  Do not renumber or reuse numeric
// values.
enum class RendererResolvedTargetResult {
  kSuccess = 0,
  kMissingActorTabData = 1,
  kRendererResolvedTargetHasNoValue = 2,
  kMaxValue = kRendererResolvedTargetHasNoValue,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:RendererResolvedTargetResult)

// LINT.IfChange(ModelPageTargetType)
// These enum values are persisted to logs.  Do not renumber or reuse numeric
// values.
enum class ModelPageTargetType {
  kDomNode = 0,
  kPoint = 1,
  kMaxValue = kPoint,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ModelPageTargetType)

// LINT.IfChange(ActorUiTaskIconError)
enum class ActorUiTaskIconError {
  // Counts the number of times we try to create a task list bubble row for an
  // invalid task id. Invalid meaning not in active or inactive task records.
  kBubbleTaskDoesntExist = 0,
  // Counts the number of times we try to create a task icon/nudge for an
  // invalid task id. Invalid meaning not in active or inactive task records.
  kNudgeTaskDoesntExist = 1,
  kMaxValue = kNudgeTaskDoesntExist,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorUiTaskIconError)

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_TYPES_H_
