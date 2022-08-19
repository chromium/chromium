// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_restore_view.h"

#include <memory>

#include "ash/glanceables/glanceables_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {
namespace {

void OnButtonPressed() {
  Shell::Get()->glanceables_controller()->RestoreSession();
}

gfx::ImageSkia CreatePlaceholderImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(gfx::kGoogleBlue500);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}
}  // namespace

GlanceablesRestoreView::GlanceablesRestoreView()
    : views::ImageButton(base::BindRepeating(&OnButtonPressed)) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_GLANCEABLES_RESTORE_SESSION));

  // TODO(crbug.com/1353119): Show a screenshot from the last session.
  SetImage(views::Button::STATE_NORMAL, CreatePlaceholderImage(300, 200));
}

GlanceablesRestoreView::~GlanceablesRestoreView() = default;

}  // namespace ash
