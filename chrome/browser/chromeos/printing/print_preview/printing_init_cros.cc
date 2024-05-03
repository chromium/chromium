// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/printing_init_cros.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"
#else
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

namespace chromeos::printing {

void InitializePrintingForWebContents(content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(::features::kPrintPreviewCrosPrimary)) {
    return;
  }

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  PrintViewManagerCros::CreateForWebContents(web_contents);
#else
  PrintViewManagerCrosBasic::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
}

}  // namespace chromeos::printing
