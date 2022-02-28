// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_test_util.h"

#include "base/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/focus_changed_observer.h"
#include "content/public/test/hit_test_region_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf_extension_test_util {

testing::AssertionResult EnsurePDFHasLoaded(
    const content::ToRenderFrameHost& frame,
    bool wait_for_hit_test_data) {
  bool load_success = false;
  if (!content::ExecuteScriptAndExtractBool(
          frame,
          R"(window.addEventListener('message', event => {
            if (event.origin !==
                    'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai') {
              return;
            }
            if (event.data.type === 'documentLoaded') {
              window.domAutomationController.send(
                  event.data.load_state === 'success');
            } else if (event.data.type === 'passwordPrompted') {
              window.domAutomationController.send(true);
            }
          });
          document.getElementsByTagName('embed')[0].postMessage(
              {type: 'initialize'});)",
          &load_success)) {
    return testing::AssertionFailure()
           << "Cannot communicate with PDF extension.";
  }

  if (wait_for_hit_test_data) {
    frame.render_frame_host()->ForEachRenderFrameHost(
        base::BindRepeating<void(content::RenderFrameHost*)>(
            content::WaitForHitTestData));
  }

  return load_success ? testing::AssertionSuccess()
                      : (testing::AssertionFailure() << "Load failed.");
}

gfx::Point ConvertPageCoordToScreenCoord(content::WebContents* contents,
                                         const gfx::Point& point) {
  if (!contents) {
    ADD_FAILURE() << "contents needs to be non-null";
    return point;
  }
  if (!content::ExecuteScript(
          contents,
          "var visiblePage = viewer.viewport.getMostVisiblePage();"
          "var visiblePageDimensions ="
          "    viewer.viewport.getPageScreenRect(visiblePage);"
          "var viewportPosition = viewer.viewport.position;"
          "var offsetParent = viewer.shadowRoot.querySelector('#container');"
          "var scrollParent = viewer.shadowRoot.querySelector('#main');"
          "var screenOffsetX = visiblePageDimensions.x - viewportPosition.x +"
          "    scrollParent.offsetLeft + offsetParent.offsetLeft;"
          "var screenOffsetY = visiblePageDimensions.y - viewportPosition.y +"
          "    scrollParent.offsetTop + offsetParent.offsetTop;"
          "var linkScreenPositionX ="
          "    Math.floor(" +
              base::NumberToString(point.x()) +
              " * viewer.viewport.internalZoom_" +
              " + screenOffsetX);"
              "var linkScreenPositionY ="
              "    Math.floor(" +
              base::NumberToString(point.y()) +
              " * viewer.viewport.internalZoom_" +
              " +"
              "    screenOffsetY);")) {
    ADD_FAILURE() << "Error executing script";
    return point;
  }

  int x;
  if (!content::ExecuteScriptAndExtractInt(
          contents, "window.domAutomationController.send(linkScreenPositionX);",
          &x)) {
    ADD_FAILURE() << "error getting linkScreenPositionX";
    return point;
  }

  int y;
  if (!content::ExecuteScriptAndExtractInt(
          contents, "window.domAutomationController.send(linkScreenPositionY);",
          &y)) {
    ADD_FAILURE() << "error getting linkScreenPositionY";
    return point;
  }

  return {x, y};
}

void SetInputFocusOnPlugin(content::WebContents* guest_contents) {
  content::FocusChangedObserver focus_observer(guest_contents);
  content::SimulateMouseClickAt(
      guest_contents, blink::WebInputEvent::kNoModifiers,
      blink::WebMouseEvent::Button::kLeft,
      ConvertPageCoordToScreenCoord(guest_contents, {1, 1}));
  focus_observer.Wait();
}

}  // namespace pdf_extension_test_util
