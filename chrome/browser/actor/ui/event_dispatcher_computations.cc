// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_metrics_types.h"
#include "chrome/browser/actor/ui/dom_node_geometry.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/actor/ui/ui_event_debugstring.h"
#include "chrome/common/chrome_features.h"

namespace actor::ui {
namespace {
using base::UmaHistogramEnumeration;
using PointTarget = std::pair<std::optional<gfx::Point>, TargetSource>;

std::optional<gfx::Point> PageTargetPoint(const PageTarget& target) {
  if (std::holds_alternative<gfx::Point>(target)) {
    return std::get<gfx::Point>(target);
  }
  return std::nullopt;
}

PointTarget FetchPointFromRenderer(ActorTabData* actor_tab_data,
                                   const PageTarget& target) {
  if (actor_tab_data == nullptr) {
    VLOG(4) << "FetchPointFromRenderer: No ActorTabData available for tab";
    RecordRendererResolvedTargetResult(
        RendererResolvedTargetResult::kMissingActorTabData);
    return {std::nullopt, TargetSource::kUnresolvableFromRenderer};
  }
  std::optional<gfx::Point> renderer_resolved_point =
      actor_tab_data->GetLastRendererResolvedTarget();
  if (!renderer_resolved_point.has_value()) {
    VLOG(4) << "FetchPointFromRenderer: No cached renderer resolved target "
               "available for tab";
    RecordRendererResolvedTargetResult(
        RendererResolvedTargetResult::kRendererResolvedTargetHasNoValue);
    return {std::nullopt, TargetSource::kUnresolvableFromRenderer};
  }
  VLOG(4) << "Retrieved renderer resolved target: "
          << renderer_resolved_point.value().ToString();
  RecordRendererResolvedTargetResult(RendererResolvedTargetResult::kSuccess);
  return {renderer_resolved_point.value(), TargetSource::kRendererResolved};
}

PointTarget DerivePointFromApc(ActorTabData* actor_tab_data,
                               const PageTarget& target) {
  // Try to compute a point target by converting the DomNode to a gfx::Point.
  if (actor_tab_data == nullptr) {
    VLOG(4) << "DerivePointFromApc: No ActorTabData available for tab";
    RecordComputedTargetResult(ComputedTargetResult::kMissingActorTabData);
    return {std::nullopt, TargetSource::kUnresolvableInApc};
  }

  auto* geom = actor_tab_data->GetLastObservedDomNodeGeometry();
  if (!geom) {
    VLOG(4) << "DerivePointFromApc: No cached APC/DomNodeGeometry available "
               "for tab";
    RecordComputedTargetResult(
        ComputedTargetResult::kMissingAnnotatedPageContent);
    return {std::nullopt, TargetSource::kUnresolvableInApc};
  }

  auto pt_target = geom->GetDomNode(std::get<DomNode>(target));
  if (!pt_target.has_value()) {
    VLOG(4) << "DerivePointFromApc: Failed to resolve point target for "
            << DebugString(target);
    RecordComputedTargetResult(ComputedTargetResult::kTargetNotResolvedInApc);
    return {std::nullopt, TargetSource::kUnresolvableInApc};
  }
  VLOG(4) << "Converted PageTarget: " << std::get<DomNode>(target) << " to "
          << pt_target.value().ToString();
  RecordComputedTargetResult(ComputedTargetResult::kSuccess);
  return {pt_target.value(), TargetSource::kDerivedFromApc};
}
}  // namespace

PointTarget ComputeMouseTarget(tabs::TabInterface::Handle tab,
                               const PageTarget& target) {
  ActorTabData* actor_tab_data = ActorTabData::From(tab.Get());
  if (base::FeatureList::IsEnabled(
          features::kGlicActorSplitValidateAndExecute) &&
      base::FeatureList::IsEnabled(features::kGlicActorUiMagicCursor)) {
    return FetchPointFromRenderer(actor_tab_data, target);
  }

  if (std::holds_alternative<gfx::Point>(target)) {
    RecordModelPageTargetType(ModelPageTargetType::kPoint);
  } else {
    RecordModelPageTargetType(ModelPageTargetType::kDomNode);
  }
  auto tool_request_point = [](gfx::Point pt) -> PointTarget {
    return {pt, TargetSource::kToolRequest};
  };
  return PageTargetPoint(target)
      .transform(tool_request_point)
      .value_or(DerivePointFromApc(actor_tab_data, target));
}

}  // namespace actor::ui
