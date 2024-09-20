// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/coral_item_remover.h"

namespace ash {

CoralItemRemover::CoralItemRemover() = default;

CoralItemRemover::~CoralItemRemover() = default;

void CoralItemRemover::RemoveItem(const coral_util::ContentItem& item) {
  removed_content_items_.insert(coral_util::GetIdentifier(item));
}

void CoralItemRemover::FilterRemovedItems(
    std::vector<coral_util::ContentItem>* items) {
  std::erase_if(*items, [this](const coral_util::ContentItem& item) {
    return removed_content_items_.contains(coral_util::GetIdentifier(item));
  });
}

}  // namespace ash
