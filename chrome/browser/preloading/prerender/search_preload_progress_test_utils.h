// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PRELOAD_PROGRESS_TEST_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PRELOAD_PROGRESS_TEST_UTILS_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/preloading/prerender/search_preload_progress_service.h"

// A test observer for SearchPreloadProgressService that can wait for the
// prewarm to finish.
class SearchPreloadProgressTestObserver {
 public:
  explicit SearchPreloadProgressTestObserver(
      SearchPreloadProgressService* service);
  ~SearchPreloadProgressTestObserver();

  SearchPreloadProgressTestObserver(const SearchPreloadProgressTestObserver&) =
      delete;
  SearchPreloadProgressTestObserver& operator=(
      const SearchPreloadProgressTestObserver&) = delete;

  void OnSearchPrewarmFinished();

  // Waits until OnSearchPrewarmFinished is called.
  void WaitForNotification();

  bool was_notified() const { return was_notified_; }

 private:
  raw_ptr<SearchPreloadProgressService> service_;
  base::CallbackListSubscription subscription_;
  bool was_notified_ = false;
  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PRELOAD_PROGRESS_TEST_UTILS_H_
