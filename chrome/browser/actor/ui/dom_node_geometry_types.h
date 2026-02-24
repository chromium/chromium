// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_TYPES_H_
#define CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_TYPES_H_

namespace actor::ui {
// LINT.IfChange(GetDomNodeResult)
// This enum is persisted in UMA logs. Do not change or reuse existing values.
enum class GetDomNodeResult {
  kSuccess = 0,
  kNoApcMainFrameData = 1,
  kNodeNotFoundInApc = 2,
  kNoGeometry = 3,
  kOffScreen = 4,
  kEmptyBoundingBox = 5,
  kMaxValue = kEmptyBoundingBox,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:GetDomNodeResult)

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_TYPES_H_
