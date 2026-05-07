// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_tab_data.h"

#include <algorithm>
#include <optional>

#include "base/feature_list.h"
#include "base/rand_util.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/ui/dom_node_geometry.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.equal.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
    : scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

ActorTabData::~ActorTabData() = default;

ActorTabData* ActorTabData::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
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
  last_observed_dom_node_geometry_.reset();
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
