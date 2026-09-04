// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_TAB_CONTEXT_TEST_UTIL_H_
#define CHROME_BROWSER_GEIC_GEIC_TAB_CONTEXT_TEST_UTIL_H_

#include <memory>

namespace content {
class WebContents;
}  // namespace content

namespace geic {

// Test helper that binds fake Mojo agents (InnerTextAgent, AIPageContentAgent)
// on a WebContents' primary main frame for context extraction tests.
class TabContextTestHelper {
 public:
  explicit TabContextTestHelper(content::WebContents* contents);
  ~TabContextTestHelper();

  TabContextTestHelper(const TabContextTestHelper&) = delete;
  TabContextTestHelper& operator=(const TabContextTestHelper&) = delete;

 private:
  class FakeInnerTextAgent;
  class FakeAIPageContentAgent;

  std::unique_ptr<FakeInnerTextAgent> inner_text_agent_;
  std::unique_ptr<FakeAIPageContentAgent> ai_page_content_agent_;
};

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_TAB_CONTEXT_TEST_UTIL_H_
