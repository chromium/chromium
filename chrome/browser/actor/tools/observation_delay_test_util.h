// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_TEST_UTIL_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/page_stability_test_util.h"
#include "chrome/common/actor/task_id.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class TestNavigationManager;
class WebContents;
}  // namespace content

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace actor {

// Helper to start a navigation in the main frame to a page that reaches
// DOMContentLoaded in the main frame but doesn't reach the load event until
// `RunToLoadEvent` is called. It does this by deferring a subframe navigation.
class NavigateToLoadDeferredPage {
 public:
  NavigateToLoadDeferredPage(content::WebContents* web_contents,
                             net::test_server::EmbeddedTestServer* server);
  ~NavigateToLoadDeferredPage();

  [[nodiscard]] bool RunToDOMContentLoadedEvent();

  [[nodiscard]] bool RunToLoadEvent();

 private:
  const GURL url_main_frame_;
  const GURL url_subframe_;
  content::WebContents* web_contents_;

  std::unique_ptr<content::TestNavigationManager> subframe_manager_;
  std::unique_ptr<content::TestNavigationManager> main_manager_;
};

class TestObservationDelayController : public ObservationDelayController {
 public:
  TestObservationDelayController(content::RenderFrameHost& target_frame,
                                 TaskId task_id,
                                 AggregatedJournal& journal,
                                 PageStabilityConfig page_stability_config);
  ~TestObservationDelayController() override;

  [[nodiscard]] bool WaitForState(State state);
  State GetState() const { return state_; }

 protected:
  void SetState(State state) override;

  std::optional<State> waiting_state_;
  base::OnceClosure quit_closure_;
};

class ObservationDelayTest : public PageStabilityTest {
 public:
  ObservationDelayTest();
  ObservationDelayTest(const ObservationDelayTest&) = delete;
  ObservationDelayTest& operator=(const ObservationDelayTest&) = delete;
  ~ObservationDelayTest() override;

  tabs::TabInterface* active_tab();

  actor::AggregatedJournal& journal() { return journal_; }

  ObservationDelayController::PageStabilityConfig PageStabilityConfig() const;

  [[nodiscard]] bool InitiateFetchRequest();

  [[nodiscard]] bool DoesReachSteadyState(
      TestObservationDelayController& controller,
      ObservationDelayController::State state);

 private:
  void SteadyStateSleep();

  actor::AggregatedJournal journal_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_TEST_UTIL_H_
