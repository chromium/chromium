// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_
#define ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace views {
class View;
}

namespace ash {

// A class wrapping menu operations for ShelfView. Responsible for building,
// running, and recording histograms.
class ASH_EXPORT ShelfMenuModelAdapter : public AppMenuModelAdapter {
 public:
  ShelfMenuModelAdapter(const std::string& app_id,
                        std::unique_ptr<ui::SimpleMenuModel> model,
                        views::View* menu_owner,
                        ui::MenuSourceType source_type,
                        base::OnceClosure on_menu_closed_callback,
                        bool is_tablet_mode,
                        bool for_application_menu_items);

  ShelfMenuModelAdapter(const ShelfMenuModelAdapter&) = delete;
  ShelfMenuModelAdapter& operator=(const ShelfMenuModelAdapter&) = delete;

  ~ShelfMenuModelAdapter() override;

  // Overridden from AppMenuModelAdapter:
  int GetCommandIdForHistograms(int command_id) override;
  void RecordHistogramOnMenuClosed() override;

  // Whether this is showing a menu for |view|.
  bool IsShowingMenuForView(const views::View& view) const;

 private:
  // The view showing the context menu. Not owned.
  // TODO(b/342519765): Fix the dangling ptr issue.
  raw_ptr<views::View, DanglingUntriaged> menu_owner_;

  // True if this adapter was created for the shelf application menu items.
  const bool for_application_menu_items_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_
