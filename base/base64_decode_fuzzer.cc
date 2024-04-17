// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/base64.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string decode_output;
  std::string_view data_piece(reinterpret_cast<const char*>(data), size);
  base::Base64Decode(data_piece, &decode_output);
  return 0;
}
