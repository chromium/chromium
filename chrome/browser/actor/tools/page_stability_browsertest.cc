// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

namespace {

using ::base::test::ScopedFeatureList;
using ::base::test::TestFuture;
using ::content::EvalJs;
using ::content::ExecJs;
using ::content::JsReplace;
using ::content::NavigationThrottle;
using ::content::NavigationThrottleRegistry;
using ::content::RenderFrameHost;
using ::content::TestNavigationManager;
using ::content::TestNavigationThrottle;
using ::content::TestNavigationThrottleInserter;
using ::content::WebContents;
using optimization_guide::proto::ClickAction;

// Note: this file doesn't actually exist, the response is manually provided by
// tests.
const char* kFetchPath = "/fetchtarget.html";

// Tests for the PageStabilityMonitor's functionality of delaying renderer-tool
// completion until the page is ready for an observation.
class ActorPageStabilityTestBase : public InProcessBrowserTest {
 public:
  ActorPageStabilityTestBase() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorPageStabilityTestBase(const ActorPageStabilityTestBase&) = delete;
  ActorPageStabilityTestBase& operator=(const ActorPageStabilityTestBase&) =
      delete;

  ~ActorPageStabilityTestBase() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    fetch_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kFetchPath);

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
    auto execution_engine =
        std::make_unique<ExecutionEngine>(browser()->profile());
    auto event_dispatcher = ui::NewUiEventDispatcher(
        actor_keyed_service()->GetActorUiStateManager());
    auto actor_task = std::make_unique<ActorTask>(
        GetProfile(), std::move(execution_engine), std::move(event_dispatcher));
    task_id_ = ActorKeyedService::Get(browser()->profile())
                   ->AddActiveTask(std::move(actor_task));
  }

  void TearDownOnMainThread() override {
    // The ActorTask owned ExecutionEngine has a pointer to the profile, which
    // must be released before the browser is torn down to avoid a dangling
    // pointer.
    actor_keyed_service()->ResetForTesting();
  }

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

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(browser()->profile());
  }

  ActorTask& task() {
    CHECK(task_id_);
    return *actor_keyed_service()->GetTask(task_id_);
  }

  net::test_server::ControllableHttpResponse& fetch_response() {
    return *fetch_response_;
  }

  void Respond(std::string_view text) {
    fetch_response_->Send(net::HTTP_OK, /*content_type=*/"text/html",
                          /*content=*/"",
                          /*cookies=*/{}, /*extra_headers=*/{});
    fetch_response_->Send(std::string(text));
    fetch_response_->Done();
  }

 protected:
  TaskId task_id_;

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse> fetch_response_;
  ScopedFeatureList scoped_feature_list_;
};

// Shorten timeouts to test they work.
// LocalTimeout is the timeout delay used when waiting on non-network actions
// like an idle main thread and display compositor frame presentation.
// GlobalTimeout is the timeout delay used end-to-end in the
template <int LocalTimeout, int GlobalTimeout>
class ActorPageStabilityTimeoutTest : public ActorPageStabilityTestBase,
                                      public ::testing::WithParamInterface<
                                          ::features::ActorPaintStabilityMode> {
 public:
  ActorPageStabilityTimeoutTest() {
    std::string local_timeout = absl::StrFormat("%dms", LocalTimeout);
    std::string global_timeout = absl::StrFormat("%dms", GlobalTimeout);
    // Make the paint timeouts high enough that the local and global
    // timeouts apply, to simulate not reaching paint stability.
    std::string paint_timeout =
        absl::StrFormat("%dms", GlobalTimeout + LocalTimeout);
    timeout_scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic, {}},
         {features::kTabstripComboButton, {}},
         {features::kGlicActor,
          {{"glic-actor-page-stability-local-timeout", local_timeout},
           {"glic-actor-page-stability-timeout", global_timeout},
           {::features::kActorPaintStabilityMode.name,
            ::features::kActorPaintStabilityMode.GetName(GetParam())},
           {::features::kActorPaintStabilityIntialPaintTimeout.name,
            paint_timeout},
           {::features::kActorPaintStabilitySubsequentPaintTimeout.name,
            paint_timeout}}}},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorPageStabilityTimeoutTest(const ActorPageStabilityTimeoutTest&) = delete;
  ActorPageStabilityTimeoutTest& operator=(
      const ActorPageStabilityTimeoutTest&) = delete;

  ~ActorPageStabilityTimeoutTest() override = default;

 private:
  ScopedFeatureList timeout_scoped_feature_list_;
};

// Shorten the timeout under test and make the other timeout very long to avoid
// tripping it.
using ActorPageStabilityLocalTimeoutTest =
    ActorPageStabilityTimeoutTest<100, 100000>;
using ActorPageStabilityGlobalTimeoutTest =
    ActorPageStabilityTimeoutTest<100000, 100>;

// Ensure that if a network request runs long, the stability monitor will
// eventually timeout.
IN_PROC_BROWSER_TEST_P(ActorPageStabilityGlobalTimeoutTest, NetworkTimeout) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_EQ(GetOutputText(), "INITIAL");

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "#btnFetchAndWork");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  task().Act(ToRequestList(action), result.GetCallback());

  // Never respond to the request
  fetch_response().WaitForRequest();

  // Ensure the stability monitor eventually allows completion.
  ExpectOkResult(result);
  ASSERT_EQ(GetOutputText(), "INITIAL");
}

// Ensure that if the main thread never becomes idle the stability monitor will
// eventually timeout.
IN_PROC_BROWSER_TEST_P(ActorPageStabilityGlobalTimeoutTest, BusyMainThread) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "#btnWorkForever");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  task().Act(ToRequestList(action), result.GetCallback());

  // Ensure the stability monitor eventually allows completion.
  ExpectOkResult(result);
}

// Ensure that if the main thread never becomes idle the stability monitor will
// eventually timeout on the local timeout.
IN_PROC_BROWSER_TEST_P(ActorPageStabilityLocalTimeoutTest, BusyMainThread) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "#btnWorkForever");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  task().Act(ToRequestList(action), result.GetCallback());

  // Ensure the stability monitor eventually allows completion.
  ExpectOkResult(result);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ActorPageStabilityGlobalTimeoutTest,
    testing::Values(::features::ActorPaintStabilityMode::kDisabled,
                    ::features::ActorPaintStabilityMode::kLogOnly,
                    ::features::ActorPaintStabilityMode::kEnabled));
INSTANTIATE_TEST_SUITE_P(
    ,
    ActorPageStabilityLocalTimeoutTest,
    testing::Values(::features::ActorPaintStabilityMode::kDisabled,
                    ::features::ActorPaintStabilityMode::kLogOnly,
                    ::features::ActorPaintStabilityMode::kEnabled));

enum class NavigationDelay { kInstant, kDelayed };
enum class NavigationType { kSameDocument, kSameSite, kCrossSite };

// Run the following test using same and cross site navigations to exercise
// paths where the RenderFrameHost is swapped or kept as well as same document
// where the navigation is synchronous in the renderer.
//
// Also run with the navigation completing without delay as well as with some
// induced delay.
// TODO(crbug.com/414662842): Move to page_stability_browsertest.cc.
class ActorPageStabilityNavigationTypesTest
    : public ActorPageStabilityTestBase,
      public testing::WithParamInterface<
          std::tuple<NavigationDelay,
                     NavigationType,
                     ::features::ActorPaintStabilityMode>> {
 public:
  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [delay, navigation_type, paint_monitor_mode] = info.param;
    std::stringstream params_description;
    switch (delay) {
      case NavigationDelay::kInstant:
        params_description << "Instant";
        break;
      case NavigationDelay::kDelayed:
        params_description << "Delayed";
        break;
    }
    switch (navigation_type) {
      case NavigationType::kSameDocument:
        params_description << "_SameDocument";
        break;
      case NavigationType::kSameSite:
        params_description << "_SameSite";
        break;
      case NavigationType::kCrossSite:
        params_description << "_CrossSite";
        break;
    }
    switch (paint_monitor_mode) {
      case ::features::ActorPaintStabilityMode::kDisabled:
        params_description << "_PaintMonitorDisabled";
        break;
      case ::features::ActorPaintStabilityMode::kLogOnly:
        params_description << "_PaintMonitorLog";
        break;
      case ::features::ActorPaintStabilityMode::kEnabled:
        params_description << "_PaintMonitorEnabled";
        break;
    }
    return params_description.str();
  }

  ActorPageStabilityNavigationTypesTest() {
    base::FieldTrialParams allowlist_params;
    allowlist_params["allowlist"] = "foo.com,bar.com";
    allowlist_params["allowlist_only"] = "true";

    page_tools_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlic, {}},
                              {features::kTabstripComboButton, {}},
                              {features::kGlicActor,
                               {{::features::kActorPaintStabilityMode.name,
                                 ::features::kActorPaintStabilityMode.GetName(
                                     std::get<2>(GetParam()))}}},
                              {kGlicActionAllowlist, allowlist_params}},
        /*disabled_features=*/{features::kGlicWarming});
  }

  NavigationType NavigationTypeParam() const { return std::get<1>(GetParam()); }

  NavigationDelay DelayTypeParam() const {
    // Note: the delay is 5s but in practice the RenderFrame is torn down by
    // navigation so this won't block the test.
    return std::get<0>(GetParam());
  }

 private:
  ScopedFeatureList page_tools_feature_list_;
};

// Ensure a page tool (click, in this case) causing a navigation of various
// types (same-doc, same-site, cross-site) works successfully waits for loading
// to finish in cases where the navigation finishes quickly or is delayed at
// various points.
IN_PROC_BROWSER_TEST_P(ActorPageStabilityNavigationTypesTest, Test) {
  const GURL url_start = embedded_https_test_server().GetURL(
      "foo.com", "/actor/cross_document_nav.html");
  GURL url_next;
  switch (NavigationTypeParam()) {
    case NavigationType::kSameDocument:
      if (DelayTypeParam() == NavigationDelay::kDelayed) {
        // Same document navigations are synchronous so it doesn't make sense
        // for there to be a delay.
        GTEST_SKIP();
      }
      url_next = embedded_https_test_server().GetURL(
          "foo.com", "/actor/cross_document_nav.html#next");
      break;
    case NavigationType::kSameSite:
      url_next = embedded_https_test_server().GetURL(
          "foo.com", "/actor/simple_iframe.html");
      break;
    case NavigationType::kCrossSite:
      url_next = embedded_https_test_server().GetURL(
          "bar.com", "/actor/simple_iframe.html");
      break;
  }

  // The subframe in the destination page is used to delay the load event (by
  // deferring its navigation commit).
  GURL::Replacements replacement;
  replacement.SetPathStr("/actor/blank.html");
  GURL url_subframe = url_next.ReplaceComponents(replacement);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  // The link in the file is relative so replace it to include the mock
  // hostname.
  ASSERT_TRUE(
      ExecJs(web_contents(),
             JsReplace("document.getElementById('link').href = $1", url_next)));

  // To ensure coverage of the case where a RenderFrameHost is reused across
  // same-site navigation, disable proactive browsing instance swaps.
  DisableProactiveBrowsingInstanceSwapFor(main_frame());

  // Send a click to the link.
  std::optional<int> link_id = GetDOMNodeId(*main_frame(), "#link");
  ASSERT_TRUE(link_id);

  // In the delay variant of the test, delay the main frame commit to ensure
  // page observation doesn't return early after a slow network response. Delay
  // the subframe in the new page as well to ensure the page tool waits on a
  // cross-document load in this case.
  std::optional<TestNavigationManager> main_frame_delay;
  std::optional<TestNavigationManager> subframe_delay;

  if (DelayTypeParam() == NavigationDelay::kDelayed) {
    main_frame_delay.emplace(web_contents(), url_next);
    subframe_delay.emplace(web_contents(), url_subframe);
  }

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), link_id.value());
  ActResultFuture result;
  task().Act(ToRequestList(action), result.GetCallback());

  if (main_frame_delay) {
    CHECK(subframe_delay);
    ASSERT_TRUE(main_frame_delay->WaitForResponse());
    Sleep(base::Milliseconds(300));
    EXPECT_FALSE(result.IsReady());
    ASSERT_TRUE(main_frame_delay->WaitForNavigationFinished());

    // Now delay the subframe to delay main document load completion.
    ASSERT_TRUE(subframe_delay->WaitForResponse());
    Sleep(base::Milliseconds(300));
    EXPECT_FALSE(result.IsReady());
    ASSERT_TRUE(subframe_delay->WaitForNavigationFinished());
  }

  ExpectOkResult(result);

  EXPECT_EQ(web_contents()->GetURL(), url_next);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ActorPageStabilityNavigationTypesTest,
    testing::Combine(
        testing::Values(NavigationDelay::kInstant, NavigationDelay::kDelayed),
        testing::Values(NavigationType::kSameDocument,
                        NavigationType::kSameSite,
                        NavigationType::kCrossSite),
        testing::Values(::features::ActorPaintStabilityMode::kDisabled,
                        ::features::ActorPaintStabilityMode::kLogOnly,
                        ::features::ActorPaintStabilityMode::kEnabled)),
    ActorPageStabilityNavigationTypesTest::DescribeParams);

// Tests specifically using the general page stability mechanism, allowing
// direct instantiation of the monitor in a renderer via Mojo.
class ActorGeneralPageStabilityTest : public ActorPageStabilityTestBase,
                                      public ::testing::WithParamInterface<
                                          ::features::ActorPaintStabilityMode> {
 public:
  ActorGeneralPageStabilityTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{::features::kGlicActor,
          {{features::kActorGeneralPageStabilityMode.name,
            features::kActorGeneralPageStabilityMode.GetName(
                features::ActorGeneralPageStabilityMode::kAllEnabled)},
           {::features::kActorPaintStabilityMode.name,
            ::features::kActorPaintStabilityMode.GetName(GetParam())},
           // Effectively disable the timeouts to prevent flakes.
           {"glic-actor-page-stability-local-timeout", "30000ms"},
           {"glic-actor-page-stability-timeout", "30000ms"},
           // Do not use an invoke delay
           {"glic-actor-page-stability-invoke-callback-delay", "0ms"}}},
         {features::kGlic, {}},
         {features::kTabstripComboButton, {}}},
        /*disabled_features=*/{features::kGlicWarming});
  }

  mojo::Remote<mojom::PageStabilityMonitor> CreatePageStabilityMonitor() {
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame;
    main_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &chrome_render_frame);

    // TODO(bokan): Once paint stability ships, the param should be replaced by
    // a new one since some tools will continue to not support it.
    bool use_paint_stability =
        GetParam() != features::ActorPaintStabilityMode::kDisabled;

    mojo::Remote<mojom::PageStabilityMonitor> monitor_remote;
    chrome_render_frame->CreatePageStabilityMonitor(
        monitor_remote.BindNewPipeAndPassReceiver(), actor::TaskId(),
        use_paint_stability);

    // Ensure the monitor is created in the renderer before returning it.
    monitor_remote.FlushForTesting();

    return monitor_remote;
  }

  std::unique_ptr<TestNavigationThrottleInserter>
  ScopedCancelAllIncomingNavigations() {
    return std::make_unique<TestNavigationThrottleInserter>(
        web_contents(),
        base::BindLambdaForTesting([&](NavigationThrottleRegistry& registry)
                                       -> void {
          auto throttle = std::make_unique<TestNavigationThrottle>(registry);
          throttle->SetResponse(TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                                TestNavigationThrottle::SYNCHRONOUS,
                                NavigationThrottle::CANCEL_AND_IGNORE);
          registry.AddThrottle(std::move(throttle));
        }));
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto paint_monitor_mode = info.param;
    std::stringstream params_description;
    switch (paint_monitor_mode) {
      case ::features::ActorPaintStabilityMode::kDisabled:
        params_description << "PaintMonitorDisabled";
        break;
      case ::features::ActorPaintStabilityMode::kLogOnly:
        params_description << "PaintMonitorLog";
        break;
      case ::features::ActorPaintStabilityMode::kEnabled:
        params_description << "PaintMonitorEnabled";
        break;
    }
    return params_description.str();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ActorGeneralPageStabilityTest,
    testing::Values(::features::ActorPaintStabilityMode::kDisabled,
                    ::features::ActorPaintStabilityMode::kLogOnly,
                    ::features::ActorPaintStabilityMode::kEnabled),
    ActorGeneralPageStabilityTest::DescribeParams);

// Ensure the page isn't considered stable until after a network fetch is
// resolved.
IN_PROC_BROWSER_TEST_P(ActorGeneralPageStabilityTest, WaitOnNetworkFetch) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  ASSERT_TRUE(ExecJs(web_contents(), "window.doFetch(() => {})"));
  fetch_response().WaitForRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // Wait long enough to have some confidence the monitor is blocking on the
  // network request.
  Sleep(base::Milliseconds(1000));

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  // Complete the fetch, ensure the monitor completes.
  Respond("NETWORK DONE");
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "NETWORK DONE");
}

// Ensure the page isn't considered stable while the main thread is busy.
IN_PROC_BROWSER_TEST_P(ActorGeneralPageStabilityTest, WaitOnMainThread) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_EQ(GetOutputText(), "INITIAL");

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_TRUE(ExecJs(
      web_contents(),
      "window.doBusyWork(/*tasks_to_run=*/4, /*task_duration_ms=*/400)"));

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // Wait long enough to have some confidence the monitor is blocking on the
  // main thread.
  Sleep(base::Seconds(1));
  EXPECT_FALSE(result.IsReady());

  // But it should eventually resolve once the tasks finish.
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "WORK DONE");
}

// Perform and commit a navigation before NotifyWhenStable is called. Expect
// that either the remote is disconnected or the NotifyWhenStable callback is
// executed.
IN_PROC_BROWSER_TEST_P(ActorGeneralPageStabilityTest,
                       NavigationBeforeNotifyNoBFCache) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  TestFuture<void> result;

  // With RenderDocument, the navigation will always use a new frame so we
  // expect to hear a disconnect rather than having the monitor reply to
  // NotifyWhenStable.
  monitor.set_disconnect_handler(result.GetCallback());

  // Navigate away and finish the navigation.
  TestNavigationManager manager(web_contents(), url2);
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace("window.location = $1", url2)));
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());
  EXPECT_TRUE(result.Wait());
}

// Perform and commit a navigation before NotifyWhenStable is called. Expect
// that either the remote is disconnected or the NotifyWhenStable callback is
// executed.
IN_PROC_BROWSER_TEST_P(ActorGeneralPageStabilityTest, NavigationBeforeNotify) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  TestFuture<void> result;

  // With RenderDocument, the navigation will always use a new frame so we
  // expect to hear a disconnect rather than having the monitor reply to
  // NotifyWhenStable.
  monitor.set_disconnect_handler(result.GetCallback());

  // Navigate away and finish the navigation.
  TestNavigationManager manager(web_contents(), url2);
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace("window.location = $1", url2)));
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());
  EXPECT_TRUE(result.Wait());
}

// Perform and fail a navigation before NotifyWhenStable is called. Expect
// that the monitor continues watching for page stability.
IN_PROC_BROWSER_TEST_P(ActorGeneralPageStabilityTest,
                       FailNavigationBeforeNotify) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  // Start and cancel a navigation before querying the monitor.
  {
    TestNavigationManager manager(web_contents(), url2);
    auto scoped_navigation_canceler = ScopedCancelAllIncomingNavigations();
    ASSERT_TRUE(
        ExecJs(web_contents(), JsReplace("window.location = $1", url2)));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    ASSERT_FALSE(manager.was_committed());
  }

  // Initiate a network fetch.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  ASSERT_TRUE(ExecJs(web_contents(), "window.doFetch(() => {})"));
  fetch_response().WaitForRequest();

  // Start waiting on the monitor.
  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // Wait long enough to have some confidence the monitor is blocking on the
  // network request.
  Sleep(base::Milliseconds(1000));

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  // Complete the fetch, ensure the monitor completes.
  Respond("NETWORK DONE");
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "NETWORK DONE");
}

// Perform and fail a navigation after NotifyWhenStable is called. Expect
// that the monitor continues watching for page stability.
IN_PROC_BROWSER_TEST_P(ActorGeneralPageStabilityTest,
                       FailNavigationAfterNotify) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  // Start a navigation but don't let it proceed to cancelation yet, it's
  // deferred for now.
  auto scoped_navigation_canceler = ScopedCancelAllIncomingNavigations();
  TestNavigationManager manager(web_contents(), url2);
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace("window.location = $1", url2)));
  ASSERT_TRUE(manager.WaitForFirstYieldAfterDidStartNavigation());

  // Start waiting for the monitor. Sleep to ensure the monitor is waiting on
  // the navigation to complete/fail.
  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());
  Sleep(base::Seconds(1));
  EXPECT_FALSE(result.IsReady());

  // Start a fetch request and then let the prior navigation fail, the new fetch
  // should block the monitor.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  ASSERT_TRUE(ExecJs(web_contents(), "window.doFetch(() => {})"));
  fetch_response().WaitForRequest();
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  ASSERT_FALSE(manager.was_committed());

  // Ensure the monitor is blocked on the network request.
  Sleep(base::Seconds(1));
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  // Complete the fetch, ensure the monitor completes.
  Respond("NETWORK DONE");
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "NETWORK DONE");
}

// Perform a navigation during the start delay of NotifyWhenStable. It should
// cause the monitor to immediately complete.
IN_PROC_BROWSER_TEST_P(ActorGeneralPageStabilityTest,
                       NavigationDuringStartDelay) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  // Wait for stability. Use a long observation_delay to ensure the navigation
  // takes place within it.
  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::Seconds(300),
                            result.GetCallback());

  TestNavigationManager manager(web_contents(), url2);
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace("window.location = $1", url2)));
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  EXPECT_TRUE(result.Wait());
}

// Perform a navigation during the the main mechanism of the monitor (in this
// case, waiting on network requests). It should cause the monitor to
// immediately complete.
IN_PROC_BROWSER_TEST_P(ActorGeneralPageStabilityTest,
                       NavigationDuringMonitoring) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  // Start a network request to block the monitor from completing.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  ASSERT_TRUE(ExecJs(web_contents(), "window.doFetch(() => {})"));
  fetch_response().WaitForRequest();

  // Wait for stability.
  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // Wait to ensure the monitor is blocking on network requests.
  Sleep(base::Seconds(1));
  EXPECT_FALSE(result.IsReady());

  // Navigating away should cause the monitor to complete.
  TestNavigationManager manager(web_contents(), url2);
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace("window.location = $1", url2)));
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  EXPECT_TRUE(result.Wait());
}

}  // namespace

}  // namespace actor
