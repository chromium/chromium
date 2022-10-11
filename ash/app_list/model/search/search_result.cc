// Copyright 2012 The Chromium Authors
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

void SearchResult::SetChipIcon(const gfx::ImageSkia& chip_icon) {
  metadata_->chip_icon = chip_icon;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetTitle(const std::u16string& title) {
  metadata_->title = title;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetTitleTags(const Tags& tags) {
  metadata_->title_tags = tags;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetTitleTextVector(const TextVector& vector) {
  metadata_->title_vector = vector;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetMultilineTitle(bool multiline_title) {
  DCHECK(metadata_->title_vector.size() <= 1 || !multiline_title);
  metadata_->multiline_title = multiline_title;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetDetails(const std::u16string& details) {
  metadata_->details = details;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetDetailsTags(const Tags& tags) {
  metadata_->details_tags = tags;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetDetailsTextVector(const TextVector& vector) {
  DCHECK(vector.size() <= 1 || !metadata_->multiline_details);
  metadata_->details_vector = vector;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetMultilineDetails(bool multiline_details) {
  DCHECK(metadata_->details_vector.size() <= 1 || !multiline_details);
  metadata_->multiline_details = multiline_details;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetBigTitleTextVector(const TextVector& vector) {
  metadata_->big_title_vector = vector;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetBigTitleSuperscriptTextVector(const TextVector& vector) {
  metadata_->big_title_superscript_vector = vector;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetKeyboardShortcutTextVector(const TextVector& vector) {
  metadata_->keyboard_shortcut_vector = vector;
  for (auto& observer : observers_)
    observer.OnMetadataChanged();
}

void SearchResult::SetAccessibleName(const std::u16string& name) {
  metadata_->accessible_name = name;
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
