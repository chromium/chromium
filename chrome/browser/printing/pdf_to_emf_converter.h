// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PDF_TO_EMF_CONVERTER_H_
#define CHROME_BROWSER_PRINTING_PDF_TO_EMF_CONVERTER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"

class GURL;

namespace printing {

class MetafilePlayer;
struct PdfRenderSettings;

class PdfConverter {
 public:
  using StartCallback = base::OnceCallback<void(uint32_t page_count)>;
  using GetPageCallback =
      base::RepeatingCallback<void(uint32_t page_index,
                                   float scale_factor,
                                   std::unique_ptr<MetafilePlayer> file)>;
  virtual ~PdfConverter();

  // Starts conversion of PDF provided as `data`. Calls `start_callback`
  // with positive `page_count`. `page_count` is 0 if initialization failed.
  static std::unique_ptr<PdfConverter> StartPdfConverter(
      scoped_refptr<base::RefCountedMemory> data,
      const PdfRenderSettings& conversion_settings,
      const std::optional<bool>& use_skia,
      const GURL& url,
      StartCallback start_callback);

  // Requests conversion of the page. `page_index` is 0-base page index for the
  // PDF provided in Start() call.
  // Calls `get_page_callback` after conversion. The `file` parameter in the
  // callback is non-null if the conversion succeeded.
  virtual void GetPage(uint32_t page_index,
                       GetPageCallback get_page_callback) = 0;
};

// Object used by tests to exercise the temporary file creation failure code
// path. As long as this object is alive, the PdfConverter will fail when
// creating temporary files.
class ScopedSimulateFailureCreatingTempFileForTests {
 public:
  ScopedSimulateFailureCreatingTempFileForTests();
  ~ScopedSimulateFailureCreatingTempFileForTests();
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PDF_TO_EMF_CONVERTER_H_
