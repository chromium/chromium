// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_popup_utils.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/size_range_layout.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/unfocusable_label.h"
#include "base/functional/bind.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
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
std::unique_ptr<views::LayoutManager> CreateDefaultCenterLayoutManager(
    bool use_wide_layout) {
  const auto insets =
      gfx::Insets::VH(kTrayPopupLabelVerticalPadding,
                      use_wide_layout ? kQsPopupLabelHorizontalPadding
                                      : kTrayPopupLabelHorizontalPadding);
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, insets);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  return box_layout;
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
  return box_layout;
}

std::unique_ptr<views::LayoutManager> CreateDefaultLayoutManager(
    TriView::Container container,
    bool use_wide_layout) {
  switch (container) {
    case TriView::Container::START:
    case TriView::Container::END:
      return CreateDefaultEndsLayoutManager();
    case TriView::Container::CENTER:
      return CreateDefaultCenterLayoutManager(use_wide_layout);
  }
  // Required by some compilers.
  NOTREACHED();
  return nullptr;
}

// Configures the default size and flex value for the specified |container|
// of the given |tri_view|. Used by CreateDefaultRowView().
void ConfigureDefaultSizeAndFlex(TriView* tri_view,
                                 TriView::Container container,
                                 bool use_wide_layout) {
  int min_width = 0;
  switch (container) {
    case TriView::Container::START:
      min_width = use_wide_layout ? kQsPopupItemMinStartWidth
                                  : kTrayPopupItemMinStartWidth;
      break;
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);
      break;
    case TriView::Container::END:
      min_width =
          use_wide_layout ? kQsPopupItemMinEndWidth : kTrayPopupItemMinEndWidth;
      break;
  }

  tri_view->SetMinSize(container,
                       gfx::Size(min_width, kTrayPopupItemMinHeight));
  constexpr int kTrayPopupItemMaxHeight = 144;
  tri_view->SetMaxSize(container, gfx::Size(SizeRangeLayout::kAbsoluteMaxSize,
                                            kTrayPopupItemMaxHeight));
}

class HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit HighlightPathGenerator(TrayPopupInkDropStyle ink_drop_style)
      : ink_drop_style_(ink_drop_style) {}

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  absl::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::RectF bounds = rect;
    bounds.Inset(gfx::InsetsF(GetInkDropInsets(ink_drop_style_)));
    float corner_radius = 0.f;
    switch (ink_drop_style_) {
      case TrayPopupInkDropStyle::HOST_CENTERED:
        corner_radius = std::min(bounds.width(), bounds.height()) / 2.f;
        bounds.ClampToCenteredSize(gfx::SizeF(corner_radius, corner_radius));
        break;
      case TrayPopupInkDropStyle::INSET_BOUNDS:
        corner_radius = kTrayPopupInkDropCornerRadius;
        break;
      case TrayPopupInkDropStyle::FILL_BOUNDS:
        break;
    }

    return gfx::RRectF(bounds, corner_radius);
  }

 private:
  const TrayPopupInkDropStyle ink_drop_style_;
};

}  // namespace

TriView* TrayPopupUtils::CreateDefaultRowView(bool use_wide_layout) {
  TriView* tri_view = CreateMultiTargetRowView(use_wide_layout);

  tri_view->SetContainerLayout(
      TriView::Container::START,
      CreateDefaultLayoutManager(TriView::Container::START, use_wide_layout));
  tri_view->SetContainerLayout(
      TriView::Container::CENTER,
      CreateDefaultLayoutManager(TriView::Container::CENTER, use_wide_layout));
  tri_view->SetContainerLayout(
      TriView::Container::END,
      CreateDefaultLayoutManager(TriView::Container::END, use_wide_layout));

  return tri_view;
}

TriView* TrayPopupUtils::CreateSubHeaderRowView(bool start_visible) {
  TriView* tri_view = CreateDefaultRowView(/*use_wide_layout=*/false);
  if (!start_visible) {
    tri_view->SetInsets(gfx::Insets::TLBR(
        0, kTrayPopupPaddingHorizontal - kTrayPopupLabelHorizontalPadding, 0,
        0));
    tri_view->SetContainerVisible(TriView::Container::START, false);
  }
  return tri_view;
}

TriView* TrayPopupUtils::CreateMultiTargetRowView(bool use_wide_layout) {
  TriView* tri_view = new TriView(0 /* padding_between_items */);

  tri_view->SetInsets(
      gfx::Insets::TLBR(0,
                        use_wide_layout ? kQsExtraMarginFromLeftEdge
                                        : kMenuExtraMarginFromLeftEdge,
                        0, use_wide_layout ? kQsExtraMarginsFromRightEdge : 0));

  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::START,
                              use_wide_layout);
  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::CENTER,
                              use_wide_layout);
  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::END,
                              use_wide_layout);

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

UnfocusableLabel* TrayPopupUtils::CreateUnfocusableLabel() {
  UnfocusableLabel* label = new UnfocusableLabel();
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetSubpixelRenderingEnabled(false);
  return label;
}

views::ImageView* TrayPopupUtils::CreateMainImageView(bool use_wide_layout) {
  auto* image = new views::ImageView;
  if (use_wide_layout) {
    image->SetPreferredSize(gfx::Size(kMenuIconSize, kMenuIconSize));
  } else {
    image->SetPreferredSize(
        gfx::Size(kTrayPopupItemMinStartWidth, kTrayPopupItemMinHeight));
  }
  return image;
}

std::unique_ptr<views::Painter> TrayPopupUtils::CreateFocusPainter() {
  return views::Painter::CreateSolidFocusPainter(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor),
      kFocusBorderThickness, gfx::InsetsF());
}

void TrayPopupUtils::ConfigureAsStickyHeader(views::View* view) {
  view->SetID(VIEW_ID_STICKY_HEADER);
  view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kMenuSeparatorVerticalPadding, 0)));
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
}

views::LabelButton* TrayPopupUtils::CreateTrayPopupButton(
    views::Button::PressedCallback callback,
    const std::u16string& text) {
  auto button =
      std::make_unique<views::MdTextButton>(std::move(callback), text);
  button->SetProminent(true);
  return button.release();
}

views::Separator* TrayPopupUtils::CreateVerticalSeparator() {
  views::Separator* separator = new views::Separator();
  separator->SetPreferredLength(24);
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  return separator;
}

void TrayPopupUtils::InstallHighlightPathGenerator(
    views::View* host,
    TrayPopupInkDropStyle ink_drop_style) {
  views::HighlightPathGenerator::Install(
      host, std::make_unique<HighlightPathGenerator>(ink_drop_style));
}

views::Separator* TrayPopupUtils::CreateListSubHeaderSeparator() {
  views::Separator* separator = new views::Separator();
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kMenuSeparatorVerticalPadding - views::Separator::kThickness, 0, 0, 0)));
  return separator;
}

views::Separator* TrayPopupUtils::CreateListItemSeparator(bool left_inset) {
  views::Separator* separator = new views::Separator();
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kMenuSeparatorVerticalPadding - views::Separator::kThickness,
      left_inset ? kMenuExtraMarginFromLeftEdge + kMenuButtonSize +
                       kTrayPopupLabelHorizontalPadding
                 : 0,
      kMenuSeparatorVerticalPadding, 0)));
  return separator;
}

bool TrayPopupUtils::CanOpenWebUISettings() {
  return Shell::Get()->session_controller()->ShouldEnableSettings();
}

void TrayPopupUtils::InitializeAsCheckableRow(HoverHighlightView* container,
                                              bool checked,
                                              bool enterprise_managed) {
  const int dip_size = GetDefaultSizeOfVectorIcon(kCheckCircleIcon);
  ui::ImageModel check_mark = ui::ImageModel::FromVectorIcon(
      kHollowCheckCircleIcon, kColorAshIconColorProminent, dip_size);
  if (enterprise_managed) {
    ui::ImageModel enterprise_managed_icon = ui::ImageModel::FromVectorIcon(
        chromeos::kEnterpriseIcon, kColorAshIconColorBlocked, dip_size);
    container->AddRightIcon(enterprise_managed_icon,
                            enterprise_managed_icon.Size().width());
  }
  container->AddRightIcon(check_mark, check_mark.Size().width());
  UpdateCheckMarkVisibility(container, checked);
}

void TrayPopupUtils::UpdateCheckMarkVisibility(HoverHighlightView* container,
                                               bool visible) {
  if (!container)
    return;
  container->SetRightViewVisible(visible);
  container->SetAccessibilityState(
      visible ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
              : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);
}

void TrayPopupUtils::SetLabelFontList(views::Label* label, FontStyle style) {
  label->SetAutoColorReadabilityEnabled(false);
  const gfx::FontList google_sans_font_list({"Google Sans"}, gfx::Font::NORMAL,
                                            16, gfx::Font::Weight::MEDIUM);
  const gfx::FontList roboto_font_list({"Roboto"}, gfx::Font::NORMAL, 16,
                                       gfx::Font::Weight::MEDIUM);

  switch (style) {
    case FontStyle::kTitle:
      label->SetFontList(google_sans_font_list);
      break;
    case FontStyle::kPodMenuHeader:
      label->SetFontList(roboto_font_list);
      break;
    case FontStyle::kSubHeader:
      label->SetFontList(roboto_font_list.Derive(-1, gfx::Font::NORMAL,
                                                 gfx::Font::Weight::MEDIUM));
      break;
    case FontStyle::kSmallTitle:
      label->SetFontList(roboto_font_list.Derive(-3, gfx::Font::NORMAL,
                                                 gfx::Font::Weight::MEDIUM));
      break;
    case FontStyle::kDetailedViewLabel:
    case FontStyle::kSystemInfo:
      label->SetFontList(roboto_font_list.Derive(-4, gfx::Font::NORMAL,
                                                 gfx::Font::Weight::NORMAL));
      break;
  }
}

}  // namespace ash
