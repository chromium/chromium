// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_view.h"

#include <numeric>

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/interacted_by_tap_recorder.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pods_container_view.h"
#include "ash/system/unified/notification_hidden_view.h"
#include "ash/system/unified/page_indicator_view.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_managed_device_view.h"
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

namespace ash {

namespace {

// Border applied to SystemTrayContainer and DetailedViewContainer to iminate
// notification list scrolling under SystemTray part of UnifiedSystemTray.
// The border paints mock notification frame behind the top corners based on
// |rect_below_scroll|.
class TopCornerBorder : public views::Border {
 public:
  TopCornerBorder() = default;

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    if (rect_below_scroll_.IsEmpty())
      return;

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

    gfx::Rect rect = rect_below_scroll_;
    rect.set_height(std::min(rect.height(), kUnifiedTrayCornerRadius * 2));
    rect.Inset(gfx::Insets(-kUnifiedTrayCornerRadius * 4, 0, 0, 0));
    canvas->DrawRoundRect(gfx::RectF(rect), kUnifiedTrayCornerRadius, flags);
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(); }

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void set_rect_below_scroll(const gfx::Rect& rect_below_scroll) {
    rect_below_scroll_ = rect_below_scroll;
  }

 private:
  gfx::Rect rect_below_scroll_;

  DISALLOW_COPY_AND_ASSIGN(TopCornerBorder);
};

// The container view for the system tray, i.e. the panel containing settings
// buttons and sliders (e.g. sign out, lock, volume slider, etc.).
class SystemTrayContainer : public views::View {
 public:
  SystemTrayContainer() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    SetBackground(UnifiedSystemTrayView::CreateBackground());

    if (!features::IsUnifiedMessageCenterRefactorEnabled())
      SetBorder(std::make_unique<TopCornerBorder>());
  }

  ~SystemTrayContainer() override = default;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  const char* GetClassName() const override { return "SystemTrayContainer"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayContainer);
};

class DetailedViewContainer : public views::View {
 public:
  DetailedViewContainer() {
    SetBackground(UnifiedSystemTrayView::CreateBackground());

    if (!features::IsUnifiedMessageCenterRefactorEnabled())
      SetBorder(std::make_unique<TopCornerBorder>());
  }

  ~DetailedViewContainer() override = default;

  // views::View:
  void Layout() override {
    for (auto* child : children())
      child->SetBoundsRect(GetContentsBounds());
    views::View::Layout();
  }

  const char* GetClassName() const override { return "DetailedViewContainer"; }

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
  return std::accumulate(children().cbegin(), children().cend(), 0,
                         [](int height, const auto* v) {
                           return height + v->GetHeightForWidth(kTrayMenuWidth);
                         });
}

void UnifiedSlidersContainerView::Layout() {
  int y = 0;
  for (auto* child : children()) {
    int height = child->GetHeightForWidth(kTrayMenuWidth);
    child->SetBounds(0, y, kTrayMenuWidth, height);
    y += height;
  }
}

gfx::Size UnifiedSlidersContainerView::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth, GetExpandedHeight() * expanded_amount_);
}

const char* UnifiedSlidersContainerView::GetClassName() const {
  return "UnifiedSlidersContainerView";
}

void UnifiedSlidersContainerView::UpdateOpacity() {
  const int height = GetPreferredSize().height();
  for (auto* child : children()) {
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
            : view_->detailed_view_container_;

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

// static
SkColor UnifiedSystemTrayView::GetBackgroundColor() {
  if (features::IsBackgroundBlurEnabled()) {
    return AshColorProvider::Get()->DeprecatedGetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparentWithBlur,
        kUnifiedMenuBackgroundColorWithBlur);
  }
  return AshColorProvider::Get()->DeprecatedGetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparentWithoutBlur,
      kUnifiedMenuBackgroundColor);
}

// static
std::unique_ptr<views::Background> UnifiedSystemTrayView::CreateBackground() {
  return views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(GetBackgroundColor(),
                                                  kUnifiedTrayCornerRadius));
}

UnifiedSystemTrayView::UnifiedSystemTrayView(
    UnifiedSystemTrayController* controller,
    bool initially_expanded)
    : expanded_amount_(initially_expanded ? 1.0 : 0.0),
      controller_(controller),
      notification_hidden_view_(new NotificationHiddenView()),
      top_shortcuts_view_(new TopShortcutsView(controller_)),
      feature_pods_container_(
          new FeaturePodsContainerView(controller_, initially_expanded)),
      page_indicator_view_(
          new PageIndicatorView(controller_, initially_expanded)),
      sliders_container_(new UnifiedSlidersContainerView(initially_expanded)),
      system_info_view_(new UnifiedSystemInfoView(controller_)),
      system_tray_container_(new SystemTrayContainer()),
      detailed_view_container_(new DetailedViewContainer()),
      focus_search_(std::make_unique<FocusSearch>(this)),
      interacted_by_tap_recorder_(
          std::make_unique<InteractedByTapRecorder>(this)) {
  DCHECK(controller_);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  SetBackground(CreateBackground());
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();

  if (!features::IsUnifiedMessageCenterRefactorEnabled()) {
    message_center_view_ = new UnifiedMessageCenterView(
        this, controller->model(), nullptr /* message_center_bubble */);
    AddChildView(message_center_view_);
    layout->SetFlexForView(message_center_view_, 1);
  }

  notification_hidden_view_->SetVisible(
      session_controller->GetUserSession(0) &&
      session_controller->IsScreenLocked() &&
      !AshMessageCenterLockScreenController::IsEnabled());
  AddChildView(notification_hidden_view_);

  AddChildView(system_tray_container_);

  system_tray_container_->AddChildView(top_shortcuts_view_);
  system_tray_container_->AddChildView(feature_pods_container_);
  system_tray_container_->AddChildView(page_indicator_view_);
  system_tray_container_->AddChildView(sliders_container_);
  system_tray_container_->AddChildView(system_info_view_);

  if (features::IsManagedDeviceUIRedesignEnabled()) {
    managed_device_view_ = new UnifiedManagedDeviceView();
    system_tray_container_->AddChildView(managed_device_view_);
  }

  detailed_view_container_->SetVisible(false);
  AddChildView(detailed_view_container_);

  // UnifiedSystemTrayView::FocusSearch makes focus traversal start from
  // |system_tray_container_|, but we have to complete the cycle by setting
  // |message_center_view_| next to |detailed_view_container_|.
  // Also, SetNextFocusableView does not support loop as mentioned in the doc,
  // we have to set null to |notification_hidden_view_|.
  notification_hidden_view_->SetNextFocusableView(nullptr);

  if (!features::IsUnifiedMessageCenterRefactorEnabled())
    detailed_view_container_->SetNextFocusableView(message_center_view_);

  top_shortcuts_view_->SetExpandedAmount(expanded_amount_);
}

UnifiedSystemTrayView::~UnifiedSystemTrayView() = default;

void UnifiedSystemTrayView::SetMaxHeight(int max_height) {
  max_height_ = max_height;

  // FeaturePodsContainer can adjust it's height by reducing the number of rows
  // it uses. It will calculate how many rows to use based on the max height
  // passed here.
  feature_pods_container_->SetMaxHeight(
      max_height - top_shortcuts_view_->GetPreferredSize().height() -
      page_indicator_view_->GetPreferredSize().height() -
      sliders_container_->GetExpandedHeight() -
      system_info_view_->GetPreferredSize().height());

  if (!features::IsUnifiedMessageCenterRefactorEnabled()) {
    message_center_view_->SetMaxHeight(max_height_);

    // Because the message center view requires a certain height to be usable,
    // it will be hidden if there isn't sufficient remaining height.
    int system_tray_height = expanded_amount_ > 0.0
                                 ? GetExpandedSystemTrayHeight()
                                 : GetCollapsedSystemTrayHeight();
    int available_height = max_height_ - system_tray_height;
    message_center_view_->SetAvailableHeight(available_height);
  }
}

void UnifiedSystemTrayView::AddFeaturePodButton(FeaturePodButton* button) {
  feature_pods_container_->AddFeaturePodButton(button);
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

void UnifiedSystemTrayView::SaveFocus() {
  auto* focus_manager = GetFocusManager();
  if (!focus_manager)
    return;

  saved_focused_view_ = focus_manager->GetFocusedView();
}

void UnifiedSystemTrayView::RestoreFocus() {
  if (saved_focused_view_)
    saved_focused_view_->RequestFocus();
}

void UnifiedSystemTrayView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  expanded_amount_ = expanded_amount;

  if (!features::IsUnifiedMessageCenterRefactorEnabled()) {
    message_center_view_->SetAvailableHeight(max_height_ -
                                             system_tray_container_->height());
  }
  top_shortcuts_view_->SetExpandedAmount(expanded_amount);
  feature_pods_container_->SetExpandedAmount(expanded_amount);
  page_indicator_view_->SetExpandedAmount(expanded_amount);
  sliders_container_->SetExpandedAmount(expanded_amount);

  if (!IsTransformEnabled()) {
    PreferredSizeChanged();
    // It is possible that the ratio between |message_center_view_| and others
    // can change while the bubble size remain unchanged.
    Layout();
    return;
  }

  // Note: currently transforms are only enabled when there are no
  // notifications, so we can consider only the system tray height.
  if (height() != GetExpandedSystemTrayHeight())
    PreferredSizeChanged();
  Layout();
}

int UnifiedSystemTrayView::GetExpandedSystemTrayHeight() const {
  return (notification_hidden_view_->GetVisible()
              ? notification_hidden_view_->GetPreferredSize().height()
              : 0) +
         top_shortcuts_view_->GetPreferredSize().height() +
         feature_pods_container_->GetExpandedHeight() +
         page_indicator_view_->GetExpandedHeight() +
         sliders_container_->GetExpandedHeight() +
         system_info_view_->GetPreferredSize().height();
}

int UnifiedSystemTrayView::GetCollapsedSystemTrayHeight() const {
  return (notification_hidden_view_->GetVisible()
              ? notification_hidden_view_->GetPreferredSize().height()
              : 0) +
         top_shortcuts_view_->GetPreferredSize().height() +
         feature_pods_container_->GetCollapsedHeight() +
         system_info_view_->GetPreferredSize().height();
}

int UnifiedSystemTrayView::GetCurrentHeight() const {
  return GetPreferredSize().height();
}

bool UnifiedSystemTrayView::IsTransformEnabled() const {
  // TODO(tetsui): Support animation by transform even when
  // UnifiedMessageCenterview is visible.
  if (features::IsUnifiedMessageCenterRefactorEnabled()) {
    return false;
  } else {
    return expanded_amount_ != 0.0 && expanded_amount_ != 1.0 &&
           !message_center_view_->GetVisible();
  }
}

void UnifiedSystemTrayView::SetNotificationRectBelowScroll(
    const gfx::Rect& rect_below_scroll) {
  static_cast<TopCornerBorder*>(system_tray_container_->border())
      ->set_rect_below_scroll(rect_below_scroll);
  static_cast<TopCornerBorder*>(detailed_view_container_->border())
      ->set_rect_below_scroll(rect_below_scroll);
  SchedulePaint();
}

int UnifiedSystemTrayView::GetVisibleFeaturePodCount() const {
  return feature_pods_container_->GetVisibleCount();
}

views::View* UnifiedSystemTrayView::GetFirstFocusableChild() {
  FocusTraversable* focus_traversable = GetFocusTraversable();
  views::View* focus_traversable_view = this;
  return focus_search_->FindNextFocusableView(
      nullptr, FocusSearch::SearchDirection::kForwards,
      FocusSearch::TraversalDirection::kDown,
      FocusSearch::StartingViewPolicy::kSkipStartingView,
      FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &focus_traversable, &focus_traversable_view);
}

views::View* UnifiedSystemTrayView::GetLastFocusableChild() {
  FocusTraversable* focus_traversable = GetFocusTraversable();
  views::View* focus_traversable_view = this;
  return focus_search_->FindNextFocusableView(
      nullptr, FocusSearch::SearchDirection::kBackwards,
      FocusSearch::TraversalDirection::kDown,
      FocusSearch::StartingViewPolicy::kSkipStartingView,
      FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &focus_traversable, &focus_traversable_view);
}

void UnifiedSystemTrayView::FocusEntered(bool reverse) {
  views::View* focus_view =
      reverse ? GetLastFocusableChild() : GetFirstFocusableChild();
  GetFocusManager()->SetFocusedView(focus_view);
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

const char* UnifiedSystemTrayView::GetClassName() const {
  return "UnifiedSystemTrayView";
}

void UnifiedSystemTrayView::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);
}

void UnifiedSystemTrayView::RemovedFromWidget() {
  if (!focus_manager_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
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

void UnifiedSystemTrayView::OnWillChangeFocus(views::View* before,
                                              views::View* now) {}

void UnifiedSystemTrayView::OnDidChangeFocus(views::View* before,
                                             views::View* now) {
  if (!features::IsUnifiedMessageCenterRefactorEnabled())
    return;

  if (feature_pods_container_->Contains(now)) {
    feature_pods_container_->EnsurePageWithButton(now);
  }

  views::View* first_view = GetFirstFocusableChild();
  views::View* last_view = GetLastFocusableChild();

  bool focused_out = false;
  if (before == last_view && now == first_view)
    focused_out = controller_->FocusOut(false);
  else if (before == first_view && now == last_view)
    focused_out = controller_->FocusOut(true);

  if (focused_out) {
    GetFocusManager()->ClearFocus();
    GetFocusManager()->SetStoredFocusView(nullptr);
  }
}

}  // namespace ash
