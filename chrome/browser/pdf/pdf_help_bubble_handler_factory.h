// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_HELP_BUBBLE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_PDF_PDF_HELP_BUBBLE_HANDLER_FACTORY_H_

#include "ui/base/interaction/element_identifier.h"

namespace pdf {

// Future home of a factory that creates `HelpBubbleHandler`.
class PdfHelpBubbleHandlerFactory {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPdfInkSignaturesDrawElementId);
};

}  // namespace pdf

#endif  // CHROME_BROWSER_PDF_PDF_HELP_BUBBLE_HANDLER_FACTORY_H_
