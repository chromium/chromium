// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_READ_WRITER_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_READ_WRITER_H_

#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
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
// Each ReadWriter instance lives entirely on the I/O thread, but its owner
// (and the callbacks) must live on the UI thread (and wrap the ReadWriter in a
// base::SequenceBound).
//
// The owner is also responsible for ensuring that only one operation is in
// flight at any one time. "An operation" starts with a Read or Write call and
// ends just before the corresponding callback is run.
class ReadWriter {
 public:
  using Close2Callback =
      base::OnceCallback<void(const Close2ResponseProto& response)>;
  using FlushCallback =
      base::OnceCallback<void(const FlushResponseProto& response)>;
  using Read2Callback =
      base::OnceCallback<void(const Read2ResponseProto& response)>;
  using Write2Callback =
      base::OnceCallback<void(const Write2ResponseProto& response)>;

  // |use_temp_file| is for the case when the |fs_url| storage system does not
  // support incremental writes, only atomic "here's the entire contents"
  // writes, such as MTP (Media Transfer Protocol, e.g. phones attached to a
  // Chromebook, which is a file-oriented rather than block-oriented protocol).
  // In this case, the Write calls are diverted to a temporary file and the
  // Close call saves that temporary file to |fs_url|.
  //
  // |temp_file_starts_with_copy| states whether to initialize that temporary
  // file with a copy of the underlying file. If |temp_file_starts_with_copy|
  // false, the temporary file is initially empty. When |use_temp_file| is
  // false, |temp_file_starts_with_copy| is ignored.
  ReadWriter(const storage::FileSystemURL& fs_url,
             const std::string& profile_path,
             bool use_temp_file,
             bool temp_file_starts_with_copy);
  ~ReadWriter();

  void Close(scoped_refptr<storage::FileSystemContext> fs_context,
             Close2Callback callback);

  void Flush(scoped_refptr<storage::FileSystemContext> fs_context,
             FlushCallback callback);

  void Read(scoped_refptr<storage::FileSystemContext> fs_context,
            int64_t offset,
            int64_t length,
            Read2Callback callback);

  void Write(scoped_refptr<storage::FileSystemContext> fs_context,
             scoped_refptr<net::StringIOBuffer> buffer,
             int64_t offset,
             int length,
             Write2Callback callback);

  // The int is a POSIX error code.
  using WriteTempFileResult = std::pair<base::ScopedFD, int>;

 private:
  // Saves the |temp_file_| to the |fs_url_|.
  void Save();

  // The CallXxx and OnXxx methods are static (but take a WeakPtr) so that the
  // callback will run even if the WeakPtr is invalidated.

  static void OnDefaultFlush(
      base::WeakPtr<ReadWriter> weak_ptr,
      FlushCallback callback,
      scoped_refptr<storage::FileSystemContext> fs_context,
      int flush_posix_error_code);

  static void OnEOFFlushBeforeActualClose(
      base::WeakPtr<ReadWriter> weak_ptr,
      Close2Callback callback,
      scoped_refptr<storage::FileSystemContext> fs_context,
      std::unique_ptr<storage::FileStreamWriter> fs_writer,
      int flush_posix_error_code);

  static void OnTempFileInitialized(base::WeakPtr<ReadWriter> weak_ptr,
                                    scoped_refptr<net::StringIOBuffer> buffer,
                                    int64_t offset,
                                    int length,
                                    Write2Callback callback,
                                    base::expected<base::ScopedFD, int> result);

  static void CallWriteTempFile(base::WeakPtr<ReadWriter> weak_ptr,
                                scoped_refptr<net::StringIOBuffer> buffer,
                                int64_t offset,
                                int length,
                                Write2Callback callback);

  static void OnRead(base::WeakPtr<ReadWriter> weak_ptr,
                     Read2Callback callback,
                     scoped_refptr<storage::FileSystemContext> fs_context,
                     std::unique_ptr<storage::FileStreamReader> fs_reader,
                     scoped_refptr<net::IOBuffer> buffer,
                     int64_t offset,
                     int length);

  static void OnWriteTempFile(base::WeakPtr<ReadWriter> weak_ptr,
                              Write2Callback callback,
                              WriteTempFileResult result);

  static void OnEOFFlushBeforeCallWriteDirect(
      base::WeakPtr<ReadWriter> weak_ptr,
      Write2Callback callback,
      scoped_refptr<storage::FileSystemContext> fs_context,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t offset,
      int length,
      std::unique_ptr<storage::FileStreamWriter> fs_writer,
      int flush_posix_error_code);

  void CallWriteDirect(Write2Callback callback,
                       scoped_refptr<storage::FileSystemContext> fs_context,
                       std::unique_ptr<storage::FileStreamWriter> fs_writer,
                       scoped_refptr<net::IOBuffer> buffer,
                       int64_t offset,
                       int length);

  static void OnWriteDirect(
      base::WeakPtr<ReadWriter> weak_ptr,
      Write2Callback callback,
      scoped_refptr<storage::FileSystemContext> fs_context,
      std::unique_ptr<storage::FileStreamWriter> fs_writer,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t offset,
      int length);

  const storage::FileSystemURL fs_url_;
  const std::string profile_path_;

  std::unique_ptr<storage::FileStreamReader> fs_reader_;
  // Unused (and set to -1) whenever fs_reader_ is nullptr. When std::move'ing
  // (or otherwise changing) the fs_reader_, we therefore assign (via = or
  // std::exchange) to read_offset_ at the same time.
  int64_t read_offset_ = -1;

  std::unique_ptr<storage::FileStreamWriter> fs_writer_;
  // Unused (and set to -1) whenever fs_writer_ is nullptr. When std::move'ing
  // (or otherwise changing) the fs_writer_, we therefore assign (via = or
  // std::exchange) to write_offset_ at the same time.
  int64_t write_offset_ = -1;

  scoped_refptr<storage::FileSystemContext> close2_fs_context_;
  Close2Callback close2_callback_;

  base::ScopedFD temp_file_;

  // The first (if any) write error we encounter. When non-zero, all future
  // Write calls fail and Save-on-Close is a no-op (other than running the
  // Close2Callback).
  int write_posix_error_code_ = 0;

  // True when the FD in |temp_file_| has been loaned out to a separate thread
  // (separate from the content::BrowserThread::IO thread that this lives on,
  // which should not be used for blocking I/O).
  bool is_loaning_temp_file_scoped_fd_ = false;

  bool is_in_flight_ = false;
  bool closed_ = false;
  bool created_temp_file_ = false;
  // storage::FileStreamWriter::Flush takes a storage::FlushMode parameter.
  // This bool field is about calling with FlushMode::kEndOfFile, not with
  // FlushMode::kDefault.
  bool fs_writer_needs_eof_flushing_ = false;

  const bool use_temp_file_;
  const bool temp_file_starts_with_copy_;

  base::WeakPtrFactory<ReadWriter> weak_ptr_factory_{this};
};

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_READ_WRITER_H_
