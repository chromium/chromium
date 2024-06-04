// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_rect_cutout_path_builder.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "ash/wm/window_restore/pine_items_container_view.h"
#include "ash/wm/window_restore/pine_screenshot_icon_row_view.h"
#include "ash/wm/window_restore/window_restore_metrics.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// TODO(http://b/322359738): Localize all these strings.

constexpr int kButtonContainerChildSpacing = 10;
// The margins for the container view which houses the cancel and restore
// buttons. The distance between this container and its siblings will be the
// margin plus `kLeftContentsChildSpacing`.
constexpr gfx::Insets kButtonContainerChildMargins = gfx::Insets::VH(14, 0);
constexpr int kContentsChildSpacing = 16;
constexpr gfx::Insets kContentsInsets(20);
constexpr int kContentsRounding = 20;
constexpr int kLeftContentsChildSpacing = 6;
constexpr int kSettingsIconSize = 24;

constexpr int kContextMenuMaxWidth = 285;
constexpr gfx::Insets kContextMenuLabelInsets = gfx::Insets::VH(0, 16);

// Width of the actions container, which includes multiple buttons that users
// can take actions to change their settings.
constexpr int kActionsContainerWidth = 300;
// Height of the container that holds the items view.
constexpr int kItemsViewContainerHeight = 240;
// Minimum height of the container that holds the screenshot.
constexpr int kScreenshotContainerMinHeight = 214;
// Minimum height of the screenshot itself.
constexpr int kScreenshotMinHeight = 88;

}  // namespace

PineContentsView::PineContentsView() : creation_time_(base::TimeTicks::Now()) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kContentsRounding));
  SetBetweenChildSpacing(kContentsChildSpacing);
  SetInsideBorderInsets(kContentsInsets);

  // Update the value of `showing_list_view_` and record it.
  const InformedRestoreContentsData* contents_data =
      Shell::Get()->pine_controller()->contents_data();
  CHECK(contents_data);
  showing_list_view_ = contents_data->image.isNull();
  RecordDialogScreenshotVisibility(!showing_list_view_);

  CreateChildViews();
  views::InstallCircleHighlightPathGenerator(settings_button_);

  // Add a highlight border to match the Quick Settings menu, i.e.,
  // `TrayBubbleView`.
  SetBorder(std::make_unique<views::HighlightBorder>(
      kContentsRounding,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
}

PineContentsView::~PineContentsView() {
  if (!close_metric_recorded_) {
    RecordPineDialogClosing(showing_list_view_
                                ? ClosePineDialogType::kListviewOther
                                : ClosePineDialogType::kScreenshotOther);
  }
}

// static
std::unique_ptr<views::Widget> PineContentsView::Create(
    const gfx::Rect& grid_bounds_in_screen) {
  auto contents_view = std::make_unique<PineContentsView>();
  gfx::Rect contents_bounds = grid_bounds_in_screen;
  contents_bounds.ClampToCenteredSize(contents_view->GetPreferredSize());

  aura::Window* root = Shell::GetRootWindowForDisplayId(
      display::Screen::GetScreen()->GetDisplayMatching(contents_bounds).id());

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = features::IsOverviewNewFocusEnabled()
                           ? views::Widget::InitParams::Activatable::kYes
                           : views::Widget::InitParams::Activatable::kNo;
  params.bounds = contents_bounds;
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.name = "PineWidget";
  params.parent = desks_util::GetActiveDeskContainerForRoot(root);

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

void PineContentsView::UpdateOrientation() {
  settings_button_ = nullptr;
  RemoveAllChildViews();
  CreateChildViews();
}

void PineContentsView::OnRestoreButtonPressed() {
  if (InformedRestoreContentsData* contents_data =
          Shell::Get()->pine_controller()->contents_data()) {
    if (contents_data->restore_callback) {
      RecordTimeToAction(base::TimeTicks::Now() - creation_time_,
                         showing_list_view_);

      RecordPineDialogClosing(
          showing_list_view_ ? ClosePineDialogType::kListviewRestoreButton
                             : ClosePineDialogType::kScreenshotRestoreButton);
      close_metric_recorded_ = true;

      // Destroys `this`.
      std::move(contents_data->restore_callback).Run();
    }
  }
}

void PineContentsView::OnCancelButtonPressed() {
  if (InformedRestoreContentsData* contents_data =
          Shell::Get()->pine_controller()->contents_data()) {
    if (contents_data->cancel_callback) {
      RecordTimeToAction(base::TimeTicks::Now() - creation_time_,
                         showing_list_view_);
      RecordPineDialogClosing(
          showing_list_view_ ? ClosePineDialogType::kListviewCancelButton
                             : ClosePineDialogType::kScreenshotCancelButton);
      close_metric_recorded_ = true;

      // Destroys `this`.
      std::move(contents_data->cancel_callback).Run();
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

views::Builder<views::ImageButton>
PineContentsView::CreateSettingsButtonBuilder() {
  return views::Builder<views::ImageButton>(
             views::CreateVectorImageButtonWithNativeTheme(
                 base::BindRepeating(&PineContentsView::OnSettingsButtonPressed,
                                     weak_ptr_factory_.GetWeakPtr()),
                 kSettingsIcon, kSettingsIconSize))
      .CopyAddressTo(&settings_button_)
      .SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysSystemOnBase, kSettingsIconSize))
      .SetID(pine::kSettingsButtonID)
      .SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SETTINGS));
}

views::Builder<views::BoxLayoutView>
PineContentsView::CreateButtonContainerBuilder() {
  return views::Builder<views::BoxLayoutView>()
      .SetBetweenChildSpacing(kButtonContainerChildSpacing)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .AddChildren(
          views::Builder<PillButton>()
              .SetCallback(
                  base::BindRepeating(&PineContentsView::OnCancelButtonPressed,
                                      weak_ptr_factory_.GetWeakPtr()))
              .SetID(pine::kCancelButtonID)
              .SetPillButtonType(PillButton::Type::kDefaultLargeWithoutIcon)
              .SetTextWithStringId(IDS_ASH_PINE_DIALOG_NO_THANKS_BUTTON),
          views::Builder<PillButton>()
              .SetCallback(
                  base::BindRepeating(&PineContentsView::OnRestoreButtonPressed,
                                      weak_ptr_factory_.GetWeakPtr()))
              .SetID(pine::kRestoreButtonID)
              .SetPillButtonType(PillButton::Type::kPrimaryLargeWithoutIcon)
              .SetTextWithStringId(IDS_ASH_PINE_DIALOG_RESTORE_BUTTON));
}

void PineContentsView::CreateChildViews() {
  const bool landscape_mode =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow())
          .is_landscape();

  SetOrientation(landscape_mode ? views::BoxLayout::Orientation::kHorizontal
                                : views::BoxLayout::Orientation::kVertical);

  const InformedRestoreContentsData* contents_data =
      Shell::Get()->pine_controller()->contents_data();
  CHECK(contents_data);
  const int title_message_id = contents_data->last_session_crashed
                                   ? IDS_ASH_PINE_DIALOG_CRASH_TITLE
                                   : IDS_ASH_PINE_DIALOG_TITLE;
  const int description_message_id = contents_data->last_session_crashed
                                         ? IDS_ASH_PINE_DIALOG_CRASH_DESCRIPTION
                                         : IDS_ASH_PINE_DIALOG_DESCRIPTION;

  auto* primary_container_view = AddChildView(
      // In landscape mode, this box layout view is the container for the left
      // hand side (in LTR) of the contents view. It contains the title,
      // description, buttons container, and settings button. In portrait mode,
      // this box layout view is the container for the header of the contents
      // view. It contains just the title and description.
      views::Builder<views::BoxLayoutView>()
          .SetBetweenChildSpacing(kLeftContentsChildSpacing)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .AddChildren(
              // Title.
              views::Builder<views::Label>()
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetMultiLine(true)
                  .SetText(l10n_util::GetStringUTF16(title_message_id))
                  .CustomConfigure(base::BindOnce([](views::Label* label) {
                    TypographyProvider::Get()->StyleLabel(
                        TypographyToken::kCrosDisplay7, *label);
                  })),
              // Description.
              views::Builder<views::Label>()
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetMultiLine(true)
                  .SetText(l10n_util::GetStringUTF16(description_message_id))
                  .CustomConfigure(base::BindOnce([](views::Label* label) {
                    TypographyProvider::Get()->StyleLabel(
                        TypographyToken::kCrosBody1, *label);
                  })))
          .Build());

  gfx::Size screenshot_size;
  views::BoxLayoutView* preview_container_view;
  if (showing_list_view_) {
    preview_container_view = AddChildView(
        std::make_unique<PineItemsContainerView>(contents_data->apps_infos));
    preview_container_view->SetID(pine::kPreviewContainerViewID);
    preview_container_view->SetPreferredSize(
        gfx::Size(pine::kPreviewContainerWidth, kItemsViewContainerHeight));
  } else {
    // TODO(http://b/338666906): Fix the screenshot view when in portrait mode,
    // and after transitioning to landscape mode.

    const gfx::ImageSkia& image = contents_data->image;
    screenshot_size = image.size();
    screenshot_size.set_height(
        std::max(kScreenshotMinHeight, screenshot_size.height()));

    views::BoxLayoutView* icon_row_container;
    views::View* icon_row_spacer;
    // This box layout is used to set the vertical space when the screenshot's
    // height is smaller than `kScreenshotContainerMinHeight`. Thus the
    // screenshot and the icon row can be centered inside the container.
    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .CopyAddressTo(&preview_container_view)
            .SetID(pine::kPreviewContainerViewID)
            .AddChildren(
                views::Builder<views::View>()
                    .SetLayoutManager(std::make_unique<views::FillLayout>())
                    .SetPreferredSize(screenshot_size)
                    .AddChildren(
                        views::Builder<views::BoxLayoutView>()
                            .CopyAddressTo(&icon_row_container)
                            .SetPaintToLayer()
                            .SetOrientation(
                                views::BoxLayout::Orientation::kVertical)
                            .AddChildren(views::Builder<views::View>()
                                             .CopyAddressTo(&icon_row_spacer)),
                        views::Builder<views::ImageView>()
                            .CopyAddressTo(&image_view_)
                            .SetPaintToLayer()
                            .SetImage(image)
                            .SetImageSize(screenshot_size)))
            .Build());

    icon_row_container->layer()->SetFillsBoundsOpaquely(false);
    icon_row_container->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(pine::kPreviewContainerRadius));
    screenshot_icon_row_view_ = icon_row_container->AddChildView(
        std::make_unique<PineScreenshotIconRowView>(contents_data->apps_infos));
    icon_row_container->SetFlexForView(icon_row_spacer, 1);
  }

  // The display orientation determines where we place the settings,
  // "No thanks", and "Restore" buttons.
  views::View* spacer;
  if (landscape_mode) {
    // Add the buttons to the left hand side container view.
    primary_container_view->AddChildView(
        CreateButtonContainerBuilder()
            .SetProperty(views::kMarginsKey, kButtonContainerChildMargins)
            .Build());
    spacer =
        primary_container_view->AddChildView(std::make_unique<views::View>());
    primary_container_view->AddChildView(CreateSettingsButtonBuilder().Build());
  } else {
    // Add a footer view that contains the buttons.
    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetCrossAxisAlignment(
                views::BoxLayout::CrossAxisAlignment::kCenter)
            .AddChildren(CreateSettingsButtonBuilder(),
                         views::Builder<views::View>().CopyAddressTo(&spacer),
                         CreateButtonContainerBuilder())
            .Build());
  }

  views::AsViewClass<views::BoxLayoutView>(spacer->parent())
      ->SetFlexForView(spacer, 1);

  // The height of the pine dialog is dynamic, depending on the height of the
  // screenshot. For the screenshot, its width is fixed as
  // `kPreviewContainerWidth` while its height is calculated based on the
  // display's aspect ratio.
  const int screenshot_height = screenshot_size.height();
  const int primary_container_height =
      showing_list_view_
          ? kItemsViewContainerHeight
          : std::max(kScreenshotContainerMinHeight, screenshot_height);

  primary_container_view->SetPreferredSize(gfx::Size(
      kActionsContainerWidth,
      landscape_mode ? primary_container_height
                     : primary_container_view->GetPreferredSize().height()));

  // Set the screenshot preview container vertical margin based on the height of
  // the screenshot.
  if (!showing_list_view_ &&
      screenshot_height < kScreenshotContainerMinHeight) {
    const int vertical_gap = kScreenshotContainerMinHeight - screenshot_height;
    const int bottom_inset = vertical_gap / 2;
    const int top_inset =
        vertical_gap % 2 == 1 ? bottom_inset + 1 : bottom_inset;
    preview_container_view->SetInsideBorderInsets(
        gfx::Insets::TLBR(top_inset, 0, bottom_inset, 0));
  }
}

void PineContentsView::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_adapter_.reset();
  context_menu_model_.reset();
}

void PineContentsView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (showing_list_view_) {
    return;
  }

  const gfx::Size icon_row_size = screenshot_icon_row_view_->GetPreferredSize();
  auto builder =
      RoundedRectCutoutPathBuilder(gfx::SizeF(image_view_->GetPreferredSize()));
  builder.CornerRadius(pine::kPreviewContainerRadius);
  builder.AddCutout(
      RoundedRectCutoutPathBuilder::Corner::kLowerLeft,
      gfx::SizeF(icon_row_size.width() - pine::kPreviewContainerRadius,
                 icon_row_size.height() - pine::kPreviewContainerRadius));
  builder.CutoutOuterCornerRadius(pine::kPreviewContainerRadius);
  builder.CutoutInnerCornerRadius(pine::kPreviewContainerRadius);
  image_view_->SetClipPath(builder.Build());
}

BEGIN_METADATA(PineContentsView)
END_METADATA

}  // namespace ash
