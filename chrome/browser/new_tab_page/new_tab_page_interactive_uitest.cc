// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "base/command_line.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#endif

namespace {
template <typename Value>
bool ContainsAll(const std::set<Value>& first, const std::set<Value>& second) {
  std::set<Value> difference;
  std::set_difference(second.begin(), second.end(), first.begin(), first.end(),
                      std::inserter(difference, difference.end()));
  return difference.empty();
}
}  // namespace

class NewTabPageTest : public InProcessBrowserTest,
                       public content::DevToolsAgentHostClient {
 public:
  NewTabPageTest() {
    features_.InitWithFeatures(
        {}, {ntp_features::kNtpOneGoogleBar, ntp_features::kNtpShortcuts,
             ntp_features::kNtpMiddleSlotPromo});
  }

  ~NewTabPageTest() override = default;

  // content::DevToolsAgentHostClient:
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    std::optional<base::Value> maybe_parsed_message =
        base::JSONReader::Read(std::string_view(
            reinterpret_cast<const char*>(message.data()), message.size()));
    CHECK(maybe_parsed_message.has_value());
    base::Value::Dict parsed_message =
        std::move(maybe_parsed_message.value()).TakeDict();
    auto* method = parsed_message.FindString("method");
    if (!method) {
      return;
    }
    if (*method == "Network.requestWillBeSent") {
      // We track all started network requests to match them to corresponding
      // load completions.
      auto request_id =
          *parsed_message.FindStringByDottedPath("params.requestId");
      auto url =
          GURL(*parsed_message.FindStringByDottedPath("params.request.url"));
      loading_resources_[request_id] = url;
    } else if (*method == "Network.loadingFinished") {
      // Cross off network request from pending loads. Once all loads have
      // completed we potentially unblock the test from waiting.
      auto request_id =
          *parsed_message.FindStringByDottedPath("params.requestId");
      auto url = loading_resources_[request_id];
      loading_resources_.erase(request_id);
      loaded_resources_.insert(url);
      if (loading_resources_.empty() &&
          ContainsAll(loaded_resources_, required_resources_) &&
          network_load_quit_closure_) {
        std::move(network_load_quit_closure_).Run();
      }
    } else if (*method == "DOM.attributeModified") {
      // Check if lazy load has completed and potentially unblock waiting test.
      auto node_id = *parsed_message.FindIntByDottedPath("params.nodeId");
      auto name = *parsed_message.FindStringByDottedPath("params.name");
      auto value = *parsed_message.FindStringByDottedPath("params.value");
      if (node_id == 3 && name == "lazy-loaded" && value == "true") {
        lazy_loaded_ = true;
      }
      if (lazy_loaded_ && lazy_load_quit_closure_) {
        std::move(lazy_load_quit_closure_).Run();
      }
    }
  }

  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    browser_view_ = static_cast<BrowserView*>(browser()->window());
    contents_ = browser_view_->GetActiveWebContents();

    // Wait for initial about:blank to load and attach DevTools before
    // navigating to the NTP.
    ASSERT_TRUE(WaitForLoadStop(contents_));
    agent_host_ = content::DevToolsAgentHost::GetOrCreateFor(contents_);
    agent_host_->AttachClient(this);
    // Enable network events. We use completion of network loads as a signal
    // of steady state.
    agent_host_->DispatchProtocolMessage(
        this, base::as_bytes(base::make_span(
                  std::string("{\"id\": 1, \"method\": \"Network.enable\"}"))));
    // Enable DOM events. We determine completion of lazy load by reading a DOM
    // attribute.
    agent_host_->DispatchProtocolMessage(
        this, base::as_bytes(base::make_span(
                  std::string("{\"id\": 2, \"method\": \"DOM.enable\"}"))));

    NavigateParams params(browser(), GURL(chrome::kChromeUINewTabPageURL),
                          ui::PageTransition::PAGE_TRANSITION_FIRST);
    Navigate(&params);
    ASSERT_TRUE(WaitForLoadStop(contents_));

    // Request the DOM. We will only receive DOM events for DOMs we have
    // requested.
    agent_host_->DispatchProtocolMessage(
        this, base::as_bytes(base::make_span(std::string(
                  "{\"id\": 3, \"method\": \"DOM.getDocument\"}"))));
    // Read initial value of lazy-loaded in case lazy load is already complete
    // at this point in time.
    lazy_loaded_ =
        EvalJs(contents_.get(),
               "document.documentElement.hasAttribute('lazy-loaded')",
               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1)
            .ExtractBool();
  }

  // Blocks until the NTP has completed lazy load.
  void WaitForLazyLoad() {
    if (lazy_loaded_) {
      return;
    }
    base::RunLoop run_loop;
    lazy_load_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Blocks until all network requests have completed and |required_resources|
  // have been loaded.
  void WaitForNetworkLoad(const std::set<GURL>& required_resources) {
    if (loading_resources_.empty() &&
        ContainsAll(loaded_resources_, required_resources)) {
      return;
    }
    required_resources_ = required_resources;
    base::RunLoop run_loop;
    network_load_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Blocks until the next animation frame.
  void WaitForAnimationFrame() {
    CHECK(EvalJs(contents_.get(),
                 "new Promise(r => requestAnimationFrame(() => r(true)))",
                 content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1)
              .ExtractBool());
  }

  // If pixel verification is enabled(--browser-ui-tests-verify-pixels)
  // verifies pixels using Skia Gold. Returns true on success or if the pixel
  // verification is skipped.
  bool VerifyUi(const std::string& screenshot_prefix,
                const std::string& screenshot_name) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kVerifyPixels)) {
      views::ViewSkiaGoldPixelDiff pixel_diff(screenshot_prefix);
      return pixel_diff.CompareViewScreenshot(
          screenshot_name, browser_view_->contents_web_view());
    }
#endif
    return true;
  }

 protected:
  base::test::ScopedFeatureList features_;
  raw_ptr<content::WebContents, DanglingUntriaged> contents_;
  raw_ptr<BrowserView, DanglingUntriaged> browser_view_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::map<std::string, GURL> loading_resources_;
  std::set<GURL> loaded_resources_;
  std::set<GURL> required_resources_;
  base::OnceClosure network_load_quit_closure_;
  bool lazy_loaded_ = false;
  base::OnceClosure lazy_load_quit_closure_;
};

// TODO(crbug.com/40197892): NewTabPageTest.LandingPagePixelTest is flaky on
// ubsan.
// TODO(crbug.com/40874245): NewTabPageTest.LandingPagePixelTest is failing on
// Win11 Tests x64.
// TODO(crbug.com/40893756): It's also found flaky on Linux Tests, Linux Tests
// (Wayland), linux-lacros-tester-rel, Mac12 Tests.
IN_PROC_BROWSER_TEST_F(NewTabPageTest, DISABLED_LandingPagePixelTest) {
  WaitForLazyLoad();
  // By default WaitForNetworkLoad waits for all resources that have started
  // loading at this point. However, sometimes not all required resources have
  // started loading yet. Specifically, images set via -webkit-mask-image cause
  // grief. To work around this we specify resources we explicitly wait for even
  // if they haven't yet started loading.
  // TODO(crbug.com/40197892): This is brittle and will rot easily. Find a
  // better way to capture those resources.
  WaitForNetworkLoad({GURL("chrome://new-tab-page/icons/icon_pencil.svg")});
  WaitForAnimationFrame();

  EXPECT_TRUE(VerifyUi("NewTabPageTest", "LandingPagePixelTest"));
}
