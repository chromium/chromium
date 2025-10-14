// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/ui/dom_node_geometry.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/actor/ui/ui_event_debugstring.h"

namespace actor::ui {
namespace {
using base::UmaHistogramEnumeration;

constexpr std::string_view kComputedTargetResultHistogram =
    "Actor.EventDispatcher.ComputedTargetResult";
constexpr std::string_view kModelPageTargetTypeHistogram =
    "Actor.EventDispatcher.ModelPageTargetType";

}  // namespace

AsyncUiEvent ComputedMouseMove(tabs::TabInterface::Handle tab,
                               const PageTarget& target) {
  if (std::holds_alternative<gfx::Point>(target)) {
    UmaHistogramEnumeration(kModelPageTargetTypeHistogram,
                            ModelPageTargetType::kPoint);
    return MouseMove(tab, std::get<gfx::Point>(target),
                     TargetSource::kToolRequest);
  }

  UmaHistogramEnumeration(kModelPageTargetTypeHistogram,
                          ModelPageTargetType::kDomNode);

  // Try to compute a point target by converting the DomNode to a gfx::Point.
  auto* actor_tab_data = ActorTabData::From(tab.Get());
  if (!actor_tab_data) {
    VLOG(4) << "ComputedMouseMove: No ActorTabData available for tab "
            << tab.raw_value();
    UmaHistogramEnumeration(kComputedTargetResultHistogram,
                            ComputedTargetResult::kMissingActorTabData);
    return MouseMove(tab, std::nullopt, TargetSource::kUnresolvableInApc);
  }

  auto* geom = actor_tab_data->GetLastObservedDomNodeGeometry();
  if (!geom) {
    VLOG(4)
        << "ComputedMouseMove: No cached APC/DomNodeGeometry available for tab "
        << tab.raw_value();
    UmaHistogramEnumeration(kComputedTargetResultHistogram,
                            ComputedTargetResult::kMissingAnnotatedPageContent);
    return MouseMove(tab, std::nullopt, TargetSource::kUnresolvableInApc);
  }

  auto pt_target = geom->GetDomNode(std::get<DomNode>(target));
  if (!pt_target.has_value()) {
    VLOG(4) << "ComputedMouseMove: Failed to resolve point target for "
            << DebugString(target);
    UmaHistogramEnumeration(kComputedTargetResultHistogram,
                            ComputedTargetResult::kTargetNotResolvedInApc);
    return MouseMove(tab, std::nullopt, TargetSource::kUnresolvableInApc);
  }
  VLOG(4) << "Converted PageTarget: " << std::get<DomNode>(target) << " to "
          << pt_target.value();
  UmaHistogramEnumeration(kComputedTargetResultHistogram,
                          ComputedTargetResult::kSuccess);
  return MouseMove(tab, pt_target.value(), TargetSource::kDerivedFromApc);
}

}  // namespace actor::ui
