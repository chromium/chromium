// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_BADGE_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_BADGE_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class Label;
}

namespace ash {

// View for a badge containing a label with an icon after it.
class ASH_EXPORT PickerBadgeView : public views::BoxLayoutView {
  METADATA_HEADER(PickerBadgeView, views::BoxLayoutView)

 public:
  PickerBadgeView();
  PickerBadgeView(const PickerBadgeView&) = delete;
  PickerBadgeView& operator=(const PickerBadgeView&) = delete;
  ~PickerBadgeView() override;

  const std::u16string& GetText() const;
  void SetText(const std::u16string& text);

  // views::BoxLayoutView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  raw_ptr<views::Label> label_ = nullptr;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerBadgeView, views::BoxLayoutView)
VIEW_BUILDER_PROPERTY(const std::u16string&, Text)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerBadgeView)

#endif  // ASH_PICKER_VIEWS_PICKER_BADGE_VIEW_H_
