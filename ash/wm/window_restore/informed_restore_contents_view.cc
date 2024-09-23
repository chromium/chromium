// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_contents_view.h"

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
#include "ash/wm/window_restore/informed_restore_constants.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_context_menu_model.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "ash/wm/window_restore/informed_restore_items_container_view.h"
#include "ash/wm/window_restore/informed_restore_screenshot_icon_row_view.h"
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
constexpr int kActionsContainerWidthSmallScreen = 200;
constexpr int kSmallScreenThreshold = 800;

// Height of the container that holds the items view.
constexpr int kItemsViewContainerHeight = 240;
// Minimum height of the container that holds the screenshot.
constexpr int kScreenshotContainerMinHeight = 214;
// Minimum height of the screenshot itself.
constexpr int kScreenshotMinHeight = 88;

}  // namespace

InformedRestoreContentsView::InformedRestoreContentsView() :
    creation_time_(base::TimeTicks::Now()) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kContentsRounding));
  SetBetweenChildSpacing(kContentsChildSpacing);
  SetInsideBorderInsets(kContentsInsets);

  auto* controller = Shell::Get()->informed_restore_controller();
  contents_data_updated_subscription_ =
      controller->RegisterContentsDataUpdateCallback(
          base::BindRepeating(&InformedRestoreContentsView::UpdateContents,
                              weak_ptr_factory_.GetWeakPtr()));

  // Update the value of `showing_list_view_` and record it.
  const InformedRestoreContentsData* contents_data =
      controller->contents_data();
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

InformedRestoreContentsView::~InformedRestoreContentsView() {
  if (!close_metric_recorded_) {
    RecordDialogClosing(showing_list_view_ ? CloseDialogType::kListviewOther
                                           : CloseDialogType::kScreenshotOther);
  }
}

// static
std::unique_ptr<views::Widget> InformedRestoreContentsView::Create(
    const gfx::Rect& grid_bounds_in_screen) {
  auto contents_view = std::make_unique<InformedRestoreContentsView>();
  gfx::Rect contents_bounds = grid_bounds_in_screen;
  contents_bounds.ClampToCenteredSize(contents_view->GetPreferredSize());

  aura::Window* root = Shell::GetRootWindowForDisplayId(
      display::Screen::GetScreen()->GetDisplayMatching(contents_bounds).id());

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.bounds = contents_bounds;
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.name = "InformedRestoreWidget";
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

void InformedRestoreContentsView::UpdateOrientation() {
  primary_container_view_ = nullptr;
  settings_button_ = nullptr;
  preview_container_view_ = nullptr;
  items_container_view_ = nullptr;
  image_view_ = nullptr;
  icon_row_container_ = nullptr;
  screenshot_icon_row_view_ = nullptr;
  RemoveAllChildViews();
  CreateChildViews();
}

void InformedRestoreContentsView::UpdateContents() {
  const auto& apps_infos =
      Shell::Get()->informed_restore_controller()->contents_data()->apps_infos;

  // Update the titles and favicons by recreating the items container or
  // screenshot icon row.
  if (showing_list_view_) {
    preview_container_view_->RemoveChildViewT(items_container_view_);
    items_container_view_ = preview_container_view_->AddChildViewAt(
        std::make_unique<InformedRestoreItemsContainerView>(apps_infos), 0);
  } else {
    icon_row_container_->RemoveChildViewT(screenshot_icon_row_view_);
    screenshot_icon_row_view_ = icon_row_container_->AddChildView(
        std::make_unique<InformedRestoreScreenshotIconRowView>(apps_infos));
    UpdateIconRowClipArea();
  }
}

void InformedRestoreContentsView::UpdatePrimaryContainerPreferredWidth(
    aura::Window* root_window,
    std::optional<bool> is_landscape) {
  const bool landscape_mode =
      is_landscape.value_or(display::Screen::GetScreen()
                                ->GetDisplayNearestWindow(root_window)
                                .is_landscape());
  const int preferred_width =
      landscape_mode && root_window->bounds().width() < kSmallScreenThreshold
          ? kActionsContainerWidthSmallScreen
          : kActionsContainerWidth;
  primary_container_view_->SetPreferredSize(gfx::Size(
      preferred_width, primary_container_view_->GetPreferredSize().height()));
}

void InformedRestoreContentsView::OnRestoreButtonPressed() {
  if (InformedRestoreContentsData* contents_data =
          Shell::Get()->informed_restore_controller()->contents_data()) {
    if (contents_data->restore_callback) {
      RecordTimeToAction(base::TimeTicks::Now() - creation_time_,
                         showing_list_view_);

      RecordDialogClosing(showing_list_view_
                              ? CloseDialogType::kListviewRestoreButton
                              : CloseDialogType::kScreenshotRestoreButton);
      close_metric_recorded_ = true;

      // Destroys `this`.
      std::move(contents_data->restore_callback).Run();
    }
  }
}

void InformedRestoreContentsView::OnCancelButtonPressed() {
  if (InformedRestoreContentsData* contents_data =
          Shell::Get()->informed_restore_controller()->contents_data()) {
    if (contents_data->cancel_callback) {
      RecordTimeToAction(base::TimeTicks::Now() - creation_time_,
                         showing_list_view_);
      RecordDialogClosing(showing_list_view_
                              ? CloseDialogType::kListviewCancelButton
                              : CloseDialogType::kScreenshotCancelButton);
      close_metric_recorded_ = true;

      // Destroys `this`.
      std::move(contents_data->cancel_callback).Run();
    }
  }
}

void InformedRestoreContentsView::OnSettingsButtonPressed() {
  context_menu_model_ = std::make_unique<InformedRestoreContextMenuModel>();
  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      context_menu_model_.get(),
      base::BindRepeating(&InformedRestoreContentsView::OnMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));

  std::unique_ptr<views::MenuItemView> root_menu_item =
      menu_model_adapter_->CreateMenu();
  const int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;

  // Add a custom view to the bottom of the menu to inform users that changes
  // will not take place until the next time they sign in.
  views::MenuItemView* container = root_menu_item->AppendMenuItem(
      InformedRestoreContextMenuModel::kDescriptionId);
  const std::u16string label = l10n_util::GetStringUTF16(
      IDS_ASH_INFORMED_RESTORE_DIALOG_CONTEXT_MENU_EXTRA_INFO);
  auto context_label = std::make_unique<views::Label>(label);
  context_label->SetMultiLine(true);
  context_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  context_label->SizeToFit(kContextMenuMaxWidth);
  context_label->SetBorder(views::CreateEmptyBorder(kContextMenuLabelInsets));
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                        *context_label);
  context_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  container->AddChildView(std::move(context_label));

  // Set the label container's a11y name to be the same as the label text so
  // that it can be read out by screen readers.
  container->SetAccessibleName(label);

  menu_runner_ =
      std::make_unique<views::MenuRunner>(std::move(root_menu_item), run_types);
  menu_runner_->RunMenuAt(
      settings_button_->GetWidget(), /*button_controller=*/nullptr,
      settings_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kBubbleRight, ui::MENU_SOURCE_NONE);
}

views::Builder<views::ImageButton>
InformedRestoreContentsView::CreateSettingsButtonBuilder() {
  return views::Builder<views::ImageButton>(
             views::CreateVectorImageButtonWithNativeTheme(
                 base::BindRepeating(
                     &InformedRestoreContentsView::OnSettingsButtonPressed,
                     weak_ptr_factory_.GetWeakPtr()),
                 kSettingsIcon, kSettingsIconSize))
      .CopyAddressTo(&settings_button_)
      .SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysSystemOnBase, kSettingsIconSize))
      .SetID(informed_restore::kSettingsButtonID)
      .SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SETTINGS));
}

views::Builder<views::BoxLayoutView>
InformedRestoreContentsView::CreateButtonContainerBuilder() {
  return views::Builder<views::BoxLayoutView>()
      .SetBetweenChildSpacing(kButtonContainerChildSpacing)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .AddChildren(
          views::Builder<PillButton>()
              .SetCallback(base::BindRepeating(
                  &InformedRestoreContentsView::OnCancelButtonPressed,
                  weak_ptr_factory_.GetWeakPtr()))
              .SetID(informed_restore::kCancelButtonID)
              .SetPillButtonType(PillButton::Type::kDefaultLargeWithoutIcon)
              .SetTextWithStringId(
                  IDS_ASH_INFORMED_RESTORE_DIALOG_NO_THANKS_BUTTON),
          views::Builder<PillButton>()
              .SetCallback(base::BindRepeating(
                  &InformedRestoreContentsView::OnRestoreButtonPressed,
                  weak_ptr_factory_.GetWeakPtr()))
              .SetID(informed_restore::kRestoreButtonID)
              .SetPillButtonType(PillButton::Type::kPrimaryLargeWithoutIcon)
              .SetTextWithStringId(
                  IDS_ASH_INFORMED_RESTORE_DIALOG_RESTORE_BUTTON));
}

void InformedRestoreContentsView::CreateChildViews() {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  const bool landscape_mode = display::Screen::GetScreen()
                                  ->GetDisplayNearestWindow(root)
                                  .is_landscape();

  SetOrientation(landscape_mode ? views::BoxLayout::Orientation::kHorizontal
                                : views::BoxLayout::Orientation::kVertical);

  const InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  CHECK(contents_data);
  const int title_message_id = IDS_ASH_INFORMED_RESTORE_DIALOG_TITLE;
  int description_message_id;
  switch (contents_data->dialog_type) {
    case InformedRestoreContentsData::DialogType::kNormal:
      description_message_id = IDS_ASH_INFORMED_RESTORE_DIALOG_DESCRIPTION;
      break;
    case InformedRestoreContentsData::DialogType::kCrash:
      description_message_id =
          IDS_ASH_INFORMED_RESTORE_DIALOG_CRASH_DESCRIPTION;
      break;
    case InformedRestoreContentsData::DialogType::kUpdate:
      description_message_id =
          IDS_ASH_INFORMED_RESTORE_DIALOG_UPDATE_DESCRIPTION;
      break;
  }

  primary_container_view_ = AddChildView(
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
                  .SetAccessibleRole(ax::mojom::Role::kHeading)
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
  preview_container_view_ =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetID(informed_restore::kPreviewContainerViewID)
                       .Build());
  if (showing_list_view_) {
    items_container_view_ = preview_container_view_->AddChildView(
        std::make_unique<InformedRestoreItemsContainerView>(
            contents_data->apps_infos));
    preview_container_view_->SetPreferredSize(gfx::Size(
        informed_restore::kPreviewContainerWidth, kItemsViewContainerHeight));
  } else {
    // TODO(http://b/338666906): Fix the screenshot view when in portrait mode,
    // and after transitioning to landscape mode.
    const gfx::ImageSkia& image = contents_data->image;
    screenshot_size = image.size();
    screenshot_size.set_height(
        std::max(kScreenshotMinHeight, screenshot_size.height()));

    views::View* icon_row_spacer;
    // This box layout is used to set the vertical space when the screenshot's
    // height is smaller than `kScreenshotContainerMinHeight`. Thus the
    // screenshot and the icon row can be centered inside the container.
    preview_container_view_->AddChildView(
        views::Builder<views::View>()
            .SetLayoutManager(std::make_unique<views::FillLayout>())
            .SetPreferredSize(screenshot_size)
            .AddChildren(
                views::Builder<views::BoxLayoutView>()
                    .CopyAddressTo(&icon_row_container_)
                    .SetPaintToLayer()
                    .SetOrientation(views::BoxLayout::Orientation::kVertical)
                    .AddChildren(views::Builder<views::View>().CopyAddressTo(
                        &icon_row_spacer)),
                views::Builder<views::ImageView>()
                    .CopyAddressTo(&image_view_)
                    .SetPaintToLayer()
                    .SetImage(image)
                    .SetImageSize(screenshot_size))
            .Build());

    icon_row_container_->layer()->SetFillsBoundsOpaquely(false);
    icon_row_container_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(informed_restore::kPreviewContainerRadius));
    screenshot_icon_row_view_ = icon_row_container_->AddChildView(
        std::make_unique<InformedRestoreScreenshotIconRowView>(
            contents_data->apps_infos));
    icon_row_container_->SetFlexForView(icon_row_spacer, 1);
  }

  // The display orientation determines where we place the settings,
  // "No thanks", and "Restore" buttons.
  views::View* spacer;
  if (landscape_mode) {
    // Add the buttons to the left hand side container view.
    primary_container_view_->AddChildView(
        CreateButtonContainerBuilder()
            .SetProperty(views::kMarginsKey, kButtonContainerChildMargins)
            .Build());
    spacer =
        primary_container_view_->AddChildView(std::make_unique<views::View>());
    primary_container_view_->AddChildView(
        CreateSettingsButtonBuilder().Build());
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

  // The height of the informed restore dialog is dynamic, depending on the
  // height of the screenshot. For the screenshot, its width is fixed as
  // `kPreviewContainerWidth` while its height is calculated based on the
  // display's aspect ratio.
  const int screenshot_height = screenshot_size.height();
  const int primary_container_height =
      showing_list_view_
          ? kItemsViewContainerHeight
          : std::max(kScreenshotContainerMinHeight, screenshot_height);

  primary_container_view_->SetPreferredSize(gfx::Size(
      kActionsContainerWidth,
      landscape_mode ? primary_container_height
                     : primary_container_view_->GetPreferredSize().height()));
  UpdatePrimaryContainerPreferredWidth(root, landscape_mode);

  // Set the screenshot preview container vertical margin based on the height of
  // the screenshot.
  if (!showing_list_view_ &&
      screenshot_height < kScreenshotContainerMinHeight) {
    const int vertical_gap = kScreenshotContainerMinHeight - screenshot_height;
    const int bottom_inset = vertical_gap / 2;
    const int top_inset =
        vertical_gap % 2 == 1 ? bottom_inset + 1 : bottom_inset;
    preview_container_view_->SetInsideBorderInsets(
        gfx::Insets::TLBR(top_inset, 0, bottom_inset, 0));
  }
}

void InformedRestoreContentsView::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_adapter_.reset();
  context_menu_model_.reset();
}

void InformedRestoreContentsView::UpdateIconRowClipArea() {
  if (showing_list_view_) {
    return;
  }

  const gfx::Size icon_row_size = screenshot_icon_row_view_->GetPreferredSize();
  auto builder =
      RoundedRectCutoutPathBuilder(gfx::SizeF(image_view_->GetPreferredSize()));
  builder.CornerRadius(informed_restore::kPreviewContainerRadius);
  builder.AddCutout(
      RoundedRectCutoutPathBuilder::Corner::kLowerLeft,
      gfx::SizeF(
          icon_row_size.width() - informed_restore::kPreviewContainerRadius,
          icon_row_size.height() - informed_restore::kPreviewContainerRadius));
  builder.CutoutOuterCornerRadius(informed_restore::kPreviewContainerRadius);
  builder.CutoutInnerCornerRadius(informed_restore::kPreviewContainerRadius);
  image_view_->SetClipPath(builder.Build());
}

void InformedRestoreContentsView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  UpdateIconRowClipArea();
}

BEGIN_METADATA(InformedRestoreContentsView)
END_METADATA

}  // namespace ash
