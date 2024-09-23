// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item_remover.h"

#include <string>
#include <vector>

#include "ash/birch/birch_item.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

namespace ash {

BirchItemRemover::BirchItemRemover(base::FilePath path,
                                   base::OnceClosure on_init_callback)
    : removed_items_proto_(path,
                           /*write_delay=*/base::TimeDelta(),
                           base::TaskPriority::USER_VISIBLE) {
  removed_items_proto_.RegisterOnInitUnsafe(std::move(on_init_callback));
  removed_items_proto_.Init();
}

BirchItemRemover::~BirchItemRemover() = default;

bool BirchItemRemover::Initialized() {
  return removed_items_proto_.initialized();
}

void BirchItemRemover::RemoveItem(BirchItem* item) {
  CHECK(removed_items_proto_.initialized());

  // Note: We are using a map for removed items for its set capabilities; the
  // map value is arbitrary.
  auto hash_and_insert = [&](const std::string& identifier,
                             google::protobuf::Map<std::string, bool>* map) {
    const std::string hashed_identifier = base::SHA1HashString(identifier);
    map->insert({hashed_identifier, false});
    removed_items_proto_.StartWrite();
  };

  switch (item->GetType()) {
    case BirchItemType::kTab: {
      hash_and_insert(static_cast<BirchTabItem*>(item)->url().spec(),
                      removed_items_proto_->mutable_removed_tab_items());
      return;
    };
    case BirchItemType::kLastActive: {
      hash_and_insert(
          static_cast<BirchLastActiveItem*>(item)->page_url().spec(),
          removed_items_proto_->mutable_removed_tab_items());
      return;
    }
    case BirchItemType::kMostVisited: {
      hash_and_insert(
          static_cast<BirchMostVisitedItem*>(item)->page_url().spec(),
          removed_items_proto_->mutable_removed_tab_items());
      return;
    }
    case BirchItemType::kSelfShare: {
      hash_and_insert(static_cast<BirchSelfShareItem*>(item)->url().spec(),
                      removed_items_proto_->mutable_removed_tab_items());
      return;
    }
    case BirchItemType::kCalendar: {
      hash_and_insert(static_cast<BirchCalendarItem*>(item)->event_id(),
                      removed_items_proto_->mutable_removed_calendar_items());
      return;
    }
    case BirchItemType::kAttachment: {
      hash_and_insert(static_cast<BirchAttachmentItem*>(item)->file_id(),
                      removed_items_proto_->mutable_removed_file_items());
      return;
    }
    case BirchItemType::kFile: {
      hash_and_insert(static_cast<BirchFileItem*>(item)->file_id(),
                      removed_items_proto_->mutable_removed_file_items());
      return;
    }
    case ash::BirchItemType::kCoral: {
      // TODO(yulunwu): implement coral birch item removal once defined.
      return;
    }
    case ash::BirchItemType::kReleaseNotes:
    case ash::BirchItemType::kWeather:
    case ash::BirchItemType::kLostMedia:
    case ash::BirchItemType::kTest: {
      NOTREACHED();
    }
  }
}

void BirchItemRemover::FilterRemovedTabs(std::vector<BirchTabItem>* tab_items) {
  CHECK(removed_items_proto_.initialized());
  std::erase_if(*tab_items, [this](const BirchTabItem& item) {
    const std::string hashed_url = base::SHA1HashString(item.url().spec());
    return removed_items_proto_->removed_tab_items().contains(hashed_url);
  });
}

void BirchItemRemover::FilterRemovedLastActiveItems(
    std::vector<BirchLastActiveItem>* items) {
  CHECK(removed_items_proto_.initialized());
  std::erase_if(*items, [this](const BirchLastActiveItem& item) {
    const std::string hashed_url = base::SHA1HashString(item.page_url().spec());
    return removed_items_proto_->removed_tab_items().contains(hashed_url);
  });
}

void BirchItemRemover::FilterRemovedMostVisitedItems(
    std::vector<BirchMostVisitedItem>* items) {
  CHECK(removed_items_proto_.initialized());
  std::erase_if(*items, [this](const BirchMostVisitedItem& item) {
    const std::string hashed_url = base::SHA1HashString(item.page_url().spec());
    return removed_items_proto_->removed_tab_items().contains(hashed_url);
  });
}

void BirchItemRemover::FilterRemovedSelfShareItems(
    std::vector<BirchSelfShareItem>* self_share_items) {
  CHECK(removed_items_proto_.initialized());
  std::erase_if(*self_share_items, [this](const BirchSelfShareItem& item) {
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
