// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/pdf_content_reader.h"

#include <utility>

#include "components/pdf/browser/pdf_document_helper.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/mojom/pdf.mojom.h"

namespace save_to_drive {
namespace {

// Returns the PDF document helper for the given `render_frame_host`.
pdf::PDFDocumentHelper* GetPDFDocumentHelper(
    content::RenderFrameHost* render_frame_host) {
  pdf::PDFDocumentHelper* pdf_helper = nullptr;
  // Find the first frame host that has the PDFDocumentHelper.
  render_frame_host->ForEachRenderFrameHostWithAction(
      [&pdf_helper](content::RenderFrameHost* rfh) {
        pdf_helper = pdf::PDFDocumentHelper::GetForCurrentDocument(rfh);
        if (pdf_helper) {
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return pdf_helper;
}

}  // namespace

PDFContentReader::PDFContentReader(content::RenderFrameHost* render_frame_host,
                                   pdf::mojom::SaveRequestType request_type)
    : render_frame_host_(render_frame_host), request_type_(request_type) {
  CHECK(render_frame_host_);
}

PDFContentReader::~PDFContentReader() {
  Close();
}

void PDFContentReader::Open(OpenCallback callback) {
  pdf::PDFDocumentHelper* pdf_helper = GetPDFDocumentHelper(render_frame_host_);
  if (!pdf_helper) {
    std::move(callback).Run(false);
    return;
  }
  pdf_helper->GetSaveDataBufferHandlerForDrive(
      request_type_,
      base::BindOnce(&PDFContentReader::OnOpen, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void PDFContentReader::OnOpen(OpenCallback callback,
                              pdf::mojom::SaveDataBufferHandlerGetResultPtr
                                  save_data_buffer_handler_get_result) {
  if (!save_data_buffer_handler_get_result) {
    std::move(callback).Run(false);
    return;
  }
  remote_buffer_handler_.reset();
  remote_buffer_handler_.Bind(
      std::move(save_data_buffer_handler_get_result->handler));
  total_file_size_ = save_data_buffer_handler_get_result->total_file_size;
  std::move(callback).Run(true);
}

size_t PDFContentReader::GetSize() {
  return remote_buffer_handler_ ? total_file_size_ : 0;
}

void PDFContentReader::Read(uint32_t offset,
                            uint32_t size,
                            ContentReadCallback callback) {
  if (!remote_buffer_handler_) {
    std::move(callback).Run(mojo_base::BigBuffer());
    return;
  }
  remote_buffer_handler_->Read(offset, size, std::move(callback));
}

void PDFContentReader::Close() {
  remote_buffer_handler_.reset();
  total_file_size_ = 0;
}

}  // namespace save_to_drive
