// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_bookmark_metrics_service.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/dino/dino_metrics.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_load_waiter.h"
#include "url/gurl.h"

namespace dino {

DinoBookmarkMetricsService::DinoBookmarkMetricsService(
    bookmarks::BookmarkModel* model)
    : model_(model) {
  CHECK(model_);
  bookmarks::ScheduleCallbackOnBookmarkModelLoad(
      *model_, base::BindOnce(&DinoBookmarkMetricsService::RecordMetrics,
                              weak_ptr_factory_.GetWeakPtr()));
}

DinoBookmarkMetricsService::~DinoBookmarkMetricsService() = default;

void DinoBookmarkMetricsService::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  model_ = nullptr;
}

void DinoBookmarkMetricsService::RecordMetrics() {
  RecordDinoHasBookmark(model_->IsBookmarked(GURL("chrome://dino/")));
}

}  // namespace dino
