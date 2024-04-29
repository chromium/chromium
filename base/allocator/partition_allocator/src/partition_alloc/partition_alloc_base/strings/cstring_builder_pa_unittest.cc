// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/strings/cstring_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::base::strings {

namespace {

template <typename T>
void FillBuffer(CStringBuilder& builder, T value, unsigned count) {
  for (unsigned i = 0; i < count; ++i) {
    builder << value;
  }
}

}  // namespace

TEST(CStringBuilderTestPA, String) {
  CStringBuilder builder;
  const char buffer[] = "Buffer\n";
  builder << "Hello, World" << '\n' << buffer << '\n';
  EXPECT_EQ("Hello, World\nBuffer\n\n", std::string(builder.c_str()));
}

TEST(CStringBuilderTestPA, Char) {
  CStringBuilder builder;
  builder << 'c' << 'h' << 'a' << ' ' << 'r' << '\\' << '\n';
  EXPECT_EQ("cha r\\\n", std::string(builder.c_str()));
  builder << '\0' << '\n';
  EXPECT_EQ("cha r\\\n\n", std::string(builder.c_str()));
}

TEST(CStringBuilderTestPA, Integer) {
  CStringBuilder builder1;
  builder1 << std::numeric_limits<uint64_t>::max();
  EXPECT_EQ("18446744073709551615", std::string(builder1.c_str()));

  builder1 << " " << std::numeric_limits<int64_t>::min();
  EXPECT_EQ("18446744073709551615 -9223372036854775808",
            std::string(builder1.c_str()));

  CStringBuilder builder2;
  builder2 << std::numeric_limits<int64_t>::max();
  EXPECT_EQ("9223372036854775807", std::string(builder2.c_str()));

  CStringBuilder builder3;
  builder3 << std::numeric_limits<int64_t>::min();
  EXPECT_EQ("-9223372036854775808", std::string(builder3.c_str()));
}

TEST(CStringBuilderTestPA, FloatingPoint) {
  CStringBuilder builder1;
  builder1 << 3.1415926;
  EXPECT_EQ("3.14159", std::string(builder1.c_str()));

  CStringBuilder builder2;
  builder2 << 0.0000725692;
  EXPECT_EQ("7.25692e-5", std::string(builder2.c_str()));

  // Zero
  CStringBuilder builder3;
  builder3 << 0.0;
  EXPECT_EQ("0", std::string(builder3.c_str()));

  // min()
  CStringBuilder builder4;
  builder4 << std::numeric_limits<double>::min();
  EXPECT_EQ("2.22507e-308", std::string(builder4.c_str()));

  // Subnormal value
  CStringBuilder builder5;
  builder5 << std::numeric_limits<double>::denorm_min();
  // denorm_min() < min()
  EXPECT_EQ("2.22507e-308", std::string(builder5.c_str()));

  // Positive Infinity
  CStringBuilder builder6;
  builder6 << std::numeric_limits<double>::infinity();
  EXPECT_EQ("inf", std::string(builder6.c_str()));

  // Negative Infinity
  CStringBuilder builder7;
  builder7 << -std::numeric_limits<double>::infinity();
  EXPECT_EQ("-inf", std::string(builder7.c_str()));

  // max()
  CStringBuilder builder8;
  builder8 << std::numeric_limits<double>::max();
  EXPECT_EQ("1.79769e+308", std::string(builder8.c_str()));

  // NaN
  CStringBuilder builder9;
  builder9 << nan("");
  EXPECT_EQ("NaN", std::string(builder9.c_str()));
}

TEST(CStringBuilderTestPA, FillBuffer) {
  CStringBuilder builder1;
  FillBuffer(builder1, ' ', CStringBuilder::kBufferSize * 2);
  EXPECT_EQ(CStringBuilder::kBufferSize - 1, strlen(builder1.c_str()));

  CStringBuilder builder2;
  FillBuffer(builder2, 3.141592653, CStringBuilder::kBufferSize * 2);
  EXPECT_EQ(CStringBuilder::kBufferSize - 1, strlen(builder2.c_str()));

  CStringBuilder builder3;
  FillBuffer(builder3, 3.14159f, CStringBuilder::kBufferSize * 2);
  EXPECT_EQ(CStringBuilder::kBufferSize - 1, strlen(builder3.c_str()));

  CStringBuilder builder4;
  FillBuffer(builder4, 65535u, CStringBuilder::kBufferSize * 2);
  EXPECT_EQ(CStringBuilder::kBufferSize - 1, strlen(builder4.c_str()));

  CStringBuilder builder5;
  FillBuffer(builder5, "Dummy Text", CStringBuilder::kBufferSize * 2);
  EXPECT_EQ(CStringBuilder::kBufferSize - 1, strlen(builder5.c_str()));
}

TEST(CStringBuilderTestPA, Pointer) {
  CStringBuilder builder1;
  char* str = reinterpret_cast<char*>(0x80000000u);
  void* ptr = str;
  builder1 << ptr;
  EXPECT_EQ("0x80000000", std::string(builder1.c_str()));

  CStringBuilder builder2;
  builder2 << reinterpret_cast<void*>(0xdeadbeafu);
  EXPECT_EQ("0xDEADBEAF", std::string(builder2.c_str()));

  // nullptr
  CStringBuilder builder3;
  builder3 << nullptr;
  EXPECT_EQ("nullptr", std::string(builder3.c_str()));

  CStringBuilder builder4;
  builder4 << reinterpret_cast<unsigned*>(0);
  EXPECT_EQ("(nil)", std::string(builder4.c_str()));
}

}  // namespace partition_alloc::internal::base::strings
