// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DINO_DINO_BOOKMARK_METRICS_SERVICE_H_
#define CHROME_BROWSER_DINO_DINO_BOOKMARK_METRICS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace dino {

// Profile-scoped service that records baseline `chrome://dino` bookmark
// prevalence (`WebUI.Dino.HasBookmark`) once `BookmarkModel` finishes loading
// on a regular profile.
class DinoBookmarkMetricsService : public KeyedService {
 public:
  // `model` must be non-null and must outlive `DinoBookmarkMetricsService` (or
  // remain valid until `Shutdown()` is called).
  explicit DinoBookmarkMetricsService(bookmarks::BookmarkModel* model);
  DinoBookmarkMetricsService(const DinoBookmarkMetricsService&) = delete;
  DinoBookmarkMetricsService& operator=(const DinoBookmarkMetricsService&) =
      delete;
  ~DinoBookmarkMetricsService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  void RecordMetrics();

  raw_ptr<bookmarks::BookmarkModel> model_;
  base::WeakPtrFactory<DinoBookmarkMetricsService> weak_ptr_factory_{this};
};

}  // namespace dino

#endif  // CHROME_BROWSER_DINO_DINO_BOOKMARK_METRICS_SERVICE_H_
