// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/controls/contextual_nudge.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

views::BubbleBorder::Arrow GetArrowForPosition(
    ContextualNudge::Position position) {
  switch (position) {
    case ContextualNudge::Position::kTop:
      return views::BubbleBorder::BOTTOM_CENTER;
    case ContextualNudge::Position::kBottom:
      return views::BubbleBorder::TOP_CENTER;
  }
}

}  // namespace

ContextualNudge::ContextualNudge(views::View* anchor,
                                 aura::Window* parent_window,
                                 Position position,
                                 const gfx::Insets& margins,
                                 const std::u16string& text,
                                 const base::RepeatingClosure& tap_callback)
    : views::BubbleDialogDelegateView(anchor,
                                      GetArrowForPosition(position),
                                      views::BubbleBorder::NO_SHADOW),
      tap_callback_(tap_callback) {
  // Bubbles that use transparent colors should not paint their ClientViews to a
  // layer as doing so could result in visual artifacts.
  SetPaintClientToLayer(false);
  set_color(SK_ColorTRANSPARENT);
  set_close_on_deactivate(false);
  set_margins(gfx::Insets());
  set_accept_events(!tap_callback.is_null());
  SetCanActivate(false);
  set_shadow(views::BubbleBorder::NO_SHADOW);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  if (parent_window) {
    set_parent_window(parent_window);
  } else if (anchor_widget()) {
    set_parent_window(
        anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
            kShellWindowId_ShelfContainer));
  }

  SetLayoutManager(std::make_unique<views::FillLayout>());

  label_ = AddChildView(std::make_unique<views::Label>(text));
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  label_->SetBorder(views::CreateEmptyBorder(margins));
  if (chromeos::features::IsJellyEnabled()) {
    label_->SetEnabledColorId(cros_tokens::kCrosSysSecondary);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                          *label_);
  } else {
    label_->SetEnabledColorId(kColorAshTextColorPrimary);
  }

  views::BubbleDialogDelegateView::CreateBubble(this);

  // TODO(sanchit.abrol@microsoft.com): Move back among the other setters after
  // the platform default setting is moved from
  // |BubbleDialogDelegateView::CreateBubble| to being the default value at
  // bubble construction.
  set_adjust_if_offscreen(false);

  // Text box for shelf nudge should be ignored for collision detection.
  CollisionDetectionUtils::IgnoreWindowForCollisionDetection(
      GetWidget()->GetNativeWindow());
}

ContextualNudge::~ContextualNudge() = default;

void ContextualNudge::UpdateAnchorRect(const gfx::Rect& rect) {
  SetAnchorRect(rect);
}

ui::LayerType ContextualNudge::GetLayerType() const {
  return ui::LAYER_NOT_DRAWN;
}

void ContextualNudge::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap && tap_callback_) {
    event->StopPropagation();
    tap_callback_.Run();
    return;
  }

  // Pass on non tap events to the shelf (so it can handle swipe gestures that
  // start on top of the nudge). Convert event to screen coordinates, as this is
  // what Shelf::ProcessGestureEvent() expects.
  ui::GestureEvent event_in_screen(*event);
  gfx::Point location_in_screen(event->location());
  View::ConvertPointToScreen(this, &location_in_screen);
  event_in_screen.set_location(location_in_screen);

  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  if (shelf->ProcessGestureEvent(event_in_screen))
    event->StopPropagation();
  else
    views::BubbleDialogDelegateView::OnGestureEvent(event);
}

}  // namespace ash
