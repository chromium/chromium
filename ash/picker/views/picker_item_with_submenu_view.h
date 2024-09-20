// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ITEM_WITH_SUBMENU_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_ITEM_WITH_SUBMENU_VIEW_H_

#include <string>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/views/picker_item_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class ImageModel;
class ImageView;
class Label;
}  // namespace views

namespace ash {

// View for a Picker item which has a submenu.
class ASH_EXPORT PickerItemWithSubmenuView : public PickerItemView {
  METADATA_HEADER(PickerItemWithSubmenuView, PickerItemView)

 public:
  PickerItemWithSubmenuView();
  PickerItemWithSubmenuView(const PickerItemWithSubmenuView&) = delete;
  PickerItemWithSubmenuView& operator=(const PickerItemWithSubmenuView&) =
      delete;
  ~PickerItemWithSubmenuView() override;

  void SetLeadingIcon(const ui::ImageModel& icon);

  void SetText(const std::u16string& text);

  void AddEntry(PickerSearchResult item, SelectItemCallback callback);

  bool IsEmpty() const;

  void ShowSubmenu();

  // PickerItemView:
  void OnMouseEntered(const ui::MouseEvent& event) override;

  const std::u16string& GetTextForTesting() const;

 private:
  raw_ptr<views::ImageView> leading_icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  std::vector<std::pair<PickerSearchResult, SelectItemCallback>> entries_;

  base::WeakPtrFactory<PickerItemWithSubmenuView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerItemWithSubmenuView, PickerItemView)
VIEW_BUILDER_PROPERTY(ui::ImageModel, LeadingIcon)
VIEW_BUILDER_PROPERTY(std::u16string, Text)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerItemWithSubmenuView)

#endif  // ASH_PICKER_VIEWS_PICKER_ITEM_WITH_SUBMENU_VIEW_H_
