// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_MODEL_H_
#define ASH_BIRCH_BIRCH_MODEL_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// The base item which is stored by the birch model.
struct ASH_EXPORT BirchItem {
  explicit BirchItem(const std::string& title);
  BirchItem(BirchItem&&);
  BirchItem(const BirchItem&);
  BirchItem& operator=(const BirchItem&);
  ~BirchItem();

  std::string title_;
};

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

  void AddItem(std::unique_ptr<BirchItem> item);

 private:
  std::vector<std::unique_ptr<BirchItem>> items_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_MODEL_H_
