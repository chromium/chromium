// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PREFS_UTIL_H_
#define CHROME_BROWSER_PRINTING_PREFS_UTIL_H_

#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

class PrefService;

namespace printing {

// Parse the printing.paper_size_default preference.
absl::optional<gfx::Size> ParsePaperSizeDefault(const PrefService& prefs);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
// Determine if out-of-process printing support is enabled.
bool IsOopPrintingEnabled();

// Determine if printing a job should be done out-of-process.
bool ShouldPrintJobOop();
#endif

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PREFS_UTIL_H_
