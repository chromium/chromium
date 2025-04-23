// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_test_helpers.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"

namespace {

class BookmarkMergedSurfaceServiceLoadedObserver
    : public BookmarkMergedSurfaceServiceObserver {
 public:
  BookmarkMergedSurfaceServiceLoadedObserver(
      BookmarkMergedSurfaceService* service,
      base::OnceClosure on_loaded)
      : on_loaded_(std::move(on_loaded)) {
    CHECK(service);
    CHECK(on_loaded_);
    if (service->loaded()) {
      OnServiceLoaded();
      return;
    }
    observation_.Observe(service);
  }

  BookmarkMergedSurfaceServiceLoadedObserver(
      const BookmarkMergedSurfaceServiceLoadedObserver&) = delete;
  BookmarkMergedSurfaceServiceLoadedObserver& operator=(
      const BookmarkMergedSurfaceServiceLoadedObserver&) = delete;

  ~BookmarkMergedSurfaceServiceLoadedObserver() override = default;

  // BookmarkMergedSurfaceServiceObserver:

  void BookmarkMergedSurfaceServiceLoaded() override { OnServiceLoaded(); }

  void BookmarkMergedSurfaceServiceBeingDeleted() override {}
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override {}
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override {}
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override {}
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override {}
  void BookmarkAllUserNodesRemoved() override {}

 private:
  void OnServiceLoaded() {
    std::move(on_loaded_).Run();
    // this might be deleted.
  }

  base::OnceClosure on_loaded_;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      observation_{this};
};

}  // namespace

void WaitForBookmarkMergedSurfaceServiceToLoad(
    BookmarkMergedSurfaceService* service) {
  CHECK(service);
  base::RunLoop run_loop;
  BookmarkMergedSurfaceServiceLoadedObserver observer(service,
                                                      run_loop.QuitClosure());
  run_loop.Run();
  CHECK(service->loaded());
}

MockPermanentFolderOrderingTrackerDelegate::
    MockPermanentFolderOrderingTrackerDelegate() = default;

MockPermanentFolderOrderingTrackerDelegate::
    ~MockPermanentFolderOrderingTrackerDelegate() = default;
