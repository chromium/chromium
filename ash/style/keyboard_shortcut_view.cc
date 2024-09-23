// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/keyboard_shortcut_view.h"

#include "ash/accelerators/keyboard_code_util.h"
#include "ash/app_list/views/search_result_inline_icon_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

KeyboardShortcutView::KeyboardShortcutView(
    const std::vector<ui::KeyboardCode>& keyboard_codes) {
  // Set up a `SearchResultInlineIconView` to represent each key in the
  // combination.
  for (size_t i = 0; i < keyboard_codes.size(); i++) {
    // Get the string for the given keyboard code.
    const auto key_code = keyboard_codes[i];

    // TODO(b/305968013): Save the strings separately to use for accessibility
    // purposes like screen readers.

    auto icon_view = std::make_unique<SearchResultInlineIconView>(
        /*use_modified_styling=*/true, /*is_first_key=*/i == 0);
    icon_view->SetCanProcessEventsWithinSubtree(false);
    icon_view->GetViewAccessibility().SetIsIgnored(true);
    icon_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred));

    // If there is an icon associated with the key (e.g. Search), then set it in
    // the rect. Otherwise, set the associated text (e.g., Shift).
    if (const gfx::VectorIcon* vector_icon =
            GetVectorIconForKeyboardCode(key_code)) {
      icon_view->SetIcon(*vector_icon);
      icon_view->SetTooltipTextForImageView(GetStringForKeyboardCode(key_code));
    } else {
      icon_view->SetText(GetStringForKeyboardCode(key_code));
    }

    AddChildView(std::move(icon_view));
  }
}

KeyboardShortcutView::~KeyboardShortcutView() = default;

BEGIN_METADATA(KeyboardShortcutView)
END_METADATA

}  // namespace ash
