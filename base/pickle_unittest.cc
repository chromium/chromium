// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/cxx17_backports.h"
#include "base/strings/utf_string_conversions.h"
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
const std::wstring testwstring(L"Hello, world");
const std::u16string teststring16(u"Hello, world");
const char testrawstring[] = "Hello new world"; // Test raw string writing
// Test raw char16_t writing, assumes UTF16 encoding is ANSI for alpha chars.
const char16_t testrawstring16[] = {'A', 'l', 'o', 'h', 'a', 0};
const char testdata[] = "AAA\0BBB\0";
const int testdatalen = base::size(testdata) - 1;

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

  std::u16string outstring16;
  EXPECT_TRUE(iter.ReadString16(&outstring16));
  EXPECT_EQ(teststring16, outstring16);

  StringPiece outstringpiece;
  EXPECT_TRUE(iter.ReadStringPiece(&outstringpiece));
  EXPECT_EQ(testrawstring, outstringpiece);

  StringPiece16 outstringpiece16;
  EXPECT_TRUE(iter.ReadStringPiece16(&outstringpiece16));
  EXPECT_EQ(testrawstring16, outstringpiece16);

  const char* outdata;
  int outdatalen;
  EXPECT_TRUE(iter.ReadData(&outdata, &outdatalen));
  EXPECT_EQ(testdatalen, outdatalen);
  EXPECT_EQ(memcmp(testdata, outdata, outdatalen), 0);

  // reads past the end should fail
  EXPECT_FALSE(iter.ReadInt(&outint));
}

}  // namespace

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
  pickle.WriteString16(teststring16);
  pickle.WriteString(testrawstring);
  pickle.WriteString16(testrawstring16);
  pickle.WriteData(testdata, testdatalen);
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
#if GTEST_HAS_DEATH_TEST
    EXPECT_DEATH(ignore_result(iter.ReadLong(&outlong)), "");
#endif
  } else {
    EXPECT_TRUE(iter.ReadLong(&outlong));
    EXPECT_EQ(testint64, outlong);
  }
}

// Tests that we can handle really small buffers.
TEST(PickleTest, SmallBuffer) {
  std::unique_ptr<char[]> buffer(new char[1]);

  // We should not touch the buffer.
  Pickle pickle(buffer.get(), 1);

  PickleIterator iter(pickle);
  int data;
  EXPECT_FALSE(iter.ReadInt(&data));
}

// Tests that we can handle improper headers.
TEST(PickleTest, BigSize) {
  int buffer[] = { 0x56035200, 25, 40, 50 };

  Pickle pickle(reinterpret_cast<char*>(buffer), sizeof(buffer));
  EXPECT_EQ(0U, pickle.size());

  PickleIterator iter(pickle);
  int data;
  EXPECT_FALSE(iter.ReadInt(&data));
}

TEST(PickleTest, UnalignedSize) {
  int buffer[] = { 10, 25, 40, 50 };

  Pickle pickle(reinterpret_cast<char*>(buffer), sizeof(buffer));

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

  const char* pickle_data = static_cast<const char*>(pickle.data());

  size_t pickle_size;

  // Data range doesn't contain header
  EXPECT_FALSE(Pickle::PeekNext(
      sizeof(CustomHeader),
      pickle_data,
      pickle_data + sizeof(CustomHeader) - 1,
      &pickle_size));

  // Data range contains header
  EXPECT_TRUE(Pickle::PeekNext(
      sizeof(CustomHeader),
      pickle_data,
      pickle_data + sizeof(CustomHeader),
      &pickle_size));
  EXPECT_EQ(pickle_size, pickle.size());

  // Data range contains header and some other data
  EXPECT_TRUE(Pickle::PeekNext(
      sizeof(CustomHeader),
      pickle_data,
      pickle_data + sizeof(CustomHeader) + 1,
      &pickle_size));
  EXPECT_EQ(pickle_size, pickle.size());

  // Data range contains full pickle
  EXPECT_TRUE(Pickle::PeekNext(
      sizeof(CustomHeader),
      pickle_data,
      pickle_data + pickle.size(),
      &pickle_size));
  EXPECT_EQ(pickle_size, pickle.size());
}

TEST(PickleTest, PeekNextOverflow) {
  struct CustomHeader : base::Pickle::Header {
    int cookies[10];
  };

  CustomHeader header;

  // Check if we can wrap around at all
  if (sizeof(size_t) > sizeof(header.payload_size))
    return;

  const char* pickle_data = reinterpret_cast<const char*>(&header);

  size_t pickle_size;

  // Wrapping around is detected and reported as maximum size_t value
  header.payload_size = static_cast<uint32_t>(
      1 - static_cast<int32_t>(sizeof(CustomHeader)));
  EXPECT_TRUE(Pickle::PeekNext(
      sizeof(CustomHeader),
      pickle_data,
      pickle_data + sizeof(CustomHeader),
      &pickle_size));
  EXPECT_EQ(pickle_size, std::numeric_limits<size_t>::max());

  // Ridiculous pickle sizes are fine (callers are supposed to
  // verify them)
  header.payload_size =
      std::numeric_limits<uint32_t>::max() / 2 - sizeof(CustomHeader);
  EXPECT_TRUE(Pickle::PeekNext(
      sizeof(CustomHeader),
      pickle_data,
      pickle_data + sizeof(CustomHeader),
      &pickle_size));
  EXPECT_EQ(pickle_size, std::numeric_limits<uint32_t>::max() / 2);
}

TEST(PickleTest, FindNext) {
  Pickle pickle;
  pickle.WriteInt(1);
  pickle.WriteString("Domo");

  const char* start = reinterpret_cast<const char*>(pickle.data());
  const char* end = start + pickle.size();

  EXPECT_EQ(end, Pickle::FindNext(pickle.header_size_, start, end));
  EXPECT_EQ(nullptr, Pickle::FindNext(pickle.header_size_, start, end - 1));
  EXPECT_EQ(end, Pickle::FindNext(pickle.header_size_, start, end + 1));
}

TEST(PickleTest, FindNextWithIncompleteHeader) {
  size_t header_size = sizeof(Pickle::Header);
  std::unique_ptr<char[]> buffer(new char[header_size - 1]);
  memset(buffer.get(), 0x1, header_size - 1);

  const char* start = buffer.get();
  const char* end = start + header_size - 1;

  EXPECT_EQ(nullptr, Pickle::FindNext(header_size, start, end));
}

#if defined(COMPILER_MSVC)
#pragma warning(push)
#pragma warning(disable: 4146)
#endif
TEST(PickleTest, FindNextOverflow) {
  size_t header_size = sizeof(Pickle::Header);
  size_t header_size2 = 2 * header_size;
  size_t payload_received = 100;
  std::unique_ptr<char[]> buffer(new char[header_size2 + payload_received]);
  const char* start = buffer.get();
  Pickle::Header* header = reinterpret_cast<Pickle::Header*>(buffer.get());
  const char* end = start + header_size2 + payload_received;
  // It is impossible to construct an overflow test otherwise.
  if (sizeof(size_t) > sizeof(header->payload_size) ||
      sizeof(uintptr_t) > sizeof(header->payload_size))
    return;

  header->payload_size = -(reinterpret_cast<uintptr_t>(start) + header_size2);
  EXPECT_EQ(nullptr, Pickle::FindNext(header_size2, start, end));

  header->payload_size = -header_size2;
  EXPECT_EQ(nullptr, Pickle::FindNext(header_size2, start, end));

  header->payload_size = 0;
  end = start + header_size;
  EXPECT_EQ(nullptr, Pickle::FindNext(header_size2, start, end));
}
#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

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

TEST(PickleTest, Resize) {
  size_t unit = Pickle::kPayloadUnit;
  std::unique_ptr<char[]> data(new char[unit]);
  char* data_ptr = data.get();
  for (size_t i = 0; i < unit; i++)
    data_ptr[i] = 'G';

  // construct a message that will be exactly the size of one payload unit,
  // note that any data will have a 4-byte header indicating the size
  const size_t payload_size_after_header = unit - sizeof(uint32_t);
  Pickle pickle;
  pickle.WriteData(
      data_ptr, static_cast<int>(payload_size_after_header - sizeof(uint32_t)));
  size_t cur_payload = payload_size_after_header;

  // note: we assume 'unit' is a power of 2
  EXPECT_EQ(unit, pickle.capacity_after_header());
  EXPECT_EQ(pickle.payload_size(), payload_size_after_header);

  // fill out a full page (noting data header)
  pickle.WriteData(data_ptr, static_cast<int>(unit - sizeof(uint32_t)));
  cur_payload += unit;
  EXPECT_EQ(unit * 2, pickle.capacity_after_header());
  EXPECT_EQ(cur_payload, pickle.payload_size());

  // one more byte should double the capacity
  pickle.WriteData(data_ptr, 1);
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

  Pickle copy_refs_source_buffer(static_cast<const char*>(source.data()),
                                 source.size());
  Pickle copy;
  copy = copy_refs_source_buffer;
  ASSERT_EQ(source.size(), copy.size());
}

TEST(PickleTest, EvilLengths) {
  Pickle source;
  std::string str(100000, 'A');
  source.WriteData(str.c_str(), 100000);
  // ReadString16 used to have its read buffer length calculation wrong leading
  // to out-of-bounds reading.
  PickleIterator iter(source);
  std::u16string str16;
  EXPECT_FALSE(iter.ReadString16(&str16));

  // And check we didn't break ReadString16.
  str16 = (wchar_t) 'A';
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
  pickle.WriteData(nullptr, 0);

  PickleIterator iter(pickle);
  const char* outdata;
  int outdatalen;
  EXPECT_TRUE(iter.ReadData(&outdata, &outdatalen));
  EXPECT_EQ(0, outdatalen);
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
  memcpy(&outdata, outdata_char, sizeof(outdata));
  EXPECT_EQ(data, outdata);
}

// Checks that when a pickle is deep-copied, the result is not larger than
// needed.
TEST(PickleTest, DeepCopyResize) {
  Pickle pickle;
  while (pickle.capacity_after_header() != pickle.payload_size())
    pickle.WriteBool(true);

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
    EXPECT_EQ(0, bytes[i]);
  }
}

// Checks that ClaimBytes properly advances the write offset.
TEST(PickleTest, ClaimBytes) {
  std::string data("Hello, world!");

  TestingPickle pickle;
  pickle.WriteUInt32(data.size());
  void* bytes = pickle.ClaimBytes(data.size());
  pickle.WriteInt(42);
  memcpy(bytes, data.data(), data.size());

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

}  // namespace base
