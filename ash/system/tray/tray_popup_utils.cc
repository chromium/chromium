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
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
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
#include "ui/views/controls/highlight_path_generator.h"
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
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(8, kTrayPopupLabelHorizontalPadding));
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  return std::move(box_layout);
}

// Creates a layout manager that positions Views horizontally. The Views will be
// centered along the horizontal and vertical axis.
std::unique_ptr<views::LayoutManager> CreateDefaultEndsLayoutManager() {
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
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

gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style) {
  if (ink_drop_style == TrayPopupInkDropStyle::HOST_CENTERED ||
      ink_drop_style == TrayPopupInkDropStyle::INSET_BOUNDS) {
    return gfx::Insets(kTrayPopupInkDropInset);
  }
  return gfx::Insets();
}

gfx::Rect GetInkDropBounds(TrayPopupInkDropStyle ink_drop_style,
                           const views::View* host) {
  gfx::Rect bounds = host->GetLocalBounds();
  bounds.Inset(GetInkDropInsets(ink_drop_style));
  return bounds;
}

class HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator(TrayPopupInkDropStyle ink_drop_style)
      : ink_drop_style_(ink_drop_style) {}

  // HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    SkPath path;

    const gfx::Rect mask_bounds =
        GetInkDropBounds(TrayPopupInkDropStyle::HOST_CENTERED, view);
    switch (ink_drop_style_) {
      case TrayPopupInkDropStyle::HOST_CENTERED: {
        gfx::Point center_point = mask_bounds.CenterPoint();
        const int radius =
            std::min(mask_bounds.width(), mask_bounds.height()) / 2;
        path.addCircle(center_point.x(), center_point.y(), radius);
        break;
      }
      case TrayPopupInkDropStyle::INSET_BOUNDS:
        path.addRoundRect(RectToSkRect(mask_bounds),
                          kTrayPopupInkDropCornerRadius,
                          kTrayPopupInkDropCornerRadius);
        break;
      case TrayPopupInkDropStyle::FILL_BOUNDS:
        path.addRect(RectToSkRect(mask_bounds));
        break;
    }
    return path;
  }

 private:
  const TrayPopupInkDropStyle ink_drop_style_;

  DISALLOW_COPY_AND_ASSIGN(HighlightPathGenerator);
};

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
  button->SetInstallFocusRingOnFocus(true);
  button->SetFocusForPlatform();
  button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  button->set_has_ink_drop_action_on_click(true);
}

void TrayPopupUtils::ConfigureAsStickyHeader(views::View* view) {
  view->SetID(VIEW_ID_STICKY_HEADER);
  view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kMenuSeparatorVerticalPadding, 0)));
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
}

void TrayPopupUtils::ConfigureContainer(TriView::Container container,
                                        views::View* container_view) {
  container_view->SetLayoutManager(CreateDefaultLayoutManager(container));
}

views::LabelButton* TrayPopupUtils::CreateTrayPopupButton(
    views::ButtonListener* listener,
    const base::string16& text) {
  auto button = views::MdTextButton::Create(listener, text);
  button->SetProminent(true);
  return button.release();
}

views::Separator* TrayPopupUtils::CreateVerticalSeparator() {
  views::Separator* separator = new views::Separator();
  separator->SetPreferredHeight(24);
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparator,
      AshColorProvider::AshColorMode::kLight));
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
    SkColor background_color) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(background_color);
  return std::make_unique<views::FloodFillInkDropRipple>(
      host->size(), GetInkDropInsets(ink_drop_style), center_point,
      ripple_attributes.base_color, ripple_attributes.inkdrop_opacity);
}

std::unique_ptr<views::InkDropHighlight> TrayPopupUtils::CreateInkDropHighlight(
    TrayPopupInkDropStyle ink_drop_style,
    const views::View* host,
    SkColor background_color) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(background_color);
  const gfx::Rect bounds = GetInkDropBounds(ink_drop_style, host);
  std::unique_ptr<views::InkDropHighlight> highlight(
      new views::InkDropHighlight(bounds.size(), 0,
                                  gfx::PointF(bounds.CenterPoint()),
                                  ripple_attributes.base_color));
  highlight->set_visible_opacity(ripple_attributes.highlight_opacity);
  return highlight;
}

void TrayPopupUtils::InstallHighlightPathGenerator(
    views::View* host,
    TrayPopupInkDropStyle ink_drop_style) {
  views::HighlightPathGenerator::Install(
      host, std::make_unique<HighlightPathGenerator>(ink_drop_style));
}

views::Separator* TrayPopupUtils::CreateListItemSeparator(bool left_inset) {
  views::Separator* separator = new views::Separator();
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparator,
      AshColorProvider::AshColorMode::kLight));
  separator->SetBorder(views::CreateEmptyBorder(
      kMenuSeparatorVerticalPadding - views::Separator::kThickness,
      left_inset
          ? kMenuExtraMarginFromLeftEdge + kMenuButtonSize +
                kTrayPopupLabelHorizontalPadding
          : 0,
      kMenuSeparatorVerticalPadding, 0));
  return separator;
}

bool TrayPopupUtils::CanOpenWebUISettings() {
  return Shell::Get()->session_controller()->ShouldEnableSettings();
}

void TrayPopupUtils::InitializeAsCheckableRow(HoverHighlightView* container,
                                              bool checked,
                                              bool enterprise_managed) {
  const int dip_size = GetDefaultSizeOfVectorIcon(kCheckCircleIcon);
  gfx::ImageSkia check_mark =
      CreateVectorIcon(kCheckCircleIcon, dip_size, gfx::kGoogleGreenDark600);
  if (enterprise_managed) {
    gfx::ImageSkia enterprise_managed_icon = CreateVectorIcon(
        kLoginScreenEnterpriseIcon, dip_size, gfx::kGoogleGrey100);
    container->AddRightIcon(enterprise_managed_icon,
                            enterprise_managed_icon.width());
  }
  container->AddRightIcon(check_mark, check_mark.width());
  UpdateCheckMarkVisibility(container, checked);
}

void TrayPopupUtils::UpdateCheckMarkVisibility(HoverHighlightView* container,
                                               bool visible) {
  container->SetRightViewVisible(visible);
  container->SetAccessibilityState(
      visible ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
              : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);
}

}  // namespace ash
