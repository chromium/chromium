// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"

#include <utility>

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/reading_list/core/reading_list_model.h"
#include "url/gurl.h"

using BookmarkNode = bookmarks::BookmarkNode;

constexpr char kReadStatusKey[] = "read_status";
constexpr char kReadStatusRead[] = "true";
constexpr char kReadStatusUnread[] = "false";

namespace {

// Sync the bookmark node with |entry|. Returns whether the conversion is
// succeeded.
bool SyncToBookmark(const ReadingListEntry& entry, BookmarkNode* bookmark) {
  DCHECK(bookmark);
  base::string16 title;
  if (!base::UTF8ToUTF16(entry.Title().c_str(), entry.Title().size(), &title)) {
    LOG(ERROR) << "Failed to convert the title to string16.";
    return false;
  }

  bookmark->set_url(entry.URL());
  bookmark->set_date_added(base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(entry.CreationTime())));
  bookmark->SetTitle(title);
  bookmark->SetMetaInfo(kReadStatusKey,
                        entry.IsRead() ? kReadStatusRead : kReadStatusUnread);
  return true;
}

}  // namespace

ReadingListManagerImpl::ReadingListManagerImpl(
    ReadingListModel* reading_list_model)
    : reading_list_model_(reading_list_model), maximum_id_(0L) {
  DCHECK(reading_list_model_);
  root_ = std::make_unique<BookmarkNode>(maximum_id_++, base::GenerateGUID(),
                                         GURL());
  DCHECK(root_->is_folder());
  reading_list_model_->AddObserver(this);
}

ReadingListManagerImpl::~ReadingListManagerImpl() {
  reading_list_model_->RemoveObserver(this);
}

void ReadingListManagerImpl::ReadingListModelLoaded(
    const ReadingListModel* model) {
  // Constructs the bookmark tree.
  root_->DeleteAll();
  for (const auto& url : model->Keys())
    AddBookmark(model->GetEntryByURL(url));
}

const BookmarkNode* ReadingListManagerImpl::Add(const GURL& url,
                                                const std::string& title) {
  DCHECK(reading_list_model_->loaded());

  // Add or swap the reading list entry.
  const auto& new_entry = reading_list_model_->AddEntry(
      url, title, reading_list::ADDED_VIA_CURRENT_APP);
  return AddBookmark(&new_entry);
}

const BookmarkNode* ReadingListManagerImpl::Get(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  return FindBookmarkByURL(url);
}

void ReadingListManagerImpl::Delete(const GURL& url) {
  DCHECK(reading_list_model_->loaded());

  RemoveBookmark(url);
  reading_list_model_->RemoveEntryByURL(url);
}

const BookmarkNode* ReadingListManagerImpl::GetRoot() const {
  DCHECK(reading_list_model_->loaded());
  return root_.get();
}

size_t ReadingListManagerImpl::size() const {
  DCHECK(reading_list_model_->loaded());
  return reading_list_model_->size();
}

size_t ReadingListManagerImpl::unread_size() const {
  DCHECK(reading_list_model_->loaded());
  return reading_list_model_->unread_size();
}

void ReadingListManagerImpl::SetReadStatus(const GURL& url, bool read) {
  DCHECK(reading_list_model_->loaded());
  const auto* entry = reading_list_model_->GetEntryByURL(url);
  if (!entry)
    return;

  reading_list_model_->SetReadStatus(url, read);
  auto* node = FindBookmarkByURL(url);
  if (node) {
    node->SetMetaInfo(kReadStatusKey,
                      read ? kReadStatusRead : kReadStatusUnread);
  }
}

BookmarkNode* ReadingListManagerImpl::FindBookmarkByURL(const GURL& url) {
  for (const auto& child : root_->children()) {
    if (url == child->url())
      return child.get();
  }
  return nullptr;
}

// Removes a reading list bookmark node by |url|.
void ReadingListManagerImpl::RemoveBookmark(const GURL& url) {
  const BookmarkNode* node = FindBookmarkByURL(url);
  if (node)
    root_->Remove(root_->GetIndexOf(node));
}

// Adds a reading list entry to the bookmark tree.
const BookmarkNode* ReadingListManagerImpl::AddBookmark(
    const ReadingListEntry* entry) {
  if (!entry)
    return nullptr;

  // Update the existing bookmark node if possible.
  BookmarkNode* node = FindBookmarkByURL(entry->URL());
  if (node) {
    bool success = SyncToBookmark(*entry, node);
    return success ? node : nullptr;
  }

  // Add a new node.
  auto new_node = std::make_unique<BookmarkNode>(
      maximum_id_++, base::GenerateGUID(), entry->URL());
  bool success = SyncToBookmark(*entry, new_node.get());
  return success ? root_->Add(std::move(new_node)) : nullptr;
}
