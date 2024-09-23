// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_PRINT_PREVIEW_PDF_STORE_H_
#define ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_PRINT_PREVIEW_PDF_STORE_H_

#include <map>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"

namespace ash::printing::print_preview {

// PrintPreviewPdfStore is the singleton class responsible for in-memory storage
// of PDF data for CrOS Print Preview. When fetching PDFs to display in the UI,
// only untrusted WebUI access is expected.
class PrintPreviewPdfStore {
 public:
  static PrintPreviewPdfStore* GetInstance();

  PrintPreviewPdfStore();
  PrintPreviewPdfStore(const PrintPreviewPdfStore&) = delete;
  PrintPreviewPdfStore& operator=(const PrintPreviewPdfStore&) = delete;
  ~PrintPreviewPdfStore();

  // `token` specifies the instance of Print Preview. `page_index` is the
  // zero-based index of the PDF page to store. Returns NULLPTR if either
  // `token` or `page_index` is not found.
  void InsertPdfData(const base::UnguessableToken& token,
                     int page_index,
                     scoped_refptr<base::RefCountedMemory> data);

  // `token` specifies the instance of Print Preview. `page_index` is the
  // zero-based index of the PDF page to retrieve.
  scoped_refptr<base::RefCountedMemory> GetPdfData(
      const base::UnguessableToken& token,
      int page_index) const;

 private:
  // Data map keyed by UnguessableToken to specify the Print Preview instance,
  // and int for the zero-based page index.
  std::map<base::UnguessableToken,
           std::map<int, scoped_refptr<base::RefCountedMemory>>>
      pdf_data_map_;
};

}  // namespace ash::printing::print_preview

#endif  // ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_PRINT_PREVIEW_PDF_STORE_H_
