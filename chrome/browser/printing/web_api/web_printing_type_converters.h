// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom-forward.h"

namespace printing {
struct PrinterSemanticCapsAndDefaults;
}  // namespace printing

namespace mojo {

template <>
struct TypeConverter<blink::mojom::WebPrinterAttributesPtr,
                     printing::PrinterSemanticCapsAndDefaults> {
  static blink::mojom::WebPrinterAttributesPtr Convert(
      const printing::PrinterSemanticCapsAndDefaults&);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_TYPE_CONVERTERS_H_
