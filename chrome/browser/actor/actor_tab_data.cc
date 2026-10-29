// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_tab_data.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/rand_util.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/ui/dom_node_geometry.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.equal.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actor {

namespace {

double ShouldRecordApcComparison() {
  if (!base::FeatureList::IsEnabled(features::kGlicActorApcComparison)) {
    return false;
  }

  double sampling_rate =
      std::clamp(features::kGlicActorApcComparisonSamplingRate.Get(), 0.0, 1.0);
  return base::ShouldRecordSubsampledMetric(sampling_rate);
}

}  // namespace

DEFINE_USER_DATA(ActorTabData);

ActorTabData::ActorTabData(tabs::TabInterface* tab)
    : scoped_unowned_user_data_(std::in_place,
                                tab->GetUnownedUserDataHost(),
                                *this) {}

ActorTabData::ActorTabData() = default;

ActorTabData::~ActorTabData() = default;

ActorTabData* ActorTabData::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

ActorTabData* ActorTabData::From(ActorSurfaceHandle actor_surface_handle) {
  ActorSurface* actor_surface = actor_surface_handle.Get();
  return actor_surface ? actor_surface->GetActorTabData() : nullptr;
}

void ActorTabData::DidObserveContent(
    const optimization_guide::AIPageContentResult& result,
    ApcSource source) {
  DidObserveContent(result.proto, source);
  last_observed_script_tool_ids_ = result.script_tool_ids;
}

void ActorTabData::DidObserveContent(
    const optimization_guide::proto::AnnotatedPageContent& content,
    ApcSource source) {
  if (last_observed_page_content_ && ShouldRecordApcComparison()) {
    // TODO(b/508314200): Consider granularizing this metric by the tool to see
    // if certain tools are more likely to result in identical content.
    RecordApcComparisonIdentical(source,
                                 *last_observed_page_content_ == content);
  }

  last_observed_page_content_.emplace(content);
  last_observed_script_tool_ids_.clear();
  last_observed_dom_node_geometry_.reset();
}

std::optional<base::UnguessableToken> ActorTabData::GetLastObservedScriptToolId(
    const base::UnguessableToken& document_id,
    const std::string& tool_name) const {
  // The map is keyed by serialized document identifiers.
  auto document_it =
      last_observed_script_tool_ids_.find(document_id.ToString());
  if (document_it == last_observed_script_tool_ids_.end()) {
    return std::nullopt;
  }
  auto tool_it = document_it->second.find(tool_name);
  if (tool_it == document_it->second.end()) {
    return std::nullopt;
  }
  return tool_it->second;
}

const optimization_guide::proto::AnnotatedPageContent*
ActorTabData::GetLastObservedPageContent() {
  if (!last_observed_page_content_) {
    return nullptr;
  }
  return &last_observed_page_content_.value();
}

const ui::DomNodeGeometry* ActorTabData::GetLastObservedDomNodeGeometry() {
  if (!last_observed_dom_node_geometry_ &&
      base::FeatureList::IsEnabled(features::kGlicActorUiMagicCursor)) {
    if (last_observed_page_content_.has_value()) {
      // Disabled unless Magic Cursor is enabled to improve latency.
      last_observed_dom_node_geometry_ =
          ui::DomNodeGeometry::InitFromApc(last_observed_page_content_.value());
    }
  }
  return last_observed_dom_node_geometry_.get();
}

void ActorTabData::SetLastRendererResolvedTarget(const gfx::Point& point) {
  last_renderer_resolved_target_ = point;
}

std::optional<gfx::Point> ActorTabData::GetLastRendererResolvedTarget() {
  return last_renderer_resolved_target_;
}

}  // namespace actor
