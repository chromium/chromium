// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/search_results_panel.h"

#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_animation_utils.h"
#include "ash/system/mahi/resources/grit/mahi_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

inline constexpr int kPanelCornerRadius = 16;
const std::u16string kSearchBoxPlaceholderText = u"Add to your search";
inline constexpr gfx::Insets kPanelPadding =
    gfx::Insets(capture_mode::kPanelPaddingSize);
inline constexpr gfx::Insets kSearchResultsViewSpacing =
    gfx::Insets::TLBR(12, 0, 0, 0);

// Returns the target container window for the panel widget.
aura::Window* GetParentContainer(aura::Window* root, bool is_active) {
  return Shell::GetContainer(
      root, is_active ? kShellWindowId_CaptureModeSearchResultsPanel
                      : kShellWindowId_SystemModalContainer);
}

std::u16string GetSearchResultsPanelTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_SCREEN_CAPTURE_SEARCH_RESULTS_PANEL_TITLE);
}

}  // namespace

SearchResultsPanel::SearchResultsPanel() {
  // We should not use `CanShowSunfishUi` here, as that could change between
  // sending the region and receiving a URL which will then create this view
  // (for example, if the Sunfish policy changes).
  DCHECK(features::IsSunfishFeatureEnabled());
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(kPanelPadding)
      .SetCollapseMargins(true);

  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetIgnoreDefaultMainAxisMargins(true)
          .SetCollapseMargins(true)
          .AddChildren(
              // Title.
              views::Builder<views::Label>()
                  .SetText(GetSearchResultsPanelTitle())
                  .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                  .SetTextStyle(views::style::STYLE_HEADLINE_5)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetEnabledColor(cros_tokens::kCrosSysOnSurface),
              // Close Button, aligned to the right by setting a
              // `FlexSpecification` with unbounded maximum flex size and
              // `LayoutAlignment::kEnd`.
              views::Builder<views::Button>(
                  IconButton::Builder()
                      .SetType(IconButton::Type::kSmallFloating)
                      .SetVectorIcon(&kMediumOrLargeCloseButtonIcon)
                      .SetAccessibleName(l10n_util::GetStringUTF16(
                          IDS_ASH_SUNFISH_SEARCH_DIALOG_CLOSE))
                      .Build())
                  .CopyAddressTo(&close_button_)
                  .SetCallback(base::BindRepeating(
                      &SearchResultsPanel::OnCloseButtonPressed,
                      weak_ptr_factory_.GetWeakPtr()))
                  .SetProperty(
                      views::kFlexBehaviorKey,
                      views::FlexSpecification(
                          views::LayoutOrientation::kHorizontal,
                          views::MinimumFlexSizeRule::kPreferred,
                          views::MaximumFlexSizeRule::kUnbounded)
                          .WithAlignment(views::LayoutAlignment::kEnd)))
          .Build());

  SetBackground(views::CreateSolidBackground(
      chromeos::features::IsSystemBlurEnabled()
          ? cros_tokens::kCrosSysSystemBaseElevated
          : cros_tokens::kCrosSysSystemBaseElevatedOpaque));
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kPanelCornerRadius});
  layer()->SetIsFastRoundedCorner(true);
  if (chromeos::features::IsSystemBlurEnabled()) {
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }

  ShowLoadingAnimation();

  // Install highlightable views for when a Sunfish session is active and the
  // `CaptureModeSessionFocusCycler` is handling focus. Set up the focus
  // predicate for the focusable views now, so they will have the correct
  // behavior before `CaptureModeSessionFocusCycler::PseudoFocus()`
  // is called on them.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(close_button_);
  CaptureModeSessionFocusCycler::HighlightHelper::Get(close_button_)
      ->SetUpFocusPredicate();

  auto* animation_view = GetViewByID(capture_mode::kLoadingAnimationViewId);
  CHECK(animation_view);
  CaptureModeSessionFocusCycler::HighlightHelper::Install(animation_view);
  CaptureModeSessionFocusCycler::HighlightHelper::Get(animation_view)
      ->SetUpFocusPredicate();
}

SearchResultsPanel::~SearchResultsPanel() = default;

// static
views::UniqueWidgetPtr SearchResultsPanel::CreateWidget(aura::Window* root,
                                                        bool is_active) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = GetParentContainer(root, is_active);
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = wm::kShadowElevationInactiveWindow;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.rounded_corners = gfx::RoundedCornersF(kPanelCornerRadius);
  params.name = "SearchResultsPanelWidget";
  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SearchResultsPanel>());
  widget->widget_delegate()->SetTitle(GetSearchResultsPanelTitle());
  return widget;
}

std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
SearchResultsPanel::GetHighlightableItems() const {
  std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
      highlightable_items;
  CHECK(close_button_);
  highlightable_items.push_back(
      CaptureModeSessionFocusCycler::HighlightHelper::Get(close_button_.get()));
  return highlightable_items;
}

CaptureModeSessionFocusCycler::HighlightableView*
SearchResultsPanel::GetHighlightableLoadingAnimation() {
  auto* animation_view = GetViewByID(capture_mode::kLoadingAnimationViewId);
  return animation_view ? CaptureModeSessionFocusCycler::HighlightHelper::Get(
                              animation_view)
                        : nullptr;
}

views::View* SearchResultsPanel::GetWebViewForFocus() {
  CHECK(search_results_view_);
  return search_results_view_->GetInitiallyFocusedView();
}

void SearchResultsPanel::Navigate(const GURL& url) {
  if (!search_results_view_) {
    // Remove the loading animation.
    auto* animation_view = GetViewByID(capture_mode::kLoadingAnimationViewId);
    CHECK(animation_view);
    RemoveChildViewT(animation_view);

    search_results_view_ =
        AddChildView(CaptureModeController::Get()->CreateSearchResultsView());
    search_results_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded));
    search_results_view_->SetProperty(views::kMarginsKey,
                                      kSearchResultsViewSpacing);
  }
  search_results_view_->Navigate(url);
}

void SearchResultsPanel::RefreshStackingOrder(aura::Window* new_root) {
  aura::Window* native_window = GetWidget()->GetNativeWindow();
  // While the capture mode session is active, we parent the panel to its own
  // container, else we parent it to the system modal container.
  aura::Window* new_parent = GetParentContainer(
      new_root ? new_root : native_window->GetRootWindow(), !!new_root);
  views::Widget::ReparentNativeView(native_window, new_parent);
}

void SearchResultsPanel::ShowLoadingAnimation() {
  if (GetViewByID(capture_mode::kLoadingAnimationViewId)) {
    return;
  }

  // Remove the search results view if present.
  if (search_results_view_) {
    auto* search_results_view = search_results_view_.get();
    search_results_view_ = nullptr;
    RemoveChildViewT(search_results_view);
  }

  CHECK(!search_results_view_);

  // Add the animation view and play it.
  auto* animation_view = AddChildView(
      views::Builder<views::AnimatedImageView>()
          // Use an ID instead of saving a `raw_ptr` to avoid a dangling pointer
          // when we remove this child later.
          .SetID(capture_mode::kLoadingAnimationViewId)
          .SetAccessibleName(l10n_util::GetStringUTF16(
              IDS_ASH_SUNFISH_RESULTS_LOADING_ACCESSIBLE_NAME))
          .SetAnimatedImage(mahi_animation_utils::GetLottieAnimationData(
              IDR_MAHI_LOADING_SUMMARY_ANIMATION))
          .Build());
  animation_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  animation_view->Play(mahi_animation_utils::GetLottiePlaybackConfig(
      *animation_view->animated_image()->skottie(),
      IDR_MAHI_LOADING_SUMMARY_ANIMATION));
}

void SearchResultsPanel::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void SearchResultsPanel::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

bool SearchResultsPanel::HasFocus() const {
  // Returns true if `this` or any of its child views has focus.
  const views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    return false;
  }

  const views::View* focused_view = focus_manager->GetFocusedView();
  if (!focused_view) {
    return false;
  }

  return Contains(focused_view);
}

void SearchResultsPanel::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    return;
  }
  RefreshPanelBounds();
}

void SearchResultsPanel::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!(metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
         DISPLAY_METRIC_DEVICE_SCALE_FACTOR | DISPLAY_METRIC_WORK_AREA))) {
    return;
  }
  RefreshPanelBounds();
}

void SearchResultsPanel::OnDidChangeFocus(View* focused_before,
                                          View* focused_now) {
  // Update the focus ring of the previously focused view, if available.
  if (focused_before) {
    if (views::FocusRing* before_ring = views::FocusRing::Get(focused_before)) {
      before_ring->SchedulePaint();
    }
  }

  // Update the focus ring of the newly focused view, if available.
  if (focused_now) {
    if (views::FocusRing* now_ring = views::FocusRing::Get(focused_now)) {
      now_ring->SchedulePaint();
    }
  }
}

void SearchResultsPanel::OnCloseButtonPressed() {
  CHECK(GetWidget());
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void SearchResultsPanel::RefreshPanelBounds() {
  views::Widget* widget = GetWidget();

  // First attempt to restore the preferred size. This is needed because when
  // the display is zoomed in, the widget may be cropped to fit within the
  // screen. On zoom out, the widget should return to its preferred size.
  gfx::Rect widget_bounds_in_screen(
      widget->GetWindowBoundsInScreen().origin(),
      gfx::Size(capture_mode::kSearchResultsPanelTotalWidth,
                capture_mode::kSearchResultsPanelTotalHeight));

  // Adjust the preferred size and bounds based on the current display.
  const display::Display display =
      display::Screen::Get()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  const gfx::Rect work_area_in_screen(display.work_area());
  if (!work_area_in_screen.Contains(widget_bounds_in_screen)) {
    widget_bounds_in_screen.AdjustToFit(work_area_in_screen);
  }
  widget->SetBounds(widget_bounds_in_screen);
}

BEGIN_METADATA(SearchResultsPanel)
END_METADATA

}  // namespace ash
