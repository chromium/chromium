// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const bool testbool1 = false;
const bool testbool2 = true;
const int testint = 2'093'847'192;
const long testlong = 1'093'847'192;
const uint16_t testuint16 = 32123;
const uint32_t testuint32 = 1593847192;
const int64_t testint64 = -0x7E8CA925'3104BDFCLL;
const uint64_t testuint64 = 0xCE8CA925'3104BDF7ULL;
const float testfloat = 3.1415926935f;
const double testdouble = 2.71828182845904523;
const std::string teststring("Hello world");  // note non-aligned string length
const std::string testemptystring("");
const std::wstring testwstring(L"Hello, world");
const std::u16string teststring16(u"Hello, world");
const char testrawstring[] = "Hello new world";  // Test raw string writing
// Test raw char16_t writing, assumes UTF16 encoding is ANSI for alpha chars.
const char16_t testrawstring16[] = {'A', 'l', 'o', 'h', 'a', 0};
const char testdata[] = "AAA\0BBB\0";
const size_t testdatalen = std::size(testdata) - 1;

// checks that the results can be read correctly from the Pickle
void VerifyResult(const Pickle& pickle) {
  PickleIterator iter(pickle);

  bool outbool;
  EXPECT_TRUE(iter.ReadBool(&outbool));
  EXPECT_FALSE(outbool);
  EXPECT_TRUE(iter.ReadBool(&outbool));
  EXPECT_TRUE(outbool);

  int outint;
  EXPECT_TRUE(iter.ReadInt(&outint));
  EXPECT_EQ(testint, outint);

  long outlong;
  EXPECT_TRUE(iter.ReadLong(&outlong));
  EXPECT_EQ(testlong, outlong);

  uint16_t outuint16;
  EXPECT_TRUE(iter.ReadUInt16(&outuint16));
  EXPECT_EQ(testuint16, outuint16);

  uint32_t outuint32;
  EXPECT_TRUE(iter.ReadUInt32(&outuint32));
  EXPECT_EQ(testuint32, outuint32);

  int64_t outint64;
  EXPECT_TRUE(iter.ReadInt64(&outint64));
  EXPECT_EQ(testint64, outint64);

  uint64_t outuint64;
  EXPECT_TRUE(iter.ReadUInt64(&outuint64));
  EXPECT_EQ(testuint64, outuint64);

  float outfloat;
  EXPECT_TRUE(iter.ReadFloat(&outfloat));
  EXPECT_EQ(testfloat, outfloat);

  double outdouble;
  EXPECT_TRUE(iter.ReadDouble(&outdouble));
  EXPECT_EQ(testdouble, outdouble);

  std::string outstring;
  EXPECT_TRUE(iter.ReadString(&outstring));
  EXPECT_EQ(teststring, outstring);

  std::string outstring2;
  EXPECT_TRUE(iter.ReadString(&outstring2));
  EXPECT_EQ(testemptystring, outstring2);

  std::u16string outstring16;
  EXPECT_TRUE(iter.ReadString16(&outstring16));
  EXPECT_EQ(teststring16, outstring16);

  std::string_view outstringpiece;
  EXPECT_TRUE(iter.ReadStringPiece(&outstringpiece));
  EXPECT_EQ(testrawstring, outstringpiece);

  std::u16string_view outstringpiece16;
  EXPECT_TRUE(iter.ReadStringPiece16(&outstringpiece16));
  EXPECT_EQ(testrawstring16, outstringpiece16);

  const char* outdata;
  size_t outdatalen;
  EXPECT_TRUE(iter.ReadData(&outdata, &outdatalen));
  EXPECT_EQ(testdatalen, outdatalen);
  EXPECT_EQ(UNSAFE_TODO(memcmp(testdata, outdata, outdatalen)), 0);

  // reads past the end should fail
  EXPECT_FALSE(iter.ReadInt(&outint));
}

}  // namespace

TEST(PickleTest, UnownedVsOwned) {
  const uint8_t buffer[1] = {0x00};

  Pickle unowned_pickle = Pickle::WithUnownedBuffer(buffer);
  EXPECT_EQ(unowned_pickle.GetTotalAllocatedSize(), 0u);

  Pickle owned_pickle = Pickle::WithData(buffer);
  EXPECT_GE(unowned_pickle.GetTotalAllocatedSize(), 0u);
}

TEST(PickleTest, EncodeDecode) {
  Pickle pickle;

  pickle.WriteBool(testbool1);
  pickle.WriteBool(testbool2);
  pickle.WriteInt(testint);
  pickle.WriteLong(testlong);
  pickle.WriteUInt16(testuint16);
  pickle.WriteUInt32(testuint32);
  pickle.WriteInt64(testint64);
  pickle.WriteUInt64(testuint64);
  pickle.WriteFloat(testfloat);
  pickle.WriteDouble(testdouble);
  pickle.WriteString(teststring);
  pickle.WriteString(testemptystring);
  pickle.WriteString16(teststring16);
  pickle.WriteString(testrawstring);
  pickle.WriteString16(testrawstring16);
  pickle.WriteData(std::string_view(testdata, testdatalen));
  VerifyResult(pickle);

  // test copy constructor
  Pickle pickle2(pickle);
  VerifyResult(pickle2);

  // test operator=
  Pickle pickle3;
  pickle3 = pickle;
  VerifyResult(pickle3);
}

// Tests that reading/writing a long works correctly when the source process
// is 64-bit.  We rely on having both 32- and 64-bit trybots to validate both
// arms of the conditional in this test.
TEST(PickleTest, LongFrom64Bit) {
  Pickle pickle;
  // Under the hood long is always written as a 64-bit value, so simulate a
  // 64-bit long even on 32-bit architectures by explicitly writing an int64_t.
  pickle.WriteInt64(testint64);

  PickleIterator iter(pickle);
  long outlong;
  if (sizeof(long) < sizeof(int64_t)) {
    // ReadLong() should return false when the original written value can't be
    // represented as a long.
    EXPECT_FALSE(iter.ReadLong(&outlong));
  } else {
    EXPECT_TRUE(iter.ReadLong(&outlong));
    EXPECT_EQ(testint64, outlong);
  }
}

// Tests that we can handle really small buffers.
TEST(PickleTest, SmallBuffer) {
  const uint8_t buffer[] = {0x00};

  // We should not touch the buffer.
  Pickle pickle = Pickle::WithUnownedBuffer(buffer);

  PickleIterator iter(pickle);
  int data;
  EXPECT_FALSE(iter.ReadInt(&data));
}

// Tests that we can handle improper headers.
TEST(PickleTest, BigSize) {
  // In this example the header indicates a size that doesn't match the total
  // data size.
  const uint32_t buffer[4] = {0x56035200, 25, 40, 50};

  Pickle pickle = Pickle::WithUnownedBuffer(as_byte_span(buffer));
  EXPECT_EQ(0U, pickle.size());

  PickleIterator iter(pickle);
  int data;
  EXPECT_FALSE(iter.ReadInt(&data));
}

// Tests that instances constructed with invalid parameter combinations can be
// properly copied. Regression test for https://crbug.com/1271311.
TEST(PickleTest, CopyWithInvalidHeader) {
  // 1. Actual header size (calculated based on the input buffer) > passed in
  // buffer size. Which results in Pickle's internal |header_| = null.
  {
    Pickle::Header header = {.payload_size = 100};
    const Pickle pickle = Pickle::WithUnownedBuffer(byte_span_from_ref(header));

    EXPECT_EQ(0U, pickle.size());
    EXPECT_TRUE(pickle.AsBytes().empty());

    Pickle copy_built_with_op = pickle;
    EXPECT_EQ(0U, copy_built_with_op.size());
    EXPECT_TRUE(copy_built_with_op.AsBytes().empty());

    Pickle copy_built_with_ctor(pickle);
    EXPECT_EQ(0U, copy_built_with_ctor.size());
    EXPECT_TRUE(copy_built_with_ctor.AsBytes().empty());
  }
  // 2. Input buffer's size < sizeof(Pickle::Header). Which must also result in
  // Pickle's internal |header_| = null.
  {
    const uint8_t data[] = {0x00, 0x00};
    const Pickle pickle = Pickle::WithUnownedBuffer(data);
    static_assert(sizeof(Pickle::Header) > sizeof(data));

    EXPECT_EQ(0U, pickle.size());
    EXPECT_TRUE(pickle.AsBytes().empty());

    Pickle copy_built_with_op = pickle;
    EXPECT_EQ(0U, copy_built_with_op.size());
    EXPECT_TRUE(copy_built_with_op.AsBytes().empty());

    Pickle copy_built_with_ctor(pickle);
    EXPECT_EQ(0U, copy_built_with_ctor.size());
    EXPECT_TRUE(copy_built_with_ctor.AsBytes().empty());
  }
}

TEST(PickleTest, UnalignedSize) {
  // In this example the header contains a size of 10, which is invalid because
  // it doesn't suit the alignment for uint32_t.
  const uint32_t buffer[] = {10, 25, 40, 50};

  Pickle pickle = Pickle::WithUnownedBuffer(as_byte_span(buffer));

  PickleIterator iter(pickle);
  int data;
  EXPECT_FALSE(iter.ReadInt(&data));
}

TEST(PickleTest, ZeroLenStr) {
  Pickle pickle;
  pickle.WriteString(std::string());

  PickleIterator iter(pickle);
  std::string outstr;
  EXPECT_TRUE(iter.ReadString(&outstr));
  EXPECT_EQ("", outstr);
}

TEST(PickleTest, ZeroLenStr16) {
  Pickle pickle;
  pickle.WriteString16(std::u16string());

  PickleIterator iter(pickle);
  std::string outstr;
  EXPECT_TRUE(iter.ReadString(&outstr));
  EXPECT_EQ("", outstr);
}

TEST(PickleTest, BadLenStr) {
  Pickle pickle;
  pickle.WriteInt(-2);

  PickleIterator iter(pickle);
  std::string outstr;
  EXPECT_FALSE(iter.ReadString(&outstr));
}

TEST(PickleTest, BadLenStr16) {
  Pickle pickle;
  pickle.WriteInt(-1);

  PickleIterator iter(pickle);
  std::u16string outstr;
  EXPECT_FALSE(iter.ReadString16(&outstr));
}

TEST(PickleTest, PeekNext) {
  struct CustomHeader : base::Pickle::Header {
    int cookies[10];
  };

  Pickle pickle(sizeof(CustomHeader));

  pickle.WriteString("Goooooooooooogle");

  const char* pickle_data = pickle.AsStringView().data();

  size_t pickle_size;

  // Data range doesn't contain header
  EXPECT_FALSE(Pickle::PeekNext(
      sizeof(CustomHeader), pickle_data,
      UNSAFE_TODO(pickle_data + sizeof(CustomHeader) - 1), &pickle_size));

  // Data range contains header
  EXPECT_TRUE(Pickle::PeekNext(sizeof(CustomHeader), pickle_data,
                               UNSAFE_TODO(pickle_data + sizeof(CustomHeader)),
                               &pickle_size));
  EXPECT_EQ(pickle_size, pickle.size());

  // Data range contains header and some other data
  EXPECT_TRUE(Pickle::PeekNext(
      sizeof(CustomHeader), pickle_data,
      UNSAFE_TODO(pickle_data + sizeof(CustomHeader) + 1), &pickle_size));
  EXPECT_EQ(pickle_size, pickle.size());

  // Data range contains full pickle
  EXPECT_TRUE(Pickle::PeekNext(sizeof(CustomHeader), pickle_data,
                               UNSAFE_TODO(pickle_data + pickle.size()),
                               &pickle_size));
  EXPECT_EQ(pickle_size, pickle.size());
}

TEST(PickleTest, PeekNextOverflow) {
  struct CustomHeader : base::Pickle::Header {
    int cookies[10];
  };

  CustomHeader header;

  // Check if we can wrap around at all
  if (sizeof(size_t) > sizeof(header.payload_size)) {
    return;
  }

  const char* pickle_data = reinterpret_cast<const char*>(&header);

  size_t pickle_size;

  // Wrapping around is detected and reported as maximum size_t value
  header.payload_size =
      static_cast<uint32_t>(1 - static_cast<int32_t>(sizeof(CustomHeader)));
  EXPECT_TRUE(Pickle::PeekNext(sizeof(CustomHeader), pickle_data,
                               UNSAFE_TODO(pickle_data + sizeof(CustomHeader)),
                               &pickle_size));
  EXPECT_EQ(pickle_size, std::numeric_limits<size_t>::max());

  // Ridiculous pickle sizes are fine (callers are supposed to
  // verify them)
  header.payload_size =
      std::numeric_limits<uint32_t>::max() / 2 - sizeof(CustomHeader);
  EXPECT_TRUE(Pickle::PeekNext(sizeof(CustomHeader), pickle_data,
                               UNSAFE_TODO(pickle_data + sizeof(CustomHeader)),
                               &pickle_size));
  EXPECT_EQ(pickle_size, std::numeric_limits<uint32_t>::max() / 2);
}

TEST(PickleTest, GetReadPointerAndAdvance) {
  Pickle pickle;

  PickleIterator iter(pickle);
  EXPECT_FALSE(iter.GetReadPointerAndAdvance(1));

  pickle.WriteInt(1);
  pickle.WriteInt(2);
  int bytes = sizeof(int) * 2;

  EXPECT_TRUE(PickleIterator(pickle).GetReadPointerAndAdvance(0));
  EXPECT_TRUE(PickleIterator(pickle).GetReadPointerAndAdvance(1));
  EXPECT_FALSE(PickleIterator(pickle).GetReadPointerAndAdvance(-1));
  EXPECT_TRUE(PickleIterator(pickle).GetReadPointerAndAdvance(bytes));
  EXPECT_FALSE(PickleIterator(pickle).GetReadPointerAndAdvance(bytes + 1));
  EXPECT_FALSE(PickleIterator(pickle).GetReadPointerAndAdvance(INT_MAX));
  EXPECT_FALSE(PickleIterator(pickle).GetReadPointerAndAdvance(INT_MIN));
}

TEST(PickleTest, ReadingTooMuchPreventsFutureReads) {
  Pickle pickle;
  pickle.WriteUInt32(1);

  // TODO(https://crbug.com/479458085): Ideally this would be checked for all of
  // the `PickleIterator::Read*()` methods. For now only the two *categories* of
  // reads are checked: scalar (via `ReadBuiltinTypeAndAlign()`) and array (via
  // `ReadArray()`).

  // Scalar
  {
    PickleIterator iter(pickle);

    uint64_t result_uint64;
    // 8 bytes cannot be read from the 4-byte pickle.
    EXPECT_FALSE(iter.ReadUInt64(&result_uint64));

    // But future calls should also fail, even if there would have been
    // sufficient bytes.
    EXPECT_EQ(iter.RemainingBytes(), 0);

    uint32_t result_uint32;
    EXPECT_FALSE(iter.ReadUInt32(&result_uint32));

    // But zero-sized reads still work, perhaps surprisingly.
    const char* data = nullptr;
    EXPECT_TRUE(iter.ReadBytes(&data, 0));
    EXPECT_TRUE(data);

    EXPECT_TRUE(iter.ReadBytes(0));
  }

  // Array
  {
    PickleIterator iter(pickle);

    // 8 bytes cannot be read from the 4-byte pickle.
    EXPECT_FALSE(iter.ReadBytes(8));

    // But future calls should also fail, even if there would have been
    // sufficient bytes.
    EXPECT_EQ(iter.RemainingBytes(), 0);

    EXPECT_FALSE(iter.ReadBytes(4));

    // But zero-sized reads still work, perhaps surprisingly.
    const char* data = nullptr;
    EXPECT_TRUE(iter.ReadBytes(&data, 0));
    EXPECT_TRUE(data);

    EXPECT_TRUE(iter.ReadBytes(0));
  }
}

// This test documents the current behavior, which is being reconsidered in
// https://crbug.com/479458085.
TEST(PickleTest, NegativeLengthDoesNotPreventFutureReads) {
  Pickle pickle;
  pickle.WriteInt(-1);
  pickle.WriteInt(456);

  PickleIterator iter(pickle);

  size_t len;
  EXPECT_FALSE(iter.ReadLength(&len));

  EXPECT_EQ(iter.RemainingBytes(), 4);

  int v;
  EXPECT_TRUE(iter.ReadInt(&v));
  EXPECT_EQ(v, 456);
}

// This test documents the current behavior, which is being reconsidered in
// https://crbug.com/479458085.
TEST(PickleTest, LongOverflowDoesNotPreventFutureReads) {
  Pickle pickle;
  pickle.WriteInt64(std::numeric_limits<int64_t>::max());
  pickle.WriteInt(456);

  PickleIterator iter(pickle);

  // Longs are always read as 64-bit integers. But how overflow is handled while
  // reading into a long varies by platform: On 32-bit platforms, it's possible
  // to keep reading despite the failure.
  //
  // Ideally this discrepancy would be avoided.

  if (sizeof(long) < sizeof(int64_t)) {
    long v;
    EXPECT_FALSE(iter.ReadLong(&v));
  } else {
    long v;
    EXPECT_TRUE(iter.ReadLong(&v));
    EXPECT_EQ(v, std::numeric_limits<long>::max());
  }

  EXPECT_EQ(iter.RemainingBytes(), 4);

  int v;
  EXPECT_TRUE(iter.ReadInt(&v));
  EXPECT_EQ(v, 456);
}

TEST(PickleTest, Resize) {
  size_t unit = Pickle::kPayloadUnit;
  auto data = base::HeapArray<char>::Uninit(unit);
  for (size_t i = 0; i < unit; i++) {
    data[i] = 'G';
  }

  // construct a message that will be exactly the size of one payload unit,
  // note that any data will have a 4-byte header indicating the size
  const size_t payload_size_after_header = unit - sizeof(uint32_t);
  Pickle pickle;
  pickle.WriteData(std::string_view(
      data.data(), payload_size_after_header - sizeof(uint32_t)));
  size_t cur_payload = payload_size_after_header;

  // note: we assume 'unit' is a power of 2
  EXPECT_EQ(unit, pickle.capacity_after_header());
  EXPECT_EQ(pickle.payload_size(), payload_size_after_header);

  // fill out a full page (noting data header)
  pickle.WriteData(std::string_view(data.data(), unit - sizeof(uint32_t)));
  cur_payload += unit;
  EXPECT_EQ(unit * 2, pickle.capacity_after_header());
  EXPECT_EQ(cur_payload, pickle.payload_size());

  // one more byte should double the capacity
  pickle.WriteData(std::string_view(data.data(), 1u));
  cur_payload += 8;
  EXPECT_EQ(unit * 4, pickle.capacity_after_header());
  EXPECT_EQ(cur_payload, pickle.payload_size());
}

namespace {

struct CustomHeader : Pickle::Header {
  int blah;
};

}  // namespace

TEST(PickleTest, HeaderPadding) {
  const uint32_t kMagic = 0x12345678;

  Pickle pickle(sizeof(CustomHeader));
  pickle.WriteInt(kMagic);

  // this should not overwrite the 'int' payload
  pickle.headerT<CustomHeader>()->blah = 10;

  PickleIterator iter(pickle);
  int result;
  ASSERT_TRUE(iter.ReadInt(&result));

  EXPECT_EQ(static_cast<uint32_t>(result), kMagic);
}

TEST(PickleTest, EqualsOperator) {
  Pickle source;
  source.WriteInt(1);

  Pickle copy_refs_source_buffer = Pickle::WithUnownedBuffer(source);
  Pickle copy;
  copy = copy_refs_source_buffer;
  ASSERT_EQ(source.size(), copy.size());
}

TEST(PickleTest, EvilLengths) {
  Pickle source;
  std::string str(100000, 'A');
  source.WriteData(std::string_view(str.c_str(), 100000u));
  // ReadString16 used to have its read buffer length calculation wrong leading
  // to out-of-bounds reading.
  PickleIterator iter(source);
  std::u16string str16;
  EXPECT_FALSE(iter.ReadString16(&str16));

  // And check we didn't break ReadString16.
  str16 = u"A";
  Pickle str16_pickle;
  str16_pickle.WriteString16(str16);
  iter = PickleIterator(str16_pickle);
  EXPECT_TRUE(iter.ReadString16(&str16));
  EXPECT_EQ(1U, str16.length());

  // Check we don't fail in a length check with invalid String16 size.
  // (1<<31) * sizeof(char16_t) == 0, so this is particularly evil.
  Pickle bad_len;
  bad_len.WriteInt(1 << 31);
  iter = PickleIterator(bad_len);
  EXPECT_FALSE(iter.ReadString16(&str16));
}

// Check we can write zero bytes of data and 'data' can be NULL.
TEST(PickleTest, ZeroLength) {
  Pickle pickle;
  pickle.WriteData(std::string_view());

  PickleIterator iter(pickle);
  const char* outdata;
  size_t outdatalen;
  EXPECT_TRUE(iter.ReadData(&outdata, &outdatalen));
  EXPECT_EQ(0u, outdatalen);
  // We can't assert that outdata is NULL.
}

// Check that ReadBytes works properly with an iterator initialized to NULL.
TEST(PickleTest, ReadBytes) {
  Pickle pickle;
  int data = 0x7abcd;
  pickle.WriteBytes(&data, sizeof(data));

  PickleIterator iter(pickle);
  const char* outdata_char = nullptr;
  EXPECT_TRUE(iter.ReadBytes(&outdata_char, sizeof(data)));

  int outdata;
  UNSAFE_TODO(memcpy(&outdata, outdata_char, sizeof(outdata)));
  EXPECT_EQ(data, outdata);
}

// Checks that when a pickle is deep-copied, the result is not larger than
// needed.
TEST(PickleTest, DeepCopyResize) {
  Pickle pickle;
  while (pickle.capacity_after_header() != pickle.payload_size()) {
    pickle.WriteBool(true);
  }

  // Make a deep copy.
  Pickle pickle2(pickle);

  // Check that there isn't any extraneous capacity.
  EXPECT_EQ(pickle.capacity_after_header(), pickle2.capacity_after_header());
}

namespace {

// Publicly exposes the ClaimBytes interface for testing.
class TestingPickle : public Pickle {
 public:
  TestingPickle() = default;

  void* ClaimBytes(size_t num_bytes) { return Pickle::ClaimBytes(num_bytes); }
};

}  // namespace

// Checks that claimed bytes are zero-initialized.
TEST(PickleTest, ClaimBytesInitialization) {
  static const int kChunkSize = 64;
  TestingPickle pickle;
  const char* bytes = static_cast<const char*>(pickle.ClaimBytes(kChunkSize));
  for (size_t i = 0; i < kChunkSize; ++i) {
    EXPECT_EQ(0, UNSAFE_TODO(bytes[i]));
  }
}

// Checks that ClaimBytes properly advances the write offset.
TEST(PickleTest, ClaimBytes) {
  std::string data("Hello, world!");

  TestingPickle pickle;
  pickle.WriteUInt32(data.size());
  void* bytes = pickle.ClaimBytes(data.size());
  pickle.WriteInt(42);
  UNSAFE_TODO(memcpy(bytes, data.data(), data.size()));

  PickleIterator iter(pickle);
  uint32_t out_data_length;
  EXPECT_TRUE(iter.ReadUInt32(&out_data_length));
  EXPECT_EQ(data.size(), out_data_length);

  const char* out_data = nullptr;
  EXPECT_TRUE(iter.ReadBytes(&out_data, out_data_length));
  EXPECT_EQ(data, std::string(out_data, out_data_length));

  int out_value;
  EXPECT_TRUE(iter.ReadInt(&out_value));
  EXPECT_EQ(42, out_value);
}

TEST(PickleTest, ReachedEnd) {
  Pickle pickle;
  pickle.WriteInt(1);
  pickle.WriteInt(2);
  pickle.WriteInt(3);

  PickleIterator iter(pickle);
  int out;

  EXPECT_FALSE(iter.ReachedEnd());
  EXPECT_TRUE(iter.ReadInt(&out));
  EXPECT_EQ(1, out);

  EXPECT_FALSE(iter.ReachedEnd());
  EXPECT_TRUE(iter.ReadInt(&out));
  EXPECT_EQ(2, out);

  EXPECT_FALSE(iter.ReachedEnd());
  EXPECT_TRUE(iter.ReadInt(&out));
  EXPECT_EQ(3, out);

  EXPECT_TRUE(iter.ReachedEnd());
  EXPECT_FALSE(iter.ReadInt(&out));
  EXPECT_TRUE(iter.ReachedEnd());
}

// Test that reading a value other than 0 or 1 as a bool does not trigger
// UBSan.
TEST(PickleTest, NonCanonicalBool) {
  Pickle pickle;
  pickle.WriteInt(0xff);

  PickleIterator iter(pickle);
  bool b;
  ASSERT_TRUE(iter.ReadBool(&b));
  EXPECT_TRUE(b);
}

// Tests the ReadData() overload that returns a span.
TEST(PickleTest, ReadDataAsSpan) {
  constexpr auto kWriteData =
      std::to_array<uint8_t>({0x01, 0x02, 0x03, 0x61, 0x62, 0x63});

  Pickle pickle;
  pickle.WriteData(kWriteData);
  pickle.WriteData(base::span<const uint8_t>());

  PickleIterator iter(pickle);
  EXPECT_THAT(iter.ReadData(), testing::Optional(kWriteData));
  EXPECT_THAT(iter.ReadData(), testing::Optional(base::span<const uint8_t>()));
  EXPECT_FALSE(iter.ReadData());
}

// Tests the ReadBytes() overload that returns a span.
TEST(PickleTest, ReadBytesAsSpan) {
  constexpr auto kWriteData =
      std::to_array<uint8_t>({0x01, 0x02, 0x03, 0x61, 0x62, 0x63});

  Pickle pickle;
  pickle.WriteBytes(kWriteData);

  PickleIterator iter(pickle);
  EXPECT_THAT(iter.ReadBytes(kWriteData.size()), testing::Optional(kWriteData));
  EXPECT_FALSE(iter.ReadBytes(kWriteData.size()));
}

TEST(PickleIteratorTest, WithData) {
  Pickle pickle;
  pickle.WriteInt(7);

  PickleIterator iter = PickleIterator::WithData(as_byte_span(pickle));
  EXPECT_FALSE(iter.ReachedEnd());

  int data;
  EXPECT_TRUE(iter.ReadInt(&data));
  EXPECT_EQ(7, data);
}

// Tests that we can handle improper headers.
TEST(PickleIteratorTest, WithDataBigSize) {
  // In this example the header indicates a size that doesn't match the total
  // data size.
  const int buffer[4] = {0x56035200, 25, 40, 50};

  PickleIterator iter = PickleIterator::WithData(as_byte_span(buffer));
  EXPECT_TRUE(iter.ReachedEnd());
}

// Tests that we can handle improper headers.
TEST(PickleIteratorTest, WithDataSizeMatchingPayloadSizeInHeader) {
  // In this example the header indicates a payload size matches exactly the
  // total size, but that is illegal since that means the header must be 0
  // bytes.
  const int buffer[1] = {4};

  PickleIterator iter = PickleIterator::WithData(as_byte_span(buffer));
  EXPECT_TRUE(iter.ReachedEnd());
}

TEST(PickleIteratorTest, WithDataInvalidHeader) {
  // 1. Actual header size (calculated based on the input buffer) > passed in
  // buffer size. Which results in the iterator behaving as if empty.
  {
    Pickle::Header header = {.payload_size = 100};
    PickleIterator iter = PickleIterator::WithData(byte_span_from_ref(header));
    EXPECT_TRUE(iter.ReachedEnd());
  }
  // 2. Input buffer's size < sizeof(Pickle::Header). Which results in the
  // iterator behaving as if empty.
  {
    const uint8_t data[] = {0x00, 0x00};
    static_assert(sizeof(Pickle::Header) > sizeof(data));
    PickleIterator iter = PickleIterator::WithData(data);
    EXPECT_TRUE(iter.ReachedEnd());
  }
}

TEST(PickleIteratorTest, WithDataUnalignedSize) {
  // In this example the header contains a size of 10, which is invalid because
  // it doesn't suit the alignment for uint32_t.
  const int32_t buffer[] = {10, 25, 40, 50};

  PickleIterator iter = PickleIterator::WithData(as_byte_span(buffer));
  EXPECT_TRUE(iter.ReachedEnd());
}

}  // namespace base
