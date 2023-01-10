// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/remote_maintenance_curtain_view.h"

#include <memory>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check_deref.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash::curtain {

namespace {

using views::Builder;
using views::FlexLayoutView;
using views::ImageView;
using views::kFlexBehaviorKey;
using views::kMarginsKey;
using views::LayoutOrientation;

constexpr gfx::Size kEnterpriseIconSize(40, 40);
constexpr gfx::Size kLockImageSize(300, 300);

constexpr gfx::Insets kEnterpriseIconMargin = gfx::Insets::VH(20, 5);
constexpr gfx::Insets kLockImageMargin = gfx::Insets::VH(20, 20);
constexpr gfx::Insets kLeftSideMargins = gfx::Insets::VH(200, 100);
constexpr gfx::Insets kRightSideMargins = gfx::Insets::VH(100, 100);

gfx::ImageSkia EnterpriseIcon(const ColorProvider& color_provider) {
  return gfx::CreateVectorIcon(
      chromeos::kEnterpriseIcon,
      color_provider.GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));
}

gfx::ImageSkia LockImage(const ColorProvider& color_provider) {
  return gfx::CreateVectorIcon(
      kSystemTrayCapsLockIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorProminent));
}

std::u16string TitleText() {
  return l10n_util::GetStringUTF16(IDS_ASH_CURTAIN_TITLE);
}

std::u16string MessageText() {
  return l10n_util::GetStringUTF16(IDS_ASH_CURTAIN_DESCRIPTION);
}

// A container that - when added as a child of a `FlexContainer` - will
// automatically resize to take an equal share of the available space.
class ResizingFlexContainer : public views::FlexLayoutView {
 public:
  ResizingFlexContainer() {
    // Tell our parent flex container that we want to be resized depending
    // on the available space.
    SetProperty(
        kFlexBehaviorKey,
        views::FlexSpecification{views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded});
  }
  ResizingFlexContainer(const ResizingFlexContainer&) = delete;
  ResizingFlexContainer& operator=(const ResizingFlexContainer&) = delete;
  ~ResizingFlexContainer() override = default;

  // `views::FlexLayoutView` implementation:

  gfx::Size CalculatePreferredSize() const override {
    // The parent Flex container will first grant each child as much space
    // as their preferred size, and then distributes all remaining space
    // equally among all children. So to ensure all children get exactly the
    // same space, we make them all report the same (small) preferred size.
    return gfx::Size(1, 1);
  }
};

BEGIN_VIEW_BUILDER(, ResizingFlexContainer, views::FlexLayoutView)
END_VIEW_BUILDER

}  // namespace

}  // namespace ash::curtain

// Allow `ResizingContainer` to be used inside a view builder hierarchy
// (`Builder<ResizingContainer>().SetXYZ()`).
//
// Must be in the global namespace.
DEFINE_VIEW_BUILDER(, ash::curtain::ResizingFlexContainer)

namespace ash::curtain {

RemoteMaintenanceCurtainView::RemoteMaintenanceCurtainView() {
  Initialize();
}

RemoteMaintenanceCurtainView::~RemoteMaintenanceCurtainView() = default;

void RemoteMaintenanceCurtainView::Initialize() {
  const ColorProvider& color_provider = CHECK_DEREF(ColorProvider::Get());
  const int shelf_size = CHECK_DEREF(ShelfConfig::Get()).shelf_size();

  // A flex rule forcing the view to maintain its fixed size.
  const views::FlexSpecification kFixedSize(
      views::MinimumFlexSizeRule::kPreferred,
      views::MaximumFlexSizeRule::kPreferred);

  Builder<FlexLayoutView>(this)
      .SetOrientation(LayoutOrientation::kVertical)
      .SetBackground(
          views::CreateSolidBackground(color_provider.GetBaseLayerColor(
              ColorProvider::BaseLayerType::kOpaque)))
      .AddChildren(
          // Main content
          Builder<ResizingFlexContainer>()
              .SetOrientation(LayoutOrientation::kHorizontal)
              .AddChildren(
                  // Left half of the screen
                  Builder<ResizingFlexContainer>()
                      .SetProperty(kMarginsKey, kLeftSideMargins)
                      .SetOrientation(LayoutOrientation::kVertical)
                      .AddChildren(
                          // Enterprise icon
                          Builder<ImageView>()
                              .SetImage(EnterpriseIcon(color_provider))
                              .SetImageSize(kEnterpriseIconSize)
                              .SetProperty(kMarginsKey, kEnterpriseIconMargin)
                              .SetHorizontalAlignment(
                                  ImageView::Alignment::kLeading),
                          // Title
                          Builder<views::Label>()
                              .SetText(TitleText())
                              .SetTextStyle(views::style::STYLE_EMPHASIZED)
                              .SetTextContext(
                                  views::style::CONTEXT_DIALOG_TITLE)
                              .SetHorizontalAlignment(
                                  gfx::HorizontalAlignment::ALIGN_LEFT)
                              .SetMultiLine(true)
                              .SetEnabledColor(
                                  color_provider.GetContentLayerColor(
                                      ColorProvider::ContentLayerType::
                                          kTextColorPrimary)),
                          // Message
                          Builder<views::Label>()
                              .SetText(MessageText())
                              .SetVerticalAlignment(
                                  gfx::VerticalAlignment::ALIGN_TOP)
                              .SetHorizontalAlignment(
                                  gfx::HorizontalAlignment::ALIGN_LEFT)
                              .SetMultiLine(true)
                              .SetEnabledColor(
                                  color_provider.GetContentLayerColor(
                                      ColorProvider::ContentLayerType::
                                          kTextColorPrimary))),
                  // Right half of the screen
                  Builder<ResizingFlexContainer>()
                      .SetProperty(kMarginsKey, kRightSideMargins)
                      .AddChildren(
                          Builder<ImageView>()
                              .SetImage(LockImage(color_provider))
                              .SetImageSize(kLockImageSize)
                              .SetProperty(kMarginsKey, kLockImageMargin)
                              .SetHorizontalAlignment(
                                  ImageView::Alignment::kCenter))),
          // Shelf
          Builder<View>()
              .SetPreferredSize(gfx::Size(0, shelf_size))
              .SetProperty(kFlexBehaviorKey, kFixedSize))
      .BuildChildren();
}

BEGIN_METADATA(RemoteMaintenanceCurtainView, views::FlexLayoutView)
END_METADATA

}  // namespace ash::curtain
