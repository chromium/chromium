// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_compositor_util.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/printing/xps_features.h"
#endif

namespace printing {

mojom::PrintCompositor::DocumentType GetCompositorDocumentType() {
#if BUILDFLAG(IS_WIN)
  // Callers using the Print Compositor means that the source is modifiable
  // (e.g., not PDF).
  return ShouldPrintUsingXps(/*source_is_pdf=*/false)
             ? mojom::PrintCompositor::DocumentType::kXPS
             : mojom::PrintCompositor::DocumentType::kPDF;
#else
  return mojom::PrintCompositor::DocumentType::kPDF;
#endif
}

}  // namespace printing
