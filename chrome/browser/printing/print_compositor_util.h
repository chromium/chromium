// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_COMPOSITOR_UTIL_H_
#define CHROME_BROWSER_PRINTING_PRINT_COMPOSITOR_UTIL_H_

#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"

namespace printing {

// Determine the type of document which the Print Compositor should generate
// for the print job.
mojom::PrintCompositor::DocumentType GetCompositorDocumentType();

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_COMPOSITOR_UTIL_H_
