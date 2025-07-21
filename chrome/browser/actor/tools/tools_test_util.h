// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_

#include <string_view>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace actor {

class ExecutionEngine;

inline constexpr int32_t kNonExistentContentNodeId =
    std::numeric_limits<int32_t>::max();

class ActorToolsTest : public InProcessBrowserTest {
 public:
  ActorToolsTest();
  ActorToolsTest(const ActorToolsTest&) = delete;
  ActorToolsTest& operator=(const ActorToolsTest&) = delete;
  ~ActorToolsTest() override;

  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;

  void GoBack();
  void TinyWait();

  content::WebContents* web_contents();
  tabs::TabInterface* active_tab();
  content::RenderFrameHost* main_frame();
  ExecutionEngine& execution_engine();
  ActorTask& actor_task() const;

 protected:
  TaskId task_id_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_for_init_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_
