// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_zero_state_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

using views::BoxLayout;
using views::ImageView;
using views::Label;

namespace ash {

CastZeroStateView::CastZeroStateView() {
  SetUseDefaultFillLayout(true);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(0, 16, 16, 16)));

  // The zero-state view are inside a rounded container.
  auto* container = AddChildView(std::make_unique<RoundedContainer>());
  container->SetBorderInsets(gfx::Insets::VH(0, 16));

  // The views are centered vertically.
  std::unique_ptr<BoxLayout> layout =
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kCenter);
  container->SetLayoutManager(std::move(layout));

  // TODO(b/252872586): UX provided unscaled PNG images as placeholders. They
  // should be replaced with a themed vector icon.
  int image_resource_id = DarkLightModeController::Get()->IsDarkModeEnabled()
                              ? IDR_TRAY_CAST_ZERO_STATE_DARK
                              : IDR_TRAY_CAST_ZERO_STATE_LIGHT;
  ImageView* image = container->AddChildView(std::make_unique<ImageView>(
      ui::ImageModel::FromResourceId(image_resource_id)));
  // The placeholders are unscaled (2x), so handle sizing here.
  image->SetImageSize(gfx::Size(176, 170));

  Label* title = container->AddChildView(std::make_unique<Label>(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_ZERO_STATE_TITLE)));
  title->SetMultiLine(true);
  bubble_utils::ApplyStyle(title, TypographyToken::kCrosTitle1);
  title->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  title->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(32, 0, 0, 0));
  title->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  Label* subtitle = container->AddChildView(std::make_unique<Label>(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_ZERO_STATE_SUBTITLE)));
  subtitle->SetMultiLine(true);
  bubble_utils::ApplyStyle(subtitle, TypographyToken::kCrosBody1);
  subtitle->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  subtitle->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(8, 0, 0, 0));
  subtitle->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

BEGIN_METADATA(CastZeroStateView)
END_METADATA

}  // namespace ash
