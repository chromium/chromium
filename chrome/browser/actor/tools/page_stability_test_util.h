// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_STABILITY_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_STABILITY_TEST_UTIL_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_content_annotations/page_stability_test_utils.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace actor {

// Actor-specific version of PageStabilityBrowserTestBase that enables
// relevant features.
class PageStabilityTest
    : public page_content_annotations::PageStabilityBrowserTestBase {
 public:
  PageStabilityTest();
  PageStabilityTest(const PageStabilityTest&) = delete;
  PageStabilityTest& operator=(const PageStabilityTest&) = delete;
  ~PageStabilityTest() override;

  void SetUpOnMainThread() override;

 protected:
  mojo::Remote<page_content_annotations::mojom::PageStabilityMonitor>
  CreatePageStabilityMonitor(bool supports_paint_stability = true);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_STABILITY_TEST_UTIL_H_
