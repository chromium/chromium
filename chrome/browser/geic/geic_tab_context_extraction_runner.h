// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_TAB_CONTEXT_EXTRACTION_RUNNER_H_
#define CHROME_BROWSER_GEIC_GEIC_TAB_CONTEXT_EXTRACTION_RUNNER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/geic/geic.mojom.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/weak_document_ptr.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

namespace geic {

class GeicBrowserHostImpl;

// Coordinates parallel extraction of tab context (inner text,
// AnnotatedPageContent, and screenshot) for GEiC grounding.
//
// Lifetime is managed by its owner (typically GeicBrowserHostImpl), which
// retains ownership until extraction completes or the host is torn down.
class TabContextExtractionRunner {
 public:
  TabContextExtractionRunner(
      base::WeakPtr<GeicBrowserHostImpl> host,
      content::WebContents* contents,
      mojom::TabMetadataPtr metadata,
      mojom::TabContextOptions options,
      mojom::GeicBrowserHost::GetContextFromFocusedTabCallback callback);

  TabContextExtractionRunner(const TabContextExtractionRunner&) = delete;
  TabContextExtractionRunner& operator=(const TabContextExtractionRunner&) =
      delete;
  ~TabContextExtractionRunner();

  // Starts parallel extraction of requested context from the web contents.
  // When complete, validates tab state via `host_` and invokes `callback_`.
  void Run();

  // Calculates the target screenshot dimensions while preserving the viewport's
  // aspect ratio and avoiding upscaling. Returns an empty gfx::Size() if no
  // downscaling is required.
  static gfx::Size CalculateScreenshotSize(const gfx::Size& view_size,
                                           uint32_t max_width,
                                           uint32_t max_height);

 private:
  void GetInnerText(content::RenderFrameHost* primary_main_frame,
                    base::RepeatingClosure closure);
  void DidGetInnerText(
      base::RepeatingClosure closure,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  void GetAnnotatedPageContent(content::WebContents* contents,
                               base::RepeatingClosure closure);
  void DidGetAnnotatedPageContent(
      base::RepeatingClosure closure,
      optimization_guide::AIPageContentResultOrError result);

  void CaptureScreenshot(content::RenderFrameHost* primary_main_frame,
                         base::RepeatingClosure closure);
  void DidCaptureScreenshot(base::RepeatingClosure closure,
                            const content::CopyFromSurfaceResult& result);
  void OnScreenshotEncoded(base::RepeatingClosure closure,
                           std::optional<std::vector<uint8_t>> jpeg_data);

  void OnAllExtractionsComplete();

  base::WeakPtr<GeicBrowserHostImpl> host_;
  content::WeakDocumentPtr document_ptr_;
  mojom::TabContextOptions options_;
  mojom::GeicBrowserHost::GetContextFromFocusedTabCallback callback_;
  mojom::TabContextDataPtr data_;
  base::WeakPtrFactory<TabContextExtractionRunner> weak_ptr_factory_{this};
};

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_TAB_CONTEXT_EXTRACTION_RUNNER_H_
