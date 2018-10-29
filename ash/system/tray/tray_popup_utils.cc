// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_popup_utils.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/size_range_layout.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"

namespace ash {

namespace {

// Creates a layout manager that positions Views vertically. The Views will be
// stretched horizontally and centered vertically.
std::unique_ptr<views::LayoutManager> CreateDefaultCenterLayoutManager() {
  // TODO(bruthig): Use constants instead of magic numbers.
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(8, kTrayPopupLabelHorizontalPadding));
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  return std::move(box_layout);
}

// Creates a layout manager that positions Views horizontally. The Views will be
// centered along the horizontal and vertical axis.
std::unique_ptr<views::LayoutManager> CreateDefaultEndsLayoutManager() {
  auto box_layout =
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  return std::move(box_layout);
}

std::unique_ptr<views::LayoutManager> CreateDefaultLayoutManager(
    TriView::Container container) {
  switch (container) {
    case TriView::Container::START:
    case TriView::Container::END:
      return CreateDefaultEndsLayoutManager();
    case TriView::Container::CENTER:
      return CreateDefaultCenterLayoutManager();
  }
  // Required by some compilers.
  NOTREACHED();
  return nullptr;
}

// Configures the default size and flex value for the specified |container|
// of the given |tri_view|. Used by CreateDefaultRowView().
void ConfigureDefaultSizeAndFlex(TriView* tri_view,
                                 TriView::Container container) {
  int min_width = 0;
  switch (container) {
    case TriView::Container::START:
      min_width = kTrayPopupItemMinStartWidth;
      break;
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);
      break;
    case TriView::Container::END:
      min_width = kTrayPopupItemMinEndWidth;
      break;
  }

  tri_view->SetMinSize(container,
                       gfx::Size(min_width, kTrayPopupItemMinHeight));
  constexpr int kTrayPopupItemMaxHeight = 144;
  tri_view->SetMaxSize(
      container,
      gfx::Size(SizeRangeLayout::kAbsoluteMaxSize, kTrayPopupItemMaxHeight));
}

}  // namespace

TriView* TrayPopupUtils::CreateDefaultRowView() {
  TriView* tri_view = CreateMultiTargetRowView();

  tri_view->SetContainerLayout(
      TriView::Container::START,
      CreateDefaultLayoutManager(TriView::Container::START));
  tri_view->SetContainerLayout(
      TriView::Container::CENTER,
      CreateDefaultLayoutManager(TriView::Container::CENTER));
  tri_view->SetContainerLayout(
      TriView::Container::END,
      CreateDefaultLayoutManager(TriView::Container::END));

  return tri_view;
}

TriView* TrayPopupUtils::CreateSubHeaderRowView(bool start_visible) {
  TriView* tri_view = CreateDefaultRowView();
  if (!start_visible) {
    tri_view->SetInsets(gfx::Insets(
        0, kTrayPopupPaddingHorizontal - kTrayPopupLabelHorizontalPadding, 0,
        0));
    tri_view->SetContainerVisible(TriView::Container::START, false);
  }
  return tri_view;
}

TriView* TrayPopupUtils::CreateMultiTargetRowView() {
  TriView* tri_view = new TriView(0 /* padding_between_items */);

  tri_view->SetInsets(gfx::Insets(0, kMenuExtraMarginFromLeftEdge, 0, 0));

  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::START);
  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::CENTER);
  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::END);

  tri_view->SetContainerLayout(TriView::Container::START,
                               std::make_unique<views::FillLayout>());
  tri_view->SetContainerLayout(TriView::Container::CENTER,
                               std::make_unique<views::FillLayout>());
  tri_view->SetContainerLayout(TriView::Container::END,
                               std::make_unique<views::FillLayout>());

  return tri_view;
}

views::Label* TrayPopupUtils::CreateDefaultLabel() {
  views::Label* label = new views::Label();
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetSubpixelRenderingEnabled(false);
  return label;
}

views::ImageView* TrayPopupUtils::CreateMainImageView() {
  auto* image = new views::ImageView;
  image->SetPreferredSize(
      gfx::Size(kTrayPopupItemMinStartWidth, kTrayPopupItemMinHeight));
  return image;
}

views::ImageView* TrayPopupUtils::CreateMoreImageView() {
  auto* image = new views::ImageView;
  image->SetPreferredSize(gfx::Size(gfx::Size(kMenuIconSize, kMenuIconSize)));
  image->EnableCanvasFlippingForRTLUI(true);
  image->SetImage(
      gfx::CreateVectorIcon(kSystemMenuArrowRightIcon, kMenuIconColor));
  return image;
}

views::Slider* TrayPopupUtils::CreateSlider(views::SliderListener* listener) {
  views::Slider* slider = new views::Slider(listener);
  slider->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kTrayPopupSliderHorizontalPadding)));
  return slider;
}

views::ToggleButton* TrayPopupUtils::CreateToggleButton(
    views::ButtonListener* listener,
    int accessible_name_id) {
  views::ToggleButton* toggle = new views::ToggleButton(listener);
  const gfx::Size toggle_size(toggle->GetPreferredSize());
  const int vertical_padding = (kMenuButtonSize - toggle_size.height()) / 2;
  const int horizontal_padding =
      (kTrayToggleButtonWidth - toggle_size.width()) / 2;
  toggle->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  toggle->SetAccessibleName(l10n_util::GetStringUTF16(accessible_name_id));
  return toggle;
}

std::unique_ptr<views::Painter> TrayPopupUtils::CreateFocusPainter() {
  return views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, kFocusBorderThickness, gfx::InsetsF());
}

void TrayPopupUtils::ConfigureTrayPopupButton(views::Button* button) {
  button->SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  button->SetFocusForPlatform();

  button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  button->set_has_ink_drop_action_on_click(true);
  button->set_ink_drop_base_color(kTrayPopupInkDropBaseColor);
  button->set_ink_drop_visible_opacity(kTrayPopupInkDropRippleOpacity);
}

void TrayPopupUtils::ConfigureAsStickyHeader(views::View* view) {
  view->set_id(VIEW_ID_STICKY_HEADER);
  view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kMenuSeparatorVerticalPadding, 0)));
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
}

void TrayPopupUtils::ShowStickyHeaderSeparator(views::View* view,
                                               bool show_separator) {
  if (show_separator) {
    const int separator_width = ash::TrayConstants::separator_width();
    view->SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(0, 0, separator_width, 0,
                                      kMenuSeparatorColor),
        gfx::Insets(kMenuSeparatorVerticalPadding, 0,
                    kMenuSeparatorVerticalPadding - separator_width, 0)));
  } else {
    view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kMenuSeparatorVerticalPadding, 0)));
  }
  view->SchedulePaint();
}

void TrayPopupUtils::ConfigureContainer(TriView::Container container,
                                        views::View* container_view) {
  container_view->SetLayoutManager(CreateDefaultLayoutManager(container));
}

views::LabelButton* TrayPopupUtils::CreateTrayPopupButton(
    views::ButtonListener* listener,
    const base::string16& text) {
  auto* button = views::MdTextButton::Create(listener, text);
  button->SetProminent(true);
  return button;
}

views::Separator* TrayPopupUtils::CreateVerticalSeparator() {
  views::Separator* separator = new views::Separator();
  separator->SetPreferredHeight(24);
  separator->SetColor(kMenuSeparatorColor);
  return separator;
}

std::unique_ptr<views::InkDrop> TrayPopupUtils::CreateInkDrop(
    views::InkDropHostView* host) {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      std::make_unique<views::InkDropImpl>(host, host->size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(false);

  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> TrayPopupUtils::CreateInkDropRipple(
    TrayPopupInkDropStyle ink_drop_style,
    const views::View* host,
    const gfx::Point& center_point,
    SkColor color) {
  return std::make_unique<views::FloodFillInkDropRipple>(
      host->size(), TrayPopupUtils::GetInkDropInsets(ink_drop_style),
      center_point, color, kTrayPopupInkDropRippleOpacity);
}

std::unique_ptr<views::InkDropHighlight> TrayPopupUtils::CreateInkDropHighlight(
    TrayPopupInkDropStyle ink_drop_style,
    const views::View* host,
    SkColor color) {
  const gfx::Rect bounds =
      TrayPopupUtils::GetInkDropBounds(ink_drop_style, host);
  std::unique_ptr<views::InkDropHighlight> highlight(
      new views::InkDropHighlight(bounds.size(), 0,
                                  gfx::PointF(bounds.CenterPoint()), color));
  highlight->set_visible_opacity(kTrayPopupInkDropHighlightOpacity);
  return highlight;
}

std::unique_ptr<views::InkDropMask> TrayPopupUtils::CreateInkDropMask(
    TrayPopupInkDropStyle ink_drop_style,
    const views::View* host) {
  if (ink_drop_style == TrayPopupInkDropStyle::FILL_BOUNDS)
    return nullptr;

  const gfx::Size layer_size = host->size();
  switch (ink_drop_style) {
    case TrayPopupInkDropStyle::HOST_CENTERED: {
      const gfx::Rect mask_bounds =
          GetInkDropBounds(TrayPopupInkDropStyle::HOST_CENTERED, host);
      const int radius =
          std::min(mask_bounds.width(), mask_bounds.height()) / 2;
      return std::make_unique<views::CircleInkDropMask>(
          layer_size, mask_bounds.CenterPoint(), radius);
    }
    case TrayPopupInkDropStyle::INSET_BOUNDS: {
      const gfx::Insets mask_insets =
          GetInkDropInsets(TrayPopupInkDropStyle::INSET_BOUNDS);
      return std::make_unique<views::RoundRectInkDropMask>(
          layer_size, mask_insets, kTrayPopupInkDropCornerRadius);
    }
    case TrayPopupInkDropStyle::FILL_BOUNDS:
      // Handled by quick return above.
      break;
  }
  // Required by some compilers.
  NOTREACHED();
  return nullptr;
}

gfx::Insets TrayPopupUtils::GetInkDropInsets(
    TrayPopupInkDropStyle ink_drop_style) {
  gfx::Insets insets;
  if (ink_drop_style == TrayPopupInkDropStyle::HOST_CENTERED ||
      ink_drop_style == TrayPopupInkDropStyle::INSET_BOUNDS) {
    insets.Set(kTrayPopupInkDropInset, kTrayPopupInkDropInset,
               kTrayPopupInkDropInset, kTrayPopupInkDropInset);
  }
  return insets;
}

gfx::Rect TrayPopupUtils::GetInkDropBounds(TrayPopupInkDropStyle ink_drop_style,
                                           const views::View* host) {
  gfx::Rect bounds = host->GetLocalBounds();
  bounds.Inset(GetInkDropInsets(ink_drop_style));
  return bounds;
}

views::Separator* TrayPopupUtils::CreateListItemSeparator(bool left_inset) {
  views::Separator* separator = new views::Separator();
  separator->SetColor(kMenuSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(
      kMenuSeparatorVerticalPadding - views::Separator::kThickness,
      left_inset
          ? kMenuExtraMarginFromLeftEdge + kMenuButtonSize +
                kTrayPopupLabelHorizontalPadding
          : 0,
      kMenuSeparatorVerticalPadding, 0));
  return separator;
}

views::Separator* TrayPopupUtils::CreateListSubHeaderSeparator() {
  views::Separator* separator = new views::Separator();
  separator->SetColor(kMenuSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(
      kMenuSeparatorVerticalPadding - views::Separator::kThickness, 0, 0, 0));
  return separator;
}

bool TrayPopupUtils::CanOpenWebUISettings() {
  return Shell::Get()->session_controller()->ShouldEnableSettings();
}

void TrayPopupUtils::InitializeAsCheckableRow(HoverHighlightView* container,
                                              bool checked) {
  gfx::ImageSkia check_mark =
      CreateVectorIcon(kCheckCircleIcon, gfx::kGoogleGreen700);
  container->AddRightIcon(check_mark, check_mark.width());
  UpdateCheckMarkVisibility(container, checked);
}

void TrayPopupUtils::UpdateCheckMarkVisibility(HoverHighlightView* container,
                                               bool visible) {
  container->SetRightViewVisible(visible);
  container->SetAccessiblityState(
      visible ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
              : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);
}

}  // namespace ash
