// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_EXTENSION_UTIL_H_
#define CHROME_BROWSER_PDF_PDF_EXTENSION_UTIL_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "pdf/buildflags.h"
#include "ui/base/webui/resource_path.h"

#if !BUILDFLAG(ENABLE_PDF)
#error "PDF must be enabled"
#endif

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
}

namespace pdf_extension_util {

// Return the extensions manifest for PDF. The manifest is loaded from
// browser_resources.grd and certain fields are replaced based on what chrome
// flags are enabled.
std::string GetManifest();

// Represents the context within which the PDF Viewer runs.
enum class PdfViewerContext {
  kPdfViewer,
  kPrintPreview,
  kAll,
};

// Gets all strings used by the PDF Viewer depending on the provided `context`.
base::Value::Dict GetStrings(PdfViewerContext context);

// Gets additional data used by the PDF Viewer UI. e.g. whether certain features
// are enabled/disabled.
base::Value::Dict GetAdditionalData(content::BrowserContext* context);

// Returns the entries in `resources` that are relevant to `context`.
// `context` must be `PdfViewerContext::kPdfViewer` or
// `PdfViewerContext::kPrintPreview`.
std::vector<webui::ResourcePath> GetResources(PdfViewerContext context);

// For OOPIF PDF viewer only. Returns true if successfully sends a save event to
// the PDF viewer, or false otherwise. Only successful if the PDF plugin should
// handle the save event.
bool MaybeDispatchSaveEvent(content::RenderFrameHost* embedder_host);

// Dispatches an extension event to the PDF viewer containing an updated PDF URL
// that was intended to be navigated to so the viewer can update its viewport
// based on the fragment of that URL.
void DispatchShouldUpdateViewportEvent(content::RenderFrameHost* embedder_host,
                                       const GURL& new_pdf_url);

}  // namespace pdf_extension_util

#endif  // CHROME_BROWSER_PDF_PDF_EXTENSION_UTIL_H_
