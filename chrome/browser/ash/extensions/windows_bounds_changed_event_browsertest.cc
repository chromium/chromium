// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

using WindowsBoundsChangedEventTest = extensions::ExtensionApiTest;

// Tests that `chrome.windows.onBoundsChanged` happens only once for an
// interactive user drag to move a browser window.
IN_PROC_BROWSER_TEST_F(WindowsBoundsChangedEventTest, Move) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("windows/bounds")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ui::test::EventGenerator generator(
      browser()->window()->GetNativeWindow()->GetRootWindow());

  // Simulates a user drag to move the browser window.
  const gfx::Rect rect = browser()->window()->GetBounds();
  generator.MoveMouseTo(rect.top_center() + gfx::Vector2d(0, 5));

  generator.PressLeftButton();
  // Needs to use multiple mouse dragged events to check.
  generator.MoveMouseBy(10, 20, /*count=*/5);
  generator.ReleaseLeftButton();

  // Verifies that the window is actually moved.
  const gfx::Rect final_rect = browser()->window()->GetBounds();
  ASSERT_NE(rect, final_rect);

  listener.Reply(base::StringPrintf(
      R"({"top": %u, "left": %u, "width": %u, "height": %u})", final_rect.y(),
      final_rect.x(), final_rect.width(), final_rect.height()));

  ASSERT_TRUE(catcher.GetNextResult());
}

// Tests that `chrome.windows.onBoundsChanged` happens only once for an
// interactive user drag to resize a browser window.
IN_PROC_BROWSER_TEST_F(WindowsBoundsChangedEventTest, Resize) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("windows/bounds")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ui::test::EventGenerator generator(
      browser()->window()->GetNativeWindow()->GetRootWindow());

  // Simulates a user drag to resize the browser window.
  const gfx::Rect rect = browser()->window()->GetBounds();
  gfx::Point resize_handle(rect.right(), rect.bottom());
  generator.MoveMouseTo(resize_handle);

  generator.PressLeftButton();
  // Needs to use multiple mouse dragged events to check.
  generator.MoveMouseBy(-10, -20, /*count=*/5);
  generator.ReleaseLeftButton();

  // Verifies that the window is actually resized.
  const gfx::Rect final_rect = browser()->window()->GetBounds();
  ASSERT_NE(rect, final_rect);

  listener.Reply(base::StringPrintf(
      R"({"top": %u, "left": %u, "width": %u, "height": %u})", final_rect.y(),
      final_rect.x(), final_rect.width(), final_rect.height()));

  ASSERT_TRUE(catcher.GetNextResult());
}
