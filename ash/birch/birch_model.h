// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_MODEL_H_
#define ASH_BIRCH_BIRCH_MODEL_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_item.h"
#include "base/observer_list.h"

namespace ash {

// Birch model, which is used to aggregate and store relevant information from
// different providers.
class ASH_EXPORT BirchModel {
 public:
  // Birch Model Observers are notified when the list of items in the model
  // has changed.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // TODO(b/305094143): Add a list of items to the model and call this
    // function when the list of items changes.
    virtual void OnItemsChanged() = 0;
  };

  BirchModel();
  BirchModel(const BirchModel&) = delete;
  BirchModel& operator=(const BirchModel&) = delete;
  ~BirchModel();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetFileSuggestItems(std::vector<BirchFileItem> file_suggest_items);

  const std::vector<BirchFileItem>& GetFileSuggestItemsForTest() const {
    return file_suggest_items_;
  }

 private:
  // A type-specific list of items for all file suggestion items.
  std::vector<BirchFileItem> file_suggest_items_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_MODEL_H_
