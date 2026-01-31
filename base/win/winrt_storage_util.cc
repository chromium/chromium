// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/winrt_storage_util.h"

#include <robuffer.h>
#include <string.h>
#include <wrl/client.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"

namespace base::win {

using IBuffer = ABI::Windows::Storage::Streams::IBuffer;

HRESULT GetPointerToBufferData(IBuffer* buffer, base::span<uint8_t>& out_span) {
  out_span = {};

  Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess>
      buffer_byte_access;
  HRESULT hr = buffer->QueryInterface(IID_PPV_ARGS(&buffer_byte_access));
  if (FAILED(hr)) {
    return hr;
  }

  UINT32 length;
  hr = buffer->get_Length(&length);
  if (FAILED(hr)) {
    return hr;
  }

  uint8_t* data;
  hr = buffer_byte_access->Buffer(&data);
  if (FAILED(hr)) {
    return hr;
  }

  // Lifetime of the pointing buffer is controlled by the buffer object.
  // SAFETY: Upon successful return from get_length() and Buffer(), the Win API
  // ensures data is valid for length bytes.
  out_span = UNSAFE_BUFFERS(base::span(data, length));
  return S_OK;
}

HRESULT CreateIBufferFromData(base::span<const uint8_t> src_span,
                              Microsoft::WRL::ComPtr<IBuffer>* buffer) {
  *buffer = nullptr;

  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBufferFactory>
      buffer_factory;
  HRESULT hr = base::win::GetActivationFactory<
      ABI::Windows::Storage::Streams::IBufferFactory,
      RuntimeClass_Windows_Storage_Streams_Buffer>(&buffer_factory);
  if (FAILED(hr)) {
    return hr;
  }

  // WinRT IBuffer lengths are UINT32, so do a checked_cast.
  const UINT32 length = base::checked_cast<UINT32>(src_span.size());

  Microsoft::WRL::ComPtr<IBuffer> internal_buffer;
  hr = buffer_factory->Create(length, &internal_buffer);
  if (FAILED(hr)) {
    return hr;
  }

  hr = internal_buffer->put_Length(length);
  if (FAILED(hr)) {
    return hr;
  }

  base::span<uint8_t> dest_span;
  hr = GetPointerToBufferData(internal_buffer.Get(), dest_span);
  if (FAILED(hr)) {
    return hr;
  }

  dest_span.copy_from(src_span);

  *buffer = std::move(internal_buffer);
  return S_OK;
}

}  // namespace base::win
