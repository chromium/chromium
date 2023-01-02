// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_READ_WRITER_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_READ_WRITER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/fusebox/fusebox.pb.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"

namespace fusebox {

// Caches a storage::FileStreamReader (and FileStreamWriter) and their offsets
// so that, when consecutive reads (or consecutive writes) are adjacent (the
// second one starts where the first one ends), the FileStreamReader (or
// FileStreamWriter) is re-used.
//
// When serving a "stateless" I/O API that passes an offset each time (such as
// the FUSE API), but the underlying storage::AsyncFileUtil doesn't support
// seeking, then re-using a cached FileStreamReader can often avoid "Shlemiel
// the Painter" quadratic performance. See
// https://wiki.c2.com/?ShlemielThePainter
//
// Each ReadWriter instance lives entirely on the I/O thread. When its owner
// lives on a different thread, wrap the ReadWriter in a base::SequenceBound.
class ReadWriter {
 public:
  using Read2Callback =
      base::OnceCallback<void(const Read2ResponseProto& response)>;
  using Write2Callback =
      base::OnceCallback<void(const Write2ResponseProto& response)>;

  explicit ReadWriter(const storage::FileSystemURL& fs_url);
  ~ReadWriter();

  void Read(scoped_refptr<storage::FileSystemContext> fs_context,
            int64_t offset,
            int64_t length,
            Read2Callback callback);

  void Write(scoped_refptr<storage::FileSystemContext> fs_context,
             scoped_refptr<net::StringIOBuffer> buffer,
             int64_t offset,
             int length,
             Write2Callback callback);

 private:
  void OnRead(Read2Callback callback,
              scoped_refptr<storage::FileSystemContext> fs_context,
              std::unique_ptr<storage::FileStreamReader> fs_reader,
              scoped_refptr<net::IOBuffer> buffer,
              int64_t offset,
              int length);

  void OnWrite(Write2Callback callback,
               scoped_refptr<storage::FileSystemContext> fs_context,
               std::unique_ptr<storage::FileStreamWriter> fs_writer,
               scoped_refptr<net::IOBuffer> buffer,
               int64_t offset,
               int length);

  const storage::FileSystemURL fs_url_;

  std::unique_ptr<storage::FileStreamReader> fs_reader_;
  // Unused whenever fs_reader_ is nullptr.
  int64_t read_offset_ = -1;

  std::unique_ptr<storage::FileStreamWriter> fs_writer_;
  // Unused whenever fs_writer_ is nullptr.
  int64_t write_offset_ = -1;

  // TODO(b/255703917): snapshot management.

  base::WeakPtrFactory<ReadWriter> weak_ptr_factory_{this};
};

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_READ_WRITER_H_
