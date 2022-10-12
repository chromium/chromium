// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_header.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/style/pill_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

constexpr gfx::Insets kQuickSettingsHeaderPadding(16);

}  // namespace

QuickSettingsHeader::QuickSettingsHeader() {
  DCHECK(features::IsQsRevampEnabled());

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kQuickSettingsHeaderPadding));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // TODO(b/251724754): Remove this temporary placeholder.
  placeholder_ = AddChildView(std::make_unique<PillButton>(
      PillButton::PressedCallback(), u"Placeholder"));
}

QuickSettingsHeader::~QuickSettingsHeader() = default;

BEGIN_METADATA(QuickSettingsHeader, views::View)
END_METADATA

}  // namespace ash
