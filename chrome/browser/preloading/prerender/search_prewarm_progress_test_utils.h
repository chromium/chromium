// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_TEST_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_TEST_UTILS_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/preloading/prerender/search_prewarm_progress_service.h"

// A test observer for SearchPrewarmProgressService that can wait for the
// prewarm to finish.
class SearchPrewarmProgressTestObserver {
 public:
  explicit SearchPrewarmProgressTestObserver(
      SearchPrewarmProgressService* service);
  ~SearchPrewarmProgressTestObserver();

  SearchPrewarmProgressTestObserver(const SearchPrewarmProgressTestObserver&) =
      delete;
  SearchPrewarmProgressTestObserver& operator=(
      const SearchPrewarmProgressTestObserver&) = delete;

  void OnSearchPrewarmFinished();

  // Waits until OnSearchPrewarmFinished is called.
  void WaitForNotification();

  bool was_notified() const { return was_notified_; }

 private:
  raw_ptr<SearchPrewarmProgressService> service_;
  base::CallbackListSubscription subscription_;
  bool was_notified_ = false;
  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_TEST_UTILS_H_
