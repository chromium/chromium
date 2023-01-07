// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_EXTENSION_UTIL_H_
#define CHROME_BROWSER_PDF_PDF_EXTENSION_UTIL_H_

#include <string>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "pdf/buildflags.h"

#if !BUILDFLAG(ENABLE_PDF)
#error "PDF must be enabled"
#endif

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

// Adds all strings used by the PDF Viewer depending on the provided `context`.
void AddStrings(PdfViewerContext context, base::Value::Dict* dict);

// Adds additional data used by the PDF Viewer UI in `dict`, for example
// whether certain features are enabled/disabled.
// `enable_annotations` only applies on platforms that supports annotations.
void AddAdditionalData(bool enable_annotations, base::Value::Dict* dict);

}  // namespace pdf_extension_util

#endif  // CHROME_BROWSER_PDF_PDF_EXTENSION_UTIL_H_
