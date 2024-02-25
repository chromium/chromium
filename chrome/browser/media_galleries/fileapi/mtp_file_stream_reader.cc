// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/mtp_file_stream_reader.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_system_context.h"

using storage::FileStreamReader;

namespace {

}  // namespace

MTPFileStreamReader::MTPFileStreamReader(
    storage::FileSystemContext* file_system_context,
    const storage::FileSystemURL& url,
    int64_t initial_offset,
    const base::Time& expected_modification_time,
    bool do_media_header_validation)
    : file_system_context_(file_system_context),
      url_(url),
      current_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      media_header_validated_(!do_media_header_validation) {}

MTPFileStreamReader::~MTPFileStreamReader() {
}

int MTPFileStreamReader::Read(net::IOBuffer* buf,
                              int buf_len,
                              net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url_);
  if (!delegate)
    return net::ERR_FAILED;

  read_callback_ = std::move(callback);
  if (!media_header_validated_) {
    scoped_refptr<net::IOBuffer> header_buf;
    int header_buf_len = 0;

    if (current_offset_ == 0 && buf_len >= net::kMaxBytesToSniff) {
      // If requested read includes all the header bytes, we read directly to
      // the original buffer, and validate the header bytes within that.
      header_buf = buf;
      header_buf_len = buf_len;
    } else {
      // Otherwise, make a special request for the header.
      header_buf =
          base::MakeRefCounted<net::IOBufferWithSize>(net::kMaxBytesToSniff);
      header_buf_len = net::kMaxBytesToSniff;
    }

    ReadBytes(url_, header_buf.get(), 0, header_buf_len,
              base::BindOnce(&MTPFileStreamReader::FinishValidateMediaHeader,
                             weak_factory_.GetWeakPtr(),
                             base::RetainedRef(header_buf),
                             base::RetainedRef(buf), buf_len));
    return net::ERR_IO_PENDING;
  }

  ReadBytes(url_, buf, current_offset_, buf_len,
            base::BindOnce(&MTPFileStreamReader::FinishRead,
                           weak_factory_.GetWeakPtr()));

  return net::ERR_IO_PENDING;
}

int64_t MTPFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url_);
  if (!delegate)
    return net::ERR_FAILED;

  get_length_callback_ = std::move(callback);
  delegate->GetFileInfo(
      url_.path(),
      base::BindOnce(&MTPFileStreamReader::FinishGetLength,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MTPFileStreamReader::CallGetLengthCallbackWithPlatformFileError,
          weak_factory_.GetWeakPtr()));

  return net::ERR_IO_PENDING;
}

void MTPFileStreamReader::FinishValidateMediaHeader(
    net::IOBuffer* header_buf,
    net::IOBuffer* buf, int buf_len,
    const base::File::Info& file_info,
    int header_bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_GE(header_bytes_read, 0);
  base::File::Error error = NativeMediaFileUtil::BufferIsMediaHeader(
      header_buf, header_bytes_read);
  if (error != base::File::FILE_OK) {
    CallReadCallbackwithPlatformFileError(error);
    return;
  }

  media_header_validated_ = true;

  // Finish the read immediately if we've already finished reading into the
  // originally requested buffer.
  if (header_buf == buf)
    return FinishRead(file_info, header_bytes_read);

  // Header buffer isn't the same as the original read buffer. Make a separate
  // request for that.
  ReadBytes(url_, buf, current_offset_, buf_len,
            base::BindOnce(&MTPFileStreamReader::FinishRead,
                           weak_factory_.GetWeakPtr()));
}

void MTPFileStreamReader::FinishRead(const base::File::Info& file_info,
                                     int bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!VerifySnapshotTime(expected_modification_time_, file_info)) {
    std::move(read_callback_).Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }

  DCHECK_GE(bytes_read, 0);
  current_offset_ += bytes_read;
  std::move(read_callback_).Run(bytes_read);
}

void MTPFileStreamReader::FinishGetLength(
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!VerifySnapshotTime(expected_modification_time_, file_info)) {
    std::move(get_length_callback_).Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }

  std::move(get_length_callback_).Run(file_info.size);
}

void MTPFileStreamReader::CallReadCallbackwithPlatformFileError(
    base::File::Error file_error) {
  std::move(read_callback_).Run(net::FileErrorToNetError(file_error));
}

void MTPFileStreamReader::CallGetLengthCallbackWithPlatformFileError(
    base::File::Error file_error) {
  std::move(get_length_callback_).Run(net::FileErrorToNetError(file_error));
}

void MTPFileStreamReader::ReadBytes(
    const storage::FileSystemURL& url,
    const scoped_refptr<net::IOBuffer>& buf,
    int64_t offset,
    int buf_len,
    MTPDeviceAsyncDelegate::ReadBytesSuccessCallback success_callback) {
  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    std::move(read_callback_).Run(net::ERR_FAILED);
    return;
  }

  delegate->ReadBytes(
      url.path(), buf, offset, buf_len, std::move(success_callback),
      base::BindRepeating(
          &MTPFileStreamReader::CallReadCallbackwithPlatformFileError,
          weak_factory_.GetWeakPtr()));
}
