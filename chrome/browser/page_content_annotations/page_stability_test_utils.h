// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_STABILITY_TEST_UTILS_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_STABILITY_TEST_UTILS_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace net::test_server {
class ControllableHttpResponse;
}  // namespace net::test_server

namespace page_content_annotations {

// Base class for browser tests that need to verify page stability.
// Provides utilities for simulating network fetches and main thread busy work.
class PageStabilityBrowserTestBase : public PlatformBrowserTest {
 public:
  PageStabilityBrowserTestBase();
  PageStabilityBrowserTestBase(const PageStabilityBrowserTestBase&) = delete;
  PageStabilityBrowserTestBase& operator=(const PageStabilityBrowserTestBase&) =
      delete;
  ~PageStabilityBrowserTestBase() override;

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

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse> fetch_response_;
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_STABILITY_TEST_UTILS_H_
