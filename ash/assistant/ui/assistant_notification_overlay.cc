// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_notification_overlay.h"

#include <memory>

#include "ash/assistant/ui/assistant_notification_view.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMarginBottomDip = 64;
constexpr int kMarginHorizontalDip = 32;

}  // namespace

AssistantNotificationOverlay::AssistantNotificationOverlay(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();

  // The AssistantViewDelegate outlives the Assistant view hierarchy.
  delegate_->AddNotificationModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

AssistantNotificationOverlay::~AssistantNotificationOverlay() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveNotificationModelObserver(this);
}

const char* AssistantNotificationOverlay::GetClassName() const {
  return "AssistantNotificationOverlay";
}

AssistantOverlay::LayoutParams AssistantNotificationOverlay::GetLayoutParams()
    const {
  using Gravity = AssistantOverlay::LayoutParams::Gravity;
  AssistantOverlay::LayoutParams layout_params;
  layout_params.gravity = Gravity::kBottom | Gravity::kCenterHorizontal;
  layout_params.margins = gfx::Insets(0, kMarginHorizontalDip, kMarginBottomDip,
                                      kMarginHorizontalDip);
  return layout_params;
}

void AssistantNotificationOverlay::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent == this)
    PreferredSizeChanged();
}

void AssistantNotificationOverlay::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (new_visibility != AssistantVisibility::kVisible)
    return;

  // We need to create views for any notifications that currently exist of type
  // |kInAssistant| as they should be shown within Assistant UI.
  using chromeos::assistant::mojom::AssistantNotificationType;
  for (const auto* notification :
       delegate_->GetNotificationModel()->GetNotificationsByType(
           AssistantNotificationType::kInAssistant)) {
    AddChildView(new AssistantNotificationView(delegate_, notification));
  }
}

void AssistantNotificationOverlay::OnNotificationAdded(
    const AssistantNotification* notification) {
  // We only show notifications of type |kInAssistant| with Assistant UI.
  using chromeos::assistant::mojom::AssistantNotificationType;
  if (notification->type == AssistantNotificationType::kInAssistant)
    AddChildView(new AssistantNotificationView(delegate_, notification));
}

void AssistantNotificationOverlay::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

}  // namespace ash
