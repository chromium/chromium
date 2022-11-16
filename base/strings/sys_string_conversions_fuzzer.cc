// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <string>
#include <tuple>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace base {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  const std::string text = provider.ConsumeRandomLengthString();
  const std::wstring wide_text = UTF8ToWide(text);

  std::ignore = SysWideToUTF8(wide_text);
  std::ignore = SysUTF8ToWide(text);
  std::ignore = SysWideToNativeMB(wide_text);
  std::ignore = SysNativeMBToWide(text);

#if BUILDFLAG(IS_WIN)
  const uint32_t code_page = provider.ConsumeIntegral<uint32_t>();
  std::ignore = SysMultiByteToWide(text, code_page);
  std::ignore = SysWideToMultiByte(wide_text, code_page);
#endif

  return 0;
}

}  // namespace base
