// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <fuzzer/FuzzedDataProvider.h>

#include <string_view>
#include <tuple>

#include "base/containers/span.h"
#include "base/pickle.h"

namespace {
constexpr int kIterations = 16;
constexpr int kReadControlBytes = 32;
constexpr int kReadDataTypes = 17;
constexpr int kMaxReadLength = 1024;
constexpr int kMaxSkipBytes = 1024;
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < kReadControlBytes) {
    return 0;
  }
  // Use the first kReadControlBytes bytes of the fuzzer input to control how
  // the pickled data is read.
  FuzzedDataProvider data_provider(data, kReadControlBytes);
  data += kReadControlBytes;
  size -= kReadControlBytes;

  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(UNSAFE_BUFFERS(base::span(data, size)));
  base::PickleIterator iter(pickle);
  for (int i = 0; i < kIterations; i++) {
    uint8_t read_type = data_provider.ConsumeIntegral<uint8_t>();
    switch (read_type % kReadDataTypes) {
      case 0: {
        bool result = 0;
        std::ignore = iter.ReadBool(&result);
        break;
      }
      case 1: {
        int result = 0;
        std::ignore = iter.ReadInt(&result);
        break;
      }
      case 2: {
        long result = 0;
        std::ignore = iter.ReadLong(&result);
        break;
      }
      case 3: {
        uint16_t result = 0;
        std::ignore = iter.ReadUInt16(&result);
        break;
      }
      case 4: {
        uint32_t result = 0;
        std::ignore = iter.ReadUInt32(&result);
        break;
      }
      case 5: {
        int64_t result = 0;
        std::ignore = iter.ReadInt64(&result);
        break;
      }
      case 6: {
        uint64_t result = 0;
        std::ignore = iter.ReadUInt64(&result);
        break;
      }
      case 7: {
        float result = 0;
        std::ignore = iter.ReadFloat(&result);
        break;
      }
      case 8: {
        double result = 0;
        std::ignore = iter.ReadDouble(&result);
        break;
      }
      case 9: {
        std::string result;
        std::ignore = iter.ReadString(&result);
        break;
      }
      case 10: {
        std::string_view result;
        std::ignore = iter.ReadStringPiece(&result);
        break;
      }
      case 11: {
        std::u16string result;
        std::ignore = iter.ReadString16(&result);
        break;
      }
      case 12: {
        std::u16string_view result;
        std::ignore = iter.ReadStringPiece16(&result);
        break;
      }
      case 13: {
        const char* data_result = nullptr;
        size_t length_result = 0;
        std::ignore = iter.ReadData(&data_result, &length_result);
        break;
      }
      case 14: {
        const char* data_result = nullptr;
        int read_length =
            data_provider.ConsumeIntegralInRange(0, kMaxReadLength);
        std::ignore =
            iter.ReadBytes(&data_result, static_cast<size_t>(read_length));
        break;
      }
      case 15: {
        size_t result = 0;
        std::ignore = iter.ReadLength(&result);
        break;
      }
      case 16: {
        std::ignore = iter.SkipBytes(static_cast<size_t>(
            data_provider.ConsumeIntegralInRange(0, kMaxSkipBytes)));
        break;
      }
    }
  }

  return 0;
}
