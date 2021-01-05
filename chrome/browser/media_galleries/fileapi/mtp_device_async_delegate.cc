// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"

#include "net/base/io_buffer.h"

MTPDeviceAsyncDelegate::ReadBytesRequest::ReadBytesRequest(
    uint32_t file_id,
    net::IOBuffer* buf,
    int64_t offset,
    int buf_len,
    ReadBytesSuccessCallback success_callback,
    ErrorCallback error_callback)
    : file_id(file_id),
      buf(buf),
      offset(offset),
      buf_len(buf_len),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

MTPDeviceAsyncDelegate::ReadBytesRequest::ReadBytesRequest(
    ReadBytesRequest&& other)
    : file_id(other.file_id),
      buf(other.buf),
      offset(other.offset),
      buf_len(other.buf_len),
      success_callback(std::move(other.success_callback)),
      error_callback(std::move(other.error_callback)) {}

MTPDeviceAsyncDelegate::ReadBytesRequest::~ReadBytesRequest() = default;
