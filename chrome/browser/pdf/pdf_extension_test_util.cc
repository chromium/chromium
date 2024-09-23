// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_test_util.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/pdf/common/pdf_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/focus_changed_observer.h"
#include "content/public/test/hit_test_region_observer.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "pdf/pdf_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace pdf_extension_test_util {

namespace {

bool IsPluginFrame(content::RenderFrameHost& frame) {
  if (!frame.GetProcess()->IsPdf())
    return false;

  EXPECT_TRUE(frame.IsCrossProcessSubframe());
  return true;
}

}  // namespace

content::RenderFrameHost* GetPdfExtensionHostFromEmbedder(
    content::RenderFrameHost* embedder_host,
    bool allow_multiple_frames) {
  // Return nullptr if multiple frames aren't allowed and there's more than one
  // child.
  if (!allow_multiple_frames && content::ChildFrameAt(embedder_host, 1)) {
    return nullptr;
  }

  // The extension host be the first child of the embedder host.
  content::RenderFrameHost* child_host =
      content::ChildFrameAt(embedder_host, 0);
  return child_host &&
                 IsPdfExtensionOrigin(child_host->GetLastCommittedOrigin())
             ? child_host
             : nullptr;
}

content::RenderFrameHost* GetOnlyPdfExtensionHost(
    content::WebContents* contents) {
  std::vector<content::RenderFrameHost*> extension_hosts =
      GetPdfExtensionHosts(contents);
  return extension_hosts.size() == 1 ? extension_hosts[0] : nullptr;
}

std::vector<content::RenderFrameHost*> GetPdfExtensionHosts(
    content::WebContents* contents) {
  std::vector<content::RenderFrameHost*> extension_hosts;
  contents->ForEachRenderFrameHost(
      [&extension_hosts](content::RenderFrameHost* host) {
        if (IsPdfExtensionOrigin(host->GetLastCommittedOrigin())) {
          extension_hosts.push_back(host);
        }
      });
  return extension_hosts;
}

content::RenderFrameHost* GetOnlyPdfPluginFrame(
    content::WebContents* contents) {
  std::vector<content::RenderFrameHost*> plugin_frames =
      GetPdfPluginFrames(contents);
  return plugin_frames.size() == 1 ? plugin_frames[0] : nullptr;
}

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

size_t CountPdfPluginProcesses(const Browser* browser) {
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
    const content::ToRenderFrameHost& frame) {
  return EnsurePDFHasLoadedWithOptions(frame, EnsurePDFHasLoadedOptions());
}

testing::AssertionResult EnsurePDFHasLoadedWithOptions(
    const content::ToRenderFrameHost& frame,
    const EnsurePDFHasLoadedOptions& options) {
  // OOPIF PDF intentionally doesn't support postMessage() API for embedders.
  // postMessage() can still be used if the script is injected into the
  // extension frame.
  static constexpr char kEnsurePdfHasLoadedScript[] = R"(
    new Promise(resolve => {
      window.addEventListener('message', event => {
        if (event.origin !==
                'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai') {
          return;
        }
        if (event.data.type === 'documentLoaded') {
          resolve(event.data.load_state === 'success');
        } else if (event.data.type === 'passwordPrompted') {
          resolve(true);
        }
      });
      %s.postMessage(
        {type: 'initialize'});
    });
  )";

  static constexpr char kOopifPostMessageTarget[] = "window";
  static constexpr char kGuestViewPostMessageTarget[] =
      "document.getElementsByTagName($1)[0]";

  bool use_oopif =
      base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif);

  // For OOPIF PDF viewer, the target frame should be the PDF extension frame.
  // Otherwise, it should be whatever frame was given.
  content::RenderFrameHost* frame_rfh = frame.render_frame_host();
  content::RenderFrameHost* target_frame =
      use_oopif ? GetPdfExtensionHostFromEmbedder(frame_rfh,
                                                  options.allow_multiple_frames)
                : frame_rfh;

  if (use_oopif && !target_frame) {
    return testing::AssertionFailure() << "Failed to get PDF extension frame.";
  }

  const std::string post_message_target =
      use_oopif ? kOopifPostMessageTarget
                : content::JsReplace(kGuestViewPostMessageTarget,
                                     options.pdf_element);
  bool load_success =
      content::EvalJs(target_frame,
                      base::StringPrintf(kEnsurePdfHasLoadedScript,
                                         post_message_target.c_str()))
          .ExtractBool();

  if (!load_success) {
    return testing::AssertionFailure() << "Load failed.";
  }

  if (options.wait_for_hit_test_data) {
    frame.render_frame_host()->ForEachRenderFrameHost(
        [](content::RenderFrameHost* render_frame_host) {
          return content::WaitForHitTestData(render_frame_host);
        });
  }

  return testing::AssertionSuccess();
}

gfx::Point ConvertPageCoordToScreenCoord(
    content::ToRenderFrameHost guest_main_frame,
    const gfx::Point& point) {
  if (!guest_main_frame.render_frame_host()) {
    ADD_FAILURE() << "The guest main frame needs to be non-null";
    return point;
  }

  static constexpr char kScript[] = R"(
    var visiblePage = viewer.viewport.getMostVisiblePage();
    var visiblePageDimensions = viewer.viewport.getPageScreenRect(visiblePage);
    var viewportPosition = viewer.viewport.position;
    var offsetParent = viewer.shadowRoot.querySelector('#container');
    var scrollParent = viewer.shadowRoot.querySelector('#main');
    var screenOffsetX = visiblePageDimensions.x - viewportPosition.x +
        scrollParent.offsetLeft + offsetParent.offsetLeft;
    var screenOffsetY = visiblePageDimensions.y - viewportPosition.y +
        scrollParent.offsetTop + offsetParent.offsetTop;
    var linkScreenPositionX = Math.floor(%d * viewer.viewport.internalZoom_ +
        screenOffsetX);
    var linkScreenPositionY = Math.floor(%d * viewer.viewport.internalZoom_ +
        screenOffsetY);
  )";
  if (!content::ExecJs(guest_main_frame,
                       base::StringPrintf(kScript, point.x(), point.y()))) {
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
  SetInputFocusOnPlugin(guest->GetGuestMainFrame(),
                        guest->embedder_web_contents());
}

void SetInputFocusOnPlugin(content::RenderFrameHost* extension_host,
                           content::WebContents* embedder_web_contents) {
  content::WaitForHitTestData(extension_host);

  const gfx::Point point_in_root_coords =
      extension_host->GetView()->TransformPointToRootCoordSpace(
          ConvertPageCoordToScreenCoord(extension_host, {1, 1}));

  content::FocusChangedObserver focus_observer(
      content::WebContents::FromRenderFrameHost(extension_host));
  content::SimulateMouseClickAt(
      embedder_web_contents, blink::WebInputEvent::kNoModifiers,
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
