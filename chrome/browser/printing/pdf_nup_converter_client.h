// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PDF_NUP_CONVERTER_CLIENT_H_
#define CHROME_BROWSER_PRINTING_PDF_NUP_CONVERTER_CLIENT_H_

#include <map>

#include "chrome/services/printing/public/mojom/pdf_nup_converter.mojom.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace printing {

// Class to manage print requests and their communication with pdf N-up
// converter service.
// Each N-up conversion request has a separate interface pointer to connect
// with remote service. The request and its printing results are tracked by its
// document cookie.
class PdfNupConverterClient
    : public content::WebContentsUserData<PdfNupConverterClient> {
 public:
  explicit PdfNupConverterClient(content::WebContents* web_contents);

  PdfNupConverterClient(const PdfNupConverterClient&) = delete;
  PdfNupConverterClient& operator=(const PdfNupConverterClient&) = delete;

  ~PdfNupConverterClient() override;

  void DoNupPdfConvert(
      int document_cookie,
      uint32_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area,
      std::vector<base::ReadOnlySharedMemoryRegion> pdf_page_regions,
      mojom::PdfNupConverter::NupPageConvertCallback callback);
  void DoNupPdfDocumentConvert(
      int document_cookie,
      uint32_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area,
      base::ReadOnlySharedMemoryRegion src_pdf_document,
      mojom::PdfNupConverter::NupDocumentConvertCallback callback);

 private:
  friend class content::WebContentsUserData<PdfNupConverterClient>;
  void OnDidNupPdfDocumentConvert(
      int document_cookie,
      mojom::PdfNupConverter::NupDocumentConvertCallback callback,
      mojom::PdfNupConverter::Status status,
      base::ReadOnlySharedMemoryRegion region);

  // Get the mojo::Remote or create a new one if none exists.
  mojo::Remote<mojom::PdfNupConverter>& GetPdfNupConverterRemote(int cookie);

  // Remove an existing mojo::Remote from `pdf_nup_converter_map_`.
  void RemovePdfNupConverterRemote(int cookie);

  mojo::Remote<mojom::PdfNupConverter> CreatePdfNupConverterRemote();

  // Stores the mapping between document cookies and their corresponding
  // mojo::Remote.
  std::map<int, mojo::Remote<mojom::PdfNupConverter>> pdf_nup_converter_map_;

  // Indicates whether to use Skia renderer is enabled by enterprise policy.
  // A nullopt value indicates that such enterprise policy is not set.
  std::optional<bool> skia_policy_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PDF_NUP_CONVERTER_CLIENT_H_
