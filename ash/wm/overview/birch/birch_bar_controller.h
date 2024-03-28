// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/birch/birch_model.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
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
class BirchBarController : public BirchModel::Observer {
 public:
  BirchBarController();
  BirchBarController(const BirchBarController&) = delete;
  BirchBarController& operator=(const BirchBarController&) = delete;
  ~BirchBarController() override;

  // Gets the instance of the controller. It can be nullptr when the Overview
  // session is shutting down.
  static BirchBarController* Get();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Register a bar view with its initialized callback which will be called
  // after initialization.
  void RegisterBar(BirchBarView* bar_view,
                   base::OnceClosure bar_initialized_callback);

  // Called if the given `bar_view` is being destroyed.
  void OnBarDestroying(BirchBarView* bar_view);

  // Show a context menu for the chip which is right clicked by the user.
  void ShowChipContextMenu(BirchChipButton* chip,
                           const gfx::Point& point,
                           ui::MenuSourceType source_type);

  // Called if a suggestion is hidden by user from context menu.
  void OnItemHiddenByUser(BirchItem* item);

  // Called if the user shows/hides the suggestions from context menu.
  void SetShowBirchSuggestions(bool show);

  // Gets if the user allows the suggestions to show.
  bool GetShowBirchSuggestions() const;

 private:
  friend class BirchBarMenuTest;

  // Fetches data from birch model if there is no fetching in progress.
  void MaybeFetchDataFromModel();

  // Called when birch items are fetched from model or the fetching process
  // timed out.
  void OnItemsFetchedFromModel();

  // initialize the given `bar_view` with the items fetched from model.
  void InitBar(BirchBarView* bar_view);

  // Called when the context menu is closed.
  void OnChipContextMenuClosed();

  // BirchModel::Observer:
  void OnBirchClientSet() override;

  // Called when the show suggestions pref changes.
  void OnShowSuggestionsPrefChanged();

  // Birch items fetched from model.
  std::vector<std::unique_ptr<BirchItem>> items_;

  std::unique_ptr<BirchBarMenuModelAdapter> chip_menu_model_adapter_;

  // The map of each bar view to corresponding initialized callback.
  base::flat_map<raw_ptr<BirchBarView>, base::OnceClosure> bar_map_;

  // Indicates if the data fetching is in progress.
  bool data_fetch_in_progress_ = false;

  // Show/hide suggestions pref change registrar.
  PrefChangeRegistrar show_suggestions_pref_registrar_;

  // Customize suggestions pref change registrar.
  PrefChangeRegistrar customize_suggestions_pref_registrar_;

  base::ScopedObservation<BirchModel, BirchModel::Observer>
      birch_model_observer_{this};

  base::WeakPtrFactory<BirchBarController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_
