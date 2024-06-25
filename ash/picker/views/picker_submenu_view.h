// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SUBMENU_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SUBMENU_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class PickerItemView;

// View for a Picker submenu, which shows a list of results in a bubble outside
// the main Picker container.
class ASH_EXPORT PickerSubmenuView : public views::WidgetDelegateView {
  METADATA_HEADER(PickerSubmenuView, views::WidgetDelegateView)

 public:
  PickerSubmenuView(const gfx::Rect& anchor_rect,
                    std::vector<std::unique_ptr<PickerItemView>> items);
  PickerSubmenuView(const PickerSubmenuView&) = delete;
  PickerSubmenuView& operator=(const PickerSubmenuView&) = delete;
  ~PickerSubmenuView() override;

  // views::WidgetDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  gfx::Rect GetDesiredBounds(const gfx::Rect& anchor_rect);
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SUBMENU_VIEW_H_
