// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_pref_names.h"

namespace prefs {

// A list of domains that have access to file:// URLs in the PDF viewer.
const char kPdfLocalFileAccessAllowedForDomains[] =
    "pdf_local_file_access_allowed_for_domains";

// Boolean pref to control whether to use Skia renderer in the PDF viewer.
const char kPdfUseSkiaRendererEnabled[] = "pdf.enable_skia";

// Boolean pref to control whether to use the OOPIF PDF viewer.
const char kPdfViewerOutOfProcessIframeEnabled[] =
    "pdf.enable_out_of_process_iframe_viewer";

// Boolean pref to control whether to enable support for XFA forms in PDF
// viewer.
const char kPdfXfaFormsEnabled[] = "pdf.xfa_forms_enabled";

}  // namespace prefs
