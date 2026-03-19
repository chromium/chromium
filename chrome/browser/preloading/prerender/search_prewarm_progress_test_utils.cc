// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_prewarm_progress_test_utils.h"

#include "base/functional/bind.h"

SearchPrewarmProgressTestObserver::SearchPrewarmProgressTestObserver(
    SearchPrewarmProgressService* service)
    : service_(service) {
  subscription_ =
      service_->RegisterSearchPrewarmFinishedCallback(base::BindRepeating(
          &SearchPrewarmProgressTestObserver::OnSearchPrewarmFinished,
          base::Unretained(this)));
}

SearchPrewarmProgressTestObserver::~SearchPrewarmProgressTestObserver() =
    default;

void SearchPrewarmProgressTestObserver::OnSearchPrewarmFinished() {
  was_notified_ = true;
  run_loop_.Quit();
}

void SearchPrewarmProgressTestObserver::WaitForNotification() {
  if (was_notified_) {
    return;
  }
  run_loop_.Run();
}
