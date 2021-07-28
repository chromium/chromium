// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_result.h"

#include <map>
#include <utility>

#include "ash/app_list/model/search/search_result_observer.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"

namespace ash {

SearchResult::SearchResult()
    : metadata_(std::make_unique<SearchResultMetadata>()) {}

SearchResult::~SearchResult() {
  for (auto& observer : observers_)
    observer.OnResultDestroying();
}

void SearchResult::SetMetadata(std::unique_ptr<SearchResultMetadata> metadata) {
  metadata_ = std::move(metadata);
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetIcon(const IconInfo& icon) {
  metadata_->icon = icon;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

size_t SearchResult::IconDimension() const {
  return metadata_->icon.dimension.value_or(
      SharedAppListConfig::instance().search_list_icon_dimension());
}

void SearchResult::SetChipIcon(const gfx::ImageSkia& chip_icon) {
  metadata_->chip_icon = chip_icon;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::set_title(const std::u16string& title) {
  metadata_->title = title;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetBadgeIcon(const ui::ImageModel& badge_icon) {
  metadata_->badge_icon = badge_icon;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetRating(float rating) {
  metadata_->rating = rating;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetFormattedPrice(const std::u16string& formatted_price) {
  metadata_->formatted_price = formatted_price;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetActions(const Actions& sets) {
  metadata_->actions = sets;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::AddObserver(SearchResultObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchResult::RemoveObserver(SearchResultObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SearchResult::Open(int event_flags) {}

void SearchResult::InvokeAction(int action_index, int event_flags) {}

}  // namespace ash
