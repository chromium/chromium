// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_occlusion_notifier.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/common/chrome_features.h"
#include "ui/views/widget/widget.h"

namespace glic {

GlicOcclusionNotifier::GlicOcclusionNotifier(GlicInstance& instance)
    : glic_instance_(instance) {
  glic_instance_->AddStateObserver(this);
}

GlicOcclusionNotifier::~GlicOcclusionNotifier() {
  glic_instance_->RemoveStateObserver(this);
}

void GlicOcclusionNotifier::PanelStateChanged(
    const mojom::PanelState& panel_state,
    const GlicWindowController::PanelStateContext& context) {
  // Under GlicMultiInstance, occlusion tracking is managed through
  // GlicFloatingUi.
  if (GlicEnabling::IsMultiInstanceEnabled()) {
    return;
  }

  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  if (!glic_instance_->IsShowing() || glic_instance_->IsAttached() ||
      !tracker) {
    return;
  }

  if (!context.glic_widget) {
    return;
  }

  if (panel_state.kind == mojom::PanelStateKind::kDetached) {
    tracker->OnPictureInPictureWidgetOpened(context.glic_widget);
  } else {
    tracker->RemovePictureInPictureWidget(context.glic_widget);
  }
}

}  // namespace glic
