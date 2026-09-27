// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_
#define ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_

#include <vector>

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/views/view_observer.h"

namespace views {
class MenuItemView;
class View;
}

namespace ash {

class ShelfItemDelegate;
class ShelfWindowPreviewBubble;

// A class wrapping menu operations for ShelfView. Responsible for building,
// running, and recording histograms.
class ASH_EXPORT ShelfMenuModelAdapter : public AppMenuModelAdapter,
                                         public views::ViewObserver {
 public:
  // Delay before showing the window preview bubble when hovering an
  // application menu item.
  static constexpr base::TimeDelta kShowPreviewDelay = base::Milliseconds(500);

  // Delay before closing the window preview bubble when an application menu
  // item is unselected. Allows smoothly updating the bubble in place if
  // another item is selected immediately afterwards.
  static constexpr base::TimeDelta kClosePreviewDelay = base::Milliseconds(150);

  ShelfMenuModelAdapter(const std::string& app_id,
                        std::unique_ptr<ui::SimpleMenuModel> model,
                        views::View* menu_owner,
                        ui::mojom::MenuSourceType source_type,
                        base::OnceClosure on_menu_closed_callback,
                        bool is_tablet_mode,
                        bool for_application_menu_items,
                        base::WeakPtr<ShelfItemDelegate> item_delegate);

  ShelfMenuModelAdapter(const ShelfMenuModelAdapter&) = delete;
  ShelfMenuModelAdapter& operator=(const ShelfMenuModelAdapter&) = delete;

  ~ShelfMenuModelAdapter() override;

  // AppMenuModelAdapter:
  int GetCommandIdForHistograms(int command_id) override;
  void RecordHistogramOnMenuClosed() override;

  // views::MenuModelAdapter:
  views::MenuItemView* AppendMenuItem(views::MenuItemView* menu,
                                      ui::MenuModel* model,
                                      size_t index) override;
  void OnMenuClosed(views::MenuItemView* menu) override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

  // Whether this is showing a menu for |view|.
  bool IsShowingMenuForView(const views::View& view) const;

  ShelfWindowPreviewBubble* preview_bubble() { return preview_bubble_.get(); }

 private:
  void OnMenuItemSelectedChanged(views::MenuItemView* item);
  void ShowPreviewBubble();
  void ClosePreviewBubble(bool animate);
  void OnPreviewBubbleClosing();

  // True if this adapter was created for the shelf application menu items.
  const bool for_application_menu_items_;

  // Delegate for retrieving windows associated with application menu items.
  base::WeakPtr<ShelfItemDelegate> item_delegate_;

  // Subscriptions for MenuItemView selection changes.
  std::vector<base::CallbackListSubscription> selection_subscriptions_;

  // Timer to delay showing or closing the window preview bubble on hover.
  base::OneShotTimer preview_timer_;

  // The view showing the context menu. Not owned.
  raw_ptr<views::View> menu_owner_ = nullptr;

  // The menu item view that is currently anchored to the preview bubble or has
  // a pending preview timer.
  raw_ptr<views::MenuItemView> preview_anchor_view_ = nullptr;

  // The current active preview bubble, if any.
  raw_ptr<ShelfWindowPreviewBubble> preview_bubble_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver>
      menu_owner_observation_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_
