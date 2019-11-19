// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/input_stream_reader.h"

#include "android_webview/browser/input_stream.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"

using content::BrowserThread;

namespace android_webview {

InputStreamReader::InputStreamReader(android_webview::InputStream* stream)
    : stream_(stream) {
  DCHECK(stream);
}

InputStreamReader::~InputStreamReader() {
}

int InputStreamReader::Seek(const net::HttpByteRange& byte_range) {
  int content_size = 0;
  net::HttpByteRange verified_byte_range(byte_range);

  int error_code = VerifyRequestedRange(&verified_byte_range, &content_size);
  if (error_code != net::OK)
    return error_code;

  error_code = SkipToRequestedRange(verified_byte_range);
  if (error_code != net::OK)
    return error_code;

  DCHECK_GE(content_size, 0);
  return content_size;
}

int InputStreamReader::ReadRawData(net::IOBuffer* dest, int dest_size) {
  if (!dest_size)
    return 0;

  DCHECK_GT(dest_size, 0);

  int bytes_read = 0;
  if (!stream_->Read(dest, dest_size, &bytes_read))
    return net::ERR_FAILED;
  else
    return bytes_read;
}

int InputStreamReader::VerifyRequestedRange(net::HttpByteRange* byte_range,
                                            int* content_size) {
  DCHECK(content_size);
  int32_t size = 0;
  if (!stream_->BytesAvailable(&size))
    return net::ERR_FAILED;

  if (size <= 0)
    return net::OK;

  // Check that the requested range was valid.
  if (!byte_range->ComputeBounds(size))
    return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;

  size = byte_range->last_byte_position() -
         byte_range->first_byte_position() + 1;
  DCHECK_GE(size, 0);
  *content_size = size;

  return net::OK;
}

int InputStreamReader::SkipToRequestedRange(
    const net::HttpByteRange& byte_range) {
  // Skip to the start of the requested data. This has to be done in a loop
  // because the underlying InputStream is not guaranteed to skip the requested
  // number of bytes.
  if (byte_range.IsValid() && byte_range.first_byte_position() > 0) {
    int64_t bytes_to_skip = byte_range.first_byte_position();
    do {
      int64_t skipped = 0;
      if (!stream_->Skip(bytes_to_skip, &skipped))
        return net::ERR_FAILED;

      if (skipped <= 0)
        return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;
      DCHECK_LE(skipped, bytes_to_skip);

      bytes_to_skip -= skipped;
    } while (bytes_to_skip > 0);
  }
  return net::OK;
}

}  // namespace android_webview
