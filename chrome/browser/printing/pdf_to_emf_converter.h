// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PDF_TO_EMF_CONVERTER_H_
#define CHROME_BROWSER_PRINTING_PDF_TO_EMF_CONVERTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted_memory.h"

namespace printing {

class MetafilePlayer;
struct PdfRenderSettings;

class PdfConverter {
 public:
  using StartCallback = base::OnceCallback<void(int page_count)>;
  using GetPageCallback =
      base::RepeatingCallback<void(int page_number,
                                   float scale_factor,
                                   std::unique_ptr<MetafilePlayer> file)>;
  virtual ~PdfConverter();

  // Starts conversion of PDF provided as |data|. Calls |start_callback|
  // with positive |page_count|. |page_count| is 0 if initialization failed.
  static std::unique_ptr<PdfConverter> StartPdfConverter(
      scoped_refptr<base::RefCountedMemory> data,
      const PdfRenderSettings& conversion_settings,
      StartCallback start_callback);

  // Requests conversion of the page. |page_number| is 0-base page number in
  // PDF provided in Start() call.
  // Calls |get_page_callback| after conversion. |emf| of callback in not NULL
  // if conversion succeeded.
  virtual void GetPage(int page_number, GetPageCallback get_page_callback) = 0;
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
