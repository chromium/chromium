// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/stub_birch_client.h"

#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ui/base/models/image_model.h"

namespace ash {

//------------------------------------------------------------------------------
// StubBirchClient::StubDataProvider
StubBirchClient::StubDataProvider::StubDataProvider() = default;

StubBirchClient::StubDataProvider::~StubDataProvider() = default;

void StubBirchClient::StubDataProvider::RunDataProviderChangedCallback() {
  NotifyDataProviderChanged();
}

void StubBirchClient::StubDataProvider::RequestBirchDataFetch() {
  did_request_birch_data_fetch_ = true;
}

//------------------------------------------------------------------------------
// StubBirchClient:
StubBirchClient::StubBirchClient()
    : calendar_provider_(std::make_unique<StubDataProvider>()),
      file_suggest_provider_(std::make_unique<StubDataProvider>()),
      recent_tabs_provider_(std::make_unique<StubDataProvider>()),
      last_active_provider_(std::make_unique<StubDataProvider>()),
      most_visited_provider_(std::make_unique<StubDataProvider>()),
      self_share_provider_(std::make_unique<StubDataProvider>()),
      lost_media_provider_(std::make_unique<StubDataProvider>()),
      release_notes_provider_(std::make_unique<StubDataProvider>()) {
  CHECK(test_dir_.CreateUniqueTempDir());
}

StubBirchClient::~StubBirchClient() = default;

StubBirchClient::StubDataProvider*
StubBirchClient::InstallStubWeatherDataProvider() {
  auto weather_provider = std::make_unique<StubDataProvider>();
  auto* weather_provider_ptr = weather_provider.get();
  Shell::Get()->birch_model()->OverrideWeatherProviderForTest(
      std::move(weather_provider));
  return weather_provider_ptr;
}

StubBirchClient::StubDataProvider*
StubBirchClient::InstallStubCoralDataProvider() {
  auto coral_provider = std::make_unique<StubDataProvider>();
  auto* coral_provider_ptr = coral_provider.get();
  Shell::Get()->birch_model()->OverrideCoralProviderForTest(
      std::move(coral_provider));
  return coral_provider_ptr;
}

bool StubBirchClient::DidRequestCalendarDataFetch() const {
  return calendar_provider_->did_request_birch_data_fetch();
}

bool StubBirchClient::DidRequestFileSuggestDataFetch() const {
  return file_suggest_provider_->did_request_birch_data_fetch();
}

bool StubBirchClient::DidRequestRecentTabsDataFetch() const {
  return recent_tabs_provider_->did_request_birch_data_fetch();
}

bool StubBirchClient::DidRequestLastActiveDataFetch() const {
  return last_active_provider_->did_request_birch_data_fetch();
}

bool StubBirchClient::DidRequestMostVisitedDataFetch() const {
  return most_visited_provider_->did_request_birch_data_fetch();
}

bool StubBirchClient::DidRequestSelfShareDataFetch() const {
  return self_share_provider_->did_request_birch_data_fetch();
}

bool StubBirchClient::DidRequestLostMediaDataFetch() const {
  return lost_media_provider_->did_request_birch_data_fetch();
}

bool StubBirchClient::DidRequestReleaseNotesDataFetch() const {
  return release_notes_provider_->did_request_birch_data_fetch();
}

BirchDataProvider* StubBirchClient::GetCalendarProvider() {
  return calendar_provider_.get();
}

BirchDataProvider* StubBirchClient::GetFileSuggestProvider() {
  return file_suggest_provider_.get();
}

BirchDataProvider* StubBirchClient::GetRecentTabsProvider() {
  return recent_tabs_provider_.get();
}

BirchDataProvider* StubBirchClient::GetLastActiveProvider() {
  return last_active_provider_.get();
}

BirchDataProvider* StubBirchClient::GetMostVisitedProvider() {
  return most_visited_provider_.get();
}

BirchDataProvider* StubBirchClient::GetSelfShareProvider() {
  return self_share_provider_.get();
}

BirchDataProvider* StubBirchClient::GetLostMediaProvider() {
  return lost_media_provider_.get();
}

BirchDataProvider* StubBirchClient::GetReleaseNotesProvider() {
  return release_notes_provider_.get();
}

void StubBirchClient::WaitForRefreshTokens(base::OnceClosure callback) {
  did_wait_for_refresh_tokens_ = true;
  std::move(callback).Run();
}

base::FilePath StubBirchClient::GetRemovedItemsFilePath() {
  return test_dir_.GetPath();
}

void StubBirchClient::RemoveFileItemFromLauncher(const base::FilePath& path) {
  last_removed_path_ = path;
}

void StubBirchClient::GetFaviconImage(
    const GURL& url,
    const bool is_page_url,
    base::OnceCallback<void(const ui::ImageModel&)> callback) {
  did_get_favicon_image_ = true;
  std::move(callback).Run(ui::ImageModel());
}

ui::ImageModel StubBirchClient::GetChromeBackupIcon() {
  return ui::ImageModel();
}

}  // namespace ash
