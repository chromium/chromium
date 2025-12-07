// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_tab_data.h"

#include "chrome/browser/actor/ui/dom_node_geometry.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actor {

DEFINE_USER_DATA(ActorTabData);

ActorTabData::ActorTabData(tabs::TabInterface* tab)
    : scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

ActorTabData::~ActorTabData() = default;

ActorTabData* ActorTabData::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void ActorTabData::DidObserveContent(
    optimization_guide::proto::AnnotatedPageContent& content) {
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
      features::kGlicActorUiOverlayMagicCursor.Get()) {
    if (last_observed_page_content_.has_value()) {
      // Disabled unless Magic Cursor is enabled to improve latency.
      last_observed_dom_node_geometry_ =
          ui::DomNodeGeometry::InitFromApc(last_observed_page_content_.value());
    }
  }
  return last_observed_dom_node_geometry_.get();
}

}  // namespace actor
