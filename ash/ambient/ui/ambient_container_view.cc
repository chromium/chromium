// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include <memory>
#include <utility>

#include "ash/ambient/ui/ambient_assistant_container_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using chromeos::assistant::features::IsAmbientAssistantEnabled;

// Appearance.
constexpr int kAssistantPreferredHeightDip = 128;

}  // namespace

AmbientContainerView::AmbientContainerView(AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kAmbientContainerView);
  Init();
}

AmbientContainerView::~AmbientContainerView() = default;

const char* AmbientContainerView::GetClassName() const {
  return "AmbientContainerView";
}

gfx::Size AmbientContainerView::CalculatePreferredSize() const {
  // TODO(b/139953389): Handle multiple displays.
  return GetWidget()->GetNativeWindow()->GetRootWindow()->bounds().size();
}

void AmbientContainerView::Layout() {
  // Layout child views first to have proper bounds set for children.
  LayoutPhotoView();

  // The assistant view may not exist if |kAmbientAssistant| feature is
  // disabled.
  if (ambient_assistant_container_view_)
    LayoutAssistantView();

  View::Layout();
}

void AmbientContainerView::Init() {
  // TODO(b/139954108): Choose a better dark mode theme color.
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  // Updates focus behavior to receive key press events.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  photo_view_ = AddChildView(std::make_unique<PhotoView>(delegate_));

  if (IsAmbientAssistantEnabled()) {
    ambient_assistant_container_view_ =
        AddChildView(std::make_unique<AmbientAssistantContainerView>());
    ambient_assistant_container_view_->SetVisible(false);
  }
}

void AmbientContainerView::LayoutPhotoView() {
  // |photo_view_| should have the same size as the widget.
  photo_view_->SetBoundsRect(GetLocalBounds());
}

void AmbientContainerView::LayoutAssistantView() {
  int preferred_width = GetPreferredSize().width();
  int preferred_height = kAssistantPreferredHeightDip;
  ambient_assistant_container_view_->SetBoundsRect(
      gfx::Rect(0, 0, preferred_width, preferred_height));
}

}  // namespace ash
