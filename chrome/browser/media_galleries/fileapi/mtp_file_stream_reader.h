// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_FILE_STREAM_READER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_FILE_STREAM_READER_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemContext;
}

class MTPFileStreamReader : public storage::FileStreamReader {
 public:
  MTPFileStreamReader(storage::FileSystemContext* file_system_context,
                      const storage::FileSystemURL& url,
                      int64_t initial_offset,
                      const base::Time& expected_modification_time,
                      bool do_media_header_validation);

  MTPFileStreamReader(const MTPFileStreamReader&) = delete;
  MTPFileStreamReader& operator=(const MTPFileStreamReader&) = delete;

  ~MTPFileStreamReader() override;

  // FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  void FinishValidateMediaHeader(
      net::IOBuffer* header_buf,
      net::IOBuffer* buf, int buf_len,
      const base::File::Info& file_info,
      int header_bytes_read);

  void FinishRead(const base::File::Info& file_info, int bytes_read);

  void FinishGetLength(const base::File::Info& file_info);

  void CallReadCallbackwithPlatformFileError(base::File::Error file_error);

  void CallGetLengthCallbackWithPlatformFileError(base::File::Error file_error);

  void ReadBytes(
      const storage::FileSystemURL& url,
      const scoped_refptr<net::IOBuffer>& buf,
      int64_t offset,
      int buf_len,
      MTPDeviceAsyncDelegate::ReadBytesSuccessCallback success_callback);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  storage::FileSystemURL url_;
  int64_t current_offset_;
  const base::Time expected_modification_time_;
  net::CompletionOnceCallback read_callback_;
  net::Int64CompletionOnceCallback get_length_callback_;

  bool media_header_validated_;

  base::WeakPtrFactory<MTPFileStreamReader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_FILE_STREAM_READER_H_
