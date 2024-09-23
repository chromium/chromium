// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PDF_BLOB_DATA_FLATTENER_H_
#define CHROME_BROWSER_PRINTING_PDF_BLOB_DATA_FLATTENER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/printing/public/mojom/pdf_flattener.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

class Profile;

namespace printing {

class MetafileSkia;

struct FlattenPdfResult {
  FlattenPdfResult(std::unique_ptr<MetafileSkia> flattened_pdf_in,
                   uint32_t page_count);
  ~FlattenPdfResult();

  // `flattened_pdf` is never null.
  std::unique_ptr<MetafileSkia> flattened_pdf;

  // `page_count` is always strictly greater than zero.
  uint32_t page_count;
};

// PdfBlobDataFlattener is responsible for reading and flattening PDF files from
// Blob objects into instances of MetafileSkia. This class is only built on
// ChromeOS; however, there are no technical limitations in expanding it to
// other platforms too.
class PdfBlobDataFlattener {
 public:
  using ReadAndFlattenPdfCallback =
      base::OnceCallback<void(std::unique_ptr<FlattenPdfResult> result)>;

  explicit PdfBlobDataFlattener(Profile* profile);
  ~PdfBlobDataFlattener();

  // We want to have an ability to disable PDF flattening for unit tests as
  // printing::mojom::PdfFlattener requires real browser instance to be able to
  // handle requests.
  static base::AutoReset<bool> DisablePdfFlatteningForTesting();

  // Runs `callback` with a MetafileSkia with the contents of the flattened PDF,
  // or `nullptr` if an error has been encountered.
  // `this` is guaranteed to outlive `callback`.
  void ReadAndFlattenPdf(mojo::PendingRemote<blink::mojom::Blob> blob,
                         ReadAndFlattenPdfCallback callback);

 private:
  void OnPdfRead(ReadAndFlattenPdfCallback callback,
                 std::string data,
                 int64_t blob_total_size);
  void OnPdfFlattened(ReadAndFlattenPdfCallback callback,
                      mojom::FlattenPdfResultPtr result);

  const raw_ref<Profile> profile_;
  mojo::Remote<mojom::PdfFlattener> flattener_;

  base::WeakPtrFactory<PdfBlobDataFlattener> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PDF_BLOB_DATA_FLATTENER_H_
