// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/winrt_storage_util.h"

#include <robuffer.h>
#include <string.h>
#include <wrl/client.h>

#include <utility>

#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"

namespace base {
namespace win {

using IBuffer = ABI::Windows::Storage::Streams::IBuffer;

HRESULT GetPointerToBufferData(IBuffer* buffer, uint8_t** out, UINT32* length) {
  *out = nullptr;

  Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess>
      buffer_byte_access;
  HRESULT hr = buffer->QueryInterface(IID_PPV_ARGS(&buffer_byte_access));
  if (FAILED(hr))
    return hr;

  hr = buffer->get_Length(length);
  if (FAILED(hr))
    return hr;

  // Lifetime of the pointing buffer is controlled by the buffer object.
  return buffer_byte_access->Buffer(out);
}

HRESULT CreateIBufferFromData(const uint8_t* data,
                              UINT32 length,
                              Microsoft::WRL::ComPtr<IBuffer>* buffer) {
  *buffer = nullptr;

  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBufferFactory>
      buffer_factory;
  HRESULT hr = base::win::GetActivationFactory<
      ABI::Windows::Storage::Streams::IBufferFactory,
      RuntimeClass_Windows_Storage_Streams_Buffer>(&buffer_factory);
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IBuffer> internal_buffer;
  hr = buffer_factory->Create(length, &internal_buffer);
  if (FAILED(hr))
    return hr;

  hr = internal_buffer->put_Length(length);
  if (FAILED(hr))
    return hr;

  uint8_t* p_buffer_data;
  hr = GetPointerToBufferData(internal_buffer.Get(), &p_buffer_data, &length);
  if (FAILED(hr))
    return hr;

  memcpy(p_buffer_data, data, length);

  *buffer = std::move(internal_buffer);

  return S_OK;
}

}  // namespace win
}  // namespace base
