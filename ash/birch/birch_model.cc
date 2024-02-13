// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_model.h"

#include <memory>
#include <vector>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_weather_provider.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr int kDataFetchTimeoutInMs = 1000;

}  // namespace

BirchModel::PendingRequest::PendingRequest() = default;

BirchModel::PendingRequest::~PendingRequest() = default;

BirchModel::BirchModel() {
  if (features::IsBirchWeatherEnabled()) {
    weather_provider_ = std::make_unique<BirchWeatherProvider>(this);
  }
}

BirchModel::~BirchModel() = default;

void BirchModel::SetCalendarItems(
    std::vector<BirchCalendarItem> calendar_items) {
  if (calendar_items != calendar_items_) {
    calendar_items_ = std::move(calendar_items);
  }
  is_calendar_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetFileSuggestItems(
    std::vector<BirchFileItem> file_suggest_items) {
  if (file_suggest_items_ != file_suggest_items) {
    file_suggest_items_ = std::move(file_suggest_items);
  }
  is_files_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetRecentTabItems(std::vector<BirchTabItem> recent_tab_items) {
  if (recent_tab_items_ != recent_tab_items) {
    recent_tab_items_ = std::move(recent_tab_items);
  }
  is_tabs_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::SetWeatherItems(std::vector<BirchWeatherItem> weather_items) {
  if (weather_items_ != weather_items) {
    weather_items_ = std::move(weather_items);
  }
  is_weather_data_fresh_ = true;
  MaybeRespondToDataFetchRequest();
}

void BirchModel::RequestBirchDataFetch(base::OnceClosure callback) {
  const bool fetch_in_progress = !pending_requests_.empty();

  size_t request_id = next_request_id_++;

  PendingRequest& request = pending_requests_[request_id];
  request.callback = std::move(callback);
  request.timer = std::make_unique<base::OneShotTimer>();
  request.timer->Start(FROM_HERE, base::Milliseconds(kDataFetchTimeoutInMs),
                       base::BindOnce(&BirchModel::HandleRequestTimeout,
                                      base::Unretained(this), request_id));

  if (fetch_in_progress) {
    return;
  }

  is_calendar_data_fresh_ = false;
  is_files_data_fresh_ = false;
  is_tabs_data_fresh_ = false;
  is_weather_data_fresh_ = false;

  // TODO(b/305094143): Call this before we begin showing birch views.
  if (birch_client_) {
    birch_client_->RequestBirchDataFetch();
  }
  if (weather_provider_) {
    weather_provider_->RequestBirchDataFetch();
  }
}

std::vector<std::unique_ptr<BirchItem>> BirchModel::GetAllItems() const {
  std::vector<std::unique_ptr<BirchItem>> all_items;

  // TODO(b/305094126): Sort items by priority.
  for (auto& event : calendar_items_) {
    all_items.push_back(std::make_unique<BirchCalendarItem>(event));
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

  return all_items;
}

bool BirchModel::IsDataFresh() {
  // TODO(jamescook): Include calendar freshness.
  return (!birch_client_ || (is_files_data_fresh_ && is_tabs_data_fresh_)) &&
         (!weather_provider_ || is_weather_data_fresh_);
}

void BirchModel::OverrideWeatherProviderForTest(
    std::unique_ptr<BirchClient> weather_provider) {
  CHECK(weather_provider_);
  weather_provider_ = std::move(weather_provider);
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
  if (!IsDataFresh()) {
    return;
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

}  // namespace ash
