// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/history/feed_history_helper.h"

#include <utility>

#include "base/bind.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_row.h"

namespace feed {

FeedHistoryHelper::FeedHistoryHelper(history::HistoryService* history_service)
    : history_service_(history_service) {}

FeedHistoryHelper::~FeedHistoryHelper() = default;

void FeedHistoryHelper::CheckURL(
    const GURL& url,
    FeedLoggingMetrics::CheckURLVisitCallback callback) {
  DCHECK(history_service_);
  history_service_->QueryURL(
      url, /*want_visits=*/false,
      base::BindOnce(&FeedHistoryHelper::OnCheckURLDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      &tracker_);
}

void FeedHistoryHelper::OnCheckURLDone(
    FeedLoggingMetrics::CheckURLVisitCallback callback,
    history::QueryURLResult result) {
  std::move(callback).Run(result.success && result.row.visit_count() != 0);
}

}  // namespace feed
