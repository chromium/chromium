// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_WINRT_STORAGE_UTIL_H_
#define BASE_WIN_WINRT_STORAGE_UTIL_H_

#include <stdint.h>
#include <windows.storage.streams.h>
#include <wrl/client.h>

#include "base/base_export.h"
#include "base/containers/span.h"

namespace base::win {

// Gets a span of bytes in the `buffer`, `out_span` is a span of bytes used by
// byte stream read and write.
BASE_EXPORT HRESULT
GetPointerToBufferData(ABI::Windows::Storage::Streams::IBuffer* buffer,
                       base::span<uint8_t>& out_span);

// Creates stream `buffer` from `src_span` that represents an array of bytes.
BASE_EXPORT HRESULT CreateIBufferFromData(
    base::span<const uint8_t> src_span,
    Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBuffer>* buffer);

}  // namespace base::win

#endif  // BASE_WIN_WINRT_STORAGE_UTIL_H_
