// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_manager.h"

#include <utility>

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "url/gurl.h"

using BookmarkNode = bookmarks::BookmarkNode;

constexpr char kReadStatusKey[] = "read_status";
constexpr char kReadStatusRead[] = "true";
constexpr char kReadStatusUnread[] = "false";

// Implementation of ReadingListManager.
// 1. Holds a in memory bookmark node tree. Contains a folder root and reading
// list nodes as children. Only has one level of children.
// 2. Talk to reading list model, and sync with the in memory bookmark tree.
// 3. TODO(xingliu): Add an observer and broadcast events to caller.
class ReadingListManagerImpl : public ReadingListManager,
                               public ReadingListModelObserver {
 public:
  explicit ReadingListManagerImpl(ReadingListModel* reading_list_model)
      : reading_list_model_(reading_list_model), maximum_id_(0L) {
    DCHECK(reading_list_model_);
    root_ = std::make_unique<BookmarkNode>(maximum_id_++, base::GenerateGUID(),
                                           GURL());
    DCHECK(root_->is_folder());
    reading_list_model_->AddObserver(this);
  }

  ~ReadingListManagerImpl() override {
    reading_list_model_->RemoveObserver(this);
  }

  // ReadingListModelObserver overrides.
  void ReadingListModelLoaded(const ReadingListModel* model) override {
    // Constructs the bookmark tree.
    root_->DeleteAll();
    for (const auto& url : model->Keys())
      AddBookmark(model->GetEntryByURL(url));
  }

  // ReadingListManager implementation.
  const BookmarkNode* Add(const GURL& url, const std::string& title) override {
    DCHECK(reading_list_model_->loaded());

    // Add or swap the reading list entry.
    const auto& new_entry = reading_list_model_->AddEntry(
        url, title, reading_list::ADDED_VIA_CURRENT_APP);
    return AddBookmark(&new_entry);
  }

  const BookmarkNode* Get(const GURL& url) override {
    DCHECK(reading_list_model_->loaded());
    return FindBookmarkByURL(url);
  }

  void Delete(const GURL& url) override {
    DCHECK(reading_list_model_->loaded());

    RemoveBookmark(url);
    reading_list_model_->RemoveEntryByURL(url);
  }

  const BookmarkNode* GetRoot() const override {
    DCHECK(reading_list_model_->loaded());
    return root_.get();
  }

  size_t size() const override {
    DCHECK(reading_list_model_->loaded());
    return reading_list_model_->size();
  }

  size_t unread_size() const override {
    DCHECK(reading_list_model_->loaded());
    return reading_list_model_->unread_size();
  }

  void SetReadStatus(const GURL& url, bool read) override {
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

 private:
  // Finds the child in the bookmark tree by URL. Returns nullptr if not found.
  // Not recursive since the reading list bookmark tree only has a folder root
  // node and one level of children.
  BookmarkNode* FindBookmarkByURL(const GURL& url) {
    for (const auto& child : root_->children()) {
      if (url == child->url())
        return child.get();
    }
    return nullptr;
  }

  // Removes a reading list bookmark node by |url|.
  void RemoveBookmark(const GURL& url) {
    const BookmarkNode* node = FindBookmarkByURL(url);
    if (node)
      root_->Remove(root_->GetIndexOf(node));
  }

  // Adds a reading list entry to the bookmark tree.
  const BookmarkNode* AddBookmark(const ReadingListEntry* entry) {
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

  // Sync the bookmark node with |entry|. Returns whether the conversion is
  // succeeded.
  static bool SyncToBookmark(const ReadingListEntry& entry,
                             BookmarkNode* bookmark) {
    DCHECK(bookmark);
    base::string16 title;
    if (!base::UTF8ToUTF16(entry.Title().c_str(), entry.Title().size(),
                           &title)) {
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

  // Contains reading list data, outlives this class.
  ReadingListModel* reading_list_model_;

  // The bookmark root for reading list articles.
  std::unique_ptr<BookmarkNode> root_;

  // Auto increment bookmark id. Will not be persisted and only used in memory.
  int64_t maximum_id_;
};

// static
std::unique_ptr<ReadingListManager> ReadingListManager::Create(
    ReadingListModel* reading_list_model) {
  return std::make_unique<ReadingListManagerImpl>(reading_list_model);
}
