// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_view.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/interacted_by_tap_recorder.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pods_container_view.h"
#include "ash/system/unified/notification_hidden_view.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_system_info_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Border applied to SystemTrayContainer and DetailedViewContainer to iminate
// notification list scrolling under SystemTray part of UnifiedSystemTray.
// The border paints mock notification frame behind the top corners based on
// |height_below_scroll|.
class TopCornerBorder : public views::Border {
 public:
  TopCornerBorder() = default;

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped(canvas);

    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(view.GetLocalBounds()),
                      SkIntToScalar(kUnifiedTrayCornerRadius),
                      SkIntToScalar(kUnifiedTrayCornerRadius));
    canvas->sk_canvas()->clipPath(path, SkClipOp::kDifference, true);

    cc::PaintFlags flags;
    flags.setColor(message_center::kNotificationBackgroundColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);

    const int height = kUnifiedTrayCornerRadius * 4;
    canvas->DrawRoundRect(
        gfx::RectF(0,
                   -height + std::min(height_below_scroll_,
                                      kUnifiedTrayCornerRadius * 2),
                   view.width(), height),
        kUnifiedTrayCornerRadius, flags);
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(); }

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void set_height_below_scroll(int height_below_scroll) {
    height_below_scroll_ = height_below_scroll;
  }

 private:
  int height_below_scroll_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TopCornerBorder);
};

class SystemTrayContainer : public views::View {
 public:
  SystemTrayContainer() {
    SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
    SetBackground(UnifiedSystemTrayView::CreateBackground());
    SetBorder(std::make_unique<TopCornerBorder>());
  }

  ~SystemTrayContainer() override = default;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayContainer);
};

class DetailedViewContainer : public views::View {
 public:
  DetailedViewContainer() {
    SetBackground(UnifiedSystemTrayView::CreateBackground());
    SetBorder(std::make_unique<TopCornerBorder>());
  }

  ~DetailedViewContainer() override = default;

  // views::View:
  void Layout() override {
    for (int i = 0; i < child_count(); ++i)
      child_at(i)->SetBoundsRect(GetContentsBounds());
    views::View::Layout();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DetailedViewContainer);
};

}  // namespace

UnifiedSlidersContainerView::UnifiedSlidersContainerView(
    bool initially_expanded)
    : expanded_amount_(initially_expanded ? 1.0 : 0.0) {
  SetVisible(initially_expanded);
}

UnifiedSlidersContainerView::~UnifiedSlidersContainerView() = default;

void UnifiedSlidersContainerView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  SetVisible(expanded_amount > 0.0);
  expanded_amount_ = expanded_amount;
  InvalidateLayout();
  UpdateOpacity();
}

int UnifiedSlidersContainerView::GetExpandedHeight() const {
  int height = 0;
  for (int i = 0; i < child_count(); ++i)
    height += child_at(i)->GetHeightForWidth(kTrayMenuWidth);
  return height;
}

void UnifiedSlidersContainerView::Layout() {
  int y = 0;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    int height = child->GetHeightForWidth(kTrayMenuWidth);
    child->SetBounds(0, y, kTrayMenuWidth, height);
    y += height;
  }
}

gfx::Size UnifiedSlidersContainerView::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth, GetExpandedHeight() * expanded_amount_);
}

void UnifiedSlidersContainerView::UpdateOpacity() {
  const int height = GetPreferredSize().height();
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    double opacity = 1.0;
    if (child->y() > height) {
      opacity = 0.0;
    } else if (child->bounds().bottom() < height) {
      opacity = 1.0;
    } else {
      const double ratio =
          static_cast<double>(height - child->y()) / child->height();
      // TODO(tetsui): Confirm the animation curve with UX.
      opacity = std::max(0., 2. * ratio - 1.);
    }
    child->layer()->SetOpacity(opacity);
  }
}

// FocusSearch whose purpose is to start focus traversal from the top of
// SystemTrayContainer.
class UnifiedSystemTrayView::FocusSearch : public views::FocusSearch {
 public:
  explicit FocusSearch(UnifiedSystemTrayView* view)
      : views::FocusSearch(view, false, false), view_(view) {}
  ~FocusSearch() override = default;

  views::View* FindNextFocusableView(
      views::View* starting_view,
      FocusSearch::SearchDirection search_direction,
      FocusSearch::TraversalDirection traversal_direction,
      FocusSearch::StartingViewPolicy check_starting_view,
      FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override {
    // Initial view that is focused when first time Tab or Shift-Tab is pressed.
    views::View* default_start_view =
        search_direction == FocusSearch::SearchDirection::kForwards
            ? view_->system_tray_container_
            : view_->notification_hidden_view_;
    return views::FocusSearch::FindNextFocusableView(
        starting_view ? starting_view : default_start_view, search_direction,
        traversal_direction,
        starting_view ? check_starting_view
                      : StartingViewPolicy::kCheckStartingView,
        can_go_into_anchored_dialog, focus_traversable, focus_traversable_view);
  }

 private:
  UnifiedSystemTrayView* const view_;

  DISALLOW_COPY_AND_ASSIGN(FocusSearch);
};

UnifiedSystemTrayView::UnifiedSystemTrayView(
    UnifiedSystemTrayController* controller,
    bool initially_expanded)
    : expanded_amount_(initially_expanded ? 1.0 : 0.0),
      controller_(controller),
      notification_hidden_view_(new NotificationHiddenView()),
      top_shortcuts_view_(new TopShortcutsView(controller_)),
      feature_pods_container_(new FeaturePodsContainerView(initially_expanded)),
      sliders_container_(new UnifiedSlidersContainerView(initially_expanded)),
      system_info_view_(new UnifiedSystemInfoView(controller_)),
      system_tray_container_(new SystemTrayContainer()),
      detailed_view_container_(new DetailedViewContainer()),
      message_center_view_(
          new UnifiedMessageCenterView(this, controller->model())),
      focus_search_(std::make_unique<FocusSearch>(this)),
      interacted_by_tap_recorder_(
          std::make_unique<InteractedByTapRecorder>(this)) {
  DCHECK(controller_);

  auto* layout = SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  SetBackground(CreateBackground());
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SessionController* session_controller = Shell::Get()->session_controller();

  AddChildView(message_center_view_);
  layout->SetFlexForView(message_center_view_, 1);

  notification_hidden_view_->SetVisible(
      session_controller->GetUserSession(0) &&
      session_controller->IsScreenLocked() &&
      !AshMessageCenterLockScreenController::IsEnabled());
  AddChildView(notification_hidden_view_);

  AddChildView(system_tray_container_);

  system_tray_container_->AddChildView(top_shortcuts_view_);
  system_tray_container_->AddChildView(feature_pods_container_);
  system_tray_container_->AddChildView(sliders_container_);
  system_tray_container_->AddChildView(system_info_view_);

  detailed_view_container_->SetVisible(false);
  AddChildView(detailed_view_container_);

  // UnifiedSystemTrayView::FocusSearch makes focus traversal start from
  // |system_tray_container_|, but we have to complete the cycle by setting
  // |message_center_view_| next to |detailed_view_container_|.
  // Also, SetNextFocusableView does not support loop as mentioned in the doc,
  // we have to set null to |notification_hidden_view_|.
  notification_hidden_view_->SetNextFocusableView(nullptr);
  detailed_view_container_->SetNextFocusableView(message_center_view_);

  top_shortcuts_view_->SetExpandedAmount(expanded_amount_);
}

UnifiedSystemTrayView::~UnifiedSystemTrayView() = default;

void UnifiedSystemTrayView::SetMaxHeight(int max_height) {
  message_center_view_->SetMaxHeight(max_height);
}

void UnifiedSystemTrayView::AddFeaturePodButton(FeaturePodButton* button) {
  feature_pods_container_->AddChildView(button);
}

void UnifiedSystemTrayView::AddSliderView(views::View* slider_view) {
  slider_view->SetPaintToLayer();
  slider_view->layer()->SetFillsBoundsOpaquely(false);
  sliders_container_->AddChildView(slider_view);
}

void UnifiedSystemTrayView::SetDetailedView(views::View* detailed_view) {
  auto system_tray_size = system_tray_container_->GetPreferredSize();
  system_tray_container_->SetVisible(false);

  detailed_view_container_->RemoveAllChildViews(true /* delete_children */);
  detailed_view_container_->AddChildView(detailed_view);
  detailed_view_container_->SetVisible(true);
  detailed_view_container_->SetPreferredSize(system_tray_size);
  detailed_view->InvalidateLayout();
  Layout();
}

void UnifiedSystemTrayView::ResetDetailedView() {
  detailed_view_container_->RemoveAllChildViews(true /* delete_children */);
  detailed_view_container_->SetVisible(false);
  system_tray_container_->SetVisible(true);
  sliders_container_->UpdateOpacity();
  PreferredSizeChanged();
  Layout();
}

void UnifiedSystemTrayView::SaveFeaturePodFocus() {
  feature_pods_container_->SaveFocus();
}

void UnifiedSystemTrayView::RestoreFeaturePodFocus() {
  feature_pods_container_->RestoreFocus();
}

void UnifiedSystemTrayView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  expanded_amount_ = expanded_amount;

  top_shortcuts_view_->SetExpandedAmount(expanded_amount);
  feature_pods_container_->SetExpandedAmount(expanded_amount);
  sliders_container_->SetExpandedAmount(expanded_amount);

  if (!IsTransformEnabled()) {
    PreferredSizeChanged();
    // It is possible that the ratio between |message_center_view_| and others
    // can change while the bubble size remain unchanged.
    Layout();
    return;
  }

  if (height() != GetExpandedHeight())
    PreferredSizeChanged();
  Layout();
}

int UnifiedSystemTrayView::GetExpandedHeight() const {
  return (notification_hidden_view_->visible()
              ? notification_hidden_view_->GetPreferredSize().height()
              : 0) +
         top_shortcuts_view_->GetPreferredSize().height() +
         feature_pods_container_->GetExpandedHeight() +
         sliders_container_->GetExpandedHeight() +
         system_info_view_->GetPreferredSize().height();
}

int UnifiedSystemTrayView::GetCurrentHeight() const {
  return GetPreferredSize().height();
}

bool UnifiedSystemTrayView::IsTransformEnabled() const {
  // TODO(tetsui): Support animation by transform even when
  // UnifiedMessageCenterview is visible.
  return expanded_amount_ != 0.0 && expanded_amount_ != 1.0 &&
         !message_center_view_->visible();
}

void UnifiedSystemTrayView::SetNotificationHeightBelowScroll(
    int height_below_scroll) {
  static_cast<TopCornerBorder*>(system_tray_container_->border())
      ->set_height_below_scroll(height_below_scroll);
  static_cast<TopCornerBorder*>(detailed_view_container_->border())
      ->set_height_below_scroll(height_below_scroll);
  SchedulePaint();
}

// static
std::unique_ptr<views::Background> UnifiedSystemTrayView::CreateBackground() {
  return views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(
          app_list_features::IsBackgroundBlurEnabled()
              ? kUnifiedMenuBackgroundColorWithBlur
              : kUnifiedMenuBackgroundColor,
          kUnifiedTrayCornerRadius));
}

void UnifiedSystemTrayView::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point screen_location = event->location();
  ConvertPointToScreen(this, &screen_location);

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      controller_->BeginDrag(screen_location);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      controller_->UpdateDrag(screen_location);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      controller_->EndDrag(screen_location);
      event->SetHandled();
      break;
    case ui::ET_SCROLL_FLING_START:
      controller_->Fling(event->details().velocity_y());
      break;
    default:
      break;
  }
}

void UnifiedSystemTrayView::ChildPreferredSizeChanged(views::View* child) {
  // The size change is not caused by SetExpandedAmount(), because they don't
  // trigger PreferredSizeChanged().
  PreferredSizeChanged();
}

views::FocusTraversable* UnifiedSystemTrayView::GetFocusTraversable() {
  return this;
}

views::FocusSearch* UnifiedSystemTrayView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* UnifiedSystemTrayView::GetFocusTraversableParent() {
  return nullptr;
}

views::View* UnifiedSystemTrayView::GetFocusTraversableParentView() {
  return this;
}

}  // namespace ash
