// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

using base::test::ScopedFeatureList;
using base::test::TestFuture;
using ::content::EvalJs;
using ::content::RenderFrameHost;
using ::content::WebContents;

const char* kFetchPath = "/fetchtarget.html";

// TODO(bokan) - Factor out into a common test harness with
// page_stability_browsertest.cc
class ObservationDelayControllerTest : public InProcessBrowserTest {
 public:
  ObservationDelayControllerTest() {
    // GlicActor is actually unneeded but enabled solely to set params.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicActor,
          {{features::kActorGeneralPageStabilityMode.name,
            features::kActorGeneralPageStabilityMode.GetName(
                features::ActorGeneralPageStabilityMode::kAllEnabled)},
           // Effectively disable the timeouts to prevent flakes.
           {"glic-actor-page-stability-local-timeout", "30000ms"},
           {"glic-actor-page-stability-timeout", "30000ms"},
           // Do not use an invoke delay
           {"glic-actor-page-stability-invoke-callback-delay", "0ms"}}},
         {features::kGlic, {}},
         {features::kTabstripComboButton, {}}},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ObservationDelayControllerTest(const ObservationDelayControllerTest&) =
      delete;
  ObservationDelayControllerTest& operator=(
      const ObservationDelayControllerTest&) = delete;

  ~ObservationDelayControllerTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    fetch_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kFetchPath);

    journal_entry_ = journal_.CreatePendingAsyncEntry(
        GURL(), actor::TaskId(), mojom::JournalTrack::kActor,
        "ObservationDelay", {});

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  // Pause execution for the specified amount of time.
  void Sleep(base::TimeDelta delta) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delta);
    run_loop.Run();
  }

  WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  std::string GetOutputText() {
    return EvalJs(web_contents(), "document.getElementById('output').innerText")
        .ExtractString();
  }

  net::test_server::ControllableHttpResponse& fetch_response() {
    return *fetch_response_;
  }

  actor::AggregatedJournal::PendingAsyncEntry& journal_entry() {
    return *journal_entry_;
  }

  void Respond(std::string_view text) {
    fetch_response_->Send(net::HTTP_OK, /*content_type=*/"text/html",
                          /*content=*/"",
                          /*cookies=*/{}, /*extra_headers=*/{});
    fetch_response_->Send(std::string(text));
    fetch_response_->Done();
  }

  ObservationDelayController::PageStabilityConfig PageStabilityConfig() const {
    // Use default values.
    return ObservationDelayController::PageStabilityConfig();
  }

 private:
  actor::AggregatedJournal journal_;
  std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> fetch_response_;
  ScopedFeatureList scoped_feature_list_;
};

// TODO(bokan): Flaky but fixed in https://crrev.com/c/6992849.
IN_PROC_BROWSER_TEST_F(ObservationDelayControllerTest,
                       DISABLED_UsePageStabilityForSameDocumentNavigation) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/observation_delay.html");
  const GURL url2 =
      embedded_test_server()->GetURL("/actor/observation_delay.html#foo");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_EQ(GetOutputText(), "INITIAL");

  ObservationDelayController controller(*main_frame(), actor::TaskId(),
                                        PageStabilityConfig());

  // Perform a same-document navigation. The page has a navigation handler
  // that will initiate a fetch from this event.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url2));

  fetch_response().WaitForRequest();

  ASSERT_FALSE(web_contents()->IsLoading());
  ASSERT_EQ(GetOutputText(), "INITIAL");

  TestFuture<void> result;
  controller.Wait(journal_entry(), result.GetCallback());

  EXPECT_FALSE(result.IsReady());

  Sleep(base::Milliseconds(1000));

  EXPECT_FALSE(result.IsReady());

  Respond("TEST COMPLETE");

  // But it should eventually resolve once the tasks finish.
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "TEST COMPLETE");
}

}  // namespace

}  // namespace actor
