// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view_base.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/check.h"
#include "base/notreached.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

OverviewHighlightController* GetHighlightController() {
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()->highlight_controller();
}

// Checks whether there are any external keyboards.
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

      // The presenter is shutdown early in the overview destruction process to
      // prevent calls to the model. Some animations on the desk bar may still
      // call this function past shutdown start. In this case we just continue
      // as if the saved desk UI should be hidden.
      OverviewSession* session = bar_view_->overview_grid()->overview_session();
      const bool should_show_saved_desk_library =
          saved_desk_util::IsSavedDesksEnabled() && session &&
          !session->is_shutting_down() &&
          session->saved_desk_presenter()->should_show_saved_desk_library();
      auto* zero_state_library_button = bar_view_->zero_state_library_button();
      const gfx::Size zero_state_library_button_size =
          should_show_saved_desk_library
              ? zero_state_library_button->GetPreferredSize()
              : gfx::Size();
      const int width_for_zero_state_library_button =
          should_show_saved_desk_library
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
        zero_state_library_button->SetVisible(should_show_saved_desk_library);
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

    // Update the size of the |host|, which is |scroll_view_contents_| here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then |scroll_view_| will know whether the contents need to
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
    const gfx::Rect scroll_bounds = bar_view_->scroll_view_->bounds();

    auto* new_desk_button_label = bar_view_->new_desk_button_label();
    auto* library_button_label = bar_view_->library_button_label();

    // `host` here is `scroll_view_contents_`.
    if (bar_view_->IsZeroState()) {
      host->SetBoundsRect(scroll_bounds);

      new_desk_button_label->SetVisible(false);
      library_button_label->SetVisible(false);

      auto* default_desk_button = bar_view_->default_desk_button();
      const gfx::Size default_desk_button_size =
          default_desk_button->GetPreferredSize();

      auto* new_desk_button = bar_view_->new_desk_button();
      const gfx::Size new_desk_button_size =
          new_desk_button->GetPreferredSize();

      // The presenter is shutdown early in the overview destruction process to
      // prevent calls to the model. Some animations on the desk bar may still
      // call this function past shutdown start. In this case we just continue
      // as if the saved desk UI should be hidden.
      OverviewSession* session = bar_view_->overview_grid()->overview_session();
      const bool should_show_saved_desk_library =
          saved_desk_util::IsSavedDesksEnabled() && session &&
          !session->is_shutting_down() &&
          session->saved_desk_presenter()->should_show_saved_desk_library();
      auto* library_button = bar_view_->library_button();
      const gfx::Size library_button_size =
          should_show_saved_desk_library ? library_button->GetPreferredSize()
                                         : gfx::Size();
      const int width_for_library_button =
          should_show_saved_desk_library
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
        library_button->SetVisible(should_show_saved_desk_library);
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
    gfx::Size library_button_size = library_button->GetPreferredSize();

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

DeskBarViewBase::DeskBarViewBase(aura::Window* root, Type type)
    : type_(type), state_(GetPerferredState(type)), root_(root) {
  CHECK(root && root->IsRootWindow());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetBorder(std::make_unique<views::HighlightBorder>(
      /*corner_radius=*/0,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderNoShadow
          : views::HighlightBorder::Type::kHighlightBorder2));

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
  right_scroll_button_ = AddChildView(std::make_unique<ScrollArrowButton>(
      base::BindRepeating(&DeskBarViewBase::ScrollToNextPage,
                          base::Unretained(this)),
      /*is_left_arrow=*/false, this));

  // Make the scroll content view animatable by painting to a layer.
  scroll_view_contents_ =
      scroll_view_->SetContents(std::make_unique<views::View>());
  scroll_view_contents_->SetPaintToLayer();

  if (chromeos::features::IsJellyrollEnabled()) {
    default_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<CrOSNextDefaultDeskButton>(this));
    new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<CrOSNextDeskIconButton>(
            this, &kDesksNewDeskButtonIcon,
            l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
            cros_tokens::kCrosSysOnPrimary, cros_tokens::kCrosSysPrimary,
            /*initially_enabled=*/DesksController::Get()->CanCreateDesks(),
            base::BindRepeating(&DeskBarViewBase::OnNewDeskButtonPressed,
                                base::Unretained(this),
                                DesksCreationRemovalSource::kButton)));
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
            base::BindRepeating(&DeskBarViewBase::OnNewDeskButtonPressed,
                                base::Unretained(this),
                                DesksCreationRemovalSource::kButton)));

    zero_state_default_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateDefaultDeskButton>(this));
    zero_state_new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateIconButton>(
            this, &kDesksNewDeskButtonIcon,
            l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
            base::BindRepeating(&DeskBarViewBase::OnNewDeskButtonPressed,
                                base::Unretained(this),
                                DesksCreationRemovalSource::kButton)));
  }

  if (saved_desk_util::IsSavedDesksEnabled()) {
    int button_text_id = IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY;
    if (!saved_desk_util::AreDesksTemplatesEnabled()) {
      button_text_id = IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER;
    }

    if (chromeos::features::IsJellyrollEnabled()) {
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

const char* DeskBarViewBase::GetClassName() const {
  return "DeskBarViewBase";
}

void DeskBarViewBase::Layout() {
  if (is_bounds_animation_on_going_) {
    return;
  }

  // It's possible that this is not owned by the overview grid anymore, because
  // when exiting overview, the bar stays alive for animation.
  if (type_ == Type::kOverview && !overview_grid_) {
    return;
  }

  // Scroll buttons are kept `kDeskBarScrollViewMinimumHorizontalPadding` away
  // from the edge of the scroll view. So the horizontal padding of the scroll
  // view is set to guarantee enough space for the scroll buttons.
  const gfx::Insets insets = (type_ == Type::kOverview)
                                 ? overview_grid_->GetGridInsets()
                                 : gfx::Insets::TLBR(0, 0, 0, 0);
  DCHECK(insets.left() == insets.right());
  const int horizontal_padding =
      std::max(kDeskBarScrollViewMinimumHorizontalPadding, insets.left());
  left_scroll_button_->SetBounds(
      horizontal_padding - kDeskBarScrollViewMinimumHorizontalPadding,
      bounds().y(), kDeskBarScrollButtonWidth, bounds().height());
  right_scroll_button_->SetBounds(
      bounds().right() - horizontal_padding -
          (kDeskBarScrollButtonWidth -
           kDeskBarScrollViewMinimumHorizontalPadding),
      bounds().y(), kDeskBarScrollButtonWidth, bounds().height());

  gfx::Rect scroll_bounds = bounds();
  // Align with the overview grid in horizontal, so only horizontal insets are
  // needed here.
  scroll_bounds.Inset(gfx::Insets::VH(0, horizontal_padding));
  scroll_view_->SetBoundsRect(scroll_bounds);

  // Clip the contents that are outside of the |scroll_view_|'s bounds.
  scroll_view_->layer()->SetMasksToBounds(true);
  scroll_view_->Layout();

  UpdateScrollButtonsVisibility();
  UpdateGradientMask();
}

bool DeskBarViewBase::OnMousePressed(const ui::MouseEvent& event) {
  DeskNameView::CommitChanges(GetWidget());
  return false;
}

void DeskBarViewBase::OnGestureEvent(ui::GestureEvent* event) {
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
  auto* controller = DesksController::Get();
  if (!controller->CanCreateDesks()) {
    return;
  }
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
  if (!chromeos::features::IsJellyrollEnabled() && mini_views_.size() == 1u) {
    SwitchToZeroState();
  }
}

void DeskBarViewBase::NudgeDeskName(int desk_index) {
  DCHECK_LT(desk_index, static_cast<int>(mini_views_.size()));

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
  if (type_ != Type::kOverview) {
    return;
  }

  const bool should_show_ui = overview_grid_->overview_session()
                                  ->saved_desk_presenter()
                                  ->should_show_saved_desk_library();
  const bool is_zero_state = IsZeroState();

  zero_state_library_button_->SetVisible(should_show_ui && is_zero_state);
  expanded_state_library_button_->SetVisible(should_show_ui && !is_zero_state);

  // Removes the button from the tabbing order if it becomes invisible.
  auto* highlight_controller = GetHighlightController();
  if (!zero_state_library_button_->GetVisible()) {
    highlight_controller->OnViewDestroyingOrDisabling(
        zero_state_library_button_);
  }
  if (!expanded_state_library_button_->GetVisible()) {
    highlight_controller->OnViewDestroyingOrDisabling(
        expanded_state_library_button_->GetInnerButton());
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
  if (type_ != Type::kOverview) {
    return;
  }

  const bool should_show_ui = overview_grid_->overview_session()
                                  ->saved_desk_presenter()
                                  ->should_show_saved_desk_library();

  library_button_label_->SetVisible(
      should_show_ui &&
      (library_button_->state() == CrOSNextDeskIconButton::State::kActive));

  // If the visibility of the library button doesn't change, return early.
  if (library_button_->GetVisible() == should_show_ui) {
    return;
  }

  library_button_->SetVisible(should_show_ui);
  if (should_show_ui) {
    if (overview_grid_->WillShowSavedDeskLibrary()) {
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
  button->UpdateState(target_state);
  Layout();
}

void DeskBarViewBase::UpdateNewMiniViews(bool initializing_bar_view,
                                         bool expanding_bar_view) {
  NOTREACHED();
}

void DeskBarViewBase::SwitchToZeroState() {
  NOTREACHED();
}

void DeskBarViewBase::SwitchToExpandedState() {
  NOTREACHED();
}

void DeskBarViewBase::HandlePressEvent(DeskMiniView* mini_view,
                                       const ui::LocatedEvent& event) {
  NOTREACHED();
}

void DeskBarViewBase::HandleLongPressEvent(DeskMiniView* mini_view,
                                           const ui::LocatedEvent& event) {
  NOTREACHED();
}

void DeskBarViewBase::HandleDragEvent(DeskMiniView* mini_view,
                                      const ui::LocatedEvent& event) {
  NOTREACHED();
}

bool DeskBarViewBase::HandleReleaseEvent(DeskMiniView* mini_view,
                                         const ui::LocatedEvent& event) {
  NOTREACHED_NORETURN();
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
  left_scroll_button_->SetVisible(visible_bounds.x() > 0);
  right_scroll_button_->SetVisible(visible_bounds.right() <
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

  DCHECK_LT(i, mini_views_size);
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
  RecordLoadSavedDeskLibraryHistogram();
  if (IsDeskNameBeingModified()) {
    DeskNameView::CommitChanges(GetWidget());
  }

  aura::Window* root = GetWidget()->GetNativeWindow()->GetRootWindow();
  OverviewSession* overview_session;
  if (overview_grid_) {
    overview_session = overview_grid_->overview_session();
  } else {
    Shell::Get()->overview_controller()->StartOverview(
        OverviewStartAction::kDeskButton);
    overview_session = Shell::Get()->overview_controller()->overview_session();
  }
  overview_session->ShowSavedDeskLibrary(base::GUID(), /*saved_desk_name=*/u"",
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

}  // namespace ash
