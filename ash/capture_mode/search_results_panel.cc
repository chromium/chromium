// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/search_results_panel.h"

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
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// TODO(sophiewen): Remove hardcoded values when we get UX specs.
inline constexpr int kPanelCornerRadius = 16;
const std::u16string kSearchBoxPlaceholderText = u"Add to your search";
inline constexpr int kPanelPaddingSize = 16;
inline constexpr gfx::Insets kPanelPadding = gfx::Insets(kPanelPaddingSize);
inline constexpr int kHeaderIconSize = 20;
inline constexpr int kSearchBoxHeight = 48;
inline constexpr int kSearchBoxRadius = 24;
inline constexpr int kSearchBoxImageRadius = 8;
inline constexpr gfx::Insets kSearchBoxViewSpacing = gfx::Insets::VH(12, 0);
inline constexpr gfx::Insets kSearchImageSpacing =
    gfx::Insets::TLBR(8, 16, 8, 12);
inline constexpr gfx::Insets kSearchTextfieldSpacing =
    gfx::Insets::TLBR(14, 0, 14, 16);
inline constexpr gfx::Insets kHeaderIconSpacing = gfx::Insets::TLBR(0, 2, 0, 8);

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

    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase1, kSearchBoxRadius));

    SetPreferredSize(gfx::Size(
        capture_mode::kSearchResultsPanelWidth - 2 * kPanelPaddingSize,
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
    image_view_->SetImage(image);
    image_view_->SetImageSize(gfx::Size(target_width, target_height));
  }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& event) override {
    if (sender->GetText().empty()) {
      return false;
    }

    if (event.type() == ui::EventType::kKeyPressed &&
        event.key_code() == ui::VKEY_RETURN) {
      const std::u16string& text = sender->GetText();
      CaptureModeController::Get()->SendMultimodalSearch(
          image_view_->GetImage(), base::UTF16ToUTF8(text));
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
  DCHECK(IsSunfishFeatureEnabledWithFeatureKey());
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
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurface),
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

  search_box_view_ = AddChildView(std::make_unique<SunfishSearchBoxView>());
  search_box_view_->SetProperty(views::kMarginsKey, kSearchBoxViewSpacing);
  search_results_view_ =
      AddChildView(CaptureModeController::Get()->CreateSearchResultsView());
  search_results_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kPanelCornerRadius));
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kPanelCornerRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
}

SearchResultsPanel::~SearchResultsPanel() = default;

// static
views::UniqueWidgetPtr SearchResultsPanel::CreateWidget(
    aura::Window* root,
    const gfx::Rect& bounds) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // TODO(b/362284723): Ensure tooltips are visible over overlay container.
  params.parent = Shell::GetContainer(root, kShellWindowId_OverlayContainer);
  params.bounds = bounds;
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = wm::kShadowElevationInactiveWindow;
  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SearchResultsPanel>());
  return widget;
}

views::Textfield* SearchResultsPanel::GetSearchBoxTextfield() const {
  return search_box_view_->textfield_;
}

void SearchResultsPanel::Navigate(const GURL& url) {
  search_results_view_->Navigate(url);
}

void SearchResultsPanel::SetSearchBoxImage(const gfx::ImageSkia& image) {
  search_box_view_->SetImage(image);
}

void SearchResultsPanel::SetSearchBoxText(const std::u16string& text) {
  search_box_view_->textfield_->SetText(text);
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

void SearchResultsPanel::OnCloseButtonPressed() {
  CHECK(GetWidget());
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

BEGIN_METADATA(SearchResultsPanel)
END_METADATA

}  // namespace ash
