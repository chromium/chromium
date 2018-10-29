// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_result.h"

#include <map>

#include "ash/app_list/model/search/search_result_observer.h"
#include "ash/public/cpp/app_list/tokenized_string.h"
#include "ash/public/cpp/app_list/tokenized_string_match.h"
#include "ui/base/models/menu_model.h"

namespace app_list {

SearchResult::SearchResult()
    : metadata_(ash::mojom::SearchResultMetadata::New()) {}

SearchResult::~SearchResult() {
  for (auto& observer : observers_)
    observer.OnResultDestroying();
}

void SearchResult::SetMetadata(ash::mojom::SearchResultMetadataPtr metadata) {
  metadata_ = std::move(metadata);
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetIcon(const gfx::ImageSkia& icon) {
  metadata_->icon = icon;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetChipIcon(const gfx::ImageSkia& chip_icon) {
  metadata_->chip_icon = chip_icon;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::set_title(const base::string16& title) {
  metadata_->title = title;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetBadgeIcon(const gfx::ImageSkia& badge_icon) {
  metadata_->badge_icon = badge_icon;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetRating(float rating) {
  metadata_->rating = rating;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetFormattedPrice(const base::string16& formatted_price) {
  metadata_->formatted_price = formatted_price;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetActions(const Actions& sets) {
  metadata_->actions = sets;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetIsInstalling(bool is_installing) {
  if (is_installing_ == is_installing)
    return;

  is_installing_ = is_installing;
  for (auto& observer : observers_)
    observer.OnIsInstallingChanged();
}

void SearchResult::SetPercentDownloaded(int percent_downloaded) {
  if (percent_downloaded_ == percent_downloaded)
    return;

  percent_downloaded_ = percent_downloaded;
  for (auto& observer : observers_)
    observer.OnPercentDownloadedChanged();
}

void SearchResult::NotifyItemInstalled() {
  for (auto& observer : observers_)
    observer.OnItemInstalled();
}

void SearchResult::AddObserver(SearchResultObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchResult::RemoveObserver(SearchResultObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SearchResult::Open(int event_flags) {}

void SearchResult::InvokeAction(int action_index, int event_flags) {}

}  // namespace app_list
