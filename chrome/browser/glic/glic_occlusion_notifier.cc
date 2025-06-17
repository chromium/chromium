// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_occlusion_notifier.h"

#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

namespace glic {

GlicOcclusionNotifier::GlicOcclusionNotifier(
    GlicWindowController& window_controller)
    : window_controller_(window_controller) {
  window_controller_->AddStateObserver(this);
}

GlicOcclusionNotifier::~GlicOcclusionNotifier() {
  window_controller_->RemoveStateObserver(this);
}

void GlicOcclusionNotifier::PanelStateChanged(
    const mojom::PanelState& panel_state,
    Browser*) {
  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  if (!tracker) {
    return;
  }

  views::Widget* glic_widget = window_controller_->GetGlicWidget();
  if (!glic_widget) {
    return;
  }

  if (panel_state.kind == mojom::PanelState_Kind::kDetached) {
    tracker->OnPictureInPictureWidgetOpened(glic_widget);
  } else {
    tracker->RemovePictureInPictureWidget(glic_widget);
  }
}

}  // namespace glic
