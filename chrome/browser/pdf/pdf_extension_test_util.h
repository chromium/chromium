// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_
#define CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_

#include <stddef.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

class Browser;

namespace content {
class RenderFrameHost;
class ToRenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class MimeHandlerViewGuest;
}  // namespace extensions

namespace gfx {
class Point;
}  // namespace gfx

namespace pdf_extension_test_util {

// Gets the PDF extension host for a given `WebContents`. There should only be
// one extension host in `contents`, otherwise returns nullptr.
content::RenderFrameHost* GetOnlyPdfExtensionHost(
    content::WebContents* contents);

// Gets all the PDF extension hosts for a given `WebContents`.
std::vector<content::RenderFrameHost*> GetPdfExtensionHosts(
    content::WebContents* contents);

// Gets the PDF plugin frame for a given `WebContents`. There should only be
// one plugin frame in `contents`, otherwise returns nullptr.
content::RenderFrameHost* GetOnlyPdfPluginFrame(content::WebContents* contents);

// Gets all the PDF plugin frames for a given `WebContents`.
std::vector<content::RenderFrameHost*> GetPdfPluginFrames(
    content::WebContents* contents);

// Counts the total number of unique PDF plugin processes.
size_t CountPdfPluginProcesses(Browser* browser);

// Ensures, inside the given `frame`, that a PDF has either finished
// loading or prompted a password. The result indicates success if the PDF loads
// successfully, otherwise it indicates failure. If it doesn't finish loading,
// the test will hang.
//
// Tests that attempt to send mouse/pointer events should pass `true` for
// `wait_for_hit_test_data`, otherwise the necessary hit test data may not be
// available by the time this function returns. (This behavior is the default,
// since the delay should be small.)
[[nodiscard]] testing::AssertionResult EnsurePDFHasLoaded(
    const content::ToRenderFrameHost& frame,
    bool wait_for_hit_test_data = true,
    const std::string& pdf_element = "embed");

gfx::Point ConvertPageCoordToScreenCoord(
    content::ToRenderFrameHost guest_main_frame,
    const gfx::Point& point);

// Synchronously sets the input focus on the plugin frame by clicking on the
// top-left corner of a PDF document.
// TODO(crbug.com/1445746): Remove this once there are no more existing use
// cases.
void SetInputFocusOnPlugin(extensions::MimeHandlerViewGuest* guest);

// Synchronously sets the input focus on the plugin frame by clicking on the
// top-left corner of a PDF document.
void SetInputFocusOnPlugin(content::RenderFrameHost* extension_host,
                           content::WebContents* embedder_web_contents);

// Returns the `MimeHandlerViewGuest` embedded in `embedder_contents`. If more
// than one `MimeHandlerViewGuest` is found, the test fails.
extensions::MimeHandlerViewGuest* GetOnlyMimeHandlerView(
    content::WebContents* embedder_contents);

}  // namespace pdf_extension_test_util

#endif  // CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_
