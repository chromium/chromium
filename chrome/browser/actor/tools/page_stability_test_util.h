// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_STABILITY_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_STABILITY_TEST_UTIL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;

namespace content {

class RenderFrameHost;
class WebContents;

}  // namespace content

namespace net::test_server {

class ControllableHttpResponse;

}  // namespace net::test_server

namespace actor {

// TODO(linnan) - Update page_stability_browsertest.cc and
// observation_delay_controller_browsertest.cc to use this test harness.
class PageStabilityTest : public InProcessBrowserTest {
 public:
  PageStabilityTest();
  PageStabilityTest(const PageStabilityTest&) = delete;
  PageStabilityTest& operator=(const PageStabilityTest&) = delete;
  ~PageStabilityTest() override;

  void SetUpOnMainThread() override;

 protected:
  void Sleep(base::TimeDelta delta);

  content::WebContents* web_contents();
  content::RenderFrameHost* main_frame();

  GURL GetPageStabilityTestURL();

  std::string GetOutputText();

  net::test_server::ControllableHttpResponse& fetch_response();

  void InitiateNetworkRequest();

  void Respond(std::string_view text);

  mojo::Remote<mojom::PageStabilityMonitor> CreatePageStabilityMonitor(
      bool supports_paint_stability = true);

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse> fetch_response_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_STABILITY_TEST_UTIL_H_
