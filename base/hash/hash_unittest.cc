// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/hash.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(HashTest, DeprecatedHashFromString) {
  std::string str;
  // Empty string (should hash to 0).
  str = "";
  EXPECT_EQ(0u, Hash(str));

  // Simple test.
  str = "hello world";
  EXPECT_EQ(2794219650u, Hash(str));

  // Change one bit.
  str = "helmo world";
  EXPECT_EQ(1006697176u, Hash(str));

  // Insert a null byte.
  str = "hello  world";
  str[5] = '\0';
  EXPECT_EQ(2319902537u, Hash(str));

  // Test that the bytes after the null contribute to the hash.
  str = "hello  worle";
  str[5] = '\0';
  EXPECT_EQ(553904462u, Hash(str));

  // Extremely long string.
  // Also tests strings with high bit set, and null byte.
  std::vector<char> long_string_buffer;
  for (int i = 0; i < 4096; ++i)
    long_string_buffer.push_back((i % 256) - 128);
  str.assign(&long_string_buffer.front(), long_string_buffer.size());
  EXPECT_EQ(2797962408u, Hash(str));

  // All possible lengths (mod 4). Tests separate code paths. Also test with
  // final byte high bit set (regression test for http://crbug.com/90659).
  // Note that the 1 and 3 cases have a weird bug where the final byte is
  // treated as a signed char. It was decided on the above bug discussion to
  // enshrine that behaviour as "correct" to avoid invalidating existing hashes.

  // Length mod 4 == 0.
  str = "hello w\xab";
  EXPECT_EQ(615571198u, Hash(str));
  // Length mod 4 == 1.
  str = "hello wo\xab";
  EXPECT_EQ(623474296u, Hash(str));
  // Length mod 4 == 2.
  str = "hello wor\xab";
  EXPECT_EQ(4278562408u, Hash(str));
  // Length mod 4 == 3.
  str = "hello worl\xab";
  EXPECT_EQ(3224633008u, Hash(str));
}

TEST(HashTest, DeprecatedHashFromCString) {
  const char* str;
  // Empty string (should hash to 0).
  str = "";
  EXPECT_EQ(0u, Hash(str));

  // Simple test.
  str = "hello world";
  EXPECT_EQ(2794219650u, Hash(str));
}

TEST(HashTest, PersistentHashFromSpan) {
  // Empty span (should hash to 0).
  EXPECT_EQ(0u, PersistentHash(base::span<const uint8_t>()));

  // The hash loads four bytes at a time, and is sensitive to the high bit of
  // the last few bytes. Test hashes of various lengths, and with and without
  // the high bit set, to confirm the persistent hash remains persistent.
  const std::string_view str1 = "hello world";
  const std::array<uint32_t, 12> kHashesByLength1 = {
      0u,          1213478405u, 2371107848u, 2412215855u,
      2296013106u, 2963130491u, 342812795u,  1345887711u,
      2394271580u, 2806845956u, 2484860346u, 2794219650u};
  for (size_t i = 0; i <= str1.size(); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(kHashesByLength1[i],
              PersistentHash(as_byte_span(str1.substr(0, i))));
  }

  const std::string_view str2 = "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa";
  const std::array<uint32_t, 12> kHashesByLength2 = {
      0u,          2524495555u, 901867827u,  52332316u,
      1053305007u, 4170027104u, 1891345481u, 2246421829u,
      1241531838u, 4191939542u, 4100345281u, 896950651u};
  for (size_t i = 0; i <= str2.size(); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(kHashesByLength2[i],
              PersistentHash(as_byte_span(str2.substr(0, i))));
  }
}

TEST(HashTest, FastHash) {
  std::string s;
  constexpr char kEmptyString[] = "";
  EXPECT_EQ(FastHash(s), FastHash(kEmptyString));
}

}  // namespace base
