// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SUBMENU_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SUBMENU_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class PickerListItemView;
class PickerSectionView;

// View for a Picker submenu, which shows a list of results in a bubble outside
// the main Picker container.
class ASH_EXPORT PickerSubmenuView : public views::WidgetDelegateView,
                                     public PickerTraversableItemContainer {
  METADATA_HEADER(PickerSubmenuView, views::WidgetDelegateView)

 public:
  PickerSubmenuView(const gfx::Rect& anchor_rect,
                    std::vector<std::unique_ptr<PickerListItemView>> items);
  PickerSubmenuView(const PickerSubmenuView&) = delete;
  PickerSubmenuView& operator=(const PickerSubmenuView&) = delete;
  ~PickerSubmenuView() override;

  // views::WidgetDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // PickerTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

 private:
  gfx::Rect GetDesiredBounds(gfx::Rect anchor_rect);

  // Section which contains the submenu items.
  raw_ptr<PickerSectionView> section_view_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SUBMENU_VIEW_H_
