// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_model.h"

namespace ash {

BirchModel::BirchModel() = default;

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

}  // namespace ash
