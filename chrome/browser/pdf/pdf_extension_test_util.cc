// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_test_util.h"

#include <stddef.h>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/focus_changed_observer.h"
#include "content/public/test/hit_test_region_observer.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf_extension_test_util {

namespace {

bool IsPluginFrame(content::RenderFrameHost& frame) {
  if (!frame.GetProcess()->IsPdf())
    return false;

  EXPECT_TRUE(frame.IsCrossProcessSubframe());
  return true;
}

}  // namespace

std::vector<content::RenderFrameHost*> GetPdfPluginFrames(
    content::WebContents* contents) {
  std::vector<content::RenderFrameHost*> plugin_frames;
  contents->ForEachRenderFrameHost(
      [&plugin_frames](content::RenderFrameHost* frame) {
        if (IsPluginFrame(*frame))
          plugin_frames.push_back(frame);
      });
  return plugin_frames;
}

size_t CountPdfPluginProcesses(Browser* browser) {
  base::flat_set<content::RenderProcessHost*> pdf_processes;

  const TabStripModel* tab_strip = browser->tab_strip_model();
  for (int tab = 0; tab < tab_strip->count(); ++tab) {
    for (content::RenderFrameHost* plugin_frame :
         GetPdfPluginFrames(tab_strip->GetWebContentsAt(tab))) {
      pdf_processes.insert(plugin_frame->GetProcess());
    }
  }

  return pdf_processes.size();
}

testing::AssertionResult EnsurePDFHasLoaded(
    const content::ToRenderFrameHost& frame,
    bool wait_for_hit_test_data,
    const std::string& pdf_element) {
  bool load_success = content::EvalJs(frame, content::JsReplace(R"(
            new Promise(resolve => {
              window.addEventListener('message', event => {
                if (event.origin !==
                        'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai') {
                  return;
                }
                if (event.data.type === 'documentLoaded') {
                  resolve(
                      event.data.load_state === 'success');
                } else if (event.data.type === 'passwordPrompted') {
                  resolve(true);
                }
              });
              document.getElementsByTagName($1)[0].postMessage(
                {type: 'initialize'});
            });
            )",
                                                                pdf_element))
                          .ExtractBool();

  if (wait_for_hit_test_data) {
    frame.render_frame_host()->ForEachRenderFrameHost(
        [](content::RenderFrameHost* render_frame_host) {
          return content::WaitForHitTestData(render_frame_host);
        });
  }

  return load_success ? testing::AssertionSuccess()
                      : (testing::AssertionFailure() << "Load failed.");
}

gfx::Point ConvertPageCoordToScreenCoord(
    content::ToRenderFrameHost guest_main_frame,
    const gfx::Point& point) {
  if (!guest_main_frame.render_frame_host()) {
    ADD_FAILURE() << "The guest main frame needs to be non-null";
    return point;
  }
  if (!content::ExecuteScript(
          guest_main_frame,
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

  int x =
      content::EvalJs(guest_main_frame, "linkScreenPositionX;").ExtractInt();

  int y =
      content::EvalJs(guest_main_frame, "linkScreenPositionY;").ExtractInt();

  return {x, y};
}

void SetInputFocusOnPlugin(extensions::MimeHandlerViewGuest* guest) {
  auto* guest_main_frame = guest->GetGuestMainFrame();
  content::WaitForHitTestData(guest_main_frame);

  const gfx::Point point_in_root_coords =
      guest_main_frame->GetView()->TransformPointToRootCoordSpace(
          ConvertPageCoordToScreenCoord(guest_main_frame, {1, 1}));

  content::FocusChangedObserver focus_observer(guest->web_contents());
  content::SimulateMouseClickAt(
      guest->embedder_web_contents(), blink::WebInputEvent::kNoModifiers,
      blink::WebMouseEvent::Button::kLeft, point_in_root_coords);
  focus_observer.Wait();
}

extensions::MimeHandlerViewGuest* GetOnlyMimeHandlerView(
    content::WebContents* embedder_contents) {
  extensions::MimeHandlerViewGuest* result = nullptr;
  embedder_contents->ForEachRenderFrameHost([&result](
                                                content::RenderFrameHost* rfh) {
    auto* guest = extensions::MimeHandlerViewGuest::FromRenderFrameHost(rfh);
    if (guest && guest->GetGuestMainFrame() == rfh) {
      // Assume exactly one MimeHandlerView.
      EXPECT_FALSE(result);
      result = guest;
    }
  });
  return result;
}

}  // namespace pdf_extension_test_util
