// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_IMPL_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/reading_list/android/reading_list_manager.h"
#include "components/reading_list/core/reading_list_model_observer.h"

class ReadingListModel;

// Implementation of ReadingListManager.
// 1. Holds a in memory bookmark node tree. Contains a folder root and reading
// list nodes as children. Only has one level of children.
// 2. Talk to reading list model, and sync with the in memory bookmark tree.
// 3. Talk to observers to report model change events.
// TODO(crbug.com/41483143): Refactor this to be part of the bookmarks dir.
// - Better renamed to ReadingListAsBookmarkAdapter when moved.
class ReadingListManagerImpl : public ReadingListManager,
                               public ReadingListModelObserver {
 public:
  using IdGenerationFunction = base::RepeatingCallback<int64_t(void)>;

  // `reading_list_model` must not be null and must outlive `this`.
  ReadingListManagerImpl(ReadingListModel* reading_list_model,
                         const IdGenerationFunction& id_gen_func);
  ~ReadingListManagerImpl() override;

  // ReadingListModelObserver overrides.
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource source) override;
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override;
  void ReadingListDidMoveEntry(const ReadingListModel* model,
                               const GURL& url) override;
  void ReadingListDidUpdateEntry(const ReadingListModel* model,
                                 const GURL& url) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;
  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override;
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override;

  // ReadingListManager implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  const bookmarks::BookmarkNode* Add(const GURL& url,
                                     const std::string& title) override;
  const bookmarks::BookmarkNode* Get(const GURL& url) const override;
  const bookmarks::BookmarkNode* GetNodeByID(int64_t id) const override;
  void GetMatchingNodes(
      const bookmarks::QueryFields& query,
      size_t max_count,
      std::vector<const bookmarks::BookmarkNode*>* results) override;
  bool IsReadingListBookmark(
      const bookmarks::BookmarkNode* node) const override;
  void Delete(const GURL& url) override;
  void DeleteAll() override;
  const bookmarks::BookmarkNode* GetRoot() const override;
  size_t size() const override;
  size_t unread_size() const override;
  void SetTitle(const GURL& url, const std::u16string& title) override;
  void SetReadStatus(const GURL& url, bool read) override;
  bool GetReadStatus(const bookmarks::BookmarkNode* node) override;
  bool IsLoaded() const override;

 private:
  // Finds the child in the bookmark tree by URL. Returns nullptr if not found.
  // Not recursive since the reading list bookmark tree only has a folder root
  // node and one level of children.
  bookmarks::BookmarkNode* FindBookmarkByURL(const GURL& url) const;

  void RemoveBookmark(const GURL& url);
  const bookmarks::BookmarkNode* AddOrUpdateBookmark(
      const ReadingListEntry* entry);
  void NotifyReadingListChanged();

  // Contains reading list data, outlives this class.
  raw_ptr<ReadingListModel> reading_list_model_;

  // The bookmark root for reading list articles.
  std::unique_ptr<bookmarks::BookmarkNode> root_;

  // Function to generate an id for reading list nodes.
  const IdGenerationFunction id_gen_func_;

  // Whether the |reading_list_model_| is loaded.
  bool loaded_;

  // Whether |reading_list_model_| is in batch update mode.
  bool performing_batch_update_;
  bool changes_applied_during_batch_;

  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_IMPL_H_
