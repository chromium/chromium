// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_preload_progress_test_utils.h"

#include "base/functional/bind.h"

SearchPreloadProgressTestObserver::SearchPreloadProgressTestObserver(
    SearchPreloadProgressService* service)
    : service_(service) {
  subscription_ =
      service_->RegisterSearchPrewarmFinishedCallback(base::BindRepeating(
          &SearchPreloadProgressTestObserver::OnSearchPrewarmFinished,
          base::Unretained(this)));
}

SearchPreloadProgressTestObserver::~SearchPreloadProgressTestObserver() =
    default;

void SearchPreloadProgressTestObserver::OnSearchPrewarmFinished() {
  was_notified_ = true;
  run_loop_.Quit();
}

void SearchPreloadProgressTestObserver::WaitForNotification() {
  if (was_notified_) {
    return;
  }
  run_loop_.Run();
}
