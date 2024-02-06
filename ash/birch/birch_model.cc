// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_model.h"

#include "ash/birch/birch_weather_provider.h"
#include "ash/constants/ash_features.h"

namespace ash {

BirchModel::BirchModel() {
  if (features::IsBirchWeatherEnabled()) {
    weather_provider_ = std::make_unique<BirchWeatherProvider>(this);
  }
}

BirchModel::~BirchModel() = default;

void BirchModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BirchModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BirchModel::SetFileSuggestItems(
    std::vector<BirchFileItem> file_suggest_items) {
  // Return early if there are no changes to the file suggest items.
  if (file_suggest_items == file_suggest_items_) {
    return;
  }

  file_suggest_items_ = std::move(file_suggest_items);

  for (auto& observer : observers_) {
    observer.OnItemsChanged();
  }
}

void BirchModel::SetRecentTabItems(std::vector<BirchTabItem> recent_tab_items) {
  // Return early if there are no changes to the tab items.
  if (recent_tab_items == recent_tab_items_) {
    return;
  }

  recent_tab_items_ = std::move(recent_tab_items);
}

void BirchModel::SetWeatherItems(std::vector<BirchWeatherItem> weather_items) {
  if (weather_items == weather_items_) {
    return;
  }
  weather_items_ = std::move(weather_items);

  for (auto& observer : observers_) {
    observer.OnItemsChanged();
  }
}

void BirchModel::RequestBirchDataFetch() {
  // TODO(b/305094143): Call this before we begin showing birch views.
  if (birch_client_) {
    birch_client_->RequestBirchDataFetch();
  }
  if (weather_provider_) {
    weather_provider_->RequestDataFetch();
  }
}

}  // namespace ash
