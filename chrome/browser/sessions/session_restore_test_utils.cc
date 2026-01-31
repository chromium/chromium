// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_test_utils.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/sessions/session_restore.h"

namespace testing {

SessionsRestoredWaiter::SessionsRestoredWaiter(
    base::OnceClosure quit_closure,
    int num_session_restores_expected)
    : quit_closure_(std::move(quit_closure)),
      num_session_restores_expected_(num_session_restores_expected) {
  callback_subscription_ = SessionRestore::RegisterOnSessionRestoredCallback(
      base::BindRepeating(&SessionsRestoredWaiter::OnSessionRestoreDone,
                          base::Unretained(this)));
}

SessionsRestoredWaiter::~SessionsRestoredWaiter() = default;

void SessionsRestoredWaiter::OnSessionRestoreDone(Profile* profile,
                                                  int num_tabs_restored) {
  if (++num_sessions_restored_ == num_session_restores_expected_)
    std::move(quit_closure_).Run();
}

}  // namespace testing
