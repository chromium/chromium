// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/pdf_content_reader.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "chrome/browser/save_to_drive/save_to_drive_utils.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/base/big_buffer.h"
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
  next_expected_offset_ = 0;
  trailing_bytes_.clear();
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
  remote_buffer_handler_->Read(
      offset, size,
      base::BindOnce(&PDFContentReader::OnRead, weak_ptr_factory_.GetWeakPtr(),
                     offset, std::move(callback)));
}

void PDFContentReader::OnRead(uint32_t offset,
                              ContentReadCallback callback,
                              mojo_base::BigBuffer buffer) {
  // The received `buffer` may be backed by shared memory that is still mapped
  // in the sender, so copy it once into local storage and use only the copy
  // from this point on.
  mojo_base::BigBuffer copy = buffer.Clone();
  buffer = mojo_base::BigBuffer();

  if (offset == 0) {
    next_expected_offset_ = 0;
    trailing_bytes_.clear();
  }

  const size_t end = static_cast<size_t>(offset) + copy.size();
  bool valid = copy.size() != 0 && offset == next_expected_offset_ &&
               end <= total_file_size_;
  if (valid && offset == 0) {
    valid = ValidatePdfMagic(base::span(copy));
  }
  if (valid) {
    // Retain a bounded window of the most recently read bytes so that the
    // end-of-file marker can be located even when it straddles a chunk
    // boundary.
    const auto tail = base::span(copy).last(
        std::min<size_t>(copy.size(), kPdfEofSearchWindow));
    trailing_bytes_.insert(trailing_bytes_.end(), tail.begin(), tail.end());
    if (trailing_bytes_.size() > kPdfEofSearchWindow) {
      trailing_bytes_.erase(trailing_bytes_.begin(),
                            trailing_bytes_.begin() +
                                (trailing_bytes_.size() - kPdfEofSearchWindow));
    }
    if (end == total_file_size_) {
      valid = ValidatePdfEof(trailing_bytes_);
    }
  }

  if (!valid) {
    std::move(callback).Run(mojo_base::BigBuffer());
    return;
  }
  next_expected_offset_ = end;
  std::move(callback).Run(std::move(copy));
}

void PDFContentReader::Close() {
  remote_buffer_handler_.reset();
  total_file_size_ = 0;
  next_expected_offset_ = 0;
  trailing_bytes_.clear();
}

}  // namespace save_to_drive
