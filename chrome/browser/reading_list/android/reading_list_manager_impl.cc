// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ui/base/l10n/l10n_util.h"
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
  std::u16string title;
  if (!base::UTF8ToUTF16(entry.Title().c_str(), entry.Title().size(), &title)) {
    LOG(ERROR) << "Failed to convert the following title to string16:"
               << entry.Title();
    return false;
  }

  bookmark->set_url(entry.URL());
  bookmark->set_date_added(base::Time::UnixEpoch() +
                           base::Microseconds(entry.CreationTime()));
  bookmark->SetTitle(title);
  bookmark->SetMetaInfo(kReadStatusKey,
                        entry.IsRead() ? kReadStatusRead : kReadStatusUnread);
  return true;
}

}  // namespace

ReadingListManagerImpl::ReadingListManagerImpl(
    ReadingListModel* reading_list_model,
    const IdGenerationFunction& id_gen_func)
    : reading_list_model_(reading_list_model),
      id_gen_func_(id_gen_func),
      loaded_(false),
      performing_batch_update_(false),
      changes_applied_during_batch_(false) {
  DCHECK(reading_list_model_);
  root_ = std::make_unique<BookmarkNode>(
      id_gen_func_.Run(), base::Uuid::GenerateRandomV4(), GURL());
  root_->SetTitle(l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE));
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
  for (const auto& url : model->GetKeys()) {
    AddOrUpdateBookmark(model->GetEntryByURL(url).get());
  }

  loaded_ = true;

  for (Observer& observer : observers_)
    observer.ReadingListLoaded();
}

void ReadingListManagerImpl::ReadingListDidAddEntry(
    const ReadingListModel* model,
    const GURL& url,
    reading_list::EntrySource source) {
  AddOrUpdateBookmark(model->GetEntryByURL(url).get());
}

void ReadingListManagerImpl::ReadingListWillRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  RemoveBookmark(url);
}

void ReadingListManagerImpl::ReadingListDidMoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  scoped_refptr<const ReadingListEntry> moved_entry =
      reading_list_model_->GetEntryByURL(url);
  DCHECK(moved_entry);
  AddOrUpdateBookmark(moved_entry.get());
}

void ReadingListManagerImpl::ReadingListDidUpdateEntry(
    const ReadingListModel* model,
    const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  scoped_refptr<const ReadingListEntry> updated_entry =
      reading_list_model_->GetEntryByURL(url);
  DCHECK(updated_entry);
  AddOrUpdateBookmark(updated_entry.get());
}

void ReadingListManagerImpl::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  // Ignores ReadingListDidApplyChanges() invocations during batch update.
  if (performing_batch_update_) {
    changes_applied_during_batch_ = true;
    return;
  }

  NotifyReadingListChanged();
}

void ReadingListManagerImpl::ReadingListModelBeganBatchUpdates(
    const ReadingListModel* model) {
  DCHECK(!changes_applied_during_batch_);
  performing_batch_update_ = true;
}

void ReadingListManagerImpl::ReadingListModelCompletedBatchUpdates(
    const ReadingListModel* model) {
  // Batch update is done -- notify the observers only once, but only if there
  // were actual changes.
  if (changes_applied_during_batch_) {
    NotifyReadingListChanged();
  }

  performing_batch_update_ = false;
  changes_applied_during_batch_ = false;
}

void ReadingListManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ReadingListManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const BookmarkNode* ReadingListManagerImpl::Add(const GURL& url,
                                                const std::string& title) {
  DCHECK(reading_list_model_->loaded());
  if (!reading_list_model_->IsUrlSupported(url))
    return nullptr;

  // Add or swap the reading list entry.
  const auto& new_entry = reading_list_model_->AddOrReplaceEntry(
      url, title, reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  const auto* node = FindBookmarkByURL(new_entry.URL());
  return node;
}

const BookmarkNode* ReadingListManagerImpl::Get(const GURL& url) const {
  DCHECK(reading_list_model_->loaded());
  return FindBookmarkByURL(url);
}

const BookmarkNode* ReadingListManagerImpl::GetNodeByID(int64_t id) const {
  if (root_->id() == id)
    return root_.get();

  for (const auto& child : root_->children()) {
    if (child->id() == id)
      return child.get();
  }

  return nullptr;
}

void ReadingListManagerImpl::GetMatchingNodes(
    const bookmarks::QueryFields& query,
    size_t max_count,
    std::vector<const BookmarkNode*>* results) {
  if (results->size() >= max_count)
    return;

  auto query_words = bookmarks::ParseBookmarkQuery(query);
  if (query_words.empty())
    return;

  for (const auto& node : root_->children()) {
    if (bookmarks::DoesBookmarkContainWords(node->GetTitle(), node->url(),
                                            query_words)) {
      results->push_back(node.get());
      if (results->size() == max_count)
        break;
    }
  }
}

bool ReadingListManagerImpl::IsReadingListBookmark(
    const BookmarkNode* node) const {
  if (!node)
    return false;

  // Not recursive since there is only one level of children.
  return (root_.get() == node) || (node->parent() == root_.get());
}

void ReadingListManagerImpl::Delete(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  reading_list_model_->RemoveEntryByURL(url, FROM_HERE);
}

void ReadingListManagerImpl::DeleteAll() {
  DCHECK(reading_list_model_->loaded());
  reading_list_model_->DeleteAllEntries(FROM_HERE);
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

void ReadingListManagerImpl::SetTitle(const GURL& url,
                                      const std::u16string& title) {
  DCHECK(reading_list_model_->loaded());
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  if (!entry)
    return;

  std::string str_title;
  if (!base::UTF16ToUTF8(title.c_str(), title.size(), &str_title)) {
    LOG(ERROR) << "Failed to convert the following title to string16:" << title;
    return;
  }
  reading_list_model_->SetEntryTitleIfExists(url, str_title);
}

void ReadingListManagerImpl::SetReadStatus(const GURL& url, bool read) {
  DCHECK(reading_list_model_->loaded());
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  if (!entry)
    return;

  reading_list_model_->SetReadStatusIfExists(url, read);
  auto* node = FindBookmarkByURL(url);
  if (node) {
    node->SetMetaInfo(kReadStatusKey,
                      read ? kReadStatusRead : kReadStatusUnread);
  }
}

bool ReadingListManagerImpl::GetReadStatus(
    const bookmarks::BookmarkNode* node) {
  if (node == root_.get())
    return false;

  std::string value;
  node->GetMetaInfo(kReadStatusKey, &value);

  if (value == kReadStatusRead)
    return true;
  if (value == kReadStatusUnread)
    return false;

  NOTREACHED_IN_MIGRATION() << "May not be reading list node.";
  return false;
}

bool ReadingListManagerImpl::IsLoaded() const {
  return loaded_;
}

BookmarkNode* ReadingListManagerImpl::FindBookmarkByURL(const GURL& url) const {
  if (!url.is_valid())
    return nullptr;

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
    root_->Remove(root_->GetIndexOf(node).value());
}

const BookmarkNode* ReadingListManagerImpl::AddOrUpdateBookmark(
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
      id_gen_func_.Run(), base::Uuid::GenerateRandomV4(), entry->URL());
  bool success = SyncToBookmark(*entry, new_node.get());
  return success ? root_->Add(std::move(new_node)) : nullptr;
}

void ReadingListManagerImpl::NotifyReadingListChanged() {
  for (Observer& observer : observers_)
    observer.ReadingListChanged();
}
