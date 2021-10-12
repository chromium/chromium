// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_delete_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/element_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

DesksTemplatesDeleteButton::DesksTemplatesDeleteButton() {
  views::Builder<DesksTemplatesDeleteButton>(this)
      .SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER)
      .SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE)
      .SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_DELETE))
      .BuildChildren();
}

DesksTemplatesDeleteButton::~DesksTemplatesDeleteButton() = default;

void DesksTemplatesDeleteButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  element_style::DecorateMediumCloseButton(this, kTrashCanIcon);
}

BEGIN_METADATA(DesksTemplatesDeleteButton, views::ImageButton)
END_METADATA

}  // namespace ash
