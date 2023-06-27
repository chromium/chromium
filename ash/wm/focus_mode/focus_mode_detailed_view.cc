// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/focus_mode/focus_mode_detailed_view.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// Margins between containers in the detailed view.
constexpr auto kContainerMargins = gfx::Insets::TLBR(2, 0, 0, 0);

}  // namespace

FocusModeDetailedView::FocusModeDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  // TODO(b/288975135): update with official string.
  CreateTitleRow(IDS_ASH_STATUS_TRAY_CAST);
  CreateScrollableList();

  // TODO(b/286932057): remove border inset and add row toggle UI.
  toggle_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kTopRounded));
  toggle_view_->SetBorderInsets(gfx::Insets::VH(32, 0));

  // TODO(b/286931575): remove border inset and add Timer UI.
  timer_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kNotRounded));
  timer_view_->SetBorderInsets(gfx::Insets::VH(56, 0));
  timer_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  // TODO(b/286931806): remove border inset and add Focus Scene UI.
  scene_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kNotRounded));
  scene_view_->SetBorderInsets(gfx::Insets::VH(100, 0));
  scene_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  // TODO(b/286932317): remove border inset and add DND UI.
  do_not_disturb_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kBottomRounded));
  do_not_disturb_view_->SetBorderInsets(gfx::Insets::VH(32, 0));
  do_not_disturb_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  scroll_content()->SizeToPreferredSize();
}

FocusModeDetailedView::~FocusModeDetailedView() = default;

BEGIN_METADATA(FocusModeDetailedView, TrayDetailedView)
END_METADATA

}  // namespace ash
