// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_annotation_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/ui/projector_color_button.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Margins between the title view and the edges around it (dp).
constexpr int kPaddingBetweenTitleAndSeparator = 3;
constexpr int kPaddingBetweenBottomAndLastTrayItem = 4;

// Width of the bubble itself (dp).
constexpr int kBubbleWidth = 372;

// Insets for the views (dp).
constexpr gfx::Insets kTitleViewPadding(4, 16, 0, 24);
constexpr gfx::Insets kPenViewPadding(4, 0, 0, 24);
constexpr gfx::Insets kLaserViewPadding(0, 0, 0, 24);

// Spacing between buttons in the title view (dp).
constexpr int kTitleViewChildSpacing = 20;

// Size of menu rows.
constexpr int kMenuRowHeight = 48;

// Color selection buttons.
constexpr int kColorButtonColorViewSize = 16;
constexpr int kColorButtonViewRadius = 28;

// Colors.
constexpr SkColor kRedPenColor = SkColorSetRGB(0xEA, 0x43, 0x35);
constexpr SkColor kYellowPenColor = SkColorSetRGB(0xFB, 0xBC, 0x04);

// TODO(b/201664243): Use AnnotatorToolType.
enum ProjectorTool { kToolNone, kToolLaser, kToolPen };

ProjectorTool GetCurrentTool() {
  auto* controller = Shell::Get()->projector_controller();
  // ProjctorController may not be available yet as the ProjectorAnnotationTray
  // is created before it.
  if (!controller)
    return kToolNone;

  if (controller->IsAnnotatorEnabled())
    return kToolPen;
  return controller->IsLaserPointerEnabled() ? kToolLaser : kToolNone;
}

const gfx::VectorIcon& GetIconForTool(ProjectorTool tool) {
  switch (tool) {
    case kToolNone:
      return kPaletteTrayIconProjectorIcon;
    case kToolLaser:
      return kPaletteModeLaserPointerIcon;
    case kToolPen:
      return kInkPenIcon;
  }

  NOTREACHED();
  return kPaletteTrayIconProjectorIcon;
}

}  // namespace

ProjectorAnnotationTray::ProjectorAnnotationTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      image_view_(
          tray_container()->AddChildView(std::make_unique<views::ImageView>())),
      laser_view_(nullptr),
      pen_view_(nullptr) {
  image_view_->SetTooltipText(GetAccessibleNameForTray());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
}

ProjectorAnnotationTray::~ProjectorAnnotationTray() = default;

bool ProjectorAnnotationTray::PerformAction(const ui::Event& event) {
  if (bubble_) {
    CloseBubble();
  } else {
    if (GetCurrentTool() == kToolNone) {
      ShowBubble();
    } else {
      DeactivateActiveTool();
    }
  }

  return true;
}

void ProjectorAnnotationTray::ClickedOutsideBubble() {
  CloseBubble();
}

std::u16string ProjectorAnnotationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_TITLE);
}

void ProjectorAnnotationTray::HandleLocaleChange() {}

void ProjectorAnnotationTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

void ProjectorAnnotationTray::CloseBubble() {
  laser_view_ = nullptr;
  pen_view_ = nullptr;
  bubble_.reset();

  shelf()->UpdateAutoHideState();
}

void ProjectorAnnotationTray::ShowBubble() {
  if (bubble_)
    return;

  DCHECK(tray_container());

  // There may still be an active tool if show bubble was called from an
  // accelerator.
  DeactivateActiveTool();

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = GetBubbleAnchor()->GetAnchorBoundsInScreen();
  init_params.anchor_rect.Inset(GetBubbleAnchorInsets());
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kBubbleWidth;
  init_params.close_on_deactivate = true;
  init_params.translucent = true;
  init_params.has_shadow = false;
  init_params.corner_radius = kTrayItemCornerRadius;
  init_params.reroute_event_handler = true;

  // Create and customize bubble view.
  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->set_margins(GetSecondaryBubbleInsets());
  bubble_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, 0, kPaddingBetweenBottomAndLastTrayItem, 0)));

  auto setup_layered_view = [](views::View* view) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
  };

  // Add title.
  auto* title_view = bubble_view->AddChildView(std::make_unique<views::View>());

  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kTitleViewPadding);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_layout->set_minimum_cross_axis_size(kMenuRowHeight);
  views::BoxLayout* layout_ptr =
      title_view->SetLayoutManager(std::move(box_layout));

  auto* title_label = title_view->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_TITLE)));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(title_label,
                                   TrayPopupUtils::FontStyle::kPodMenuHeader);
  layout_ptr->SetFlexForView(title_label, 1, true);

  setup_layered_view(title_view);

  // Add horizontal separator between the title and tools.
  auto* separator =
      bubble_view->AddChildView(std::make_unique<views::Separator>());
  setup_layered_view(separator);
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor));
  separator->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      kPaddingBetweenTitleAndSeparator, 0, kMenuSeparatorVerticalPadding, 0)));

  // Add drawing tools
  {
    auto* marker_view_container =
        bubble_view->AddChildView(std::make_unique<views::View>());

    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kPenViewPadding,
        kTitleViewChildSpacing);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    box_layout->set_minimum_cross_axis_size(kMenuRowHeight);
    views::BoxLayout* layout_ptr =
        marker_view_container->SetLayoutManager(std::move(box_layout));

    SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
    gfx::ImageSkia icon =
        CreateVectorIcon(kInkPenIcon, kMenuIconSize, icon_color);
    pen_view_ = marker_view_container->AddChildView(
        std::make_unique<HoverHighlightView>(this));
    pen_view_->AddIconAndLabel(icon, l10n_util::GetStringUTF16(IDS_PEN_BUTTON));
    layout_ptr->SetFlexForView(pen_view_, 1, true);

    // TODO(b/201664243): Only draw outer circle on hover or selection.
    marker_view_container->AddChildView(std::make_unique<ProjectorColorButton>(
        base::BindRepeating(&ProjectorAnnotationTray::OnPenColorPressed,
                            base::Unretained(this), kRedPenColor),
        kRedPenColor, kColorButtonColorViewSize, kColorButtonViewRadius,
        l10n_util::GetStringUTF16(IDS_RED_COLOR_BUTTON)));
    marker_view_container->AddChildView(std::make_unique<ProjectorColorButton>(
        base::BindRepeating(&ProjectorAnnotationTray::OnPenColorPressed,
                            base::Unretained(this), kYellowPenColor),
        kYellowPenColor, kColorButtonColorViewSize, kColorButtonViewRadius,
        l10n_util::GetStringUTF16(IDS_YELLOW_COLOR_BUTTON)));
    marker_view_container->AddChildView(std::make_unique<ProjectorColorButton>(
        base::BindRepeating(&ProjectorAnnotationTray::OnPenColorPressed,
                            base::Unretained(this), SK_ColorBLACK),
        SK_ColorBLACK, kColorButtonColorViewSize, kColorButtonViewRadius,
        l10n_util::GetStringUTF16(IDS_BLACK_COLOR_BUTTON)));
    marker_view_container->AddChildView(std::make_unique<ProjectorColorButton>(
        base::BindRepeating(&ProjectorAnnotationTray::OnPenColorPressed,
                            base::Unretained(this), SK_ColorWHITE),
        SK_ColorWHITE, kColorButtonColorViewSize, kColorButtonViewRadius,
        l10n_util::GetStringUTF16(IDS_WHITE_COLOR_BUTTON)));

    setup_layered_view(marker_view_container);
  }

  // Add Laser Pointer
  {
    auto* laser_view_container =
        bubble_view->AddChildView(std::make_unique<views::View>());

    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kLaserViewPadding,
        kTitleViewChildSpacing);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    box_layout->set_minimum_cross_axis_size(kMenuRowHeight);
    views::BoxLayout* layout_ptr =
        laser_view_container->SetLayoutManager(std::move(box_layout));

    SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
    gfx::ImageSkia icon = CreateVectorIcon(kPaletteModeLaserPointerIcon,
                                           kMenuIconSize, icon_color);
    laser_view_ = laser_view_container->AddChildView(
        std::make_unique<HoverHighlightView>(this));
    laser_view_->AddIconAndLabel(
        icon, l10n_util::GetStringUTF16(IDS_LASER_POINTER_BUTTON));

    layout_ptr->SetFlexForView(laser_view_, 1, true);
    setup_layered_view(laser_view_container);
  }

  // Show the bubble.
  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view);
  SetIsActive(true);
}

TrayBubbleView* ProjectorAnnotationTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* ProjectorAnnotationTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void ProjectorAnnotationTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateIcon();
}

void ProjectorAnnotationTray::OnViewClicked(views::View* sender) {
  auto* projector_controller = ProjectorControllerImpl::Get();
  DCHECK(projector_controller);

  if (sender == pen_view_) {
    projector_controller->OnMarkerPressed();
  } else if (sender == laser_view_) {
    projector_controller->OnLaserPointerPressed();
  }

  CloseBubble();
  UpdateIcon();
}

void ProjectorAnnotationTray::DeactivateActiveTool() {
  auto* controller = Shell::Get()->projector_controller();
  DCHECK(controller);
  controller->ResetTools();
  UpdateIcon();
}

void ProjectorAnnotationTray::UpdateIcon() {
  ProjectorTool tool = GetCurrentTool();
  image_view_->SetImage(gfx::CreateVectorIcon(
      GetIconForTool(tool),
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
}

void ProjectorAnnotationTray::OnPenColorPressed(SkColor color) {
  // TODO(b/201664243) Pass the color for the marker.
  auto* projector_controller = ProjectorControllerImpl::Get();
  DCHECK(projector_controller);
  projector_controller->OnMarkerPressed();
  CloseBubble();
}

}  // namespace ash
