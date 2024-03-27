// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_OOP_FEATURES_H_
#define CHROME_BROWSER_PRINTING_OOP_FEATURES_H_

#include "printing/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_OOP_PRINTING), "OOPPD must be enabled");

namespace printing {

// This file contains queries to determine behavior related to the
// out-of-process printing feature, as to whether the feature itself or certain
// parts of it should be enabled or not.  It encapsulates the results from
// feature flag, its parameters, and any policy overrides.

// Determine if out-of-process printing support is enabled.
bool IsOopPrintingEnabled();

// Determine if printing a job should be done out-of-process.
bool ShouldPrintJobOop();

// Determine if a Print Backend service should be launched early, after the
// browser has finished its startup.
bool ShouldEarlyStartPrintBackendService();

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_OOP_FEATURES_H_
