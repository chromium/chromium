// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_controller.h"

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "ash/wm/overview/birch/birch_bar_menu_model_adapter.h"
#include "ash/wm/overview/birch/birch_bar_view.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

// Returns the pref service to use for Birch bar prefs.
PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

}  // namespace

BirchBarController::BirchBarController() {
  // Init and register the show suggestions pref callback.
  show_suggestions_pref_registrar_.Init(GetPrefService());
  show_suggestions_pref_registrar_.Add(
      prefs::kBirchShowSuggestions,
      base::BindRepeating(&BirchBarController::OnShowSuggestionsPrefChanged,
                          base::Unretained(this)));

  if (GetShowBirchSuggestions()) {
    // Fetching data from model if going to show the suggestions.
    MaybeFetchDataFromModel();
  }
}

BirchBarController::~BirchBarController() {
  // Avoid dangling pointers to our `items_`.
  for (auto& bar_and_callback : bar_map_) {
    BirchBarView* bar_view = bar_and_callback.first;
    bar_view->Shutdown();
  }
}

// static.
BirchBarController* BirchBarController::Get() {
  if (auto* overview_session = GetOverviewSession()) {
    return overview_session->birch_bar_controller();
  }
  return nullptr;
}

// static.
void BirchBarController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kBirchShowSuggestions, true);
}

void BirchBarController::RegisterBar(
    BirchBarView* bar_view,
    base::OnceClosure bar_initialized_callback) {
  // Register the bar view and its initialized callback.
  bar_map_[bar_view] = std::move(bar_initialized_callback);

  // Directly initialize the bar view if data fetching is done.
  if (!birch_model_observer_.IsObserving() && !data_fetch_in_progress_) {
    InitBar(bar_view);
  }
}

void BirchBarController::OnBarDestroying(BirchBarView* bar_view) {
  // Clear the initialized callback.
  auto callback_iter = bar_map_.find(bar_view);
  if (callback_iter != bar_map_.end()) {
    bar_map_.erase(callback_iter);
  }
}

void BirchBarController::ShowChipContextMenu(BirchChipButton* chip,
                                             const gfx::Point& point,
                                             ui::MenuSourceType source_type) {
  chip_menu_model_adapter_ = std::make_unique<BirchBarMenuModelAdapter>(
      std::make_unique<BirchBarContextMenuModel>(
          /*delegate=*/chip, BirchBarContextMenuModel::Type::kChipMenu),
      chip->GetWidget(), source_type,
      base::BindOnce(&BirchBarController::OnChipContextMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      Shell::Get()->IsInTabletMode());

  chip_menu_model_adapter_->Run(gfx::Rect(point, gfx::Size()),
                                views::MenuAnchorPosition::kBubbleTopRight,
                                views::MenuRunner::CONTEXT_MENU |
                                    views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                                    views::MenuRunner::FIXED_ANCHOR);
}

void BirchBarController::OnItemHiddenByUser(BirchItem* item) {
  // Remove the item from birch bars. If there is an extra item not showing in
  // the bars, push it in the bars.
  BirchItem* extra_item = items_.size() > BirchBarView::kMaxChipsNum
                              ? items_[BirchBarView::kMaxChipsNum].get()
                              : nullptr;
  for (auto& bar_and_callback : bar_map_) {
    BirchBarView* bar_view = bar_and_callback.first;
    bar_view->RemoveChip(item);
    if (extra_item) {
      bar_view->AddChip(extra_item);
    }
  }

  // Erase the item from model and controller.
  Shell::Get()->birch_model()->RemoveItem(item);
  std::erase_if(items_, base::MatchesUniquePtr(item));
}

void BirchBarController::SetShowBirchSuggestions(bool show) {
  // Register the show suggestions option to user's pref.
  show_suggestions_pref_registrar_.prefs()->SetBoolean(
      prefs::kBirchShowSuggestions, show);
}

bool BirchBarController::GetShowBirchSuggestions() const {
  return show_suggestions_pref_registrar_.prefs()->GetBoolean(
      prefs::kBirchShowSuggestions);
}

void BirchBarController::MaybeFetchDataFromModel() {
  if (data_fetch_in_progress_) {
    return;
  }

  auto* birch_model = Shell::Get()->birch_model();
  if (birch_model->birch_client()) {
    // Fetching data from model.
    data_fetch_in_progress_ = true;
    birch_model->RequestBirchDataFetch(
        base::BindOnce(&BirchBarController::OnItemsFetchedFromModel,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (!birch_model_observer_.IsObserving()) {
    // Observe birch model and wait until client is set before requesting data.
    birch_model_observer_.Observe(birch_model);
  }
}

void BirchBarController::OnItemsFetchedFromModel() {
  // When data fetching completes, use the fetched items to initialize all the
  // bar views.
  data_fetch_in_progress_ = false;
  items_ = Shell::Get()->birch_model()->GetItemsForDisplay();

  // Record an impression if there are suggestion chips to show.
  if (!items_.empty()) {
    base::UmaHistogramBoolean("Ash.Birch.Bar.Impression", true);
  }
  // Record impressions for the suggestion chips.
  for (const auto& item : items_) {
    base::UmaHistogramEnumeration("Ash.Birch.Chip.Impression", item->GetType());
  }
  // Record number of chips being shown.
  base::UmaHistogramCustomCounts("Ash.Birch.ChipCount", items_.size(),
                                 /*min=*/0, /*exclusive_max=*/10,
                                 /*buckets=*/10);

  for (auto& bar_and_callback : bar_map_) {
    InitBar(bar_and_callback.first);
  }
}

void BirchBarController::InitBar(BirchBarView* bar_view) {
  CHECK(!data_fetch_in_progress_);

  for (auto& item : items_) {
    if (bar_view->GetChipsNum() == BirchBarView::kMaxChipsNum) {
      break;
    }
    bar_view->AddChip(item.get());
  }

  // Only run bar initialized callback if there are fetched items.
  if (items_.size() && !bar_map_[bar_view].is_null()) {
    std::move(bar_map_[bar_view]).Run();
  }
}

void BirchBarController::OnChipContextMenuClosed() {
  chip_menu_model_adapter_.reset();
}

void BirchBarController::OnBirchClientSet() {
  // No longer need to observe the birch model once client is set.
  birch_model_observer_.Reset();

  // Fetching data from model.
  MaybeFetchDataFromModel();
}

void BirchBarController::OnShowSuggestionsPrefChanged() {
  const bool show = GetShowBirchSuggestions();

  // If to show all birch bars, we should re-fetch data from model.
  if (show) {
    MaybeFetchDataFromModel();
  }

  auto* overview_session = GetOverviewSession();
  for (auto& root : Shell::GetAllRootWindows()) {
    auto* overview_grid = overview_session->GetGridWithRootWindow(root);
    if (show) {
      overview_grid->MaybeInitBirchBarWidget(/*by_user=*/true);
    } else {
      overview_grid->DestroyBirchBarWidget(/*by_user=*/true);
    }
  }
}

}  // namespace ash
