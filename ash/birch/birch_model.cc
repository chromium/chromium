// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_model.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_ranker.h"
#include "ash/birch/birch_weather_provider.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr base::TimeDelta kDataFetchPostLoginTimeoutInMs =
    base::Milliseconds(3000);
constexpr base::TimeDelta kDataFetchTimeoutInMs = base::Milliseconds(1000);

// Returns the pref service to use for Birch prefs.
PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

}  // namespace

BirchModel::PendingRequest::PendingRequest() = default;

BirchModel::PendingRequest::~PendingRequest() = default;

BirchModel::BirchModel() {
  if (features::IsBirchWeatherEnabled()) {
    weather_provider_ = std::make_unique<BirchWeatherProvider>(this);
  }
  Shell::Get()->session_controller()->AddObserver(this);
  SimpleGeolocationProvider::GetInstance()->AddObserver(this);
}

BirchModel::~BirchModel() {
  SimpleGeolocationProvider::GetInstance()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void BirchModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BirchModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void BirchModel::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kBirchUseCalendar, true);
  registry->RegisterBooleanPref(prefs::kBirchUseFileSuggest, true);
  registry->RegisterBooleanPref(prefs::kBirchUseRecentTabs, true);
  registry->RegisterBooleanPref(prefs::kBirchUseWeather, true);
  registry->RegisterBooleanPref(prefs::kBirchUseReleaseNotes, true);
}

void BirchModel::SetCalendarItems(
    const std::vector<BirchCalendarItem>& calendar_items) {
  if (is_fetching_calendar_) {
    base::UmaHistogramCounts100("Ash.Birch.ResultsReturned.Calendar",
                                calendar_items.size());
    base::UmaHistogramTimes("Ash.Birch.Latency.Calendar",
                            GetNow() - fetch_start_time_);
    is_fetching_calendar_ = false;
  }
  if (calendar_items != calendar_items_) {
    calendar_items_ = std::move(calendar_items);
  }
  is_calendar_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetAttachmentItems(
    const std::vector<BirchAttachmentItem>& attachment_items) {
  if (is_fetching_attachment_) {
    base::UmaHistogramCounts100("Ash.Birch.ResultsReturned.Attachment",
                                attachment_items.size());
    // There is no separate latency measurement for attachments because they
    // come from the calendar provider.
    is_fetching_attachment_ = false;
  }
  if (attachment_items != attachment_items_) {
    attachment_items_ = std::move(attachment_items);
  }
  is_attachment_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetFileSuggestItems(
    const std::vector<BirchFileItem>& file_suggest_items) {
  if (is_fetching_file_suggest_) {
    base::UmaHistogramCounts100("Ash.Birch.ResultsReturned.File",
                                file_suggest_items.size());
    base::UmaHistogramTimes("Ash.Birch.Latency.File",
                            GetNow() - fetch_start_time_);
    is_fetching_file_suggest_ = false;
  }
  if (file_suggest_items_ != file_suggest_items) {
    file_suggest_items_ = std::move(file_suggest_items);
  }
  is_files_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetRecentTabItems(
    const std::vector<BirchTabItem>& recent_tab_items) {
  if (is_fetching_recent_tab_) {
    base::UmaHistogramCounts100("Ash.Birch.ResultsReturned.Tab",
                                recent_tab_items.size());
    base::UmaHistogramTimes("Ash.Birch.Latency.Tab",
                            GetNow() - fetch_start_time_);
    is_fetching_recent_tab_ = false;
  }
  if (recent_tab_items_ != recent_tab_items) {
    recent_tab_items_ = std::move(recent_tab_items);
  }
  is_tabs_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetWeatherItems(
    const std::vector<BirchWeatherItem>& weather_items) {
  if (is_fetching_weather_) {
    base::UmaHistogramCounts100("Ash.Birch.ResultsReturned.Weather",
                                weather_items.size());
    base::UmaHistogramTimes("Ash.Birch.Latency.Weather",
                            GetNow() - fetch_start_time_);
    is_fetching_weather_ = false;
  }
  if (weather_items_ != weather_items) {
    weather_items_ = std::move(weather_items);
  }
  is_weather_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetReleaseNotesItems(
    const std::vector<BirchReleaseNotesItem>& release_notes_items) {
  if (is_fetching_release_notes_) {
    base::UmaHistogramCounts100("Ash.Birch.ResultsReturned.ReleaseNotes",
                                release_notes_items.size());
    base::UmaHistogramTimes("Ash.Birch.Latency.ReleaseNotes",
                            GetNow() - fetch_start_time_);
    is_fetching_release_notes_ = false;
  }
  if (release_notes_items != release_notes_items_) {
    release_notes_items_ = std::move(release_notes_items);
  }
  is_release_notes_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetClientAndInit(BirchClient* client) {
  birch_client_ = client;

  if (birch_client_) {
    // `BirchItemRemover` calls `MaybeRespondToDataFetchRequest` once it
    // has completed initializing, this way any data fetch requests which have
    // completed can be responded to.
    item_remover_ = std::make_unique<BirchItemRemover>(
        birch_client_->GetRemovedItemsFilePath(),
        /*on_init_callback=*/base::BindOnce(
            &BirchModel::MaybeRespondToDataFetchRequest,
            base::Unretained(this)));
    for (auto& observer : observers_) {
      observer.OnBirchClientSet();
    }
  } else {
    item_remover_.reset();
  }
}

void BirchModel::RequestBirchDataFetch(bool is_post_login,
                                       base::OnceClosure callback) {
  if (!Shell::Get()->session_controller()->IsUserPrimary()) {
    // Fetches are only supported for the primary user. Return with empty data.
    ClearAllItems();
    std::move(callback).Run();
    return;
  }

  PrefService* prefs = GetPrefService();
  if (!prefs) {
    std::move(callback).Run();
    return;
  }

  const bool fetch_in_progress = !pending_requests_.empty();

  size_t request_id = next_request_id_++;

  PendingRequest& request = pending_requests_[request_id];
  request.callback = std::move(callback);
  request.timer = std::make_unique<base::OneShotTimer>();
  request.timer->Start(
      FROM_HERE,
      is_post_login ? kDataFetchPostLoginTimeoutInMs : kDataFetchTimeoutInMs,
      base::BindOnce(&BirchModel::HandleRequestTimeout, base::Unretained(this),
                     request_id));

  if (fetch_in_progress) {
    return;
  }

  is_post_login_fetch_ = is_post_login;
  fetch_start_time_ = GetNow();

  bool did_fetch = false;
  if (birch_client_) {
    if (prefs->GetBoolean(prefs::kBirchUseCalendar)) {
      is_calendar_data_fresh_ = false;
      is_attachment_data_fresh_ = false;  // Attachments use the same provider.
      is_fetching_calendar_ = true;
      is_fetching_attachment_ = true;
      birch_client_->GetCalendarProvider()->RequestBirchDataFetch();
      did_fetch = true;
    }
    if (prefs->GetBoolean(prefs::kBirchUseFileSuggest)) {
      is_files_data_fresh_ = false;
      is_fetching_file_suggest_ = true;
      birch_client_->GetFileSuggestProvider()->RequestBirchDataFetch();
      did_fetch = true;
    }
    if (prefs->GetBoolean(prefs::kBirchUseRecentTabs)) {
      is_tabs_data_fresh_ = false;
      is_fetching_recent_tab_ = true;
      birch_client_->GetRecentTabsProvider()->RequestBirchDataFetch();
      did_fetch = true;
    }
    if (prefs->GetBoolean(prefs::kBirchUseReleaseNotes)) {
      is_release_notes_data_fresh_ = false;
      is_fetching_release_notes_ = true;
      birch_client_->GetReleaseNotesProvider()->RequestBirchDataFetch();
      did_fetch = true;
    }
  }
  if (weather_provider_ && prefs->GetBoolean(prefs::kBirchUseWeather)) {
    is_weather_data_fresh_ = false;
    is_fetching_weather_ = true;
    weather_provider_->RequestBirchDataFetch();
    did_fetch = true;
  }

  // If we didn't actually fetch, respond immediately.
  if (!did_fetch) {
    std::move(request.callback).Run();
    pending_requests_.erase(request_id);
  }
}

std::vector<std::unique_ptr<BirchItem>> BirchModel::GetAllItems() {
  if (!IsItemRemoverInitialized()) {
    // With no initialized item remover, return an empty list of items to avoid
    // returning items previously removed by the user.
    return {};
  }

  item_remover_->FilterRemovedTabs(&recent_tab_items_);
  item_remover_->FilterRemovedCalendarItems(&calendar_items_);
  item_remover_->FilterRemovedAttachmentItems(&attachment_items_);
  item_remover_->FilterRemovedFileItems(&file_suggest_items_);

  BirchRanker ranker(GetNow());
  ranker.RankCalendarItems(&calendar_items_);
  ranker.RankAttachmentItems(&attachment_items_);
  ranker.RankFileSuggestItems(&file_suggest_items_);
  ranker.RankRecentTabItems(&recent_tab_items_);
  ranker.RankWeatherItems(&weather_items_);
  ranker.RankReleaseNotesItems(&release_notes_items_);

  // Avoid showing a duplicate file which is both an attachment and file
  // suggestion by erasing the item with the higher ranking.
  std::unordered_map<std::string, BirchAttachmentItem>
      file_id_to_attachment_item;
  for (auto& attachment : attachment_items_) {
    file_id_to_attachment_item.emplace(attachment.file_id(), attachment);
  }
  std::erase_if(file_suggest_items_, [&file_id_to_attachment_item](
                                         const auto& file_suggest_item) {
    if (file_id_to_attachment_item.contains(file_suggest_item.file_id())) {
      if (file_suggest_item.ranking() >
          file_id_to_attachment_item.at(file_suggest_item.file_id())
              .ranking()) {
        // Duplicate item with a higher ranked file suggest item. Erase the file
        // suggest item.
        return true;
      }
      // Duplicate item with a higher ranked attachment item. Erase the
      // attachment item.
      file_id_to_attachment_item.erase(file_suggest_item.file_id());
    }
    return false;
  });

  std::vector<std::unique_ptr<BirchItem>> all_items;
  for (auto& event : calendar_items_) {
    all_items.push_back(std::make_unique<BirchCalendarItem>(event));
  }
  for (auto& event : file_id_to_attachment_item) {
    all_items.push_back(std::make_unique<BirchAttachmentItem>(event.second));
  }
  for (auto& tab : recent_tab_items_) {
    all_items.push_back(std::make_unique<BirchTabItem>(tab));
  }
  for (auto& file_suggestion : file_suggest_items_) {
    all_items.push_back(std::make_unique<BirchFileItem>(file_suggestion));
  }
  for (auto& weather_item : weather_items_) {
    all_items.push_back(std::make_unique<BirchWeatherItem>(weather_item));
  }
  for (auto& release_notes_item : release_notes_items_) {
    all_items.push_back(
        std::make_unique<BirchReleaseNotesItem>(release_notes_item));
  }
  // Sort items by ranking.
  std::sort(all_items.begin(), all_items.end(),
            [](const auto& item_a, const auto& item_b) {
              return item_a->ranking() < item_b->ranking();
            });

  return all_items;
}

std::vector<std::unique_ptr<BirchItem>> BirchModel::GetItemsForDisplay() {
  std::vector<std::unique_ptr<BirchItem>> results = GetAllItems();

  // Remove any items with no ranking, as these should not be shown.
  std::erase_if(results, [](const auto& item) {
    return item->ranking() == std::numeric_limits<float>::max();
  });

  return results;
}

bool BirchModel::IsDataFresh() {
  PrefService* prefs = GetPrefService();
  if (!prefs) {
    return false;
  }
  // Data types are considered fresh if their prefs are disabled, since a
  // disabled pref means the data type won't be fetched.
  bool calendar_fresh =
      is_calendar_data_fresh_ || !prefs->GetBoolean(prefs::kBirchUseCalendar);
  // Calendar attachments use the same provider as calendar events.
  bool attachments_fresh =
      is_attachment_data_fresh_ || !prefs->GetBoolean(prefs::kBirchUseCalendar);
  bool file_suggest_fresh =
      is_files_data_fresh_ || !prefs->GetBoolean(prefs::kBirchUseFileSuggest);
  bool recent_tabs_fresh =
      is_tabs_data_fresh_ || !prefs->GetBoolean(prefs::kBirchUseRecentTabs);
  bool release_notes_fresh = is_release_notes_data_fresh_ ||
                             !prefs->GetBoolean(prefs::kBirchUseReleaseNotes);
  bool is_birch_client_fresh =
      !birch_client_ ||
      (calendar_fresh && attachments_fresh && file_suggest_fresh &&
       recent_tabs_fresh && release_notes_fresh);

  // Use the same logic for weather.
  bool is_weather_fresh = !weather_provider_ || is_weather_data_fresh_ ||
                          !prefs->GetBoolean(prefs::kBirchUseWeather);
  return is_birch_client_fresh && is_weather_fresh;
}

void BirchModel::RemoveItem(BirchItem* item) {
  if (!IsItemRemoverInitialized()) {
    return;
  }
  // Record that the user hid a chip, with the type of the chip.
  base::UmaHistogramEnumeration("Ash.Birch.Chip.Hidden", item->GetType());

  item_remover_->RemoveItem(item);
}

void BirchModel::OnActiveUserSessionChanged(const AccountId& account_id) {
  if (!has_active_user_session_changed_) {
    // This is the initial notification on signin.
    has_active_user_session_changed_ = true;
    InitPrefChangeRegistrars();
    RecordProviderHiddenHistograms();
    return;
  }

  // On multi-profile switch, first cancel any pending requests.
  pending_requests_.clear();

  // Clear the existing data and mark the data as not fresh.
  ClearAllItems();
  MarkDataNotFresh();
}

void BirchModel::OnGeolocationPermissionChanged(bool enabled) {
  // If geolocation permission is disabled, remove any cached weather data.
  if (!enabled) {
    weather_items_.clear();
    is_weather_data_fresh_ = false;
  }
}

void BirchModel::OverrideWeatherProviderForTest(
    std::unique_ptr<BirchDataProvider> weather_provider) {
  CHECK(weather_provider_);
  weather_provider_ = std::move(weather_provider);
}

void BirchModel::OverrideClockForTest(base::Clock* clock) {
  clock_override_ = clock;
}

void BirchModel::HandleRequestTimeout(size_t request_id) {
  auto request = pending_requests_.find(request_id);
  if (request == pending_requests_.end()) {
    return;
  }

  base::OnceClosure callback = std::move(request->second.callback);
  pending_requests_.erase(request);
  std::move(callback).Run();
}

void BirchModel::MaybeRespondToDataFetchRequest() {
  if (!IsDataFresh() || !IsItemRemoverInitialized()) {
    return;
  }

  // Was this a real fetch being completed (rather than a provider supplying
  // data outside of a fetch)?
  bool was_fetch = !pending_requests_.empty();
  if (was_fetch) {
    // All data providers have replied, so compute total latency.
    base::TimeDelta latency = GetNow() - fetch_start_time_;
    if (is_post_login_fetch_) {
      base::UmaHistogramTimes("Ash.Birch.TotalLatencyPostLogin", latency);
    } else {
      base::UmaHistogramTimes("Ash.Birch.TotalLatency", latency);
    }
  }

  std::vector<base::OnceClosure> callbacks;
  for (auto& request : pending_requests_) {
    callbacks.push_back(std::move(request.second.callback));
  }
  pending_requests_.clear();

  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

base::Time BirchModel::GetNow() const {
  if (clock_override_) {
    return clock_override_->Now();
  }
  return base::Time::Now();
}

void BirchModel::ClearAllItems() {
  calendar_items_.clear();
  attachment_items_.clear();
  file_suggest_items_.clear();
  recent_tab_items_.clear();
  weather_items_.clear();
  release_notes_items_.clear();
}

void BirchModel::MarkDataNotFresh() {
  is_calendar_data_fresh_ = false;
  is_attachment_data_fresh_ = false;
  is_files_data_fresh_ = false;
  is_tabs_data_fresh_ = false;
  is_weather_data_fresh_ = false;
  is_release_notes_data_fresh_ = false;
}

void BirchModel::InitPrefChangeRegistrars() {
  PrefService* prefs = GetPrefService();
  calendar_pref_registrar_.Init(prefs);
  calendar_pref_registrar_.Add(
      prefs::kBirchUseCalendar,
      base::BindRepeating(&BirchModel::OnCalendarPrefChanged,
                          base::Unretained(this)));
  file_suggest_pref_registrar_.Init(prefs);
  file_suggest_pref_registrar_.Add(
      prefs::kBirchUseFileSuggest,
      base::BindRepeating(&BirchModel::OnFileSuggestPrefChanged,
                          base::Unretained(this)));
  recent_tab_pref_registrar_.Init(prefs);
  recent_tab_pref_registrar_.Add(
      prefs::kBirchUseRecentTabs,
      base::BindRepeating(&BirchModel::OnRecentTabPrefChanged,
                          base::Unretained(this)));
  weather_pref_registrar_.Init(prefs);
  weather_pref_registrar_.Add(
      prefs::kBirchUseWeather,
      base::BindRepeating(&BirchModel::OnWeatherPrefChanged,
                          base::Unretained(this)));
  release_notes_pref_registrar_.Init(prefs);
  release_notes_pref_registrar_.Add(
      prefs::kBirchUseReleaseNotes,
      base::BindRepeating(&BirchModel::OnReleaseNotesPrefChanged,
                          base::Unretained(this)));
}

void BirchModel::OnCalendarPrefChanged() {
  PrefService* prefs = GetPrefService();
  if (!prefs->GetBoolean(prefs::kBirchUseCalendar)) {
    calendar_items_.clear();
    attachment_items_.clear();  // Attachments come from the same provider.
  } else {
    is_calendar_data_fresh_ = false;
    is_attachment_data_fresh_ = false;
  }
}

void BirchModel::OnFileSuggestPrefChanged() {
  PrefService* prefs = GetPrefService();
  if (!prefs->GetBoolean(prefs::kBirchUseFileSuggest)) {
    file_suggest_items_.clear();
  } else {
    is_files_data_fresh_ = false;
  }
}

void BirchModel::OnRecentTabPrefChanged() {
  PrefService* prefs = GetPrefService();
  if (!prefs->GetBoolean(prefs::kBirchUseRecentTabs)) {
    recent_tab_items_.clear();
  } else {
    is_tabs_data_fresh_ = false;
  }
}

void BirchModel::OnWeatherPrefChanged() {
  PrefService* prefs = GetPrefService();
  if (!prefs->GetBoolean(prefs::kBirchUseWeather)) {
    weather_items_.clear();
  } else {
    is_weather_data_fresh_ = false;
  }
}

void BirchModel::OnReleaseNotesPrefChanged() {
  PrefService* prefs = GetPrefService();
  if (!prefs->GetBoolean(prefs::kBirchUseReleaseNotes)) {
    release_notes_items_.clear();
  } else {
    is_release_notes_data_fresh_ = false;
  }
}

void BirchModel::RecordProviderHiddenHistograms() {
  PrefService* prefs = GetPrefService();
  base::UmaHistogramBoolean("Ash.Birch.ProviderHidden.Calendar",
                            !prefs->GetBoolean(prefs::kBirchUseCalendar));
  base::UmaHistogramBoolean("Ash.Birch.ProviderHidden.FileSuggest",
                            !prefs->GetBoolean(prefs::kBirchUseFileSuggest));
  base::UmaHistogramBoolean("Ash.Birch.ProviderHidden.RecentTabs",
                            !prefs->GetBoolean(prefs::kBirchUseRecentTabs));
  base::UmaHistogramBoolean("Ash.Birch.ProviderHidden.Weather",
                            !prefs->GetBoolean(prefs::kBirchUseWeather));
  base::UmaHistogramBoolean("Ash.Birch.ProviderHidden.ReleaseNotes",
                            !prefs->GetBoolean(prefs::kBirchUseReleaseNotes));
}

bool BirchModel::IsItemRemoverInitialized() {
  return item_remover_ && item_remover_->Initialized();
}

}  // namespace ash
