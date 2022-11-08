// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_screen_ai_annotator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

namespace screen_ai {

namespace {

class MockAXScreenAIAnnotator : public AXScreenAIAnnotator {
 public:
  explicit MockAXScreenAIAnnotator(content::BrowserContext* context)
      : AXScreenAIAnnotator(context) {}

  // TODO(https://1278249): Consider making Screen AI component available for
  // tests. The test should refrain from trying to bind to it while it is not
  // available.
  MOCK_METHOD(void,
              BindToScreenAIService,
              (content::BrowserContext*),
              (override));

  MOCK_METHOD(void,
              OnScreenshotReceived,
              (const ui::AXTreeID& ax_tree_id,
               const base::Time& start_time,
               gfx::Image snapshot),
              (override));
};

}  // namespace

using ScreenAIServiceTest = InProcessBrowserTest;

// TODO(https://crbug.com/1278249): Test is disabled as it requires delayed
// connection to the service, but for PDF use case we need immediate connection
// or adding extra boilerplate code to trigger it. Since PDF is the primary
// goal, the test is disabled until the issue is fixed.
IN_PROC_BROWSER_TEST_F(ScreenAIServiceTest, DISABLED_ScreenshotTest) {
  MockAXScreenAIAnnotator* annotator =
      new MockAXScreenAIAnnotator(browser()->profile());
  // TODO(https://crbug.com/1278249): Pass |annotator| to
  // AXScreenAIAnnotatorFactory to be used for test.

  base::RunLoop run_loop;

  EXPECT_CALL(*annotator, BindToScreenAIService);
  EXPECT_CALL(*annotator, OnScreenshotReceived)
      .WillOnce([&run_loop](const ui::AXTreeID& ax_tree_id,
                            const base::Time& start_time, gfx::Image snapshot) {
        EXPECT_FALSE(snapshot.IsEmpty());
        EXPECT_GT(snapshot.Size().width(), 0);
        EXPECT_GT(snapshot.Size().height(), 0);
        run_loop.Quit();
      });

  browser()->RunScreenAIAnnotator();
  run_loop.Run();

  // TODO(https://crbug.com/1278249): Add a test that mocks
  // |OnScreenshotReceived| and returns the expected proto, and observe its
  // application on the accessibility tree(s).
}

}  // namespace screen_ai
