// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view_base.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/utility/haptics_util.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/work_area_insets.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/event_monitor.h"
#include "ui/views/highlight_border.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Duration of delay when Bento Bar Desk Button is clicked.
constexpr base::TimeDelta kAnimationDelayDuration = base::Milliseconds(100);

OverviewHighlightController* GetHighlightController() {
  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller || !overview_controller->InOverviewSession()) {
    return nullptr;
  }
  return overview_controller->overview_session()->highlight_controller();
}

// Check whether there are any external keyboards.
bool HasExternalKeyboard() {
  for (const ui::InputDevice& device :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (device.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }
  return false;
}

// Initialize a scoped layer animation settings for scroll view contents.
void InitScrollContentsAnimationSettings(
    ui::ScopedLayerAnimationSettings& settings) {
  settings.SetTransitionDuration(kDeskBarScrollDuration);
  settings.SetTweenType(gfx::Tween::ACCEL_20_DECEL_60);
}

gfx::Rect GetGestureEventScreenRect(const ui::Event& event) {
  CHECK(event.IsGestureEvent());
  return event.AsGestureEvent()->details().bounding_box();
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskBarScrollViewLayout:

// All the desk bar contents except the background view are added to
// be the children of the `scroll_view_` to support scrollable desk bar.
// `DeskBarScrollViewLayout` will help lay out the contents of the
// `scroll_view_`.
class DeskBarScrollViewLayout : public views::LayoutManager {
 public:
  explicit DeskBarScrollViewLayout(DeskBarViewBase* bar_view)
      : bar_view_(bar_view) {}
  DeskBarScrollViewLayout(const DeskBarScrollViewLayout&) = delete;
  DeskBarScrollViewLayout& operator=(const DeskBarScrollViewLayout&) = delete;
  ~DeskBarScrollViewLayout() override = default;

  void LayoutInternal(views::View* host) {
    const gfx::Rect scroll_bounds = bar_view_->scroll_view_->bounds();

    // `host` here is `scroll_view_contents_`.
    if (bar_view_->IsZeroState()) {
      host->SetBoundsRect(scroll_bounds);
      auto* zero_state_default_desk_button =
          bar_view_->zero_state_default_desk_button();
      const gfx::Size zero_state_default_desk_button_size =
          zero_state_default_desk_button->GetPreferredSize();

      auto* zero_state_new_desk_button =
          bar_view_->zero_state_new_desk_button();
      const gfx::Size zero_state_new_desk_button_size =
          zero_state_new_desk_button->GetPreferredSize();

      auto* zero_state_library_button = bar_view_->zero_state_library_button();
      const gfx::Size zero_state_library_button_size =
          bar_view_->ShouldShowLibraryUi()
              ? zero_state_library_button->GetPreferredSize()
              : gfx::Size();
      const int width_for_zero_state_library_button =
          bar_view_->ShouldShowLibraryUi()
              ? zero_state_library_button_size.width() +
                    kDeskBarZeroStateButtonSpacing
              : 0;

      const int content_width = zero_state_default_desk_button_size.width() +
                                kDeskBarZeroStateButtonSpacing +
                                zero_state_new_desk_button_size.width() +
                                width_for_zero_state_library_button;
      zero_state_default_desk_button->SetBoundsRect(
          gfx::Rect(gfx::Point((scroll_bounds.width() - content_width) / 2,
                               kDeskBarZeroStateY),
                    zero_state_default_desk_button_size));
      // Update this button's text since it may changes while removing a desk
      // and going back to the zero state.
      zero_state_default_desk_button->UpdateLabelText();
      // Make sure these two buttons are always visible while in zero state bar
      // since they are invisible in expanded state bar.
      zero_state_default_desk_button->SetVisible(true);
      zero_state_new_desk_button->SetVisible(true);
      zero_state_new_desk_button->SetBoundsRect(gfx::Rect(
          gfx::Point(zero_state_default_desk_button->bounds().right() +
                         kDeskBarZeroStateButtonSpacing,
                     kDeskBarZeroStateY),
          zero_state_new_desk_button_size));

      if (zero_state_library_button) {
        zero_state_library_button->SetBoundsRect(
            gfx::Rect(gfx::Point(zero_state_new_desk_button->bounds().right() +
                                     kDeskBarZeroStateButtonSpacing,
                                 kDeskBarZeroStateY),
                      zero_state_library_button_size));
        zero_state_library_button->SetVisible(bar_view_->ShouldShowLibraryUi());
      }
      return;
    }

    std::vector<DeskMiniView*> mini_views = bar_view_->mini_views();
    if (mini_views.empty()) {
      return;
    }
    // When RTL is enabled, we still want desks to be laid our in LTR, to match
    // the spatial order of desks. Therefore, we reverse the order of the mini
    // views before laying them out.
    if (base::i18n::IsRTL()) {
      base::ranges::reverse(mini_views);
    }

    auto* expanded_state_library_button =
        bar_view_->expanded_state_library_button();
    const bool expanded_state_library_button_visible =
        expanded_state_library_button &&
        expanded_state_library_button->GetVisible();

    gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();

    // The new desk button and library button in the expanded bar view has the
    // same size as mini view.
    const int num_items = static_cast<int>(mini_views.size()) +
                          (expanded_state_library_button_visible ? 2 : 1);

    // Content width is sum of the width of all views, and plus the spacing
    // between the views, the focus ring's thickness and padding on each sides.
    const int content_width =
        num_items * (mini_view_size.width() + kDeskBarMiniViewsSpacing) -
        kDeskBarMiniViewsSpacing +
        kDeskBarDeskPreviewViewFocusRingThicknessAndPadding * 2;
    width_ = std::max(scroll_bounds.width(), content_width);

    // Update the size of the `host`, which is `scroll_view_contents_` here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then `scroll_view_` will know whether the contents need to
    // be scolled or not.
    host->SetSize(gfx::Size(width_, scroll_bounds.height()));

    // The x of the first mini view should include the focus ring thickness and
    // padding into consideration, otherwise the focus ring won't be drawn on
    // the left side of the first mini view.
    int x = (width_ - content_width) / 2 +
            kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    const int y =
        kDeskBarMiniViewsY - mini_views[0]->GetPreviewBorderInsets().top();
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
      x += (mini_view_size.width() + kDeskBarMiniViewsSpacing);
    }
    bar_view_->expanded_state_new_desk_button()->SetBoundsRect(
        gfx::Rect(gfx::Point(x, y), mini_view_size));

    if (expanded_state_library_button) {
      x += (mini_view_size.width() + kDeskBarMiniViewsSpacing);
      expanded_state_library_button->SetBoundsRect(
          gfx::Rect(gfx::Point(x, y), mini_view_size));
    }
  }

  // Layout the label which is shown below the desk icon button when the button
  // is at active state.
  void LayoutDeskIconButtonLabel(views::Label* label,
                                 const gfx::Rect& icon_button_bounds,
                                 DeskNameView* desk_name_view,
                                 int label_text_id) {
    label->SetText(gfx::ElideText(
        l10n_util::GetStringUTF16(label_text_id), gfx::FontList(),
        icon_button_bounds.width() - desk_name_view->GetInsets().width(),
        gfx::ELIDE_TAIL));

    const gfx::Size button_label_size = label->GetPreferredSize();

    label->SetBoundsRect(gfx::Rect(
        gfx::Point(
            icon_button_bounds.x() +
                ((icon_button_bounds.width() - button_label_size.width()) / 2),
            icon_button_bounds.bottom() +
                kDeskBarDeskIconButtonAndLabelSpacing),
        gfx::Size(button_label_size.width(), desk_name_view->height())));
  }

  // TODO(conniekxu): After CrOS Next is launched, remove function
  // `LayoutInternal`, and move this to Layout.
  void LayoutInternalCrOSNext(views::View* host) {
    TRACE_EVENT0("ui", "DeskBarScrollViewLayout::LayoutInternalCrOSNext");

    const gfx::Rect scroll_bounds = bar_view_->scroll_view_->bounds();

    auto* new_desk_button_label = bar_view_->new_desk_button_label();
    auto* library_button_label = bar_view_->library_button_label();

    // `host` here is `scroll_view_contents_`.
    if (bar_view_->IsZeroState()) {
      host->SetBoundsRect(scroll_bounds);

      new_desk_button_label->SetVisible(false);
      if (library_button_label) {
        library_button_label->SetVisible(false);
      }

      auto* default_desk_button = bar_view_->default_desk_button();
      const gfx::Size default_desk_button_size =
          default_desk_button->GetPreferredSize();

      auto* new_desk_button = bar_view_->new_desk_button();
      const gfx::Size new_desk_button_size =
          new_desk_button->GetPreferredSize();

      auto* library_button = bar_view_->library_button();
      const gfx::Size library_button_size =
          bar_view_->ShouldShowLibraryUi() ? library_button->GetPreferredSize()
                                           : gfx::Size();
      const int width_for_library_button =
          bar_view_->ShouldShowLibraryUi()
              ? library_button_size.width() + kDeskBarZeroStateButtonSpacing
              : 0;

      const int content_width =
          default_desk_button_size.width() + kDeskBarZeroStateButtonSpacing +
          new_desk_button_size.width() + width_for_library_button;
      default_desk_button->SetBoundsRect(
          gfx::Rect(gfx::Point((scroll_bounds.width() - content_width) / 2,
                               kDeskBarZeroStateY),
                    default_desk_button_size));

      // Update this button's text since it may changes while removing a desk
      // and going back to the zero state.
      default_desk_button->UpdateLabelText();
      // Make sure default desk button is always visible while in zero state
      // bar.
      default_desk_button->SetVisible(true);
      new_desk_button->SetBoundsRect(
          gfx::Rect(gfx::Point(default_desk_button->bounds().right() +
                                   kDeskBarZeroStateButtonSpacing,
                               kDeskBarZeroStateY),
                    new_desk_button_size));

      if (library_button) {
        library_button->SetBoundsRect(
            gfx::Rect(gfx::Point(new_desk_button->bounds().right() +
                                     kDeskBarZeroStateButtonSpacing,
                                 kDeskBarZeroStateY),
                      library_button_size));
        library_button->SetVisible(bar_view_->ShouldShowLibraryUi());
      }
      return;
    }

    std::vector<DeskMiniView*> mini_views = bar_view_->mini_views();
    if (mini_views.empty()) {
      return;
    }
    // When RTL is enabled, we still want desks to be laid our in LTR, to match
    // the spatial order of desks. Therefore, we reverse the order of the mini
    // views before laying them out.
    if (base::i18n::IsRTL()) {
      base::ranges::reverse(mini_views);
    }

    auto* library_button = bar_view_->library_button();
    const bool library_button_visible =
        library_button && library_button->GetVisible();
    gfx::Size library_button_size =
        library_button ? library_button->GetPreferredSize() : gfx::Size();

    gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();

    auto* new_desk_button = bar_view_->new_desk_button();
    gfx::Size new_desk_button_size = new_desk_button->GetPreferredSize();

    // Content width is sum of the width of all views, and plus the spacing
    // between the views, the focus ring's thickness and padding on each sides.
    const int content_width =
        mini_views.size() *
            (mini_view_size.width() + kDeskBarMiniViewsSpacing) +
        (new_desk_button_size.width() + kDeskBarMiniViewsSpacing) +
        (library_button_visible ? 1 : 0) *
            (library_button_size.width() + kDeskBarMiniViewsSpacing) -
        kDeskBarMiniViewsSpacing +
        kDeskBarDeskPreviewViewFocusRingThicknessAndPadding * 2;
    width_ = std::max(scroll_bounds.width(), content_width);

    // Update the size of the `host`, which is `scroll_view_contents_` here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then `scroll_view_` will know whether the contents need to
    // be scolled or not.
    host->SetSize(gfx::Size(width_, scroll_bounds.height()));

    // The x of the first mini view should include the focus ring thickness and
    // padding into consideration, otherwise the focus ring won't be drawn on
    // the left side of the first mini view.
    int x = (width_ - content_width) / 2 +
            kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    const int y =
        kDeskBarMiniViewsY - mini_views[0]->GetPreviewBorderInsets().top();
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
      x += (mini_view_size.width() + kDeskBarMiniViewsSpacing);
    }

    const gfx::Rect new_desk_button_bounds(
        gfx::Rect(gfx::Point(x, y), new_desk_button_size));
    new_desk_button->SetBoundsRect(new_desk_button_bounds);

    auto* desk_name_view = mini_views[0]->desk_name_view();

    LayoutDeskIconButtonLabel(new_desk_button_label, new_desk_button_bounds,
                              desk_name_view, IDS_ASH_DESKS_NEW_DESK_BUTTON);
    new_desk_button_label->SetVisible(new_desk_button->state() ==
                                      CrOSNextDeskIconButton::State::kActive);

    if (library_button) {
      x += (new_desk_button_size.width() + kDeskBarMiniViewsSpacing);
      const gfx::Rect library_button_bounds(
          gfx::Rect(gfx::Point(x, y), library_button_size));
      library_button->SetBoundsRect(library_button_bounds);
      LayoutDeskIconButtonLabel(
          library_button_label, library_button_bounds, desk_name_view,
          /*label_text_id=*/
          saved_desk_util::AreDesksTemplatesEnabled()
              ? IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY
              : IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER);
      library_button_label->SetVisible(library_button->state() ==
                                       CrOSNextDeskIconButton::State::kActive);
    }
  }

  // views::LayoutManager:
  void Layout(views::View* host) override {
    if (chromeos::features::IsJellyrollEnabled()) {
      LayoutInternalCrOSNext(host);
    } else {
      LayoutInternal(host);
    }
  }

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    return gfx::Size(width_, bar_view_->bounds().height());
  }

 private:
  raw_ptr<DeskBarViewBase, ExperimentalAsh> bar_view_;

  // Width of the scroll view. It is the contents' preferred width if it exceeds
  // the desk bar view's width or just the desk bar view's width if not.
  int width_ = 0;
};

// -----------------------------------------------------------------------------
// DeskBarHoverObserver:

class DeskBarHoverObserver : public ui::EventObserver {
 public:
  DeskBarHoverObserver(DeskBarViewBase* owner, aura::Window* widget_window)
      : owner_(owner),
        event_monitor_(views::EventMonitor::CreateWindowMonitor(
            this,
            widget_window,
            {ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_DRAGGED, ui::ET_MOUSE_RELEASED,
             ui::ET_MOUSE_MOVED, ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_EXITED,
             ui::ET_GESTURE_LONG_PRESS, ui::ET_GESTURE_LONG_TAP,
             ui::ET_GESTURE_TAP, ui::ET_GESTURE_TAP_DOWN})) {}

  DeskBarHoverObserver(const DeskBarHoverObserver&) = delete;
  DeskBarHoverObserver& operator=(const DeskBarHoverObserver&) = delete;

  ~DeskBarHoverObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_MOUSE_DRAGGED:
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_ENTERED:
      case ui::ET_MOUSE_EXITED:
        owner_->OnHoverStateMayHaveChanged();
        break;

      case ui::ET_GESTURE_LONG_PRESS:
      case ui::ET_GESTURE_LONG_TAP:
        owner_->OnGestureTap(GetGestureEventScreenRect(event),
                             /*is_long_gesture=*/true);
        break;

      case ui::ET_GESTURE_TAP:
      case ui::ET_GESTURE_TAP_DOWN:
        owner_->OnGestureTap(GetGestureEventScreenRect(event),
                             /*is_long_gesture=*/false);
        break;

      default:
        NOTREACHED();
        break;
    }
  }

 private:
  raw_ptr<DeskBarViewBase, ExperimentalAsh> owner_;

  std::unique_ptr<views::EventMonitor> event_monitor_;
};

DeskBarViewBase::DeskBarViewBase(aura::Window* root, Type type)
    : type_(type), state_(GetPerferredState(type)), root_(root) {
  CHECK(root && root->IsRootWindow());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();

  if (features::IsBackgroundBlurEnabled() &&
      (is_jellyroll_enabled || type_ == Type::kDeskButton)) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }

  const float corner_radius = type_ == Type::kOverview
                                  ? kDeskBarCornerRadiusOverview
                                  : kDeskBarCornerRadiusOverviewDeskButton;

  SetBorder(std::make_unique<views::HighlightBorder>(
      corner_radius,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderNoShadow
          : views::HighlightBorder::Type::kHighlightBorder2));

  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));

  // Use layer scrolling so that the contents will paint on top of the parent,
  // which uses SetPaintToLayer()
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->SetPaintToLayer();
  scroll_view_->layer()->SetFillsBoundsOpaquely(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);

  left_scroll_button_ = AddChildView(std::make_unique<ScrollArrowButton>(
      base::BindRepeating(&DeskBarViewBase::ScrollToPreviousPage,
                          base::Unretained(this)),
      /*is_left_arrow=*/true, this));
  left_scroll_button_->RemoveFromFocusList();
  right_scroll_button_ = AddChildView(std::make_unique<ScrollArrowButton>(
      base::BindRepeating(&DeskBarViewBase::ScrollToNextPage,
                          base::Unretained(this)),
      /*is_left_arrow=*/false, this));
  right_scroll_button_->RemoveFromFocusList();

  // Make the scroll content view animatable by painting to a layer.
  scroll_view_contents_ =
      scroll_view_->SetContents(std::make_unique<views::View>());
  scroll_view_contents_->SetPaintToLayer();

  if (is_jellyroll_enabled) {
    default_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<CrOSNextDefaultDeskButton>(this));
    new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<CrOSNextDeskIconButton>(
            this, &kDesksNewDeskButtonIcon,
            l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
            cros_tokens::kCrosSysOnPrimary, cros_tokens::kCrosSysPrimary,
            /*initially_enabled=*/DesksController::Get()->CanCreateDesks(),
            base::BindRepeating(
                &DeskBarViewBase::OnNewDeskButtonPressed,
                base::Unretained(this),
                type_ == Type::kDeskButton
                    ? DesksCreationRemovalSource::kDeskButtonDeskBarButton
                    : DesksCreationRemovalSource::kButton)));
    new_desk_button_label_ =
        scroll_view_contents_->AddChildView(std::make_unique<views::Label>());
    new_desk_button_label_->SetPaintToLayer();
    new_desk_button_label_->layer()->SetFillsBoundsOpaquely(false);
  } else {
    expanded_state_new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ExpandedDesksBarButton>(
            this, &kDesksNewDeskButtonIcon,
            l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
            /*initially_enabled=*/DesksController::Get()->CanCreateDesks(),
            base::BindRepeating(
                &DeskBarViewBase::OnNewDeskButtonPressed,
                base::Unretained(this),
                type_ == Type::kDeskButton
                    ? DesksCreationRemovalSource::kDeskButtonDeskBarButton
                    : DesksCreationRemovalSource::kButton)));

    zero_state_default_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateDefaultDeskButton>(this));
    zero_state_new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateIconButton>(
            this, &kDesksNewDeskButtonIcon,
            l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
            base::BindRepeating(
                &DeskBarViewBase::OnNewDeskButtonPressed,
                base::Unretained(this),
                type_ == Type::kDeskButton
                    ? DesksCreationRemovalSource::kDeskButtonDeskBarButton
                    : DesksCreationRemovalSource::kButton)));
  }

  if (saved_desk_util::IsSavedDesksEnabled()) {
    int button_text_id = IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY;
    if (!saved_desk_util::AreDesksTemplatesEnabled()) {
      button_text_id = IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER;
    }

    if (is_jellyroll_enabled) {
      library_button_ = scroll_view_contents_->AddChildView(
          std::make_unique<CrOSNextDeskIconButton>(
              this, &kDesksTemplatesIcon,
              l10n_util::GetStringUTF16(button_text_id),
              cros_tokens::kCrosSysOnSecondaryContainer,
              cros_tokens::kCrosSysInversePrimary,
              /*initially_enabled=*/true,
              base::BindRepeating(&DeskBarViewBase::OnLibraryButtonPressed,
                                  base::Unretained(this))));
      library_button_label_ =
          scroll_view_contents_->AddChildView(std::make_unique<views::Label>());
      library_button_label_->SetFontList(
          TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosAnnotation1));
      library_button_label_->SetPaintToLayer();
      library_button_label_->layer()->SetFillsBoundsOpaquely(false);
    } else {
      expanded_state_library_button_ = scroll_view_contents_->AddChildView(
          std::make_unique<ExpandedDesksBarButton>(
              this, &kDesksTemplatesIcon,
              l10n_util::GetStringUTF16(button_text_id),
              /*initially_enabled=*/true,
              base::BindRepeating(&DeskBarViewBase::OnLibraryButtonPressed,
                                  base::Unretained(this))));
      zero_state_library_button_ = scroll_view_contents_->AddChildView(
          std::make_unique<ZeroStateIconButton>(
              this, &kDesksTemplatesIcon,
              l10n_util::GetStringUTF16(button_text_id),
              base::BindRepeating(&DeskBarViewBase::OnLibraryButtonPressed,
                                  base::Unretained(this))));
    }
  }

  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &DeskBarViewBase::OnContentsScrolled, base::Unretained(this)));
  on_contents_scroll_ended_subscription_ =
      scroll_view_->AddContentsScrollEndedCallback(base::BindRepeating(
          &DeskBarViewBase::OnContentsScrollEnded, base::Unretained(this)));

  scroll_view_contents_->SetLayoutManager(
      std::make_unique<DeskBarScrollViewLayout>(this));

  DesksController::Get()->AddObserver(this);
}

DeskBarViewBase::~DeskBarViewBase() {
  DesksController::Get()->RemoveObserver(this);
  if (drag_view_) {
    EndDragDesk(drag_view_, /*end_by_user=*/false);
  }
}

// static
int DeskBarViewBase::GetPreferredBarHeight(aura::Window* root,
                                           Type type,
                                           State state) {
  int height = 0;
  switch (type) {
    case Type::kDeskButton:
      CHECK_EQ(State::kExpanded, state);
      height =
          DeskPreviewView::GetHeight(root) + kDeskBarNonPreviewAllocatedHeight;
      break;
    case Type::kOverview:
      if (state == State::kZero) {
        height = kDeskBarZeroStateHeight;
      } else {
        height = DeskPreviewView::GetHeight(root) +
                 kDeskBarNonPreviewAllocatedHeight;
      }
      break;
  }

  return height;
}

// static
DeskBarViewBase::State DeskBarViewBase::GetPerferredState(Type type) {
  State state = State::kZero;
  switch (type) {
    case Type::kDeskButton:
      // Desk button desk bar is always expaneded.
      state = State::kExpanded;
      break;
    case Type::kOverview: {
      // Overview desk bar can be zero state if both conditions below are true.
      //   - there is only one desk;
      //   - not currently showing saved desk library;
      OverviewController* overview_controller =
          Shell::Get()->overview_controller();
      DesksController* desk_controller = DesksController::Get();
      if (desk_controller->GetNumberOfDesks() == 1 &&
          overview_controller->InOverviewSession() &&
          !overview_controller->overview_session()
               ->IsShowingSavedDeskLibrary()) {
        state = State::kZero;
      } else {
        state = State::kExpanded;
      }
      break;
    }
  }

  return state;
}

// static
std::unique_ptr<views::Widget> DeskBarViewBase::CreateDeskWidget(
    aura::Window* root,
    const gfx::Rect& bounds,
    Type type) {
  CHECK(root && root->IsRootWindow());

  std::unique_ptr<views::Widget> widget;
  switch (type) {
    case Type::kOverview:
    case Type::kDeskButton: {
      widget = std::make_unique<views::Widget>();
      views::Widget::InitParams params(
          views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
      params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
      params.activatable = views::Widget::InitParams::Activatable::kYes;
      params.accept_events = true;
      params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
      // This widget will be parented to the currently-active desk container on
      // `root`.
      params.context = root;
      params.bounds = bounds;
      params.name = type == Type::kOverview ? "OverviewDeskBarWidget"
                                            : "DeskButtonDeskBarWidget";

      // Even though this widget exists on the active desk container, it should
      // not show up in the MRU list, and it should not be mirrored in the desks
      // mini_views.
      params.init_properties_container.SetProperty(kExcludeInMruKey, true);
      params.init_properties_container.SetProperty(kHideInDeskMiniViewKey,
                                                   true);
      widget->Init(std::move(params));

      auto* window = widget->GetNativeWindow();
      window->SetId(kShellWindowId_DesksBarWindow);
      ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_NONE);

      break;
    }
  }

  return widget;
}

void DeskBarViewBase::Layout() {
  TRACE_EVENT0("ui", "DeskBarViewBase::Layout");

  if (is_bounds_animation_on_going_) {
    return;
  }

  // It's possible that this is not owned by the overview grid anymore, because
  // when exiting overview, the bar stays alive for animation.
  if (type_ == Type::kOverview && !overview_grid_) {
    return;
  }

  // Refresh bounds as preferred. This is needed for dynamic width for the bar.
  // TODO(b/293658108): Move dynamic width update out of `Layout`.
  gfx::Size preferred_size = CalculatePreferredSize();
  gfx::Rect new_bounds = GetAvailableBounds();
  ShelfAlignment shelf_alignment = Shelf::ForWindow(root_)->alignment();
  switch (shelf_alignment) {
    case ShelfAlignment::kBottom:
      new_bounds.ClampToCenteredSize(preferred_size);
      break;
    case ShelfAlignment::kLeft:
    case ShelfAlignment::kRight:
      if ((shelf_alignment == ShelfAlignment::kRight) == base::i18n::IsRTL()) {
        new_bounds.set_size(preferred_size);
      } else {
        new_bounds.set_origin({new_bounds.right() - preferred_size.width(),
                               new_bounds.bottom() - preferred_size.height()});
        new_bounds.set_size(preferred_size);
      }
      break;
    case ShelfAlignment::kBottomLocked:
      return;
  }
  SetBoundsRect(new_bounds);

  // Scroll buttons are kept `scroll_view_padding` away from the edge of the
  // scroll view. So the horizontal padding of the scroll view is set to
  // guarantee enough space for the scroll buttons.
  const gfx::Insets insets = (type_ == Type::kOverview)
                                 ? overview_grid_->GetGridInsets()
                                 : gfx::Insets();
  CHECK(insets.left() == insets.right());
  const int scroll_view_padding =
      (type_ == Type::kOverview
           ? kDeskBarScrollViewMinimumHorizontalPaddingOverview
           : kDeskBarScrollViewMinimumHorizontalPaddingDeskButton);
  const int horizontal_padding = std::max(scroll_view_padding, insets.left());
  left_scroll_button_->SetBounds(horizontal_padding - scroll_view_padding,
                                 bounds().y(), kDeskBarScrollButtonWidth,
                                 bounds().height());
  right_scroll_button_->SetBounds(
      bounds().right() - horizontal_padding -
          (kDeskBarScrollButtonWidth - scroll_view_padding),
      bounds().y(), kDeskBarScrollButtonWidth, bounds().height());

  gfx::Rect scroll_bounds(size());
  // Align with the overview grid in horizontal, so only horizontal insets are
  // needed here.
  scroll_bounds.Inset(gfx::Insets::VH(0, horizontal_padding));
  scroll_view_->SetBoundsRect(scroll_bounds);

  // Clip the contents that are outside of the `scroll_view_`'s bounds.
  scroll_view_->layer()->SetMasksToBounds(true);
  scroll_view_->Layout();

  UpdateScrollButtonsVisibility();
  UpdateGradientMask();
}

bool DeskBarViewBase::OnMousePressed(const ui::MouseEvent& event) {
  if (desk_activation_timer_.IsRunning()) {
    return false;
  }
  DeskNameView::CommitChanges(GetWidget());
  return false;
}

void DeskBarViewBase::OnGestureEvent(ui::GestureEvent* event) {
  if (desk_activation_timer_.IsRunning()) {
    return;
  }
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_DOWN:
      DeskNameView::CommitChanges(GetWidget());
      break;

    default:
      break;
  }
}

void DeskBarViewBase::Init() {
  UpdateNewMiniViews(/*initializing_bar_view=*/true,
                     /*expanding_bar_view=*/false);

  // When the bar is initialized, scroll to make active desk mini view visible.
  auto it = base::ranges::find_if(mini_views_, [](DeskMiniView* mini_view) {
    return mini_view->desk()->is_active();
  });
  if (it != mini_views_.end()) {
    ScrollToShowViewIfNecessary(*it);
  }

  hover_observer_ = std::make_unique<DeskBarHoverObserver>(
      this, GetWidget()->GetNativeWindow());
}

bool DeskBarViewBase::IsZeroState() const {
  return state_ == DeskBarViewBase::State::kZero;
}

bool DeskBarViewBase::IsDraggingDesk() const {
  return drag_view_ != nullptr;
}

bool DeskBarViewBase::IsDeskNameBeingModified() const {
  if (!GetWidget()->IsActive()) {
    return false;
  }

  for (auto* mini_view : mini_views_) {
    if (mini_view->IsDeskNameBeingModified()) {
      return true;
    }
  }
  return false;
}

void DeskBarViewBase::ScrollToShowViewIfNecessary(const views::View* view) {
  CHECK(base::Contains(scroll_view_contents_->children(), view));
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const gfx::Rect view_bounds = view->bounds();
  const bool beyond_left = view_bounds.x() < visible_bounds.x();
  const bool beyond_right = view_bounds.right() > visible_bounds.right();
  auto* scroll_bar = scroll_view_->horizontal_scroll_bar();
  if (beyond_left) {
    scroll_view_->ScrollToPosition(
        scroll_bar, view_bounds.right() - scroll_view_->bounds().width());
  } else if (beyond_right) {
    scroll_view_->ScrollToPosition(scroll_bar, view_bounds.x());
  }
}

DeskMiniView* DeskBarViewBase::FindMiniViewForDesk(const Desk* desk) const {
  for (auto* mini_view : mini_views_) {
    if (mini_view->desk() == desk) {
      return mini_view;
    }
  }

  return nullptr;
}

int DeskBarViewBase::GetMiniViewIndex(const DeskMiniView* mini_view) const {
  auto iter = base::ranges::find(mini_views_, mini_view);
  return (iter == mini_views_.cend())
             ? -1
             : std::distance(mini_views_.cbegin(), iter);
}

void DeskBarViewBase::OnNewDeskButtonPressed(
    DesksCreationRemovalSource desks_creation_removal_source) {
  if (desk_activation_timer_.IsRunning()) {
    return;
  }
  auto* controller = DesksController::Get();
  if (!controller->CanCreateDesks()) {
    return;
  }

  base::UmaHistogramBoolean(type_ == Type::kDeskButton
                                ? kDeskButtonDeskBarNewDeskHistogramName
                                : kOverviewDeskBarNewDeskHistogramName,
                            true);

  controller->NewDesk(desks_creation_removal_source);
  NudgeDeskName(mini_views_.size() - 1);

  // TODO(b/277081702): When desk order is adjusted for RTL, remove the check
  // below to always make new desk button visible.
  if (!base::i18n::IsRTL()) {
    if (new_desk_button_) {
      ScrollToShowViewIfNecessary(new_desk_button_);
    } else if (expanded_state_new_desk_button_) {
      ScrollToShowViewIfNecessary(expanded_state_new_desk_button_);
    }
  }
}

void DeskBarViewBase::OnSavedDeskLibraryHidden() {
  if (type_ == Type::kOverview && !chromeos::features::IsJellyrollEnabled() &&
      mini_views_.size() == 1u) {
    SwitchToZeroState();
  }
}

void DeskBarViewBase::NudgeDeskName(int desk_index) {
  CHECK_LT(desk_index, static_cast<int>(mini_views_.size()));

  auto* name_view = mini_views_[desk_index]->desk_name_view();
  name_view->RequestFocus();

  // Set `name_view`'s accessible name to the default desk name since its text
  // is cleared.
  if (name_view->GetAccessibleName().empty()) {
    name_view->SetAccessibleName(
        DesksController::GetDeskDefaultName(desk_index));
  }

  if (type_ == Type::kOverview) {
    UpdateOverviewHighlightForFocus(name_view);

    // If we're in tablet mode and there are no external keyboards, open up the
    // virtual keyboard.
    if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
        !HasExternalKeyboard()) {
      keyboard::KeyboardUIController::Get()->ShowKeyboard(/*lock=*/false);
    }
  }
}

void DeskBarViewBase::UpdateButtonsForSavedDeskGrid() {
  if (IsZeroState() || !saved_desk_util::IsSavedDesksEnabled()) {
    return;
  }

  FindMiniViewForDesk(Shell::Get()->desks_controller()->active_desk())
      ->UpdateFocusColor();

  if (type_ == Type::kOverview) {
    if (chromeos::features::IsJellyrollEnabled()) {
      library_button_->set_paint_as_active(
          overview_grid_->IsShowingSavedDeskLibrary());
      library_button_->UpdateFocusState();
    } else {
      expanded_state_library_button_->set_active(
          overview_grid_->IsShowingSavedDeskLibrary());
      expanded_state_library_button_->UpdateFocusColor();
    }
  }
}

void DeskBarViewBase::UpdateDeskButtonsVisibility() {
  if (chromeos::features::IsJellyrollEnabled()) {
    UpdateDeskButtonsVisibilityCrOSNext();
    return;
  }
  const bool is_zero_state = IsZeroState();
  zero_state_default_desk_button_->SetVisible(is_zero_state);
  zero_state_new_desk_button_->SetVisible(is_zero_state);
  expanded_state_new_desk_button_->SetVisible(!is_zero_state);

  UpdateLibraryButtonVisibility();
}

void DeskBarViewBase::UpdateDeskButtonsVisibilityCrOSNext() {
  const bool is_zero_state = IsZeroState();
  default_desk_button_->SetVisible(is_zero_state);
  new_desk_button_label_->SetVisible(new_desk_button_->state() ==
                                     CrOSNextDeskIconButton::State::kActive);

  UpdateLibraryButtonVisibilityCrOSNext();
}

void DeskBarViewBase::UpdateLibraryButtonVisibility() {
  if (chromeos::features::IsJellyrollEnabled()) {
    UpdateLibraryButtonVisibilityCrOSNext();
    return;
  }
  if (!saved_desk_util::IsSavedDesksEnabled()) {
    return;
  }

  const bool is_zero_state = IsZeroState();

  zero_state_library_button_->SetVisible(ShouldShowLibraryUi() &&
                                         is_zero_state);
  expanded_state_library_button_->SetVisible(ShouldShowLibraryUi() &&
                                             !is_zero_state);

  if (type_ == Type::kOverview) {
    if (auto* highlight_controller = GetHighlightController()) {
      // Remove the button from the tabbing order if it becomes invisible.
      if (!zero_state_library_button_->GetVisible()) {
        highlight_controller->OnViewDestroyingOrDisabling(
            zero_state_library_button_);
      }
      if (!expanded_state_library_button_->GetVisible()) {
        highlight_controller->OnViewDestroyingOrDisabling(
            expanded_state_library_button_->GetInnerButton());
      }
    }
  } else {
    // TODO(b/277988182): Add support for desk button desk bar.
  }

  const int begin_x = GetFirstMiniViewXOffset();
  Layout();

  if (mini_views_.empty()) {
    return;
  }

  // The mini views and new desk button are already laid out in the earlier
  // `Layout()` call. This call shifts the transforms of the mini views and new
  // desk button and then animates to the identity transform.
  PerformLibraryButtonVisibilityAnimation(
      mini_views_,
      is_zero_state
          ? static_cast<views::View*>(zero_state_new_desk_button_)
          : static_cast<views::View*>(expanded_state_new_desk_button_),
      begin_x - GetFirstMiniViewXOffset());
}

void DeskBarViewBase::UpdateLibraryButtonVisibilityCrOSNext() {
  if (!saved_desk_util::IsSavedDesksEnabled()) {
    return;
  }

  library_button_label_->SetVisible(
      ShouldShowLibraryUi() &&
      (library_button_->state() == CrOSNextDeskIconButton::State::kActive));

  // If the visibility of the library button doesn't change, return early.
  if (library_button_->GetVisible() == ShouldShowLibraryUi()) {
    return;
  }

  library_button_->SetVisible(ShouldShowLibraryUi());
  if (ShouldShowLibraryUi()) {
    if (type_ == Type::kOverview &&
        overview_grid_->IsShowingSavedDeskLibrary()) {
      library_button_->UpdateState(CrOSNextDeskIconButton::State::kActive);
    } else {
      library_button_->UpdateState(CrOSNextDeskIconButton::State::kExpanded);
    }
  }

  if (mini_views_.empty()) {
    return;
  }

  const int begin_x = GetFirstMiniViewXOffset();
  Layout();

  // The mini views and new desk button are already laid out in the earlier
  // `Layout()` call. This call shifts the transforms of the mini views and new
  // desk button and then animates to the identity transform.
  PerformLibraryButtonVisibilityAnimation(mini_views_, new_desk_button_,
                                          begin_x - GetFirstMiniViewXOffset());
}

void DeskBarViewBase::UpdateDeskIconButtonState(
    CrOSNextDeskIconButton* button,
    CrOSNextDeskIconButton::State target_state) {
  CHECK(chromeos::features::IsJellyrollEnabled());
  CHECK_NE(target_state, CrOSNextDeskIconButton::State::kZero);

  if (button->state() == target_state) {
    return;
  }

  const int begin_x = GetFirstMiniViewXOffset();
  gfx::Rect current_bounds = button->GetBoundsInScreen();

  button->UpdateState(target_state);
  Layout();

  gfx::RectF target_bounds = gfx::RectF(new_desk_button_->GetBoundsInScreen());
  gfx::Transform scale_transform;
  const int shift_x = begin_x - GetFirstMiniViewXOffset();
  scale_transform.Translate(shift_x, 0);
  scale_transform.Scale(current_bounds.width() / target_bounds.width(),
                        current_bounds.height() / target_bounds.height());

  PerformDeskIconButtonScaleAnimationCrOSNext(button, this, scale_transform,
                                              shift_x);
}

void DeskBarViewBase::OnHoverStateMayHaveChanged() {
  for (auto* mini_view : mini_views_) {
    mini_view->UpdateDeskButtonVisibility();
  }
}

void DeskBarViewBase::OnGestureTap(const gfx::Rect& screen_rect,
                                   bool is_long_gesture) {
  if (desk_activation_timer_.IsRunning()) {
    return;
  }
  for (auto* mini_view : mini_views_) {
    mini_view->OnWidgetGestureTap(screen_rect, is_long_gesture);
  }
}

bool DeskBarViewBase::ShouldShowLibraryUi() {
  // Only update visibility when needed. This will save a lot of repeated work.
  if (library_ui_visibility_ == LibraryUiVisibility::kToBeChecked) {
    if (!saved_desk_util::IsSavedDesksEnabled() ||
        Shell::Get()->tablet_mode_controller()->InTabletMode()) {
      library_ui_visibility_ = LibraryUiVisibility::kHidden;
    } else {
      auto* desk_model = Shell::Get()->saved_desk_delegate()->GetDeskModel();
      CHECK(desk_model);
      size_t saved_desk_count = desk_model->GetDeskTemplateEntryCount() +
                                desk_model->GetSaveAndRecallDeskEntryCount();
      library_ui_visibility_ = saved_desk_count ? LibraryUiVisibility::kVisible
                                                : LibraryUiVisibility::kHidden;
    }
  }

  return library_ui_visibility_ == LibraryUiVisibility::kVisible;
}

void DeskBarViewBase::SetDragDetails(const gfx::Point& screen_location,
                                     bool dragged_item_over_bar) {
  last_dragged_item_screen_location_ = screen_location;
  const bool old_dragged_item_over_bar = dragged_item_over_bar_;
  dragged_item_over_bar_ = dragged_item_over_bar;

  if (!old_dragged_item_over_bar && !dragged_item_over_bar) {
    return;
  }

  for (auto* mini_view : mini_views_) {
    mini_view->UpdateFocusColor();
  }

  if (DesksController::Get()->CanCreateDesks()) {
    if (chromeos::features::IsJellyrollEnabled()) {
      new_desk_button_->UpdateFocusState();
    } else {
      expanded_state_new_desk_button()->UpdateFocusColor();
    }
  }
}

void DeskBarViewBase::HandlePressEvent(DeskMiniView* mini_view,
                                       const ui::LocatedEvent& event) {
  if (mini_view->is_animating_to_remove()) {
    return;
  }

  DeskNameView::CommitChanges(GetWidget());

  if (ui::EventTarget* target = event.target()) {
    gfx::PointF location = target->GetScreenLocationF(event);
    InitDragDesk(mini_view, location);
  }
}

void DeskBarViewBase::HandleLongPressEvent(DeskMiniView* mini_view,
                                           const ui::LocatedEvent& event) {
  if (mini_view->is_animating_to_remove()) {
    return;
  }

  DeskNameView::CommitChanges(GetWidget());

  // Initialize and start drag.
  gfx::PointF location = event.target()->GetScreenLocationF(event);
  InitDragDesk(mini_view, location);
  StartDragDesk(mini_view, location, event.IsMouseEvent());

  mini_view->OpenContextMenu(ui::MENU_SOURCE_LONG_PRESS);
}

void DeskBarViewBase::HandleDragEvent(DeskMiniView* mini_view,
                                      const ui::LocatedEvent& event) {
  // Do not perform drag if drag proxy is not initialized, or the mini view is
  // animating to be removed.
  if (!drag_proxy_ || mini_view->is_animating_to_remove()) {
    return;
  }

  mini_view->MaybeCloseContextMenu();

  gfx::PointF location = event.target()->GetScreenLocationF(event);

  // If the drag proxy is initialized, start the drag. If the drag started,
  // continue drag.
  switch (drag_proxy_->state()) {
    case DeskDragProxy::State::kInitialized:
      StartDragDesk(mini_view, location, event.IsMouseEvent());
      break;
    case DeskDragProxy::State::kStarted:
      ContinueDragDesk(mini_view, location);
      break;
    default:
      NOTREACHED();
  }
}

bool DeskBarViewBase::HandleReleaseEvent(DeskMiniView* mini_view,
                                         const ui::LocatedEvent& event) {
  // Do not end drag if the proxy is not initialized, or the mini view is
  // animating to be removed.
  if (!drag_proxy_ || mini_view->is_animating_to_remove()) {
    return false;
  }

  // If the drag didn't start, finalize the drag. Otherwise, end the drag and
  // snap back the desk.
  switch (drag_proxy_->state()) {
    case DeskDragProxy::State::kInitialized:
      FinalizeDragDesk();
      return false;
    case DeskDragProxy::State::kStarted:
      EndDragDesk(mini_view, /*end_by_user=*/true);
      break;
    default:
      NOTREACHED();
  }
  return true;
}

void DeskBarViewBase::OnActivateDeskTimer(const base::Uuid& uuid) {
  OnUiUpdateDone();

  auto* desk_controller = DesksController::Get();
  if (Desk* desk = desk_controller->GetDeskByUuid(uuid)) {
    desk_controller->ActivateDesk(
        desk, type_ == Type::kDeskButton
                  ? DesksSwitchSource::kDeskButtonMiniViewButton
                  : DesksSwitchSource::kMiniViewButton);
  }
}

void DeskBarViewBase::HandleClickEvent(DeskMiniView* mini_view) {
  // A timer to delay closing the desk bar.
  if (!ui::ScopedAnimationDurationScaleMode::is_zero()) {
    desk_activation_timer_.Start(
        FROM_HERE,
        ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
            kAnimationDelayDuration,
        base::BindOnce(&DeskBarViewBase::OnActivateDeskTimer,
                       base::Unretained(this), mini_view->desk()->uuid()));
  } else {
    OnActivateDeskTimer(mini_view->desk()->uuid());
  }
}

void DeskBarViewBase::InitDragDesk(DeskMiniView* mini_view,
                                   const gfx::PointF& location_in_screen) {
  CHECK(!mini_view->is_animating_to_remove());

  // If another view is being dragged, then end the drag.
  if (drag_view_) {
    EndDragDesk(drag_view_, /*end_by_user=*/false);
  }

  drag_view_ = mini_view;

  gfx::PointF preview_origin_in_screen(
      drag_view_->GetPreviewBoundsInScreen().origin());
  const float init_offset_x =
      location_in_screen.x() - preview_origin_in_screen.x();

  // Create a drag proxy for the dragged desk.
  drag_proxy_ =
      std::make_unique<DeskDragProxy>(this, drag_view_, init_offset_x);
}

void DeskBarViewBase::StartDragDesk(DeskMiniView* mini_view,
                                    const gfx::PointF& location_in_screen,
                                    bool is_mouse_dragging) {
  CHECK(drag_view_);
  CHECK(drag_proxy_);
  CHECK_EQ(mini_view, drag_view_);
  CHECK(!mini_view->is_animating_to_remove());

  // Hide the dragged mini view.
  drag_view_->layer()->SetOpacity(0.0f);

  // Create a drag proxy widget, scale it up and move its x-coordinate according
  // to the x of `location_in_screen`.
  drag_proxy_->InitAndScaleAndMoveToX(location_in_screen.x());

  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kGrabbing);

  // Fire a haptic event if necessary.
  if (is_mouse_dragging) {
    haptics_util::PlayHapticTouchpadEffect(
        ui::HapticTouchpadEffect::kTick,
        ui::HapticTouchpadEffectStrength::kMedium);
  }
}

void DeskBarViewBase::ContinueDragDesk(DeskMiniView* mini_view,
                                       const gfx::PointF& location_in_screen) {
  CHECK(drag_view_);
  CHECK(drag_proxy_);
  CHECK_EQ(mini_view, drag_view_);
  CHECK(!mini_view->is_animating_to_remove());

  drag_proxy_->DragToX(location_in_screen.x());

  // Check if the desk is on the scroll arrow buttons. Do not determine move
  // index while scrolling, since the positions of the desks on bar keep varying
  // during this process.
  if (MaybeScrollByDraggedDesk()) {
    return;
  }

  const auto drag_view_iter = base::ranges::find(mini_views_, drag_view_);
  CHECK(drag_view_iter != mini_views_.cend());

  const int old_index = drag_view_iter - mini_views_.cbegin();

  const int drag_pos_screen_x = drag_proxy_->GetBoundsInScreen().origin().x();

  // Determine the target location for the desk to be reordered.
  const int new_index = DetermineMoveIndex(drag_pos_screen_x);

  if (old_index != new_index) {
    Shell::Get()->desks_controller()->ReorderDesk(old_index, new_index);
  }
}

void DeskBarViewBase::EndDragDesk(DeskMiniView* mini_view, bool end_by_user) {
  CHECK(drag_view_);
  CHECK(drag_proxy_);
  CHECK_EQ(mini_view, drag_view_);
  CHECK(!mini_view->is_animating_to_remove());

  base::UmaHistogramBoolean(type_ == Type::kDeskButton
                                ? kDeskButtonDeskBarReorderDeskHistogramName
                                : kOverviewDeskBarReorderDeskHistogramName,
                            true);

  // Update default desk names after dropping.
  Shell::Get()->desks_controller()->UpdateDesksDefaultNames();
  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kPointer);

  // We update combine desks tooltips here to reflect the updated desk default
  // names.
  MaybeUpdateCombineDesksTooltips();

  // Stop scroll even if the desk is on the scroll arrow buttons.
  left_scroll_button_->OnDeskHoverEnd();
  right_scroll_button_->OnDeskHoverEnd();

  // If the reordering is ended by the user (release the drag), perform the
  // snapping back animation and scroll the bar to target position. If current
  // drag is ended due to the start of a new drag or the end of the overview,
  // directly finalize current drag.
  if (end_by_user) {
    ScrollToShowViewIfNecessary(drag_view_);
    drag_proxy_->SnapBackToDragView();
  } else {
    FinalizeDragDesk();
  }
}

void DeskBarViewBase::FinalizeDragDesk() {
  if (drag_view_) {
    drag_view_->layer()->SetOpacity(1.0f);
    drag_view_ = nullptr;
  }
  drag_proxy_.reset();
}

void DeskBarViewBase::OnDeskAdded(const Desk* desk, bool from_undo) {
  DeskNameView::CommitChanges(GetWidget());

  if (chromeos::features::IsJellyrollEnabled()) {
    const bool is_expanding_bar_view =
        new_desk_button_->state() == CrOSNextDeskIconButton::State::kZero;
    UpdateNewMiniViews(/*initializing_bar_view=*/false, is_expanding_bar_view);
    MaybeUpdateCombineDesksTooltips();
    if (!DesksController::Get()->CanCreateDesks()) {
      new_desk_button_->SetEnabled(/*enabled=*/false);
    }
  } else {
    const bool is_expanding_bar_view =
        zero_state_new_desk_button_->GetVisible();
    UpdateNewMiniViews(/*initializing_bar_view=*/false, is_expanding_bar_view);
    MaybeUpdateCombineDesksTooltips();

    if (!DesksController::Get()->CanCreateDesks()) {
      expanded_state_new_desk_button_->SetButtonState(/*enabled=*/false);
    }
  }
}

void DeskBarViewBase::OnDeskRemoved(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  auto iter = base::ranges::find(mini_views_, desk, &DeskMiniView::desk);

  // There are cases where a desk may be removed before the `desk_bar_view`
  // finishes initializing (i.e. removed on a separate root window before the
  // overview starting animation completes). In those cases, that mini_view
  // would not exist and the bar view will already be in the correct state so we
  // do not need to update the UI (https://crbug.com/1346154).
  if (iter == mini_views_.end()) {
    return;
  }

  if (type_ == Type::kOverview) {
    if (auto* highlight_controller = GetHighlightController()) {
      // Let the highlight controller know the view is destroying before it is
      // removed from the collection because it needs to know the index of the
      // mini view, or the desk name view (if either is currently highlighted)
      // relative to other traversable views.
      // The order here matters, we call it first on the desk_name_view since it
      // comes later in the highlight order (See documentation of
      // `OnViewDestroyingOrDisabling()`).
      highlight_controller->OnViewDestroyingOrDisabling(
          (*iter)->desk_name_view());
      highlight_controller->OnViewDestroyingOrDisabling(
          (*iter)->desk_preview());
    }
  } else {
    // TODO(b/277988182): Add support for desk button desk bar.
  }

  if (chromeos::features::IsJellyrollEnabled()) {
    new_desk_button_->SetEnabled(/*enabled=*/true);
  } else {
    expanded_state_new_desk_button_->SetButtonState(/*enabled=*/true);
  }

  for (auto* mini_view : mini_views_) {
    mini_view->UpdateDeskButtonVisibility();
  }

  // If Jellyroll is not enabled, switch to zero state if there will be one desk
  // after removal, unless we are viewing the saved desk library.
  if (type_ == Type::kOverview && !chromeos::features::IsJellyrollEnabled() &&
      mini_views_.size() == 2u &&
      !overview_grid_->IsShowingSavedDeskLibrary()) {
    SwitchToZeroState();
    return;
  }

  const int begin_x = GetFirstMiniViewXOffset();
  // Remove the mini view from the list now. And remove it from its parent
  // after the animation is done.
  DeskMiniView* removed_mini_view = *iter;
  auto partition_iter = mini_views_.erase(iter);

  // End dragging desk if remove a dragged desk.
  if (drag_view_ == removed_mini_view) {
    EndDragDesk(removed_mini_view, /*end_by_user=*/false);
  }

  // There is desk removal animation for overview bar but not for desk button
  // desk bar.
  if (type_ == Type::kOverview) {
    Layout();
    PerformRemoveDeskMiniViewAnimation(
        this, removed_mini_view,
        std::vector<DeskMiniView*>(mini_views_.begin(), partition_iter),
        std::vector<DeskMiniView*>(partition_iter, mini_views_.end()),
        begin_x - GetFirstMiniViewXOffset());
  } else {
    removed_mini_view->parent()->RemoveChildViewT(removed_mini_view);
    Layout();
  }

  MaybeUpdateCombineDesksTooltips();
}

void DeskBarViewBase::OnDeskReordered(int old_index, int new_index) {
  desks_util::ReorderItem(mini_views_, old_index, new_index);

  // Update the order of child views.
  auto* reordered_view = mini_views_[new_index];
  reordered_view->parent()->ReorderChildView(reordered_view, new_index);
  reordered_view->parent()->NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged, true);

  Layout();

  // Call the animation function after reorder the mini views.
  PerformReorderDeskMiniViewAnimation(old_index, new_index, mini_views_);
  MaybeUpdateCombineDesksTooltips();
}

void DeskBarViewBase::OnDeskActivationChanged(const Desk* activated,
                                              const Desk* deactivated) {
  for (auto* mini_view : mini_views_) {
    const Desk* desk = mini_view->desk();
    if (desk == activated || desk == deactivated) {
      mini_view->UpdateFocusColor();
    }
  }
}

void DeskBarViewBase::OnDeskNameChanged(const Desk* desk,
                                        const std::u16string& new_name) {
  MaybeUpdateCombineDesksTooltips();
}

void DeskBarViewBase::UpdateNewMiniViews(bool initializing_bar_view,
                                         bool expanding_bar_view) {
  TRACE_EVENT0("ui", "DeskBarViewBase::UpdateNewMiniViews");

  const auto& desks = DesksController::Get()->desks();
  if (initializing_bar_view) {
    UpdateDeskButtonsVisibility();
  }
  if (IsZeroState() && !expanding_bar_view) {
    return;
  }

  // This should not be called when a desk is removed.
  DCHECK_LE(mini_views_.size(), desks.size());

  const int begin_x = GetFirstMiniViewXOffset();
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);

  // New mini views can be added at any index, so we need to iterate through and
  // insert new mini views in a position in `mini_views_` that corresponds to
  // their index in the `DeskController`'s list of desks.
  int mini_view_index = 0;
  std::vector<DeskMiniView*> new_mini_views;
  for (const auto& desk : desks) {
    if (!FindMiniViewForDesk(desk.get())) {
      DeskMiniView* mini_view = scroll_view_contents_->AddChildViewAt(
          std::make_unique<DeskMiniView>(this, root_window, desk.get()),
          mini_view_index);
      mini_views_.insert(mini_views_.begin() + mini_view_index, mini_view);
      new_mini_views.push_back(mini_view);
    }
    ++mini_view_index;
  }

  if (expanding_bar_view) {
    SwitchToExpandedState();
    return;
  }

  if (chromeos::features::IsJellyrollEnabled()) {
    if (new_desk_button_->state() == CrOSNextDeskIconButton::State::kActive) {
      // Make sure the new desk button is updated to expanded state from the
      // active state. This can happen when dropping the window on the new desk
      // button.
      new_desk_button_->UpdateState(CrOSNextDeskIconButton::State::kExpanded);
    }
  }

  Layout();

  if (initializing_bar_view) {
    return;
  }

  // We need to compile lists of the mini views on either side of the new mini
  // views so that they can be moved to make room for the new mini views in the
  // desk bar.
  auto left_partition_iter =
      base::ranges::find(mini_views_, new_mini_views.front());
  auto right_partition_iter =
      std::next(base::ranges::find(mini_views_, new_mini_views.back()));

  // A vector between `left_partition_iter` and `right_partition_iter` should be
  // the same as `new_mini_views` if they were added correctly.
  DCHECK(std::vector<DeskMiniView*>(left_partition_iter,
                                    right_partition_iter) == new_mini_views);

  PerformNewDeskMiniViewAnimation(
      this, new_mini_views,
      std::vector<DeskMiniView*>(mini_views_.begin(), left_partition_iter),
      std::vector<DeskMiniView*>(right_partition_iter, mini_views_.end()),
      begin_x - GetFirstMiniViewXOffset());
}

void DeskBarViewBase::SwitchToZeroState() {
  CHECK(!chromeos::features::IsJellyrollEnabled());
  CHECK_EQ(type_, Type::kOverview);

  state_ = DeskBarViewBase::State::kZero;

  // In zero state, if the only desk is being dragged, we should end dragging.
  // Because the dragged desk's mini view is removed, the mouse released or
  // gesture ended events cannot be received. `drag_view_` will keep the stale
  // reference of removed mini view and `drag_proxy_` will not be reset.
  if (drag_view_) {
    EndDragDesk(drag_view_, /*end_by_user=*/false);
  }

  std::vector<DeskMiniView*> removed_mini_views = mini_views_;
  mini_views_.clear();

  if (auto* highlight_controller = GetHighlightController()) {
    OverviewHighlightableView* view = highlight_controller->highlighted_view();
    // Reset the highlight if it is highlighted on a descendant of `this`.
    if (view && Contains(view->GetView())) {
      highlight_controller->ResetHighlightedView();
    }
  }

  // Keep current layout until the animation is completed since the animation
  // for going back to zero state is based on the expanded bar's current
  // layout.
  PerformExpandedStateToZeroStateMiniViewAnimation(this, removed_mini_views);
}

void DeskBarViewBase::SwitchToExpandedState() {
  state_ = DeskBarViewBase::State::kExpanded;

  UpdateDeskButtonsVisibility();
  if (chromeos::features::IsJellyrollEnabled()) {
    PerformZeroStateToExpandedStateMiniViewAnimationCrOSNext(this);
  } else {
    PerformZeroStateToExpandedStateMiniViewAnimation(this);
  }
}

void DeskBarViewBase::OnUiUpdateDone() {
  if (on_update_ui_closure_for_testing_) {
    std::move(on_update_ui_closure_for_testing_).Run();
  }
}

int DeskBarViewBase::GetFirstMiniViewXOffset() const {
  // `GetMirroredX` is used here to make sure the removing and adding a desk
  // transform is correct while in RTL layout.
  return mini_views_.empty() ? bounds().CenterPoint().x()
                             : mini_views_[0]->GetMirroredX();
}

int DeskBarViewBase::DetermineMoveIndex(int location_screen_x) const {
  const int views_size = static_cast<int>(mini_views_.size());

  // We find the target position according to the x-axis coordinate of the
  // desks' center positions in screen in ascending order.
  for (int new_index = 0; new_index != views_size - 1; ++new_index) {
    auto* mini_view = mini_views_[new_index];

    // Note that we cannot directly use `GetBoundsInScreen`. Because we may
    // perform animation (transform) on mini views. The bounds gotten from
    // `GetBoundsInScreen` may be the intermediate bounds during animation.
    // Therefore, we transfer a mini view's origin from its parent level to
    // avoid the influence of its own transform.
    gfx::Point center_screen_pos = mini_view->GetMirroredBounds().CenterPoint();
    views::View::ConvertPointToScreen(mini_view->parent(), &center_screen_pos);
    if (location_screen_x < center_screen_pos.x()) {
      return new_index;
    }
  }

  return views_size - 1;
}

void DeskBarViewBase::UpdateScrollButtonsVisibility() {
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  left_scroll_button_->SetVisible(width() == GetAvailableBounds().width() &&
                                  visible_bounds.x() > 0);
  right_scroll_button_->SetVisible(width() == GetAvailableBounds().width() &&
                                   visible_bounds.right() <
                                       scroll_view_contents_->bounds().width());
}

void DeskBarViewBase::UpdateGradientMask() {
  const bool is_rtl = base::i18n::IsRTL();
  const bool is_left_scroll_button_visible = left_scroll_button_->GetVisible();
  const bool is_right_scroll_button_visible =
      right_scroll_button_->GetVisible();
  const bool is_left_visible_only =
      is_left_scroll_button_visible && !is_right_scroll_button_visible;

  bool should_show_start_gradient = false;
  bool should_show_end_gradient = false;
  // Show the both sides gradients during scroll if the corresponding scroll
  // button is visible. Otherwise, show the start/end gradient only in last page
  // and show the end/start gradient if there are contents beyond the right/left
  // side of the visible bounds with LTR/RTL layout.
  if (scroll_view_->is_scrolling()) {
    should_show_start_gradient =
        is_rtl ? is_right_scroll_button_visible : is_left_scroll_button_visible;
    should_show_end_gradient =
        is_rtl ? is_left_scroll_button_visible : is_right_scroll_button_visible;
  } else {
    should_show_start_gradient =
        is_rtl ? is_right_scroll_button_visible : is_left_visible_only;
    should_show_end_gradient =
        is_rtl ? is_left_visible_only : is_right_scroll_button_visible;
  }

  // The bounds of the start and end gradient will be the same regardless it is
  // LTR or RTL layout. While the `left_scroll_button_` will be changed from
  // left to right and `right_scroll_button_` will be changed from right to left
  // if it is RTL layout.

  // Horizontal linear gradient, from left to right.
  gfx::LinearGradient gradient_mask(/*angle=*/0);

  // Fraction of layer width that gradient will be applied to.
  const float fade_position =
      should_show_start_gradient || should_show_end_gradient
          ? static_cast<float>(kDeskBarGradientZoneLength) /
                scroll_view_->bounds().width()
          : 0;

  // Left fade in section.
  if (should_show_start_gradient) {
    gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
    gradient_mask.AddStep(fade_position, 255);
  }
  // Right fade out section.
  if (should_show_end_gradient) {
    gradient_mask.AddStep((1 - fade_position), 255);
    gradient_mask.AddStep(1, 0);
  }

  scroll_view_->layer()->SetGradientMask(gradient_mask);
  scroll_view_->SchedulePaint();
}

void DeskBarViewBase::ScrollToPreviousPage() {
  ui::ScopedLayerAnimationSettings settings(
      scroll_view_contents_->layer()->GetAnimator());
  InitScrollContentsAnimationSettings(settings);
  scroll_view_->ScrollToPosition(
      scroll_view_->horizontal_scroll_bar(),
      GetAdjustedUncroppedScrollPosition(scroll_view_->GetVisibleRect().x() -
                                         scroll_view_->width()));
}

void DeskBarViewBase::ScrollToNextPage() {
  ui::ScopedLayerAnimationSettings settings(
      scroll_view_contents_->layer()->GetAnimator());
  InitScrollContentsAnimationSettings(settings);
  scroll_view_->ScrollToPosition(
      scroll_view_->horizontal_scroll_bar(),
      GetAdjustedUncroppedScrollPosition(scroll_view_->GetVisibleRect().x() +
                                         scroll_view_->width()));
}

int DeskBarViewBase::GetAdjustedUncroppedScrollPosition(int position) const {
  // Let the ScrollView handle it if the given `position` is invalid or it can't
  // be adjusted.
  if (position <= 0 || position >= scroll_view_contents_->bounds().width() -
                                       scroll_view_->width()) {
    return position;
  }

  int adjusted_position = position;
  int i = 0;
  gfx::Rect mini_view_bounds;
  const int mini_views_size = static_cast<int>(mini_views_.size());
  for (; i < mini_views_size; i++) {
    mini_view_bounds = mini_views_[i]->bounds();

    // Return early if there is no desk preview cropped at the start position.
    if (mini_view_bounds.x() >= position) {
      return position - kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    }

    if (mini_view_bounds.x() < position &&
        mini_view_bounds.right() > position) {
      break;
    }
  }

  CHECK_LT(i, mini_views_size);
  if ((position - mini_view_bounds.x()) < mini_view_bounds.width() / 2) {
    adjusted_position = mini_view_bounds.x();
  } else {
    adjusted_position = mini_view_bounds.right();
    if (i + 1 < mini_views_size) {
      adjusted_position = mini_views_[i + 1]->bounds().x();
    }
  }
  return adjusted_position -
         kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
}

void DeskBarViewBase::OnLibraryButtonPressed() {
  if (desk_activation_timer_.IsRunning()) {
    return;
  }
  RecordLoadSavedDeskLibraryHistogram();

  base::UmaHistogramBoolean(type_ == Type::kDeskButton
                                ? kDeskButtonDeskBarOpenLibraryHistogramName
                                : kOverviewDeskBarOpenLibraryHistogramName,
                            true);

  if (IsDeskNameBeingModified()) {
    DeskNameView::CommitChanges(GetWidget());
  }

  aura::Window* root = GetWidget()->GetNativeWindow()->GetRootWindow();
  OverviewSession* overview_session;
  if (overview_grid_) {
    overview_session = overview_grid_->overview_session();
  } else {
    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    bool is_overview_started =
        overview_controller &&
        overview_controller->StartOverview(OverviewStartAction::kDeskButton);
    // If overview refuses to start, do nothing.
    if (!is_overview_started) {
      return;
    }
    overview_session = overview_controller->overview_session();
  }
  overview_session->ShowSavedDeskLibrary(base::Uuid(), /*saved_desk_name=*/u"",
                                         root);
}

void DeskBarViewBase::MaybeUpdateCombineDesksTooltips() {
  for (auto* mini_view : mini_views_) {
    // If desk is being removed, do not update the tooltip.
    if (mini_view->desk()->is_desk_being_removed()) {
      continue;
    }
    mini_view->desk_action_view()->UpdateCombineDesksTooltip(
        DesksController::Get()->GetCombineDesksTargetName(mini_view->desk()));
  }
}

void DeskBarViewBase::OnContentsScrolled() {
  UpdateScrollButtonsVisibility();
  UpdateGradientMask();
}

void DeskBarViewBase::OnContentsScrollEnded() {
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const int current_position = visible_bounds.x();
  const int adjusted_position =
      GetAdjustedUncroppedScrollPosition(current_position);
  if (current_position != adjusted_position) {
    scroll_view_->ScrollToPosition(scroll_view_->horizontal_scroll_bar(),
                                   adjusted_position);
  }
  UpdateGradientMask();
}

bool DeskBarViewBase::MaybeScrollByDraggedDesk() {
  CHECK(drag_proxy_);

  const gfx::Rect proxy_bounds = drag_proxy_->GetBoundsInScreen();

  // If the desk proxy overlaps a scroll button, scroll the bar in the
  // corresponding direction.
  for (ScrollArrowButton* scroll_button : {
           left_scroll_button_,
           right_scroll_button_,
       }) {
    if (scroll_button->GetVisible() &&
        proxy_bounds.Intersects(scroll_button->GetBoundsInScreen())) {
      scroll_button->OnDeskHoverStart();
      return true;
    }
    scroll_button->OnDeskHoverEnd();
  }

  return false;
}

gfx::Rect DeskBarViewBase::GetAvailableBounds() const {
  return GetWidget()->GetRootView()->bounds();
}

BEGIN_METADATA(DeskBarViewBase, View)
END_METADATA

}  // namespace ash
