// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_notification_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kLineHeightDip = 20;
constexpr int kPaddingLeftDip = 16;
constexpr int kPaddingRightDip = 8;
constexpr int kPreferredHeightDip = 48;
constexpr int kShadowElevationDip = 6;

// Animation.
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

// Helpers ---------------------------------------------------------------------

views::View* CreateButton(
    const chromeos::assistant::mojom::AssistantNotificationButtonPtr& button,
    views::ButtonListener* listener) {
  SuggestionChipView::Params params;
  params.text = base::UTF8ToUTF16(button->label);
  return new SuggestionChipView(params, listener);
}

}  // namespace

AssistantNotificationView::AssistantNotificationView(
    AssistantViewDelegate* delegate,
    const AssistantNotification* notification)
    : delegate_(delegate), notification_id_(notification->client_id) {
  InitLayout(notification);

  // The AssistantViewDelegate outlives the Assistant view hierarchy.
  delegate_->AddNotificationModelObserver(this);
}

AssistantNotificationView::~AssistantNotificationView() {
  delegate_->RemoveNotificationModelObserver(this);
}

const char* AssistantNotificationView::GetClassName() const {
  return "AssistantNotificationView";
}

void AssistantNotificationView::AddedToWidget() {
  UpdateVisibility(/*visible=*/true);
}

gfx::Size AssistantNotificationView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantNotificationView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantNotificationView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  UpdateBackground();
}

void AssistantNotificationView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  const auto it = std::find(buttons_.begin(), buttons_.end(), sender);
  const int notification_button_index = std::distance(buttons_.begin(), it);
  delegate_->OnNotificationButtonPressed(notification_id_,
                                         notification_button_index);
}

void AssistantNotificationView::OnImplicitAnimationsCompleted() {
  // When the view's layer has animated to |0.f| opacity, the underlying
  // notification has been removed and our associated view can be deleted. Note
  // that we check for opacity within epsilon to avoid exact float comparison.
  if (cc::MathUtil::IsWithinEpsilon(layer()->opacity(), 0.f))
    delete this;
}

void AssistantNotificationView::OnNotificationUpdated(
    const AssistantNotification* notification) {
  // We only care about the |notification| being updated if it is the
  // notification associated with this view.
  if (notification->client_id != notification_id_)
    return;

  using AssistantNotificationType =
      chromeos::assistant::mojom::AssistantNotificationType;

  // If the notification associated with this view is no longer of type
  // |kInAssistant|, it should not be shown in Assistant UI.
  if (notification->type != AssistantNotificationType::kInAssistant) {
    UpdateVisibility(/*visible=*/false);
    return;
  }

  // Title/Message.
  title_->SetText(base::UTF8ToUTF16(notification->title));
  message_->SetText(base::UTF8ToUTF16(notification->message));

  // Old buttons.
  for (views::View* button : buttons_)
    delete button;
  buttons_.clear();

  // New buttons.
  for (const auto& notification_button : notification->buttons) {
    views::View* button = CreateButton(notification_button, /*listener=*/this);
    container_->AddChildView(button);
    buttons_.push_back(button);
  }

  // Because |container_| has a fixed size, we need to explicitly trigger a
  // layout/paint pass ourselves when manipulating child views.
  container_->Layout();
  container_->SchedulePaint();
}

void AssistantNotificationView::OnNotificationRemoved(
    const AssistantNotification* notification,
    bool from_server) {
  if (notification->client_id == notification_id_)
    UpdateVisibility(/*visible=*/false);
}

void AssistantNotificationView::InitLayout(
    const AssistantNotification* notification) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Initialize opacity to |0.f| as the layer for this view will be animated in
  // when the view is added to its widget.
  layer()->SetOpacity(0.f);

  // Background/shadow.
  background_layer_.SetFillsBoundsOpaquely(false);
  layer()->Add(&background_layer_);

  // Container.
  container_ = new views::View();
  container_->SetPreferredSize(gfx::Size(INT_MAX, INT_MAX));
  container_->SetPaintToLayer();
  container_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(container_);

  auto* layout_manager =
      container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingLeftDip, 0, kPaddingRightDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  gfx::FontList font_list =
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(1);

  // Title.
  title_ = new views::Label(base::UTF8ToUTF16(notification->title));
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(kTextColorPrimary);
  title_->SetFontList(font_list.DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_->SetLineHeight(kLineHeightDip);
  container_->AddChildView(title_);

  // Message.
  message_ = new views::Label(base::UTF8ToUTF16(notification->message));
  message_->SetAutoColorReadabilityEnabled(false);
  message_->SetEnabledColor(kTextColorSecondary);
  message_->SetFontList(font_list);
  message_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  message_->SetLineHeight(kLineHeightDip);
  container_->AddChildView(message_);

  layout_manager->SetFlexForView(message_, 1);

  // Buttons.
  for (const auto& notification_button : notification->buttons) {
    views::View* button = CreateButton(notification_button, /*listener=*/this);
    container_->AddChildView(button);
    buttons_.push_back(button);
  }
}

void AssistantNotificationView::UpdateBackground() {
  gfx::ShadowValues shadow_values =
      gfx::ShadowValue::MakeMdShadowValues(kShadowElevationDip);

  shadow_delegate_ = std::make_unique<views::BorderShadowLayerDelegate>(
      shadow_values, GetLocalBounds(),
      /*fill_color=*/SK_ColorWHITE,
      /*corner_radius=*/height() / 2);

  background_layer_.set_delegate(shadow_delegate_.get());
  background_layer_.SetBounds(
      gfx::ToEnclosingRect(shadow_delegate_->GetPaintedBounds()));
}

void AssistantNotificationView::UpdateVisibility(bool visible) {
  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());

  // We observe the animation to receive an event on its completion. When the
  // layer for this view has completed animating to a hidden state, this view
  // is deleted as the underlying notification has been removed.
  animation.AddObserver(this);

  // Parameters.
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation.SetTransitionDuration(kAnimationDuration);
  animation.SetTweenType(gfx::Tween::Type::EASE_IN_OUT);

  // Animate opacity to a visible/hidden state.
  layer()->SetOpacity(visible ? 1.f : 0.f);
}

}  // namespace ash
