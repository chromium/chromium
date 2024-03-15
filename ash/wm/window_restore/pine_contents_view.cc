// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "ash/wm/window_restore/pine_items_container_view.h"
#include "ash/wm/window_restore/pine_screenshot_icon_row_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// TODO(http://b/322359738): Localize all these strings.
// TODO(http://b/322360273): Match specs.
// TODO(http://b/328459389): Update `SetFontList()` to use
// `ash::TypographyProvider`.

constexpr gfx::Size kItemsContainerPreferredSize(
    320,
    pine::kItemsContainerInsets.height() +
        pine::kItemIconBackgroundPreferredSize.height() * pine::kMaxItems +
        pine::kItemsContainerChildSpacing * (pine::kMaxItems - 1));

constexpr int kButtonContainerChildSpacing = 10;
constexpr int kContentsChildSpacing = 20;
constexpr gfx::Insets kContentsInsets = gfx::Insets::VH(15, 15);
constexpr int kContentsRounding = 20;
constexpr int kContentsTitleFontSize = 22;
constexpr int kContentsDescriptionFontSize = 14;
constexpr int kLeftContentsChildSpacing = 20;
constexpr int kSettingsIconSize = 24;
constexpr int kContextMenuMaxWidth = 285;
constexpr gfx::Insets kContextMenuLabelInsets = gfx::Insets::VH(0, 16);

}  // namespace

PineContentsView::PineContentsView() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kContentsRounding));
  SetBetweenChildSpacing(kContentsChildSpacing);
  SetInsideBorderInsets(kContentsInsets);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  views::View* spacer;
  AddChildView(
      // This box layout view is the container for the left hand side (in LTR)
      // of the contents view. It contains the title, buttons container and
      // settings button.
      views::Builder<views::BoxLayoutView>()
          .SetBetweenChildSpacing(kLeftContentsChildSpacing)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetPreferredSize(kItemsContainerPreferredSize)
          .AddChildren(
              // Title.
              views::Builder<views::Label>()
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                  .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                             kContentsTitleFontSize,
                                             gfx::Font::Weight::BOLD))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetText(
                      l10n_util::GetStringUTF16(IDS_ASH_PINE_DIALOG_TITLE)),
              // Description.
              views::Builder<views::Label>()
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                  .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                             kContentsDescriptionFontSize,
                                             gfx::Font::Weight::NORMAL))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetMultiLine(true)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_ASH_PINE_DIALOG_DESCRIPTION)),
              // This box layout view is the container for the "No thanks" and
              // "Restore" pill buttons.
              views::Builder<views::BoxLayoutView>()
                  .SetBetweenChildSpacing(kButtonContainerChildSpacing)
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                  .AddChildren(
                      views::Builder<PillButton>()
                          .CopyAddressTo(&cancel_button_for_testing_)
                          .SetCallback(base::BindRepeating(
                              &PineContentsView::OnCancelButtonPressed,
                              weak_ptr_factory_.GetWeakPtr()))
                          .SetPillButtonType(
                              PillButton::Type::kDefaultLargeWithoutIcon)
                          .SetTextWithStringId(
                              IDS_ASH_PINE_DIALOG_NO_THANKS_BUTTON),
                      views::Builder<PillButton>()
                          .CopyAddressTo(&restore_button_for_testing_)
                          .SetCallback(base::BindRepeating(
                              &PineContentsView::OnRestoreButtonPressed,
                              weak_ptr_factory_.GetWeakPtr()))
                          .SetPillButtonType(
                              PillButton::Type::kPrimaryLargeWithoutIcon)
                          .SetTextWithStringId(
                              IDS_ASH_PINE_DIALOG_RESTORE_BUTTON)),
              views::Builder<views::View>().CopyAddressTo(&spacer),
              views::Builder<views::ImageButton>(
                  views::CreateVectorImageButtonWithNativeTheme(
                      base::BindRepeating(
                          &PineContentsView::OnSettingsButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
                      kSettingsIcon, kSettingsIconSize))
                  .CopyAddressTo(&settings_button_)
                  .SetBackground(views::CreateThemedRoundedRectBackground(
                      cros_tokens::kCrosSysSystemOnBase, kSettingsIconSize))
                  .SetTooltipText(
                      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SETTINGS)))
          .Build());

  views::AsViewClass<views::BoxLayoutView>(spacer->parent())
      ->SetFlexForView(spacer, 1);

  const PineContentsData* pine_contents_data =
      Shell::Get()->pine_controller()->pine_contents_data();
  CHECK(pine_contents_data);
  if (pine_contents_data->image.isNull()) {
    items_container_view_ =
        AddChildView(std::make_unique<PineItemsContainerView>(
            pine_contents_data->apps_infos));
    items_container_view_->SetPreferredSize(kItemsContainerPreferredSize);
  } else {
    const gfx::ImageSkia& pine_image = pine_contents_data->image;
    const gfx::Size preview_size = pine_image.size();

    views::View* icon_row_spacer;
    AddChildView(
        views::Builder<views::View>()
            .SetLayoutManager(std::make_unique<views::FillLayout>())
            .SetPreferredSize(preview_size)
            .AddChildren(
                views::Builder<views::ImageView>()
                    .SetImage(pine_image)
                    .SetImageSize(preview_size),
                views::Builder<views::BoxLayoutView>()
                    .SetOrientation(views::BoxLayout::Orientation::kVertical)
                    .AddChildren(views::Builder<views::View>().CopyAddressTo(
                        &icon_row_spacer)))
            .Build());

    auto* icon_row_container =
        views::AsViewClass<views::BoxLayoutView>(icon_row_spacer->parent());
    screenshot_icon_row_view_ = icon_row_container->AddChildView(
        std::make_unique<PineScreenshotIconRowView>(
            pine_contents_data->apps_infos));
    icon_row_container->SetFlexForView(icon_row_spacer, 1);
  }

  // Add a highlight border to match the Quick Settings menu, i.e.,
  // `TrayBubbleView`.
  SetBorder(std::make_unique<views::HighlightBorder>(
      kContentsRounding,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
}

PineContentsView::~PineContentsView() = default;

// static
std::unique_ptr<views::Widget> PineContentsView::Create(
    const gfx::Rect& grid_bounds_in_screen) {
  auto contents_view = std::make_unique<PineContentsView>();
  gfx::Rect contents_bounds = grid_bounds_in_screen;
  contents_bounds.ClampToCenteredSize(contents_view->GetPreferredSize());

  aura::Window* root = Shell::GetRootWindowForDisplayId(
      display::Screen::GetScreen()->GetDisplayMatching(contents_bounds).id());

  views::Widget::InitParams params;
  params.bounds = contents_bounds;
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.name = "PineWidget";
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = desks_util::GetActiveDeskContainerForRoot(root);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::move(contents_view));
  // Overview uses custom animations so remove the default ones.
  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeWindow(),
                                             wm::ANIMATE_NONE);
  auto* layer = widget->GetLayer();
  layer->SetFillsBoundsOpaquely(false);

  // Add blur to help with contrast between the background and the text. Uses
  // the same settings as the Quick Settings menu, i.e., `TrayBubbleView`.
  if (features::IsBackgroundBlurEnabled()) {
    layer->SetRoundedCornerRadius(gfx::RoundedCornersF(kContentsRounding));
    layer->SetIsFastRoundedCorner(true);
    layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }

  return widget;
}

void PineContentsView::OnRestoreButtonPressed() {
  if (PineContentsData* pine_contents_data =
          Shell::Get()->pine_controller()->pine_contents_data()) {
    if (pine_contents_data->restore_callback) {
      // Destroys `this`.
      std::move(pine_contents_data->restore_callback).Run();
    }
  }
}

void PineContentsView::OnCancelButtonPressed() {
  if (PineContentsData* pine_contents_data =
          Shell::Get()->pine_controller()->pine_contents_data()) {
    if (pine_contents_data->cancel_callback) {
      // Destroys `this`.
      std::move(pine_contents_data->cancel_callback).Run();
    }
  }
}

void PineContentsView::OnSettingsButtonPressed() {
  context_menu_model_ = std::make_unique<PineContextMenuModel>();
  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      context_menu_model_.get(),
      base::BindRepeating(&PineContentsView::OnMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));

  std::unique_ptr<views::MenuItemView> root_menu_item =
      menu_model_adapter_->CreateMenu();
  const int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;

  // Add a custom view to the bottom of the menu to inform users that changes
  // will not take place until the next time they sign in.
  views::MenuItemView* container =
      root_menu_item->AppendMenuItem(PineContextMenuModel::kDescriptionId);
  auto context_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_PINE_DIALOG_CONTEXT_MENU_EXTRA_INFO));
  context_label->SetMultiLine(true);
  context_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  context_label->SizeToFit(kContextMenuMaxWidth);
  context_label->SetBorder(views::CreateEmptyBorder(kContextMenuLabelInsets));
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                        *context_label);
  context_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  container->AddChildView(std::move(context_label));

  menu_runner_ =
      std::make_unique<views::MenuRunner>(std::move(root_menu_item), run_types);
  menu_runner_->RunMenuAt(
      settings_button_->GetWidget(), /*button_controller=*/nullptr,
      settings_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kBubbleRight, ui::MENU_SOURCE_NONE);
}

void PineContentsView::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_adapter_.reset();
  context_menu_model_.reset();
}

BEGIN_METADATA(PineContentsView)
END_METADATA

}  // namespace ash
