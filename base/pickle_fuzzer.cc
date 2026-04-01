// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/notreached.h"

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
  UNSAFE_TODO(data += kReadControlBytes);
  size -= kReadControlBytes;

  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(UNSAFE_BUFFERS(base::span(data, size)));
  base::PickleIterator iter(pickle);
  for (int i = 0; i < kIterations; i++) {
    uint8_t read_type =
        data_provider.ConsumeIntegral<uint8_t>() % kReadDataTypes;
    bool ok;
    switch (read_type) {
      case 0: {
        bool result = false;
        ok = iter.ReadBool(&result);
        break;
      }
      case 1: {
        int result = 0;
        ok = iter.ReadInt(&result);
        break;
      }
      case 2: {
        long result = 0;
        ok = iter.ReadLong(&result);
        break;
      }
      case 3: {
        uint16_t result = 0;
        ok = iter.ReadUInt16(&result);
        break;
      }
      case 4: {
        uint32_t result = 0;
        ok = iter.ReadUInt32(&result);
        break;
      }
      case 5: {
        int64_t result = 0;
        ok = iter.ReadInt64(&result);
        break;
      }
      case 6: {
        uint64_t result = 0;
        ok = iter.ReadUInt64(&result);
        break;
      }
      case 7: {
        float result = 0;
        ok = iter.ReadFloat(&result);
        break;
      }
      case 8: {
        double result = 0;
        ok = iter.ReadDouble(&result);
        break;
      }
      case 9: {
        std::string result;
        ok = iter.ReadString(&result);
        break;
      }
      case 10: {
        std::string_view result;
        ok = iter.ReadStringPiece(&result);
        break;
      }
      case 11: {
        std::u16string result;
        ok = iter.ReadString16(&result);
        break;
      }
      case 12: {
        ok = iter.ReadData().has_value();
        break;
      }
      case 13: {
        const char* data_result = nullptr;
        int read_length =
            data_provider.ConsumeIntegralInRange(0, kMaxReadLength);
        ok = iter.ReadBytes(&data_result, static_cast<size_t>(read_length));
        break;
      }
      case 14: {
        int read_length =
            data_provider.ConsumeIntegralInRange(0, kMaxReadLength);
        ok = iter.ReadBytes(static_cast<size_t>(read_length)).has_value();
        break;
      }
      case 15: {
        size_t result = 0;
        ok = iter.ReadLength(&result);
        break;
      }
      case 16: {
        ok = iter.SkipBytes(static_cast<size_t>(
            data_provider.ConsumeIntegralInRange(0, kMaxSkipBytes)));
        break;
      }
      default:
        NOTREACHED();
    }
    // Any failure should cause the iterator to be poisoned
    // (https://crbug.com/479458085).
    CHECK(ok || iter.ReachedEnd()) << static_cast<int>(read_type);
  }

  return 0;
}
