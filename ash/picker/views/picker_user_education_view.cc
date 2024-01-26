// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_user_education_view.h"

#include <string>
#include <string_view>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr ui::ColorId kBackgroundColor = cros_tokens::kCrosSysSystemBase;

// There is a 24px gap between each item.
constexpr auto kItemMargins = gfx::Insets::VH(0, 12);

// The margin for all the items as a whole.
constexpr auto kInteriorMargin = gfx::Insets::TLBR(8, 16, 8, 16);

// Displays a key binding to educate the user about.
// Contains an icon representing the key binding and a label describing what the
// key binding does.
class PickerUserEducationItemView : public views::View {
  METADATA_HEADER(PickerUserEducationItemView, views::View)

 public:
  explicit PickerUserEducationItemView(const std::u16string_view label)
      : label_(label) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(std::make_unique<views::Label>(label_));
  }

 private:
  // TODO(b/314876439): Add icons for each item.
  std::u16string label_;
};

BEGIN_METADATA(PickerUserEducationItemView)
END_METADATA

}  // namespace

PickerUserEducationView::PickerUserEducationView() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, kItemMargins)
      .SetInteriorMargin(kInteriorMargin);
  SetBackground(views::CreateThemedSolidBackground(kBackgroundColor));

  // TODO(b/314876439): Use finalized strings once they are available.
  AddChildView(std::make_unique<PickerUserEducationItemView>(u"a"));
  AddChildView(std::make_unique<PickerUserEducationItemView>(u"b"));
  AddChildView(std::make_unique<PickerUserEducationItemView>(u"c"));
}

PickerUserEducationView::~PickerUserEducationView() = default;

BEGIN_METADATA(PickerUserEducationView, views::View)
END_METADATA

}  // namespace ash
