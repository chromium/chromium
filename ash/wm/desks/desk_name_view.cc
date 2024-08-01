// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_name_view.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {

constexpr int kDeskNameViewHorizontalPadding = 6;

}  // namespace

DeskNameView::DeskNameView(DeskMiniView* mini_view)
    : DeskTextfield(SystemTextfield::Type::kSmall), mini_view_(mini_view) {
  views::Builder<DeskNameView>(this)
      .SetBorder(views::CreateEmptyBorder(
          gfx::Insets::VH(0, kDeskNameViewHorizontalPadding)))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
      .BuildChildren();

  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_DESK_NAME));
}

DeskNameView::~DeskNameView() = default;

void DeskNameView::OnFocus() {
  DeskTextfield::OnFocus();

  // When this gets focus, scroll to make `mini_view_` visible.
  mini_view_->owner_bar()->ScrollToShowViewIfNecessary(mini_view_);
}

BEGIN_METADATA(DeskNameView)
END_METADATA

}  // namespace ash
