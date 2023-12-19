// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_model.h"

namespace ash {

BirchItem::BirchItem(const std::string& title) : title_(title) {}

BirchItem::~BirchItem() = default;

BirchModel::BirchModel() = default;

BirchModel::~BirchModel() = default;

void BirchModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BirchModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BirchModel::AddItem(std::unique_ptr<BirchItem> item) {
  items_.push_back(std::move(item));

  for (auto& observer : observers_) {
    observer.OnItemsChanged();
  }
}

}  // namespace ash
