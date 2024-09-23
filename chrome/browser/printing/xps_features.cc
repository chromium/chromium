// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/xps_features.h"

#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "base/feature_list.h"
#include "chrome/browser/printing/oop_features.h"
#include "printing/printing_features.h"
#endif

namespace printing {

bool IsXpsPrintCapabilityRequired() {
  // Require XPS printing to be used out-of-process.
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // TODO(crbug.com/40283514):  Incorporate policy override check.
  return ShouldPrintJobOop() &&
         (base::FeatureList::IsEnabled(features::kUseXpsForPrinting) ||
          base::FeatureList::IsEnabled(features::kUseXpsForPrintingFromPdf));
#else
  return false;
#endif
}

bool ShouldPrintUsingXps(bool source_is_pdf) {
  // Require XPS to be used out-of-process.
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // TODO(crbug.com/40283514):  Incorporate policy override check.
  return ShouldPrintJobOop() &&
         base::FeatureList::IsEnabled(source_is_pdf
                                          ? features::kUseXpsForPrintingFromPdf
                                          : features::kUseXpsForPrinting);
#else
  return false;
#endif
}

}  // namespace printing
