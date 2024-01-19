// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view_base.h"

#include <vector>

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
#include "ash/wm/desks/desk.h"
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
#include "ash/wm/overview/overview_focus_cycler.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/work_area_insets.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/event_monitor.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Duration of delay when Bento Bar Desk Button is clicked.
constexpr base::TimeDelta kAnimationDelayDuration = base::Milliseconds(100);

OverviewFocusCycler* GetFocusCycler() {
  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller || !overview_controller->InOverviewSession()) {
    return nullptr;
  }
  return overview_controller->overview_session()->focus_cycler();
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

void SetupBackgroundView(DeskBarViewBase* bar_view) {
  const bool type_is_desk_button =
      bar_view->type() == DeskBarViewBase::Type::kDeskButton;
  auto* view = type_is_desk_button ? bar_view->background_view() : bar_view;
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
  if (features::IsBackgroundBlurEnabled()) {
    view->layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    view->layer()->SetBackdropFilterQuality(
        ColorProvider::kBackgroundBlurQuality);
  }

  const int corner_radius = type_is_desk_button
                                ? kDeskBarCornerRadiusOverviewDeskButton
                                : kDeskBarCornerRadiusOverview;
  view->SetBorder(std::make_unique<views::HighlightBorder>(
      corner_radius, views::HighlightBorder::Type::kHighlightBorderNoShadow));
  view->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));
  view->SetBackground(
      views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
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

  int GetContentViewX(int contents_width) const {
    // The x of the first mini view should include the focus ring thickness and
    // padding into consideration, otherwise the focus ring won't be drawn on
    // the left side of the first mini view. `bar_view` is centralized in
    // overview mode or when shelf is on the bottom. When shelf is on the
    // left/right, bar view anchors to the desk button.
    const auto shelf_type = Shelf::ForWindow(bar_view_->root())->alignment();
    if (bar_view_->type() == DeskBarViewBase::Type::kOverview ||
        shelf_type == ShelfAlignment::kBottom ||
        shelf_type == ShelfAlignment::kBottomLocked) {
      return (width_ - contents_width) / 2 +
             kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    }

    if (shelf_type == ShelfAlignment::kLeft) {
      return kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    }

    CHECK_EQ(shelf_type, ShelfAlignment::kRight);
    return width_ - contents_width +
           kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
  }

  void LayoutBackground() {
    if (!bar_view_->background_view_) {
      return;
    }

    const ShelfAlignment shelf_alignment =
        Shelf::ForWindow(bar_view_->root_)->alignment();
    const gfx::Rect perferred_bounds =
        gfx::Rect(bar_view_->CalculatePreferredSize());
    const gfx::Rect current_bounds = gfx::Rect(bar_view_->size());
    gfx::Rect new_bounds = perferred_bounds;
    if (shelf_alignment == ShelfAlignment::kBottom) {
      new_bounds = current_bounds;
      new_bounds.ClampToCenteredSize(perferred_bounds.size());
    } else if ((shelf_alignment == ShelfAlignment::kLeft) ==
               base::i18n::IsRTL()) {
      new_bounds.Offset(current_bounds.width() - perferred_bounds.width(), 0);
    }
    bar_view_->background_view_->SetBoundsRect(new_bounds);
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

  // Updates the visibility of child views based on current `bar_view_`.
  void UpdateChildViewsVisibility() {
    auto* default_desk_button = bar_view_->default_desk_button();
    auto* new_desk_button = bar_view_->new_desk_button();
    auto* new_desk_button_label = bar_view_->new_desk_button_label();
    auto* library_button = bar_view_->library_button();
    auto* library_button_label = bar_view_->library_button_label();
    const bool zero_state = bar_view_->IsZeroState();
    default_desk_button->SetVisible(zero_state);
    new_desk_button->SetVisible(true);
    new_desk_button_label->SetVisible(
        !zero_state &&
        new_desk_button->state() == CrOSNextDeskIconButton::State::kActive);
    if (library_button) {
      library_button->SetVisible(bar_view_->ShouldShowLibraryUi());
    }
    if (library_button_label) {
      library_button_label->SetVisible(
          !zero_state &&
          library_button->state() == CrOSNextDeskIconButton::State::kActive);
    }
  }

  // views::LayoutManager:
  void Layout(views::View* host) override {
    TRACE_EVENT0("ui", "DeskBarScrollViewLayout::Layout");

    const gfx::Rect scroll_bounds = bar_view_->scroll_view_->bounds();

    // Update visibility of child views so that `GetPreferredSize()` returns
    // correct size.
    UpdateChildViewsVisibility();

    const gfx::Size contents_size = host->GetPreferredSize();

    // `host` here is `scroll_view_contents_`.
    if (bar_view_->IsZeroState()) {
      host->SetBoundsRect(scroll_bounds);

      // Update default desk button. In addition, update its button text since
      // it may change while removing a desk and going back to the zero state.
      // Make sure default desk button is always visible while in zero state
      // bar.
      auto* default_desk_button = bar_view_->default_desk_button();
      default_desk_button->SetBoundsRect(gfx::Rect(
          gfx::Point((scroll_bounds.width() - contents_size.width()) / 2,
                     kDeskBarZeroStateY),
          default_desk_button->GetPreferredSize()));
      default_desk_button->UpdateLabelText();

      // Update new desk button.
      auto* new_desk_button = bar_view_->new_desk_button();
      new_desk_button->SetBoundsRect(
          gfx::Rect(gfx::Point(default_desk_button->bounds().right() +
                                   kDeskBarZeroStateButtonSpacing,
                               kDeskBarZeroStateY),
                    new_desk_button->GetPreferredSize()));

      // Update library button.
      if (auto* library_button = bar_view_->library_button()) {
        library_button->SetBoundsRect(
            gfx::Rect(gfx::Point(new_desk_button->bounds().right() +
                                     kDeskBarZeroStateButtonSpacing,
                                 kDeskBarZeroStateY),
                      library_button->GetPreferredSize()));
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
    const bool is_rtl = base::i18n::IsRTL();
    if (is_rtl) {
      base::ranges::reverse(mini_views);
    }

    width_ = std::max(scroll_bounds.width(), contents_size.width());

    // Update the size of the `host`, which is `scroll_view_contents_` here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then `scroll_view_` will know whether the contents need to
    // be scolled or not.
    host->SetSize(gfx::Size(width_, contents_size.height()));

    const int increment = is_rtl ? -1 : 1;
    const int y =
        kDeskBarMiniViewsY - mini_views[0]->GetPreviewBorderInsets().top();
    const gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();
    auto layout_mini_views = [&](int& x) {
      const int start = is_rtl ? mini_views.size() - 1 : 0;
      const int end = is_rtl ? -1 : mini_views.size();
      const int delta_x =
          (mini_view_size.width() + kDeskBarMiniViewsSpacing) * increment;
      for (int i = start; i != end; i += increment) {
        auto* mini_view = mini_views[i];
        mini_view->SetBoundsRect(
            gfx::Rect(gfx::Point(is_rtl ? x - mini_view_size.width() : x, y),
                      mini_view_size));
        x += delta_x;
      }
    };
    auto* desk_name_view = mini_views[0]->desk_name_view();
    auto layout_new_desk_button = [&](int& x) {
      auto* new_desk_button = bar_view_->new_desk_button();
      const gfx::Size new_desk_button_size =
          new_desk_button->GetPreferredSize();
      const gfx::Rect new_desk_button_bounds(gfx::Rect(
          gfx::Point(is_rtl ? x - new_desk_button_size.width() : x, y),
          new_desk_button_size));
      new_desk_button->SetBoundsRect(new_desk_button_bounds);
      LayoutDeskIconButtonLabel(bar_view_->new_desk_button_label(),
                                new_desk_button_bounds, desk_name_view,
                                IDS_ASH_DESKS_NEW_DESK_BUTTON);
      x +=
          (new_desk_button_size.width() + kDeskBarMiniViewsSpacing) * increment;
    };
    auto layout_library_button = [&](int& x) {
      auto* library_button = bar_view_->library_button();
      if (!library_button) {
        return;
      }
      const gfx::Size library_button_size =
          library_button ? library_button->GetPreferredSize() : gfx::Size();
      const gfx::Rect library_button_bounds(
          gfx::Rect(gfx::Point(is_rtl ? x - library_button_size.width() : x, y),
                    library_button_size));
      library_button->SetBoundsRect(library_button_bounds);
      LayoutDeskIconButtonLabel(
          bar_view_->library_button_label(), library_button_bounds,
          desk_name_view,
          /*label_text_id=*/
          saved_desk_util::AreDesksTemplatesEnabled()
              ? IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY
              : IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER);
      x += (library_button_size.width() + kDeskBarMiniViewsSpacing) * increment;
    };

    // When the desk bar is in middle of bar shrink animation, the bounds of
    // scroll view contents is actually wider than `contents_width`. When RTL is
    // not on, we layout UIs from left to right with `x` indicating the current
    // available position to place the next UI; to make animation work for RTL,
    // we need to layout from right to left.
    // TODO(b/301665941): improve layout calculation for RTL.
    if (is_rtl) {
      int x = width_ - GetContentViewX(contents_size.width());
      layout_library_button(x);
      layout_new_desk_button(x);
      layout_mini_views(x);
    } else {
      int x = GetContentViewX(contents_size.width());
      layout_mini_views(x);
      layout_new_desk_button(x);
      layout_library_button(x);
    }

    LayoutBackground();
  }

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    int width = 0;
    std::vector<views::View*> child_views;

    for (auto* mini_view : bar_view_->mini_views_) {
      child_views.emplace_back(mini_view);
    }

    child_views.emplace_back(bar_view_->default_desk_button_);
    child_views.emplace_back(bar_view_->new_desk_button_);
    child_views.emplace_back(bar_view_->library_button_);

    const int child_spacing =
        bar_view_->state_ == DeskBarViewBase::State::kExpanded
            ? kDeskBarMiniViewsSpacing
            : kDeskBarZeroStateButtonSpacing;
    for (auto* child : child_views) {
      if (!child || !child->GetVisible()) {
        continue;
      }
      if (width) {
        width += child_spacing;
      }
      width += child->GetPreferredSize().width();
    }
    width += kDeskBarDeskPreviewViewFocusRingThicknessAndPadding * 2;

    return gfx::Size(
        width, DeskBarViewBase::GetPreferredBarHeight(
                   bar_view_->root(), bar_view_->type_, bar_view_->state_));
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

  // Background layer is needed for desk bar animation.
  if (type_ == Type::kDeskButton) {
    background_view_ = AddChildView(std::make_unique<views::View>());
  }
  SetupBackgroundView(this);

  // Use layer scrolling so that the contents will paint on top of the parent,
  // which uses `SetPaintToLayer()`.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  scroll_view_->layer()->SetMasksToBounds(true);
  scroll_view_->SetBackgroundColor(std::nullopt);
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
  // If this is a desk button desk bar, the bar does not paint to a layer,
  // therefore, the scroll arrow buttons need to be painted.
  if (type_ == Type::kDeskButton) {
    left_scroll_button_->SetPaintToLayer();
    left_scroll_button_->layer()->SetFillsBoundsOpaquely(false);
    right_scroll_button_->SetPaintToLayer();
    right_scroll_button_->layer()->SetFillsBoundsOpaquely(false);
  }

  // Since we created a `ScrollView` with scrolling with layers enabled, it will
  // automatically create a layer for our contents.
  scroll_view_contents_ =
      scroll_view_->SetContents(std::make_unique<views::View>());
  CHECK(scroll_view_contents_->layer());

  default_desk_button_ = scroll_view_contents_->AddChildView(
      std::make_unique<CrOSNextDefaultDeskButton>(this));
  new_desk_button_ = scroll_view_contents_->AddChildView(
      std::make_unique<CrOSNextDeskIconButton>(
          this, &kDesksNewDeskButtonIcon,
          l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
          cros_tokens::kCrosSysOnPrimary, cros_tokens::kCrosSysPrimary,
          /*initially_enabled=*/DesksController::Get()->CanCreateDesks(),
          base::BindRepeating(
              &DeskBarViewBase::OnNewDeskButtonPressed, base::Unretained(this),
              type_ == Type::kDeskButton
                  ? DesksCreationRemovalSource::kDeskButtonDeskBarButton
                  : DesksCreationRemovalSource::kButton)));
  new_desk_button_label_ =
      scroll_view_contents_->AddChildView(std::make_unique<views::Label>());
  new_desk_button_label_->SetPaintToLayer();
  new_desk_button_label_->layer()->SetFillsBoundsOpaquely(false);

  if (saved_desk_util::IsSavedDesksEnabled()) {
    int button_text_id = IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY;
    if (!saved_desk_util::AreDesksTemplatesEnabled()) {
      button_text_id = IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER;
    }

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
  TRACE_EVENT0("ui", "DeskBarViewBase::~DeskBarViewBase");

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

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.accept_events = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = bounds;

  // The contents of this widget will have a textured layer, so we can mark
  // the widget's layer as not drawn.
  params.layer_type = ui::LAYER_NOT_DRAWN;

  if (type == Type::kOverview) {
    // Overview desk bar should live under the currently-active desk container
    // on `root`.
    params.context = root;
    params.name = "OverviewDeskBarWidget";
    // Even though this widget exists on the active desk container, it should
    // not show up in the MRU list, and it should not be mirrored in the desks
    // mini_views.
    params.init_properties_container.SetProperty(kOverviewUiKey, true);
    params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  } else {
    // Desk button desk bar should live under the shelf bubble container on
    // `root`.
    params.parent =
        Shell::GetContainer(root, kShellWindowId_ShelfBubbleContainer);
    params.name = "DeskButtonDeskBarWidget";
  }

  widget->Init(std::move(params));

  auto* window = widget->GetNativeWindow();
  window->SetId(kShellWindowId_DesksBarWindow);
  wm::SetWindowVisibilityAnimationTransition(window, wm::ANIMATE_NONE);

  return widget;
}

void DeskBarViewBase::Layout() {
  TRACE_EVENT0("ui", "DeskBarViewBase::Layout");

  if (pause_layout_) {
    return;
  }

  // It's possible that this is not owned by the overview grid anymore, because
  // when exiting overview, the bar stays alive for animation.
  if (type_ == Type::kOverview && !overview_grid_) {
    return;
  }

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
  // When the bar reaches its max possible size, it's size does not change, but
  // we still need to layout child UIs to their right positions.
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
  // It's possible that window occlusion state change triggers some new windows
  // to show up during desk bar initialization process. It should not broadcast
  // the desk content update since desk mini view may not be ready. Please refer
  // to b/320530730.
  Desk::ScopedContentUpdateNotificationDisabler desks_scoped_notify_disabler(
      DesksController::Get()->desks(), /*notify_when_destroyed=*/false);

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
    ScrollToShowViewIfNecessary(new_desk_button_);
  }
}

void DeskBarViewBase::NudgeDeskName(int desk_index) {
  CHECK_LT(desk_index, static_cast<int>(mini_views_.size()));

  auto* name_view = mini_views_[desk_index]->desk_name_view();
  name_view->RequestFocus();

  // Set `name_view`'s accessible name if its text is cleared.
  if (name_view->GetAccessibleName().empty()) {
    name_view->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_DESKS_DESK_NAME));
  }

  if (type_ == Type::kOverview) {
    MoveFocusToView(name_view);

    // If we're in tablet mode and there are no external keyboards, open up the
    // virtual keyboard.
    if (display::Screen::GetScreen()->InTabletMode() &&
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
    library_button_->set_paint_as_active(
        overview_grid_->IsShowingSavedDeskLibrary());
    library_button_->UpdateFocusState();
  }
}

void DeskBarViewBase::UpdateDeskButtonsVisibility() {
  const bool is_zero_state = IsZeroState();
  default_desk_button_->SetVisible(is_zero_state);
  new_desk_button_label_->SetVisible(new_desk_button_->state() ==
                                     CrOSNextDeskIconButton::State::kActive);

  UpdateLibraryButtonVisibility();
}

void DeskBarViewBase::UpdateLibraryButtonVisibility() {
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

  PerformDeskIconButtonScaleAnimation(button, this, scale_transform, shift_x);
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
        display::Screen::GetScreen()->InTabletMode()) {
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
    new_desk_button_->UpdateFocusState();
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
      // During a mouse drag, if we touch any other mini view, since the other
      // mini view receives `ET_GESTURE_END` event, hence `mini_view` here might
      // be different than `drag_view_`. Thus, we use `drag_view_`. Please refer
      // to b/296106746.
      EndDragDesk(drag_view_, /*end_by_user=*/true);
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
    chromeos::haptics_util::PlayHapticTouchpadEffect(
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

  const bool is_expanding_bar_view =
      new_desk_button_->state() == CrOSNextDeskIconButton::State::kZero;
  UpdateNewMiniViews(/*initializing_bar_view=*/false, is_expanding_bar_view);
  MaybeUpdateCombineDesksTooltips();
  if (!DesksController::Get()->CanCreateDesks()) {
    new_desk_button_->SetEnabled(/*enabled=*/false);
  }
}

void DeskBarViewBase::OnDeskRemoved(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  auto iter = base::ranges::find(mini_views_, desk, &DeskMiniView::desk);

  // There are cases where a desk may be removed before the `desk_bar_view`
  // finishes initializing (i.e. removed on a separate root window before the
  // overview starting animation completes). In those cases, that mini_view
  // would not exist and the bar view will already be in the correct state so we
  // do not need to update the UI (crbug.com/1346154).
  if (iter == mini_views_.end()) {
    return;
  }

  if (type_ == Type::kOverview) {
    if (auto* focus_cycler = GetFocusCycler()) {
      // Let the focus cycler know the view is destroying before it is removed
      // from the collection because it needs to know the index of the mini
      // view, or the desk name view (if either is currently focused) relative
      // to other traversable views.
      // The order here matters, we call it first on the desk_name_view since it
      // comes later in the focus order (See documentation of
      // `OnViewDestroyingOrDisabling()`).
      focus_cycler->OnViewDestroyingOrDisabling((*iter)->desk_name_view());
      focus_cycler->OnViewDestroyingOrDisabling((*iter)->desk_preview());
    }
  }

  new_desk_button_->SetEnabled(/*enabled=*/true);

  for (auto* mini_view : mini_views_) {
    mini_view->UpdateDeskButtonVisibility();
  }

  // Remove the mini view from the list now. And remove it from its parent
  // after the animation is done.
  DeskMiniView* removed_mini_view = *iter;
  mini_views_.erase(iter);

  // End dragging desk if remove a dragged desk.
  if (drag_view_ == removed_mini_view) {
    EndDragDesk(removed_mini_view, /*end_by_user=*/false);
  }

  // Document all the current X coordinates of the views before we perform a
  // layout operation.
  const auto views_previous_x_map = GetAnimatableViewsCurrentXMap();

  // There is desk removal animation for overview bar but not for desk button
  // desk bar.
  if (type_ == Type::kOverview) {
    Layout();
    // Overview bar desk removal will preform mini view removal animation, while
    // desk button bar removes mini view immediately.
    PerformRemoveDeskMiniViewAnimation(removed_mini_view);
  } else {
    const auto old_background_bounds = background_view_->GetBoundsInScreen();
    // Desk button bar does not have mini view removal animation, mini view will
    // disappear immediately. Desk button bar will shrink during desk removal.
    removed_mini_view->parent()->RemoveChildViewT(removed_mini_view);
    scroll_view_->Layout();
    PerformDeskBarRemoveDeskAnimation(this, old_background_bounds);
  }
  PerformDeskBarChildViewShiftAnimation(this, views_previous_x_map);
  MaybeUpdateCombineDesksTooltips();
}

void DeskBarViewBase::OnDeskReordered(int old_index, int new_index) {
  desks_util::ReorderItem(mini_views_, old_index, new_index);

  // Update the order of child views.
  auto* reordered_view = mini_views_[new_index];
  reordered_view->parent()->ReorderChildView(reordered_view, new_index);
  reordered_view->parent()->NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged, true);

  // Update the desk indices in the shortcut views.
  reordered_view->UpdateDeskButtonVisibility();
  mini_views_[old_index]->UpdateDeskButtonVisibility();

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
  // Document all the current X coordinates of the views before we perform a
  // layout operation.
  const auto views_previous_x_map = GetAnimatableViewsCurrentXMap();

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

  if (new_desk_button_->state() == CrOSNextDeskIconButton::State::kActive) {
    // Make sure the new desk button is updated to expanded state from the
    // active state. This can happen when dropping the window on the new desk
    // button.
    new_desk_button_->UpdateState(CrOSNextDeskIconButton::State::kExpanded);
  }

  const gfx::Rect old_bar_bounds = this->GetBoundsInScreen();

  // Bar widget bounds may need an update. Please note, we pause layout here so
  // it does not do it twice.
  pause_layout_ = true;
  UpdateBarBounds();
  pause_layout_ = false;

  Layout();

  if (initializing_bar_view) {
    return;
  }

  if (type_ == Type::kDeskButton) {
    PerformDeskBarAddDeskAnimation(this, old_bar_bounds);
  }
  PerformAddDeskMiniViewAnimation(new_mini_views,
                                  begin_x - GetFirstMiniViewXOffset());
  PerformDeskBarChildViewShiftAnimation(this, views_previous_x_map);
}

void DeskBarViewBase::SwitchToExpandedState() {
  state_ = DeskBarViewBase::State::kExpanded;

  UpdateDeskButtonsVisibility();
  PerformZeroStateToExpandedStateMiniViewAnimation(this);
}

void DeskBarViewBase::OnUiUpdateDone() {
  if (on_update_ui_closure_for_testing_) {
    std::move(on_update_ui_closure_for_testing_).Run();
  }
}

void DeskBarViewBase::UpdateBarBounds() {}

int DeskBarViewBase::GetFirstMiniViewXOffset() const {
  return mini_views_.empty() ? bounds().CenterPoint().x()
                             : mini_views_[0]->GetMirroredX();
}

base::flat_map<views::View*, int>
DeskBarViewBase::GetAnimatableViewsCurrentXMap() const {
  base::flat_map<views::View*, int> result;
  auto insert_view = [&](views::View* view) {
    if (view) {
      result.emplace(view, view->GetBoundsInScreen().x());
    }
  };

  for (auto* mini_view : mini_views_) {
    insert_view(mini_view);
  }
  insert_view(new_desk_button_);
  insert_view(library_button_);
  return result;
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

BEGIN_METADATA(DeskBarViewBase, View)
END_METADATA

}  // namespace ash
