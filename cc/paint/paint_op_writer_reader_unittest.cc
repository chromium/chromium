// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/compiler_specific.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/test/test_options_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(PaintOpWriterReaderTest, SizeT) {
  static_assert(PaintOpWriter::SerializedSize<int>() == 4u);
  static_assert(PaintOpWriter::SerializedSize<size_t>() == 8u);
  static_assert(PaintOpWriter::SerializedSize(static_cast<size_t>(0u)) == 8u);

  char buffer[128];
  TestOptionsProvider options_provider;
  UNSAFE_TODO(memset(buffer, 0xa5, std::size(buffer)));
  PaintOpWriter writer(buffer, std::size(buffer),
                       options_provider.serialize_options(),
                       /*enable_security_constraints*/ true);
  int i = 0x5555aaaa;
  size_t s1 = static_cast<size_t>(0x123456789abcdef0L);
  size_t s2 = static_cast<size_t>(0xfedcba9876543210L);
  writer.WriteSize(s1);
  writer.Write(i);
  writer.WriteSize(s2);
  EXPECT_EQ(20u, writer.size());

  PaintOpReader reader(buffer, writer.size(),
                       options_provider.deserialize_options(),
                       /*enable_security_constraints*/ true);
  int read_i;
  size_t read_s1, read_s2;
  reader.ReadSize(&read_s1);
  reader.Read(&read_i);
  reader.ReadSize(&read_s2);
  EXPECT_EQ(i, read_i);
  EXPECT_EQ(s1, read_s1);
  EXPECT_EQ(s2, read_s2);
  EXPECT_TRUE(reader.valid());
  EXPECT_EQ(0u, reader.remaining_bytes());

  reader.ReadSize(&read_s2);
  EXPECT_FALSE(reader.valid());
}

TEST(PaintOpWriterReaderTest, Vector) {
  char buffer[128];
  TestOptionsProvider options_provider;
  UNSAFE_TODO(memset(buffer, 0xa5, std::size(buffer)));
  PaintOpWriter writer(buffer, std::size(buffer),
                       options_provider.serialize_options(),
                       /*enable_security_constraints*/ true);

  writer.Write(std::vector<float>{});
  EXPECT_EQ(writer.size(), 8u);
  writer.Write(std::vector<float>{1, 2});
  EXPECT_EQ(writer.size(), 24u);
  writer.Write(std::vector<uint32_t>{1, 2, 3});
  EXPECT_EQ(writer.size(), 44u);

  PaintOpReader reader(buffer, writer.size(),
                       options_provider.deserialize_options(),
                       /*enable_security_constraints*/ true);

  std::vector<float> float_vec;
  reader.Read(float_vec);
  EXPECT_EQ(float_vec, std::vector<float>{});
  reader.Read(float_vec);
  EXPECT_EQ(float_vec, std::vector<float>({1, 2}));
  std::vector<uint32_t> uint_vec;
  reader.Read(uint_vec);
  EXPECT_EQ(uint_vec, std::vector<uint32_t>({1, 2, 3}));
}

TEST(PaintOpWriterReaderTest, SkString) {
  char buffer[128];
  TestOptionsProvider options_provider;
  UNSAFE_TODO(memset(buffer, 0xa5, std::size(buffer)));
  PaintOpWriter writer(buffer, std::size(buffer),
                       options_provider.serialize_options(),
                       /*enable_security_constraints=*/true);
  const SkString original("test string");

  writer.Write(original);
  // 8 bytes for the `original.size()` (size_t), 11 bytes for the string, but
  // aligned to 12 bytes.
  EXPECT_EQ(writer.size(), 20u);

  PaintOpReader reader(buffer, writer.size(),
                       options_provider.deserialize_options(),
                       /*enable_security_constraints=*/true);
  SkString deseralized;
  reader.Read(&deseralized);
  EXPECT_EQ(deseralized, original);
}

TEST(PaintOpWriterReaderTest, EmptySkString) {
  char buffer[128];
  TestOptionsProvider options_provider;
  UNSAFE_TODO(memset(buffer, 0xa5, std::size(buffer)));
  PaintOpWriter writer(buffer, std::size(buffer),
                       options_provider.serialize_options(),
                       /*enable_security_constraints=*/true);
  const SkString original;

  writer.Write(original);
  // 8 bytes for size_t 0.
  EXPECT_EQ(writer.size(), 8u);

  PaintOpReader reader(buffer, writer.size(),
                       options_provider.deserialize_options(),
                       /*enable_security_constraints=*/true);
  SkString deseralized;
  reader.Read(&deseralized);
  EXPECT_EQ(deseralized, original);
}

namespace {
struct UniformTestCase {
  std::vector<PaintShader::FloatUniform> scalars;
  std::vector<PaintShader::Float2Uniform> float2s;
  std::vector<PaintShader::Float4Uniform> float4s;
  std::vector<PaintShader::IntUniform> ints;
  size_t expected_size;
};

using PaintOpWriterReaderUniformTest = testing::TestWithParam<UniformTestCase>;
}  // namespace

TEST_P(PaintOpWriterReaderUniformTest, Uniforms) {
  char buffer[128];
  TestOptionsProvider options_provider;
  UNSAFE_TODO(memset(buffer, 0xa5, std::size(buffer)));
  PaintOpWriter writer(buffer, std::size(buffer),
                       options_provider.serialize_options(),
                       /*enable_security_constraints=*/true);
  const auto& scalars = GetParam().scalars;
  const auto& float2s = GetParam().float2s;
  const auto& float4s = GetParam().float4s;
  const auto& ints = GetParam().ints;
  if (!scalars.empty()) {
    writer.Write(scalars);
  } else if (!float2s.empty()) {
    writer.Write(float2s);
  } else if (!float4s.empty()) {
    writer.Write(float4s);
  } else if (!ints.empty()) {
    writer.Write(ints);
  } else {
    ASSERT_TRUE(false);
  }

  EXPECT_EQ(writer.size(), GetParam().expected_size);

  PaintOpReader reader(buffer, writer.size(),
                       options_provider.deserialize_options(),
                       /*enable_security_constraints=*/true);

  if (!scalars.empty()) {
    std::vector<PaintShader::FloatUniform> deseralized;
    reader.Read(&deseralized);
    EXPECT_THAT(deseralized, ::testing::UnorderedElementsAreArray(scalars));
  } else if (!float2s.empty()) {
    std::vector<PaintShader::Float2Uniform> deseralized;
    reader.Read(&deseralized);
    EXPECT_THAT(deseralized, ::testing::UnorderedElementsAreArray(float2s));
  } else if (!float4s.empty()) {
    std::vector<PaintShader::Float4Uniform> deseralized;
    reader.Read(&deseralized);
    EXPECT_THAT(deseralized, ::testing::UnorderedElementsAreArray(float4s));
  } else if (!ints.empty()) {
    std::vector<PaintShader::IntUniform> deseralized;
    reader.Read(&deseralized);
    EXPECT_THAT(deseralized, ::testing::UnorderedElementsAreArray(ints));
  } else {
    ASSERT_TRUE(false);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    PaintOpWriterReaderUniformTest,
    testing::ValuesIn<UniformTestCase>(
        {UniformTestCase{
             .scalars = {{.name = SkString("var1"), .value = 1.f},
                         {.name = SkString("variable2"), .value = 2.f}},
             // count = 8,
             // "var1" = 8+4, 1.f = 4,
             // "variable2" = 8+12 (9 aligned up to 12), 2.f = 4
             .expected_size = 48u},
         UniformTestCase{
             .float2s = {{.name = SkString("var1"), .value = SkV2{1.f, 2.f}},
                         {.name = SkString("variable2"),
                          .value = SkV2{3.f, 4.f}}},
             // count = 8,
             // "var1" = 8+4, value = 8,
             // "variable2" = 8+12 (9 aligned up to 12), value = 8
             .expected_size = 56u},
         UniformTestCase{.float4s = {{.name = SkString("var1"),
                                      .value = SkV4{1.f, 2.f, 3.f, 4.f}},
                                     {.name = SkString("variable2"),
                                      .value = SkV4{5.f, 6.f, 7.f, 8.f}}},
                         // count = 8,
                         // "var1" = 8+4, value = 16,
                         // "variable2" = 8+12 (9 aligned up to 12), value = 16
                         .expected_size = 72u},
         UniformTestCase{.ints = {{.name = SkString("var1"), .value = 1},
                                  {.name = SkString("variable2"), .value = 2}},
                         // Should match scalars case.
                         .expected_size = 48u}}),
    [](const ::testing::TestParamInfo<UniformTestCase> info) {
      if (!info.param.scalars.empty()) {
        return "Scalar";
      } else if (!info.param.float2s.empty()) {
        return "SkV2";
      } else if (!info.param.float4s.empty()) {
        return "SkV4";
      } else if (!info.param.ints.empty()) {
        return "Int";
      } else {
        NOTREACHED();
      }
    });

}  // namespace cc
