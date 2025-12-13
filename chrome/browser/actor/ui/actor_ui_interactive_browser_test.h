// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_INTERACTIVE_BROWSER_TEST_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_INTERACTIVE_BROWSER_TEST_H_

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/test/interaction/interactive_browser_test.h"

// TODO(chrstne): Move interactive tests to a new tests/ folder
class ActorUiInteractiveBrowserTest : public InteractiveBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  void StartActingOnTab();
  void PauseTask();
  void CompleteTask();

  actor::ActorKeyedService* actor_keyed_service() {
    return actor::ActorKeyedService::Get(browser()->profile());
  }

  actor::TaskId task_id() const { return task_id_; }

 private:
  actor::TaskId task_id_;
};

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_INTERACTIVE_BROWSER_TEST_H_
