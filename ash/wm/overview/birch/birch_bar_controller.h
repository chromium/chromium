// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/birch/birch_model.h"
#include "ash/wm/overview/birch/birch_bar_constants.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point.h"

class PrefRegistrySimple;

namespace ash {

class BirchBarMenuModelAdapter;
class BirchBarView;
class BirchChipButton;
class BirchItem;

// The controller used to manage the birch bar in every `OverviewGrid`. It will
// fetch data from `BirchModel` and distribute the data to birch bars.
class ASH_EXPORT BirchBarController : public BirchModel::Observer,
                                      public ui::SimpleMenuModel::Delegate {
 public:
  explicit BirchBarController(bool from_pine_service);
  BirchBarController(const BirchBarController&) = delete;
  BirchBarController& operator=(const BirchBarController&) = delete;
  ~BirchBarController() override;

  // Gets the instance of the controller. It can be nullptr when the Overview
  // session is shutting down.
  static BirchBarController* Get();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  std::vector<raw_ptr<BirchBarView>>& bar_views() { return bar_views_; }

  bool is_informed_restore() const { return is_informed_restore_; }

  // Register a bar view.
  void RegisterBar(BirchBarView* bar_view);

  // Called if the given `bar_view` is being destroyed.
  void OnBarDestroying(BirchBarView* bar_view);

  // Show a context menu for the chip which is right clicked by the user.
  void ShowChipContextMenu(BirchChipButton* chip,
                           BirchSuggestionType chip_type,
                           const gfx::Point& point,
                           ui::MenuSourceType source_type);

  // Called if a suggestion is hidden by user from context menu.
  void OnItemHiddenByUser(BirchItem* item);

  // Called if the user shows/hides the suggestions from context menu.
  void SetShowBirchSuggestions(bool show);

  // Gets if the user allows the suggestions to show.
  bool GetShowBirchSuggestions() const;

  // Called if the user shows/hides the given type of suggestions.
  void SetShowSuggestionType(BirchSuggestionType type, bool show);

  // Gets if the user allows to show the given type of suggestions.
  bool GetShowSuggestionType(BirchSuggestionType type) const;

  // Gets if the suggestion data loading is in progress.
  bool IsDataLoading() const;

  // Toggles temperature units for weather chip between F and C.
  void ToggleTemperatureUnits();

  // Executes the commands from bar and chip context menus. `from_chip` will be
  // true if the command is from a chip context menu.
  // Please note that most of the bar menu commands should be executed by the
  // switch button and checkboxes, see `BirchBarMenuModelAdapter` for details.
  // However, due to the way how `MenuController` processes gesture events, the
  // submenu may close on touch such that switch button and checkbox callbacks
  // are not triggered. To solve the issue, we make `SimpleMenuModel::Delegate`
  // to execute the commands for switch button and checkboxes on touch event.
  // This is not a normal usage. For more details, please see the bug comment in
  // http://b/360072119.
  void ExecuteMenuCommand(int command_id, bool from_chip);

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  BirchBarMenuModelAdapter* chip_menu_model_adapter_for_testing() {
    return chip_menu_model_adapter_.get();
  }

 private:
  friend class BirchBarMenuTest;

  // Fetches data from birch model if there is no fetching in progress.
  void MaybeFetchDataFromModel();

  // Called when birch items are fetched from model or the fetching process
  // timed out.
  void OnItemsFetchedFromModel();

  // initialize the given `bar_view` with the `items`.
  void InitBarWithItems(BirchBarView* bar_view,
                        const std::vector<std::unique_ptr<BirchItem>>& items);

  // Remove the chips corresponding to the given `item` from the bars and fill
  // in the chips if there are extra items to show. Note that the `item` is not
  // removed from `items_` list in this method.
  void RemoveItemChips(BirchItem* item);

  // Called when the context menu is closed.
  void OnChipContextMenuClosed();

  // BirchModel::Observer:
  void OnBirchClientSet() override;

  // Called when the show suggestions pref changes.
  void OnShowSuggestionsPrefChanged();

  // Called when the customize suggestion prefs change.
  void OnCustomizeSuggestionsPrefChanged();

  // Called when recevice a lost media item.
  void OnLostMediaItemReceived();

  // Called when the lost media is removed.
  void OnLostMediaItemRemoved();

  // Called when the lost media item is updated with the `updated_item`.
  void OnLostMediaItemUpdated(std::unique_ptr<BirchItem> updated_item);

  // Birch items fetched from model.
  std::vector<std::unique_ptr<BirchItem>> items_;

  std::unique_ptr<BirchBarMenuModelAdapter> chip_menu_model_adapter_;

  std::vector<raw_ptr<BirchBarView>> bar_views_;

  // Indicates if the data fetching is in progress.
  bool data_fetch_in_progress_ = false;

  // True if the overview session is an informed restore session.
  const bool is_informed_restore_;

  // Show/hide suggestions pref change registrar.
  PrefChangeRegistrar show_suggestions_pref_registrar_;

  // Customize suggestions pref change registrar.
  PrefChangeRegistrar customize_suggestions_pref_registrar_;

  // To avoid sending multiple data requests when reset suggestions, the
  // variable is used as an indicator to block the data request from
  // `OnCustomizeSuggestionsPrefChanged`.
  bool hold_data_request_on_suggestion_pref_change_ = false;

  base::ScopedObservation<BirchModel, BirchModel::Observer>
      birch_model_observer_{this};

  base::WeakPtrFactory<BirchBarController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_
