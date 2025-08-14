// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_H_
#define CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_H_

#include "chrome/browser/actor/shared_types.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "ui/gfx/geometry/point.h"

namespace actor::ui {

// LINT.IfChange(GetDomNodeResult)
// This enum is persisted in UMA logs. Do not change or reuse existing values.
enum class GetDomNodeResult {
  kSuccess = 0,
  kNoApcMainFrameData = 1,
  kNodeNotFoundInApc = 2,
  kNoGeometry = 3,
  kOffScreen = 4,
  kMaxValue = kOffScreen,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:GetDomNodeResult)

// Find the coordinates of the center of a DomNode given an AnnotatedPageContent
// proto.  Returns nullopt if the coordinates cannot be resolved.
std::optional<gfx::Point> GetDomNodePointFromApc(
    const optimization_guide::proto::AnnotatedPageContent& apc,
    const DomNode& node);

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_H_
