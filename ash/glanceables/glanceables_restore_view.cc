// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_restore_view.h"

#include <memory>

#include "ash/glanceables/glanceables_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

void OnButtonPressed() {
  Shell::Get()->glanceables_controller()->RestoreSession();
}

}  // namespace

GlanceablesRestoreView::GlanceablesRestoreView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);

  AddPillButton();
}

GlanceablesRestoreView::~GlanceablesRestoreView() = default;

void GlanceablesRestoreView::AddPillButton() {
  pill_button_ = AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&OnButtonPressed),
      l10n_util::GetStringUTF16(IDS_GLANCEABLES_RESTORE)));
  // TODO(crbug.com/1353119): Use color provider.
  pill_button_->SetButtonTextColor(gfx::kGoogleGrey800);
  pill_button_->SetBackgroundColor(gfx::kGoogleGrey500);
}

}  // namespace ash
