// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/coral_item_remover.h"

#include "ash/birch/coral_util.h"

namespace ash {

CoralItemRemover::CoralItemRemover() = default;

CoralItemRemover::~CoralItemRemover() = default;

void CoralItemRemover::RemoveItem(const coral::mojom::EntityPtr& item) {
  RemoveItem(coral_util::GetIdentifier(item));
}

void CoralItemRemover::RemoveItem(const coral::mojom::Entity& item) {
  RemoveItem(coral_util::GetIdentifier(item));
}

void CoralItemRemover::RemoveItem(const std::string& item_identifier) {
  removed_content_items_.insert(item_identifier);
}

void CoralItemRemover::FilterRemovedItems(
    std::vector<coral::mojom::EntityPtr>* items) {
  std::erase_if(*items, [this](const coral::mojom::EntityPtr& item) {
    return removed_content_items_.contains(coral_util::GetIdentifier(*item));
  });
}

}  // namespace ash
