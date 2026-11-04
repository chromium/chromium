// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_PDF_CONTENT_READER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_PDF_CONTENT_READER_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/mojom/pdf.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace save_to_drive {

class PDFContentReader;

// A `ContentReader` implementation for PDF files. This class is used to read
// the content of a PDF file. It uses the `SaveDataBufferHandler` mojo interface
// to read the PDF file in chunks.
class PDFContentReader : public ContentReader {
 public:
  PDFContentReader(content::RenderFrameHost* render_frame_host,
                   pdf::mojom::SaveRequestType request_type);

  ~PDFContentReader() override;

  // ContentReader:
  void Open(OpenCallback callback) override;
  size_t GetSize() override;
  void Read(uint32_t offset,
            uint32_t size,
            ContentReadCallback callback) override;
  void Close() override;

 private:
  friend class PDFContentReaderTest;

  void OnOpen(OpenCallback callback,
              pdf::mojom::SaveDataBufferHandlerGetResultPtr result);
  void OnRead(uint32_t offset,
              ContentReadCallback callback,
              mojo_base::BigBuffer buffer);

  // The handler to read the data to save to Google Drive. This is only valid
  // after the file is opened successfully. This is reset when the file is
  // closed.
  mojo::Remote<pdf::mojom::SaveDataBufferHandler> remote_buffer_handler_;

  size_t total_file_size_ = 0;

  // The expected `offset` of the next call to `Read()`. Reads must start at
  // zero and proceed sequentially; restarting from zero is allowed.
  size_t next_expected_offset_ = 0;

  // The most recently read trailing bytes, used to verify the end-of-file
  // marker on the final chunk.
  std::vector<uint8_t> trailing_bytes_;

  // The `RenderFrameHost` associated to the frame containing the PDF plugin.
  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  const pdf::mojom::SaveRequestType request_type_;

  base::WeakPtrFactory<PDFContentReader> weak_ptr_factory_{this};
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_PDF_CONTENT_READER_H_
