// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/md5_boringssl.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/hash/md5.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/boringssl/src/include/openssl/md5.h"

namespace base {
void MD5Init(MD5Context* context) {
  MD5_Init(context);
}

void MD5Update(MD5Context* context, std::string_view data) {
  MD5Update(context, base::as_byte_span(data));
}

void MD5Update(MD5Context* context, base::span<const uint8_t> data) {
  MD5_Update(context, data.data(), data.size());
}

void MD5Final(MD5Digest* digest, MD5Context* context) {
  MD5_Final(digest->a.data(), context);
}

std::string MD5DigestToBase16(const MD5Digest& digest) {
  return ToLowerASCII(HexEncode(digest.a));
}

void MD5Sum(base::span<const uint8_t> data, MD5Digest* digest) {
  MD5(data.data(), data.size(), digest->a.data());
}

std::string MD5String(std::string_view str) {
  MD5Digest digest;
  MD5Sum(base::as_byte_span(str), &digest);
  return MD5DigestToBase16(digest);
}
}  // namespace base
