// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_CORAL_ITEM_REMOVER_H_
#define ASH_BIRCH_CORAL_ITEM_REMOVER_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/public/cpp/coral_util.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"

namespace ash {

// Manages a list of ContentItems which have been removed by the user. Removed
// items are stored for the current session only. ContentItem lists can be
// filtered to erase any items that have been removed by the user.
class ASH_EXPORT CoralItemRemover {
 public:
  CoralItemRemover();
  CoralItemRemover(const CoralItemRemover&) = delete;
  CoralItemRemover& operator=(const CoralItemRemover&) = delete;
  ~CoralItemRemover();

  // Records the coral::mojom::EntityKey to be removed for the current session.
  void RemoveItem(const coral::mojom::EntityKey& key);

  // Records the coral::mojom::EntityKeyPtr to be removed for the current
  // session.
  void RemoveItem(const coral::mojom::EntityKeyPtr& key);

  // Records the coral::mojom::Entity to be removed for the current session.
  void RemoveItem(const coral::mojom::Entity& item);

  // Erases from the ContentItem list any items which have been removed by the
  // user. The list is mutated in place.
  void FilterRemovedItems(std::vector<coral::mojom::EntityPtr>* content_items);

  const std::set<std::string>& RemovedContentItemsForTest() const {
    return removed_content_items_;
  }

 private:
  // Stores the unique identifier for content items that should be filtered for
  //  the rest of the current user session.
  std::set<std::string> removed_content_items_;
};

}  // namespace ash

#endif  // ASH_BIRCH_CORAL_ITEM_REMOVER_H_
