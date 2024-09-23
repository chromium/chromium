// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_MENU_MODEL_ADAPTER_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_MENU_MODEL_ADAPTER_H_

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/ash_export.h"
#include "ash/style/checkbox.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class BirchBarContextMenuModel;

// The menu model adapter for `BirchBarContextMenuModel`. It customizes certain
// menu items by adding widgets like switch button and checkbox. `for_chip_menu`
// is true if the model adapter is used for a `BirchChipContextMenuModel`. It's
// false if the model is `BirchBarContextMenuModel`.
class ASH_EXPORT BirchBarMenuModelAdapter : public AppMenuModelAdapter,
                                            public Checkbox::Delegate {
 public:
  BirchBarMenuModelAdapter(
      std::unique_ptr<ui::SimpleMenuModel> birch_menu_model,
      views::Widget* widget_owner,
      ui::MenuSourceType source_type,
      base::OnceClosure on_menu_closed_callback,
      bool is_tablet_mode,
      bool for_chip_menu);
  BirchBarMenuModelAdapter(const BirchBarMenuModelAdapter&) = delete;
  BirchBarMenuModelAdapter& operator=(const BirchBarMenuModelAdapter&) = delete;
  ~BirchBarMenuModelAdapter() override;

  // Checkbox::Delegate:
  void OnButtonSelected(OptionButtonBase* button) override;
  void OnButtonClicked(OptionButtonBase* button) override;

 protected:
  // AppMenuModelAdapter:
  views::MenuItemView* AppendMenuItem(views::MenuItemView* menu,
                                      ui::MenuModel* model,
                                      size_t index) override;
  void RecordHistogramOnMenuClosed() override;

 private:
  const bool for_chip_menu_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_MENU_MODEL_ADAPTER_H_
