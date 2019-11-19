// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_nup_converter_client.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/pdf_nup_converter.mojom.h"

namespace printing {

PdfNupConverterClient::PdfNupConverterClient(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

PdfNupConverterClient::~PdfNupConverterClient() {}

void PdfNupConverterClient::DoNupPdfConvert(
    int document_cookie,
    uint32_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area,
    std::vector<base::ReadOnlySharedMemoryRegion> pdf_page_regions,
    mojom::PdfNupConverter::NupPageConvertCallback callback) {
  auto& nup_converter = GetPdfNupConverterRemote(document_cookie);
  nup_converter->NupPageConvert(pages_per_sheet, page_size, printable_area,
                                std::move(pdf_page_regions),
                                std::move(callback));
}

void PdfNupConverterClient::DoNupPdfDocumentConvert(
    int document_cookie,
    uint32_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area,
    base::ReadOnlySharedMemoryRegion src_pdf_document,
    mojom::PdfNupConverter::NupDocumentConvertCallback callback) {
  auto& nup_converter = GetPdfNupConverterRemote(document_cookie);
  nup_converter->NupDocumentConvert(
      pages_per_sheet, page_size, printable_area, std::move(src_pdf_document),
      base::BindOnce(&PdfNupConverterClient::OnDidNupPdfDocumentConvert,
                     base::Unretained(this), document_cookie,
                     std::move(callback)));
}

void PdfNupConverterClient::OnDidNupPdfDocumentConvert(
    int document_cookie,
    mojom::PdfNupConverter::NupDocumentConvertCallback callback,
    mojom::PdfNupConverter::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  RemovePdfNupConverterRemote(document_cookie);
  std::move(callback).Run(status, std::move(region));
}

mojo::Remote<mojom::PdfNupConverter>&
PdfNupConverterClient::GetPdfNupConverterRemote(int cookie) {
  auto iter = pdf_nup_converter_map_.find(cookie);
  if (iter != pdf_nup_converter_map_.end()) {
    DCHECK(iter->second.is_bound());
    return iter->second;
  }

  auto iterator =
      pdf_nup_converter_map_.emplace(cookie, CreatePdfNupConverterRemote())
          .first;
  return iterator->second;
}

void PdfNupConverterClient::RemovePdfNupConverterRemote(int cookie) {
  size_t erased = pdf_nup_converter_map_.erase(cookie);
  DCHECK_EQ(erased, 1u);
}

mojo::Remote<mojom::PdfNupConverter>
PdfNupConverterClient::CreatePdfNupConverterRemote() {
  mojo::Remote<mojom::PdfNupConverter> pdf_nup_converter;
  GetPrintingService()->BindPdfNupConverter(
      pdf_nup_converter.BindNewPipeAndPassReceiver());
  pdf_nup_converter->SetWebContentsURL(web_contents_->GetLastCommittedURL());
  return pdf_nup_converter;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PdfNupConverterClient)

}  // namespace printing
