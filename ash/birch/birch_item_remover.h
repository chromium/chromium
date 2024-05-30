// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_ITEM_REMOVER_H_
#define ASH_BIRCH_BIRCH_ITEM_REMOVER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/removed_items.pb.h"
#include "ash/utility/persistent_proto.h"
#include "base/functional/callback_forward.h"

namespace ash {

class BirchCalendarItem;
class BirchItem;
class BirchTabItem;

// Manages a list of BirchItems which have been removed by the user. Removed
// items are recorded using a persistent proto. BirchItem lists can be filtered
// to erase any items that have been removed by the user. Removed items are
// written to disk immediately as a posted task with no delay and no queueing.
class ASH_EXPORT BirchItemRemover {
 public:
  // `path` is the file path which the persistent proto is written to.
  // `on_init_callback` will be called once the underlying pesrsistent proto has
  // completed initialization.
  explicit BirchItemRemover(base::FilePath path,
                            base::OnceClosure on_init_callback);
  BirchItemRemover(const BirchItemRemover&) = delete;
  BirchItemRemover& operator=(const BirchItemRemover&) = delete;
  ~BirchItemRemover();

  // Whether the underlying persistent proto has been initialized.
  bool Initialized();

  // Record the BirchItem to be removed persistently.
  void RemoveItem(BirchItem* item);

  // Erases from the BirchItem list any items which have been removed by the
  // user. The list is mutated in place.
  void FilterRemovedTabs(std::vector<BirchTabItem>* tab_items);
  void FilterRemovedLastActiveItems(std::vector<BirchLastActiveItem>* items);
  void FilterRemovedMostVisitedItems(std::vector<BirchMostVisitedItem>* items);
  void FilterRemovedSelfShareItems(
      std::vector<BirchSelfShareItem>* self_share_items);
  void FilterRemovedCalendarItems(
      std::vector<BirchCalendarItem>* calendar_items);
  void FilterRemovedAttachmentItems(
      std::vector<BirchAttachmentItem>* file_items);
  void FilterRemovedFileItems(std::vector<BirchFileItem>* file_items);

  void SetProtoInitCallbackForTest(base::OnceClosure callback);

 private:
  PersistentProto<RemovedItemsProto> removed_items_proto_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_ITEM_REMOVER_H_
