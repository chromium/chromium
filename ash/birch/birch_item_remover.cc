// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item_remover.h"

#include <string>
#include <vector>

#include "ash/birch/birch_item.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/time/time.h"

namespace ash {

BirchItemRemover::BirchItemRemover(base::FilePath path,
                                   base::OnceClosure on_init_callback)
    : removed_items_proto_(path, /*write_delay=*/base::TimeDelta()) {
  removed_items_proto_.RegisterOnInitUnsafe(std::move(on_init_callback));
  removed_items_proto_.Init();
}

BirchItemRemover::~BirchItemRemover() = default;

bool BirchItemRemover::Initialized() {
  return removed_items_proto_.initialized();
}

void BirchItemRemover::RemoveItem(BirchItem* item) {
  CHECK(removed_items_proto_.initialized());
  if (item->GetType() == BirchItemType::kTab) {
    BirchTabItem* tab_item = static_cast<BirchTabItem*>(item);
    const std::string hashed_url = base::SHA1HashString(tab_item->url().spec());

    // Add the hashed url to the `removed_tab_items` map.
    // Note: We are using a map for its set capabilities; the map value is
    // arbitrary.
    removed_items_proto_->mutable_removed_tab_items()->insert(
        {hashed_url, false});
    removed_items_proto_.StartWrite();
    return;
  }
  if (item->GetType() == BirchItemType::kCalendar) {
    BirchCalendarItem* calendar_item = static_cast<BirchCalendarItem*>(item);
    const std::string hashed_event_id =
        base::SHA1HashString(calendar_item->event_id());

    // Add the hashed event id to the `removed_calendar_items` map.
    // Note: We are using a map for its set capabilities; the map value is
    // arbitrary.
    removed_items_proto_->mutable_removed_calendar_items()->insert(
        {hashed_event_id, false});
    removed_items_proto_.StartWrite();
    return;
  }
  if (item->GetType() == BirchItemType::kAttachment ||
      item->GetType() == BirchItemType::kFile) {
    std::string hashed_file_id;
    if (item->GetType() == BirchItemType::kAttachment) {
      hashed_file_id = base::SHA1HashString(
          static_cast<BirchAttachmentItem*>(item)->file_id());
    } else {
      hashed_file_id =
          base::SHA1HashString(static_cast<BirchFileItem*>(item)->file_id());
    }

    // Add the hashed file id to the `removed_file_items` map.
    // Note: We are using a map for its set capabilities; the map value is
    // arbitrary.
    removed_items_proto_->mutable_removed_file_items()->insert(
        {hashed_file_id, false});
    removed_items_proto_.StartWrite();
    return;
  }
}

void BirchItemRemover::FilterRemovedTabs(std::vector<BirchTabItem>* tab_items) {
  CHECK(removed_items_proto_.initialized());
  std::erase_if(*tab_items, [this](const BirchTabItem& item) {
    const std::string hashed_url = base::SHA1HashString(item.url().spec());
    return removed_items_proto_->removed_tab_items().contains(hashed_url);
  });
}

void BirchItemRemover::FilterRemovedCalendarItems(
    std::vector<BirchCalendarItem>* calendar_items) {
  CHECK(removed_items_proto_.initialized());
  std::erase_if(*calendar_items, [this](const BirchCalendarItem& item) {
    const std::string hashed_id = base::SHA1HashString(item.event_id());
    return removed_items_proto_->removed_calendar_items().contains(hashed_id);
  });
}

void BirchItemRemover::FilterRemovedAttachmentItems(
    std::vector<BirchAttachmentItem>* attachment_items) {
  CHECK(removed_items_proto_.initialized());
  std::erase_if(*attachment_items, [this](const BirchAttachmentItem& item) {
    const std::string hashed_id = base::SHA1HashString(item.file_id());
    return removed_items_proto_->removed_file_items().contains(hashed_id);
  });
}

void BirchItemRemover::FilterRemovedFileItems(
    std::vector<BirchFileItem>* file_items) {
  CHECK(removed_items_proto_.initialized());
  std::erase_if(*file_items, [this](const BirchFileItem& item) {
    const std::string hashed_id = base::SHA1HashString(item.file_id());
    return removed_items_proto_->removed_file_items().contains(hashed_id);
  });
}

void BirchItemRemover::SetProtoInitCallbackForTest(base::OnceClosure callback) {
  removed_items_proto_.RegisterOnInitUnsafe(std::move(callback));
}

}  // namespace ash
