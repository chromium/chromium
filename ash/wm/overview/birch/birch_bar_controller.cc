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

// Records the ranking of each item in `items` in a histogram selected based on
// the time of day. The histogram time cutoffs are chosen based on BirchRanker
// behavior.
void RecordTimeOfDayRankingHistogram(
    const std::vector<std::unique_ptr<BirchItem>>& items) {
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  const char* now_histogram = nullptr;
  if (exploded.hour < 5) {
    now_histogram = "Ash.Birch.Ranking.0000to0500";
  } else if (exploded.hour < 12) {
    now_histogram = "Ash.Birch.Ranking.0500to1200";
  } else if (exploded.hour < 17) {
    now_histogram = "Ash.Birch.Ranking.1200to1700";
  } else {
    now_histogram = "Ash.Birch.Ranking.1700to0000";
  }
  for (const auto& item : items) {
    int ranking_int = static_cast<int>(item->ranking());
    base::UmaHistogramCounts100(now_histogram, ranking_int);
    // Also record an aggregate for the day.
    base::UmaHistogramCounts100("Ash.Birch.Ranking.Total", ranking_int);
  }
}

}  // namespace

BirchBarController::BirchBarController(bool from_pine_service)
    : from_pine_service_(from_pine_service) {
  // Init and register the show suggestions pref changed callback.
  show_suggestions_pref_registrar_.Init(GetPrefService());
  show_suggestions_pref_registrar_.Add(
      prefs::kBirchShowSuggestions,
      base::BindRepeating(&BirchBarController::OnShowSuggestionsPrefChanged,
                          base::Unretained(this)));

  // Init and register the customize suggestion pref changed callback.
  customize_suggestions_pref_registrar_.Init(GetPrefService());

  for (const auto& suggestion_pref :
       {prefs::kBirchUseCalendar, prefs::kBirchUseWeather,
        prefs::kBirchUseFileSuggest, prefs::kBirchUseRecentTabs,
        prefs::kBirchUseReleaseNotes}) {
    customize_suggestions_pref_registrar_.Add(
        suggestion_pref,
        base::BindRepeating(
            &BirchBarController::OnCustomizeSuggestionsPrefChanged,
            base::Unretained(this)));
  }

  if (GetShowBirchSuggestions()) {
    // Fetching data from model if going to show the suggestions.
    MaybeFetchDataFromModel();
  }
}

BirchBarController::~BirchBarController() {
  // Avoid dangling pointers to our `items_`.
  for (auto& bar_view : bar_views_) {
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

void BirchBarController::RegisterBar(BirchBarView* bar_view) {
  bar_views_.emplace_back(bar_view);

  // Directly initialize the bar view if data fetching is done.
  if (!birch_model_observer_.IsObserving() && !data_fetch_in_progress_) {
    InitBarWithItems(bar_view, items_);
  } else if (from_pine_service_) {
    // Perform loading animation at the beginning of pine section.
    bar_view->Loading();
  }
}

void BirchBarController::OnBarDestroying(BirchBarView* bar_view) {
  // Remove the bar view.
  auto iter = std::find(bar_views_.begin(), bar_views_.end(), bar_view);
  if (iter != bar_views_.end()) {
    bar_views_.erase(iter);
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
  chip_menu_model_adapter_->set_close_menu_on_customizing_suggestions(true);
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
  for (auto& bar_view : bar_views_) {
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

void BirchBarController::ExecuteCommand(int command_id, int event_flags) {
  if (command_id ==
      base::to_underlying(BirchBarContextMenuModel::CommandId::kReset)) {
    bool suggestion_pref_changed = false;
    {
      // Holding the data fetch requests to avoid sending multiple requests.
      base::AutoReset<bool> hold_data_request(
          &hold_data_request_on_suggestion_pref_change_, true);
      for (const auto& pref_name :
           {prefs::kBirchUseWeather, prefs::kBirchUseCalendar,
            prefs::kBirchUseFileSuggest, prefs::kBirchUseRecentTabs}) {
        auto* pref_service = GetPrefService();
        suggestion_pref_changed |= !pref_service->GetBoolean(pref_name);
        pref_service->SetBoolean(pref_name, true);
      }
    }

    // If there are suggestion prefs being changed, call suggestion pref changed
    // callback.
    if (suggestion_pref_changed) {
      OnCustomizeSuggestionsPrefChanged();
    }
  }
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
        /*is_post_login=*/from_pine_service_,
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

  // Cache the new items in a temp variable to avoid dangling ptrs when update
  // the birch bar.
  auto items = Shell::Get()->birch_model()->GetItemsForDisplay();

  // Record an impression if there are suggestion chips to show.
  if (!items.empty()) {
    base::UmaHistogramBoolean("Ash.Birch.Bar.Impression", true);
  }
  // Record impressions for the suggestion chips.
  for (const auto& item : items) {
    base::UmaHistogramEnumeration("Ash.Birch.Chip.Impression", item->GetType());
  }
  // Record number of chips being shown.
  base::UmaHistogramCustomCounts("Ash.Birch.ChipCount", items.size(),
                                 /*min=*/0, /*exclusive_max=*/10,
                                 /*buckets=*/10);
  RecordTimeOfDayRankingHistogram(items);

  for (auto& bar_view : bar_views_) {
    InitBarWithItems(bar_view, items);
  }

  items_ = std::move(items);
}

void BirchBarController::InitBarWithItems(
    BirchBarView* bar_view,
    const std::vector<std::unique_ptr<BirchItem>>& items) {
  CHECK(!data_fetch_in_progress_);

  std::vector<raw_ptr<BirchItem>> items_to_show;
  for (auto& item : items) {
    if (items_to_show.size() == BirchBarView::kMaxChipsNum) {
      break;
    }
    items_to_show.emplace_back(item.get());
  }

  bar_view->SetupChips(items_to_show);
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

void BirchBarController::OnCustomizeSuggestionsPrefChanged() {
  if (hold_data_request_on_suggestion_pref_change_) {
    return;
  }

  for (auto& bar_view : bar_views_) {
    bar_view->Reloading();
  }

  MaybeFetchDataFromModel();
}

}  // namespace ash
