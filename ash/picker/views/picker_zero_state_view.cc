// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <memory>
#include <string>
#include <vector>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr ui::ColorId kBackgroundColor = cros_tokens::kCrosSysSystemBase;
const std::vector<std::vector<std::u16string>> kCategories = {{
                                                                  u"Emojis",
                                                                  u"Symbols",
                                                                  u"Emoticons",
                                                                  u"Gifs",
                                                              },
                                                              {u"Placeholder"}};
}  // namespace

PickerZeroStateView::PickerZeroStateView() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetBackground(views::CreateThemedSolidBackground(kBackgroundColor));
  for (const auto& category : kCategories) {
    for (const auto& entry : category) {
      AddChildView(std::make_unique<views::Label>(entry));
    }
    auto separator = std::make_unique<views::Separator>();
    // Ensure separator is full width.
    separator->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);

    AddChildView(std::move(separator));
  }
}

PickerZeroStateView::~PickerZeroStateView() = default;

BEGIN_METADATA(PickerZeroStateView, views::View)
END_METADATA

}  // namespace ash
