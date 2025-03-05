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
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

inline constexpr int kPanelCornerRadius = 16;
const std::u16string kSearchBoxPlaceholderText = u"Add to your search";
inline constexpr gfx::Insets kPanelPadding =
    gfx::Insets(capture_mode::kPanelPaddingSize);
inline constexpr int kHeaderIconSize = 20;
inline constexpr int kSearchBoxHeight = 48;
inline constexpr int kSearchBoxRadius = 24;
inline constexpr int kSearchBoxImageRadius = 8;
inline constexpr gfx::Insets kSearchBoxViewSpacing = gfx::Insets::VH(12, 0);
inline constexpr gfx::Insets kSearchResultsViewSpacing =
    gfx::Insets::TLBR(12, 0, 0, 0);
inline constexpr gfx::Insets kSearchImageSpacing =
    gfx::Insets::TLBR(8, 16, 8, 12);
inline constexpr gfx::Insets kSearchTextfieldSpacing =
    gfx::Insets::TLBR(14, 0, 14, 16);
inline constexpr gfx::Insets kHeaderIconSpacing = gfx::Insets::TLBR(0, 2, 0, 8);

// Returns the target container window for the panel widget.
aura::Window* GetParentContainer(aura::Window* root, bool is_active) {
  return Shell::GetContainer(
      root, is_active ? kShellWindowId_CaptureModeSearchResultsPanel
                      : kShellWindowId_SystemModalContainer);
}

}  // namespace

// TODO: crbug.com/377764351 - Fix the textfield being too far to the left when
// the region is very narrow (height >> width).
// `SunfishSearchBoxView` contains an image thumbnail and a textfield.
class SunfishSearchBoxView : public views::View,
                             public views::TextfieldController {
  METADATA_HEADER(SunfishSearchBoxView, views::View)

 public:
  SunfishSearchBoxView() {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetCollapseMargins(true);
    // TODO(b/356878705): Replace with the captured region screenshot when the
    // backend is hooked up. Currently using the search icon as a placeholder.
    AddChildView(views::Builder<views::ImageView>()
                     .CopyAddressTo(&image_view_)
                     .SetImage(ui::ImageModel::FromVectorIcon(
                         vector_icons::kGoogleColorIcon))
                     .SetProperty(views::kMarginsKey, kSearchImageSpacing)
                     .Build());
    AddChildView(
        views::Builder<views::Textfield>()
            .CopyAddressTo(&textfield_)
            .SetController(this)
            .SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT)
            .SetPlaceholderText(kSearchBoxPlaceholderText)
            .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                TypographyToken::kCrosBody2))
            .SetProperty(views::kMarginsKey, kSearchTextfieldSpacing)
            .SetProperty(views::kFlexBehaviorKey,
                         views::FlexSpecification(
                             views::LayoutOrientation::kHorizontal,
                             views::MinimumFlexSizeRule::kPreferred,
                             views::MaximumFlexSizeRule::kUnbounded))
            .SetBackgroundEnabled(false)
            .SetBorder(nullptr)
            .Build());

    SetBackground(views::CreateRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase1, kSearchBoxRadius));

    SetPreferredSize(gfx::Size(capture_mode::kSearchResultsPanelWebViewWidth,
                               kSearchBoxHeight));

    image_view_->SetPaintToLayer();
    image_view_->layer()->SetFillsBoundsOpaquely(false);
    image_view_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(kSearchBoxImageRadius));
  }
  SunfishSearchBoxView(const SunfishSearchBoxView&) = delete;
  SunfishSearchBoxView& operator=(const SunfishSearchBoxView&) = delete;
  ~SunfishSearchBoxView() override = default;

  void SetImage(const gfx::ImageSkia& image) {
    if (image.isNull()) {
      return;
    }
    // Resize the image to fit in the searchbox, keeping the same aspect ratio.
    const int target_height = height();
    const int target_width = (image.width() * target_height) / image.height();
    image_view_->SetImage(ui::ImageModel::FromImageSkia(image));
    image_view_->SetImageSize(gfx::Size(target_width, target_height));
  }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& event) override {
    if (sender->GetText().empty()) {
      return false;
    }

    if (!textfield_->HasFocus()) {
      return false;
    }

    if (event.type() == ui::EventType::kKeyPressed &&
        event.key_code() == ui::VKEY_RETURN) {
      CaptureModeController::Get()->SendMultimodalSearch(
          image_view_->GetImage(), base::UTF16ToUTF8(sender->GetText()));
      return true;
    }

    return false;
  }

 private:
  friend class SearchResultsPanel;

  // Owned by the views hierarchy.
  raw_ptr<views::ImageView> image_view_;
  raw_ptr<views::Textfield> textfield_;
};

BEGIN_METADATA(SunfishSearchBoxView)
END_METADATA

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
              // Lens icon.
              views::Builder<views::ImageView>()
                  .SetImage(ui::ImageModel::FromVectorIcon(
                      kLensIcon, ui::kColorMenuIcon, kHeaderIconSize))
                  .SetProperty(views::kMarginsKey, kHeaderIconSpacing),
              // Title.
              views::Builder<views::Label>()
                  .SetText(u"Search with Lens")
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetFontList(
                      TypographyProvider::Get()->ResolveTypographyToken(
                          TypographyToken::kCrosTitle1))
                  .SetEnabledColor(cros_tokens::kCrosSysOnSurface),
              // Close Button, aligned to the right by setting a
              // `FlexSpecification` with unbounded maximum flex size and
              // `LayoutAlignment::kEnd`.
              views::Builder<views::Button>(
                  IconButton::Builder()
                      .SetType(IconButton::Type::kSmallFloating)
                      .SetVectorIcon(&kMediumOrLargeCloseButtonIcon)
                      .SetAccessibleName(u"Close Panel")
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

  // Lens Web API uses its own sticky search box, so there's no need to create a
  // native one.
  if (!features::IsSunfishLensWebEnabled()) {
    search_box_view_ = AddChildView(std::make_unique<SunfishSearchBoxView>());
    search_box_view_->SetProperty(views::kMarginsKey, kSearchBoxViewSpacing);
  }

  search_results_view_ =
      AddChildView(CaptureModeController::Get()->CreateSearchResultsView());
  search_results_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Without the native search box, there needs to be padding between the header
  // and the web view.
  if (features::IsSunfishLensWebEnabled()) {
    search_results_view_->SetProperty(views::kMarginsKey,
                                      kSearchResultsViewSpacing);
  }

  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kPanelCornerRadius));
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kPanelCornerRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  // Install highlightable views for when a Sunfish session is active and the
  // `CaptureModeSessionFocusCycler` is handling focus. Set up the focus
  // predicate for the focusable views now, so they will have the correct
  // behavior before `CaptureModeSessionFocusCycler::PseudoFocus()`
  // is called on them.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(close_button_);
  CaptureModeSessionFocusCycler::HighlightHelper::Get(close_button_)
      ->SetUpFocusPredicate();

  if (!features::IsSunfishLensWebEnabled()) {
    CaptureModeSessionFocusCycler::HighlightHelper::Install(
        search_box_view_->textfield_);
    CaptureModeSessionFocusCycler::HighlightHelper::Get(
        search_box_view_->textfield_)
        ->SetUpFocusPredicate();
  }
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
  params.name = "SearchResultsPanelWidget";
  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SearchResultsPanel>());
  return widget;
}

views::Textfield* SearchResultsPanel::GetSearchBoxTextfield() const {
  return search_box_view_ ? search_box_view_->textfield_ : nullptr;
}

std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
SearchResultsPanel::GetHighlightableItems() const {
  return {
      CaptureModeSessionFocusCycler::HighlightHelper::Get(close_button_.get()),
      CaptureModeSessionFocusCycler::HighlightHelper::Get(
          search_box_view_->textfield_.get())};
}

void SearchResultsPanel::Navigate(const GURL& url) {
  search_results_view_->Navigate(url);
}

void SearchResultsPanel::SetSearchBoxImage(const gfx::ImageSkia& image) {
  CHECK(search_box_view_);
  search_box_view_->SetImage(image);
}

void SearchResultsPanel::SetSearchBoxText(const std::u16string& text) {
  CHECK(search_box_view_);
  search_box_view_->textfield_->SetText(text);
}

void SearchResultsPanel::RefreshStackingOrder(aura::Window* new_root) {
  aura::Window* native_window = GetWidget()->GetNativeWindow();
  // While the capture mode session is active, we parent the panel to its own
  // container, else we parent it to the system modal container.
  aura::Window* new_parent = GetParentContainer(
      new_root ? new_root : native_window->GetRootWindow(), !!new_root);
  views::Widget::ReparentNativeView(native_window, new_parent);
}

bool SearchResultsPanel::IsTextfieldPseudoFocused() const {
  return CaptureModeSessionFocusCycler::HighlightHelper::Get(
             search_box_view_->textfield_)
      ->has_focus();
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

void SearchResultsPanel::OnWillChangeFocus(View* focused_before,
                                           View* focused_now) {}

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
                capture_mode::kSearchResultsPanelHeight));

  // Adjust the preferred size and bounds based on the current display.
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
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
