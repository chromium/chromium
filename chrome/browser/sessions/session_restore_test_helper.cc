// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_test_helper.h"

#include "base/bind.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

SessionRestoreTestHelper::SessionRestoreTestHelper()
    : restore_notification_seen_(false), loop_is_running_(false) {
  callback_subscription_ = SessionRestore::RegisterOnSessionRestoredCallback(
      base::Bind(&SessionRestoreTestHelper::OnSessionRestoreDone,
                 weak_ptr_factory.GetWeakPtr()));
}

SessionRestoreTestHelper::~SessionRestoreTestHelper() {
}

void SessionRestoreTestHelper::Wait() {
  if (restore_notification_seen_)
    return;

  loop_is_running_ = true;
  message_loop_runner_ = new content::MessageLoopRunner;
  message_loop_runner_->Run();
  EXPECT_TRUE(restore_notification_seen_);
}

void SessionRestoreTestHelper::OnSessionRestoreDone(
    int /* num_tabs_restored */) {
  restore_notification_seen_ = true;
  if (!loop_is_running_)
    return;

  message_loop_runner_->Quit();
  loop_is_running_ = false;
}
