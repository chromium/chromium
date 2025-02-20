// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_ORDERING_STORAGE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_ORDERING_STORAGE_H_

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/values.h"

class BookmarkMergedSurfaceService;
class BookmarkParentFolderChildren;

// `BookmarkMergedSurfaceOrderingStorage` handles writing custom ordering
// tracked by `BookmarkMergedSurfaceService` to disk.
// The ordering is stored in a form of a dict with the key the permanent folder
// and the value an in order list of child nodes's Ids.
class BookmarkMergedSurfaceOrderingStorage
    : public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  // How often the file is saved at most.
  static constexpr base::TimeDelta kSaveDelay = base::Milliseconds(2000);
  static constexpr std::string_view kBookmarkBarFolderNameKey = "bookmark_bar";
  static constexpr std::string_view kOtherBookmarkFolderNameKey = "other";
  static constexpr std::string_view kMobileFolderNameKey = "mobile";

  // `service` must not be null and must outlive `this` object.
  BookmarkMergedSurfaceOrderingStorage(
      const BookmarkMergedSurfaceService* service,
      const base::FilePath& file_path);

  BookmarkMergedSurfaceOrderingStorage(
      const BookmarkMergedSurfaceOrderingStorage&) = delete;
  BookmarkMergedSurfaceOrderingStorage& operator=(
      const BookmarkMergedSurfaceOrderingStorage&) = delete;

  // Upon destruction, if there is a pending save, it is saved immediately.
  ~BookmarkMergedSurfaceOrderingStorage() override;

  // Schedules saving the ordering to disk.
  void ScheduleSave();

  // base::ImportantFileWriter::BackgroundDataSerializer:
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  bool HasScheduledSaveForTesting() const;

 private:
  void SaveNowIfScheduled();

  // Returns a dict with the key representing one of the
  // `BookmarkParentFolder::PermanentFolderType` and the value a list of child
  // nodes ids if non-default order is tracked.
  base::Value::Dict EncodeOrderingToDict() const;

  // Returns list of child nodes id.
  static base::Value::List EncodeChildren(
      const BookmarkParentFolderChildren& children);

  const raw_ptr<const BookmarkMergedSurfaceService> service_;
  const base::FilePath path_;

  // Sequenced task runner where disk writes will be performed at.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Helper to write bookmark data safely.
  base::ImportantFileWriter writer_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_ORDERING_STORAGE_H_
