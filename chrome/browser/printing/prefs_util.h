// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PREFS_UTIL_H_
#define CHROME_BROWSER_PRINTING_PREFS_UTIL_H_

#include <optional>

#include "printing/buildflags/buildflags.h"
#include "ui/gfx/geometry/size.h"

class PrefService;

namespace printing {

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Parse the printing.paper_size_default preference.
std::optional<gfx::Size> ParsePaperSizeDefault(const PrefService& prefs);
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
// Determine the out-of-process printing support preference.
std::optional<bool> OopPrintingPref();
#endif

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PREFS_UTIL_H_
