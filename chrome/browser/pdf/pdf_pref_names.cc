// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_pref_names.h"

namespace prefs {

// A list of domains that have access to file:// URLs in PDF Viewer.
const char kPdfLocalFileAccessAllowedForDomains[] =
    "pdf_local_file_access_allowed_for_domains";

// Boolean pref to control whether to use Skia renderer in the PDF viewer
const char kPdfUseSkiaRendererEnabled[] = "pdf.enable_skia";

}  // namespace prefs
