// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_controller.h"

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/overview/birch/birch_bar_constants.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "ash/wm/overview/birch/birch_bar_menu_model_adapter.h"
#include "ash/wm/overview/birch/birch_bar_util.h"
#include "ash/wm/overview/birch/birch_bar_view.h"
#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"
#include "ash/wm/overview/birch/birch_privacy_nudge_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/views/controls/menu/menu_controller.h"

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

std::string GetPrefNameFromSuggestionType(BirchSuggestionType type) {
  switch (type) {
    case BirchSuggestionType::kWeather:
      return prefs::kBirchUseWeather;
    case BirchSuggestionType::kCalendar:
      return prefs::kBirchUseCalendar;
    case BirchSuggestionType::kDrive:
      return prefs::kBirchUseFileSuggest;
    case BirchSuggestionType::kChromeTab:
      return prefs::kBirchUseChromeTabs;
    case BirchSuggestionType::kMedia:
      return prefs::kBirchUseLostMedia;
    case BirchSuggestionType::kCoral:
      return prefs::kBirchUseCoral;
    case BirchSuggestionType::kExplore:
    case BirchSuggestionType::kUndefined:
      NOTREACHED();
  }
}

}  // namespace

BirchBarController::BirchBarController(bool is_informed_restore)
    : is_informed_restore_(is_informed_restore) {
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
        prefs::kBirchUseFileSuggest, prefs::kBirchUseChromeTabs,
        prefs::kBirchUseLostMedia, prefs::kBirchUseReleaseNotes,
        prefs::kBirchUseCoral}) {
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
    bar_view->ShutdownChips();
  }

  // Chips are shutdown, stop observing lost media change if we did.
  OnLostMediaItemRemoved();
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
  if (!IsDataLoading()) {
    InitBarWithItems(bar_view, items_);
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
                                             BirchSuggestionType chip_type,
                                             const gfx::Point& point,
                                             ui::MenuSourceType source_type) {
  chip_menu_model_adapter_ = std::make_unique<BirchBarMenuModelAdapter>(
      std::make_unique<BirchChipContextMenuModel>(
          /*delegate=*/chip, chip_type),
      chip->GetWidget(), source_type,
      base::BindOnce(&BirchBarController::OnChipContextMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      Shell::Get()->IsInTabletMode(), /*for_chip_menu=*/true);
  BirchPrivacyNudgeController::DidShowContextMenu();
  chip_menu_model_adapter_->Run(gfx::Rect(point, gfx::Size()),
                                views::MenuAnchorPosition::kBubbleTopRight,
                                views::MenuRunner::CONTEXT_MENU |
                                    views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                                    views::MenuRunner::FIXED_ANCHOR);
}

void BirchBarController::OnItemHiddenByUser(BirchItem* item) {
  // Do not remove the item if the bars are animating.
  if (std::ranges::any_of(bar_views_, [](BirchBarView* bar_view) {
        return bar_view->IsAnimating();
      })) {
    return;
  }

  RemoveItemChips(item);

  // Erase the item from model and controller.
  Shell::Get()->birch_model()->RemoveItem(item);
  if (item->GetType() == BirchItemType::kLostMedia) {
    OnLostMediaItemRemoved();
  }
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

void BirchBarController::SetShowSuggestionType(BirchSuggestionType type,
                                               bool show) {
  customize_suggestions_pref_registrar_.prefs()->SetBoolean(
      GetPrefNameFromSuggestionType(type), show);
}

bool BirchBarController::GetShowSuggestionType(BirchSuggestionType type) const {
  return customize_suggestions_pref_registrar_.prefs()->GetBoolean(
      GetPrefNameFromSuggestionType(type));
}

bool BirchBarController::IsDataLoading() const {
  return birch_model_observer_.IsObserving() || data_fetch_in_progress_;
}

void BirchBarController::ToggleTemperatureUnits() {
  // Toggle the preference.
  auto* pref_service = GetPrefService();
  bool current_value = pref_service->GetBoolean(prefs::kBirchUseCelsius);
  pref_service->SetBoolean(prefs::kBirchUseCelsius, !current_value);

  // Refresh the suggestion chips.
  for (auto& bar_view : bar_views_) {
    bar_view->SetState(BirchBarView::State::kReloading);
  }
  MaybeFetchDataFromModel();
}

void BirchBarController::ExecuteMenuCommand(int command_id, bool from_chip) {
  using CommandId = BirchBarContextMenuModel::CommandId;
  switch (command_id) {
    case base::to_underlying(CommandId::kShowSuggestions):
      // Note that the menu should be dismissed before changing the show
      // suggestions pref which may destroy the chips.
      if (auto* menu_controller = views::MenuController::GetActiveInstance()) {
        menu_controller->Cancel(views::MenuController::ExitType::kAll);
      } else if (from_chip) {
        // When tapping on the "Show suggestions" switch button, the menu
        // controller may be destroyed before executing the command. To avoid
        // UAF, reset the menu model adapter.
        chip_menu_model_adapter_.reset();
      }

      SetShowBirchSuggestions(/*show=*/!GetShowBirchSuggestions());
      break;
    case base::to_underlying(CommandId::kWeatherSuggestions):
    case base::to_underlying(CommandId::kCalendarSuggestions):
    case base::to_underlying(CommandId::kDriveSuggestions):
    case base::to_underlying(CommandId::kChromeTabSuggestions):
    case base::to_underlying(CommandId::kMediaSuggestions):
    case base::to_underlying(CommandId::kCoralSuggestions): {
      // To avoid UAF, dismiss the menu before changing the pref which
      // would destroy current chips.
      auto* menu_controller = views::MenuController::GetActiveInstance();
      if (from_chip && menu_controller) {
        menu_controller->Cancel(views::MenuController::ExitType::kAll);
      }

      const BirchSuggestionType suggestion_type =
          birch_bar_util::CommandIdToSuggestionType(command_id);
      const bool current_show_status = GetShowSuggestionType(suggestion_type);
      SetShowSuggestionType(suggestion_type, !current_show_status);
      break;
    }
    case base::to_underlying(CommandId::kReset): {
      bool suggestion_pref_changed = false;
      {
        // Holding the data fetch requests to avoid sending multiple requests.
        base::AutoReset<bool> hold_data_request(
            &hold_data_request_on_suggestion_pref_change_, true);
        auto* pref_service = GetPrefService();
        for (const auto& pref_name :
             {prefs::kBirchUseWeather, prefs::kBirchUseCalendar,
              prefs::kBirchUseFileSuggest, prefs::kBirchUseChromeTabs,
              prefs::kBirchUseLostMedia, prefs::kBirchUseCoral}) {
          suggestion_pref_changed |= !pref_service->GetBoolean(pref_name);
          pref_service->SetBoolean(pref_name, true);
        }
      }

      // If there are suggestion prefs being changed, call suggestion pref
      // changed callback.
      if (suggestion_pref_changed) {
        OnCustomizeSuggestionsPrefChanged();
      }
      break;
    }
    default:
      break;
  }
}

void BirchBarController::ExecuteCommand(int command_id, int event_flags) {
  ExecuteMenuCommand(command_id, /*from_chip=*/false);
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
        /*is_post_login=*/is_informed_restore_,
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

  // Clear the old lost media item if it exists.
  OnLostMediaItemRemoved();

  items_ = std::move(items);

  for (const auto& item : items_) {
    if (item->GetType() == BirchItemType::kLostMedia) {
      OnLostMediaItemReceived();
    }
  }
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

void BirchBarController::RemoveItemChips(BirchItem* item) {
  // Remove the item from birch bars. If there is an extra item not showing in
  // the bars, push it in the bars.
  BirchItem* extra_item = items_.size() > BirchBarView::kMaxChipsNum
                              ? items_[BirchBarView::kMaxChipsNum].get()
                              : nullptr;
  for (auto& bar_view : bar_views_) {
    bar_view->RemoveChip(item, extra_item);
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

  auto* overview_session = GetOverviewSession();
  for (auto& root : Shell::GetAllRootWindows()) {
    auto* overview_grid = overview_session->GetGridWithRootWindow(root);
    if (show) {
      MaybeFetchDataFromModel();
      overview_grid->MaybeInitBirchBarWidget(/*by_user=*/true);
    } else {
      overview_grid->ShutdownBirchBarWidgetByUser();
    }
  }
}

void BirchBarController::OnCustomizeSuggestionsPrefChanged() {
  if (hold_data_request_on_suggestion_pref_change_) {
    return;
  }

  for (auto& bar_view : bar_views_) {
    bar_view->SetState(BirchBarView::State::kReloading);
  }

  MaybeFetchDataFromModel();
}

void BirchBarController::OnLostMediaItemReceived() {
  Shell::Get()->birch_model()->SetLostMediaDataChangedCallback(
      base::BindRepeating(&BirchBarController::OnLostMediaItemUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void BirchBarController::OnLostMediaItemRemoved() {
  if (auto* birch_model = Shell::Get()->birch_model()) {
    birch_model->ResetLostMediaDataChangedCallback();
  }
}

void BirchBarController::OnLostMediaItemUpdated(
    std::unique_ptr<BirchItem> updated_item) {
  auto lost_media_item =
      std::find_if(items_.begin(), items_.end(), [](const auto& item) {
        return item->GetType() == BirchItemType::kLostMedia;
      });
  if (lost_media_item == items_.end()) {
    return;
  }

  // If the lost media item is null, remove the lost media chip from bars.
  // Otherwise, update the chip with new item contents.
  if (!updated_item) {
    RemoveItemChips(lost_media_item->get());
    items_.erase(lost_media_item);
    OnLostMediaItemRemoved();
  } else {
    *(lost_media_item->get()) = *(updated_item.get());
    for (auto bar_view : bar_views_) {
      bar_view->UpdateChip(lost_media_item->get());
    }
  }
}

}  // namespace ash
