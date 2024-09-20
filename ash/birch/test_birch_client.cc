// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/test_birch_client.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_service.h"
#include "ui/base/models/image_model.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// TestBirchDataProvider:
template <typename T>
TestBirchDataProvider<T>::TestBirchDataProvider(
    DataFetchedCallback data_fetched_callback,
    const std::string& pref_name)
    : data_fetched_callback_(data_fetched_callback), pref_name_(pref_name) {}

template <typename T>
TestBirchDataProvider<T>::~TestBirchDataProvider() = default;

template <typename T>
void TestBirchDataProvider<T>::ClearItems() {
  items_.clear();
}

template <typename T>
void TestBirchDataProvider<T>::RunDataProviderChangedCallback() {
  NotifyDataProviderChanged();
}

template <typename T>
void TestBirchDataProvider<T>::RequestBirchDataFetch() {
  data_fetched_callback_.Run(
      (pref_name_.empty() || Shell::Get()
                                 ->session_controller()
                                 ->GetPrimaryUserPrefService()
                                 ->GetBoolean(pref_name_))
          ? items_
          : std::vector<T>());
}

// Do the same for any type that needs to be used outside of this file.
template class TestBirchDataProvider<BirchLostMediaItem>;
template class TestBirchDataProvider<BirchWeatherItem>;
template class TestBirchDataProvider<BirchCoralItem>;

////////////////////////////////////////////////////////////////////////////////
// TestBirchClient:
TestBirchClient::TestBirchClient(BirchModel* birch_model) {
  calendar_provider_ =
      std::make_unique<TestBirchDataProvider<BirchCalendarItem>>(
          base::BindRepeating(&TestBirchClient::HandleCalendarFetch,
                              base::Unretained(this)),
          prefs::kBirchUseCalendar);
  file_provider_ = std::make_unique<TestBirchDataProvider<BirchFileItem>>(
      base::BindRepeating(&BirchModel::SetFileSuggestItems,
                          base::Unretained(birch_model)),
      prefs::kBirchUseFileSuggest);
  tab_provider_ = std::make_unique<TestBirchDataProvider<BirchTabItem>>(
      base::BindRepeating(&BirchModel::SetRecentTabItems,
                          base::Unretained(birch_model)),
      prefs::kBirchUseChromeTabs);
  last_active_provider_ =
      std::make_unique<TestBirchDataProvider<BirchLastActiveItem>>(
          base::BindRepeating(&BirchModel::SetLastActiveItems,
                              base::Unretained(birch_model)),
          prefs::kBirchUseChromeTabs);
  most_visited_provider_ =
      std::make_unique<TestBirchDataProvider<BirchMostVisitedItem>>(
          base::BindRepeating(&BirchModel::SetMostVisitedItems,
                              base::Unretained(birch_model)),
          prefs::kBirchUseChromeTabs);
  self_share_provider_ =
      std::make_unique<TestBirchDataProvider<BirchSelfShareItem>>(
          base::BindRepeating(&BirchModel::SetSelfShareItems,
                              base::Unretained(birch_model)),
          prefs::kBirchUseChromeTabs);
  lost_media_provider_ =
      std::make_unique<TestBirchDataProvider<BirchLostMediaItem>>(
          base::BindRepeating(&BirchModel::SetLostMediaItems,
                              base::Unretained(birch_model)),
          prefs::kBirchUseLostMedia);
  release_notes_provider_ =
      std::make_unique<TestBirchDataProvider<BirchReleaseNotesItem>>(
          base::BindRepeating(&BirchModel::SetReleaseNotesItems,
                              base::Unretained(birch_model)),
          std::string());
  CHECK(test_dir_.CreateUniqueTempDir());
}

TestBirchClient::~TestBirchClient() = default;

void TestBirchClient::SetCalendarItems(
    const std::vector<BirchCalendarItem>& items) {
  calendar_provider_->set_items(items);
}

void TestBirchClient::SetFileSuggestItems(
    const std::vector<BirchFileItem>& items) {
  file_provider_->set_items(items);
}

void TestBirchClient::SetRecentTabsItems(
    const std::vector<BirchTabItem>& items) {
  tab_provider_->set_items(items);
}

void TestBirchClient::SetLastActiveItems(
    const std::vector<BirchLastActiveItem>& items) {
  last_active_provider_->set_items(items);
}

void TestBirchClient::SetMostVisitedItems(
    const std::vector<BirchMostVisitedItem>& items) {
  most_visited_provider_->set_items(items);
}

void TestBirchClient::SetReleaseNotesItems(
    const std::vector<BirchReleaseNotesItem>& items) {
  release_notes_provider_->set_items(items);
}

void TestBirchClient::SetSelfShareItems(
    const std::vector<BirchSelfShareItem>& items) {
  self_share_provider_->set_items(items);
}

void TestBirchClient::SetLostMediaItems(
    const std::vector<BirchLostMediaItem>& items) {
  lost_media_provider_->set_items(items);
}

void TestBirchClient::Reset() {
  calendar_provider_->ClearItems();
  file_provider_->ClearItems();
  tab_provider_->ClearItems();
  last_active_provider_->ClearItems();
  release_notes_provider_->ClearItems();
  self_share_provider_->ClearItems();
  lost_media_provider_->ClearItems();
}

BirchDataProvider* TestBirchClient::GetCalendarProvider() {
  return calendar_provider_.get();
}

BirchDataProvider* TestBirchClient::GetFileSuggestProvider() {
  return file_provider_.get();
}

BirchDataProvider* TestBirchClient::GetRecentTabsProvider() {
  return tab_provider_.get();
}

BirchDataProvider* TestBirchClient::GetLastActiveProvider() {
  return last_active_provider_.get();
}

BirchDataProvider* TestBirchClient::GetMostVisitedProvider() {
  return most_visited_provider_.get();
}

BirchDataProvider* TestBirchClient::GetSelfShareProvider() {
  return self_share_provider_.get();
}

BirchDataProvider* TestBirchClient::GetLostMediaProvider() {
  return lost_media_provider_.get();
}

BirchDataProvider* TestBirchClient::GetReleaseNotesProvider() {
  return release_notes_provider_.get();
}

void TestBirchClient::WaitForRefreshTokens(base::OnceClosure callback) {
  std::move(callback).Run();
}

base::FilePath TestBirchClient::GetRemovedItemsFilePath() {
  return test_dir_.GetPath();
}

ui::ImageModel TestBirchClient::GetChromeBackupIcon() {
  return ui::ImageModel();
}

void TestBirchClient::HandleCalendarFetch(
    const std::vector<BirchCalendarItem>& items) {
  // The production calendar provider sets both calendar items and attachment
  // items. Set both so the fetch can complete.
  Shell::Get()->birch_model()->SetCalendarItems(items);
  Shell::Get()->birch_model()->SetAttachmentItems({});
}

}  // namespace ash
