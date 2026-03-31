// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <limits>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "base/bits.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "build/build_config.h"

namespace base {

namespace {

[[nodiscard]] inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                                           uint8_t& result) {
  return reader.ReadU8NativeEndian(result);
}

[[nodiscard]] inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                                           uint16_t& result) {
  return reader.ReadU16NativeEndian(result);
}

[[nodiscard]] inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                                           uint32_t& result) {
  return reader.ReadU32NativeEndian(result);
}

[[nodiscard]] inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                                           uint64_t& result) {
  return reader.ReadU64NativeEndian(result);
}

[[nodiscard]] inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                                           int32_t& result) {
  return reader.ReadI32NativeEndian(result);
}

[[nodiscard]] inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                                           int64_t& result) {
  return reader.ReadI64NativeEndian(result);
}

[[nodiscard]] inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                                           float& result) {
  return reader.ReadFloatNativeEndian(result);
}

[[nodiscard]] inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                                           double& result) {
  return reader.ReadDoubleNativeEndian(result);
}

// Advances `reader` after having read `bytes_read` bytes so that the next read
// occurs with `uint32_t` alignment.
void AlignAfterRead(SpanReader<const uint8_t>& reader,
                    const size_t bytes_read) {
  const size_t aligned_size = bits::AlignUp(bytes_read, sizeof(uint32_t));
  const size_t diff = aligned_size - bytes_read;

  const size_t skip = std::min(diff, reader.remaining());

  const bool ok = reader.Skip(skip).has_value();
  // Success should be guaranteed by the `std::min()`.
  DCHECK(ok);
}

void SkipToEnd(SpanReader<const uint8_t>& reader) {
  // It is tempting to replace this with `reader = SpanReader<const uint8_t>();`
  // but that causes subsequent zero-sized reads to receive a nullptr instead of
  // a non-nullptr.

  const bool ok = reader.Skip(reader.remaining()).has_value();
  DCHECK(ok);
}

template <typename T>
[[nodiscard]] bool ReadBuiltinTypeAndAlign(SpanReader<const uint8_t>& reader,
                                           T* result) {
  if (!ReadNativeEndian(reader, *result)) {
    SkipToEnd(reader);
    return false;
  }

  AlignAfterRead(reader, sizeof(T));
  return true;
}

[[nodiscard]] bool ReadLengthAndAlign(SpanReader<const uint8_t>& reader,
                                      size_t* result) {
  // `SpanReader` does not expose methods for reading machine-sized types, but
  // the rest of this class assumes this to be true already.
  static_assert(sizeof(int) == sizeof(int32_t));

  int result_int;
  if (!ReadBuiltinTypeAndAlign(reader, &result_int)) {
    return false;
  }

  if (result_int < 0) {
    SkipToEnd(reader);
    return false;
  }

  *result = static_cast<size_t>(result_int);
  return true;
}

[[nodiscard]] bool ReadBytesAndAlign(SpanReader<const uint8_t>& reader,
                                     const size_t num_bytes,
                                     span<const uint8_t>& result) {
  if (!reader.ReadInto(num_bytes, result)) {
    SkipToEnd(reader);
    return false;
  }

  AlignAfterRead(reader, num_bytes);
  return true;
}

// This supports bytes but not arbitrary types because the start of the data
// doesn't necessarily correspond to a suitably aligned pointer.
[[nodiscard]] bool ReadLengthDelimitedArrayAndAlign(
    SpanReader<const uint8_t>& reader,
    span<const uint8_t>& result) {
  size_t len;
  if (!ReadLengthAndAlign(reader, &len)) {
    return false;
  }
  return ReadBytesAndAlign(reader, len, result);
}

}  // namespace

// static
const size_t Pickle::kPayloadUnit = 64;

static const size_t kCapacityReadOnly = static_cast<size_t>(-1);

PickleIterator::PickleIterator(const Pickle& pickle)
    : reader_(pickle.payload_bytes()) {}

PickleIterator PickleIterator::WithData(span<const uint8_t> data) {
  if (data.size() < sizeof(Pickle::Header)) {
    return PickleIterator();
  }
  // Make a copy of the header instead of dereferencing `data` with
  // reinterpret_cast in case the memory is not aligned, which would lead to
  // Undefined Behavior. This scenario should be rare as in most cases memory
  // allocations are aligned, but it is not guaranteed by this API which accepts
  // arbitrary spans.
  Pickle::Header header;
  byte_span_from_ref(header).copy_from_nonoverlapping(
      data.first(sizeof(header)));

  if (header.payload_size > data.size() - sizeof(Pickle::Header)) {
    return PickleIterator();
  }
  const size_t header_size = data.size() - header.payload_size;
  if (header_size != bits::AlignUp(header_size, sizeof(uint32_t))) {
    return PickleIterator();
  }
  DCHECK_GE(header_size, sizeof(Pickle::Header));

  PickleIterator iter;
  iter.reader_ = SpanReader(data.subspan(header_size));
  return iter;
}

bool PickleIterator::ReadBool(bool* result) {
  // Not all bit patterns are valid bools. Avoid undefined behavior by reading
  // a type with no padding bits, then converting to bool.
  uint8_t v;
  if (!ReadBuiltinTypeAndAlign(reader_, &v)) {
    return false;
  }
  *result = v != 0;
  return true;
}

bool PickleIterator::ReadInt(int* result) {
  // `SpanReader` does not expose methods for reading machine-sized types, but
  // the rest of this class assumes this to be true already.
  static_assert(sizeof(int) == sizeof(int32_t));

  return ReadBuiltinTypeAndAlign(reader_, result);
}

bool PickleIterator::ReadLong(long* result) {
  // Always read long as a 64-bit value to ensure compatibility between 32-bit
  // and 64-bit processes.
  int64_t result_int64 = 0;
  if (!ReadInt64(&result_int64)) {
    return false;
  }
  if (!IsValueInRangeForNumericType<long>(result_int64)) {
    SkipToEnd(reader_);
    return false;
  }
  *result = static_cast<long>(result_int64);
  return true;
}

bool PickleIterator::ReadLength(size_t* result) {
  return ReadLengthAndAlign(reader_, result);
}

bool PickleIterator::ReadUInt16(uint16_t* result) {
  return ReadBuiltinTypeAndAlign(reader_, result);
}

bool PickleIterator::ReadUInt32(uint32_t* result) {
  return ReadBuiltinTypeAndAlign(reader_, result);
}

bool PickleIterator::ReadInt64(int64_t* result) {
  return ReadBuiltinTypeAndAlign(reader_, result);
}

bool PickleIterator::ReadUInt64(uint64_t* result) {
  return ReadBuiltinTypeAndAlign(reader_, result);
}

bool PickleIterator::ReadFloat(float* result) {
  return ReadBuiltinTypeAndAlign(reader_, result);
}

bool PickleIterator::ReadDouble(double* result) {
  return ReadBuiltinTypeAndAlign(reader_, result);
}

bool PickleIterator::ReadString(std::string* result) {
  std::string_view view;
  if (!ReadStringPiece(&view)) {
    return false;
  }
  result->assign(view);
  return true;
}

bool PickleIterator::ReadStringPiece(std::string_view* result) {
  span<const uint8_t> bytes;
  if (!ReadLengthDelimitedArrayAndAlign(reader_, bytes)) {
    return false;
  }
  span<const char> chars = as_chars(bytes);
  *result = std::string_view(chars.data(), chars.size());
  return true;
}

bool PickleIterator::ReadString16(std::u16string* result) {
  size_t len;
  if (!ReadLength(&len)) {
    return false;
  }

  size_t num_bytes;
  if (!CheckMul(len, sizeof(char16_t)).AssignIfValid(&num_bytes)) {
    // It doesn't seem possible for this branch to be taken currently:
    // `len` is limited to
    // `std::numeric_limits<int>::max()`, which is then cast to a `size_t`,
    // meaning that multiplying that value by `sizeof(char16_t) == 2` does not
    // overflow. If there were ever a method like
    // `ReadChar16(const char16_t* data, size_t length)` where the length may
    // exceed `INT_MAX`, then the overflow would be possible. In any case, the
    // checked multiplication is good for future-proofing.
    SkipToEnd(reader_);
    return false;
  }

  span<const uint8_t> bytes;
  if (!ReadBytesAndAlign(reader_, num_bytes, bytes)) {
    return false;
  }

  // This is necessary because it is not safe to reinterpret_cast the data
  // pointer for use with `std::u16string::assign()`, as the pointer may not
  // have the proper alignment to avoid undefined behavior.
  result->resize_and_overwrite(len, [&](char16_t* p, size_t n) {
    // SAFETY: `resize_and_overwrite` ensures `p` points to `n` elements.
    as_writable_bytes(UNSAFE_BUFFERS(span(p, n)))
        .copy_from_nonoverlapping(bytes);
    return n;
  });

  return true;
}

std::optional<span<const uint8_t>> PickleIterator::ReadData() {
  span<const uint8_t> bytes;
  if (!ReadLengthDelimitedArrayAndAlign(reader_, bytes)) {
    return std::nullopt;
  }
  return bytes;
}

bool PickleIterator::ReadBytes(const char** data, size_t length) {
  span<const uint8_t> bytes;
  if (!ReadBytesAndAlign(reader_, length, bytes)) {
    return false;
  }
  *data = as_chars(bytes).data();
  return true;
}

[[nodiscard]] std::optional<span<const uint8_t>> PickleIterator::ReadBytes(
    size_t length) {
  span<const uint8_t> bytes;
  if (!ReadBytesAndAlign(reader_, length, bytes)) {
    return std::nullopt;
  }
  return bytes;
}

Pickle::Attachment::Attachment() = default;

Pickle::Attachment::~Attachment() = default;

// Payload is uint32_t aligned.

Pickle::Pickle()
    : header_(nullptr),
      header_size_(sizeof(Header)),
      capacity_after_header_(0),
      write_offset_(0) {
  static_assert(std::has_single_bit(Pickle::kPayloadUnit),
                "Pickle::kPayloadUnit must be a power of two");
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle::Pickle(size_t header_size)
    : header_(nullptr),
      header_size_(bits::AlignUp(header_size, sizeof(uint32_t))),
      capacity_after_header_(0),
      write_offset_(0) {
  DCHECK_GE(header_size, sizeof(Header));
  DCHECK_LE(header_size, kPayloadUnit);
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle Pickle::WithData(span<const uint8_t> data) {
  // Create a pickle with unowned data, then do a copy to internalize the data.
  Pickle pickle(kUnownedData, data);
  Pickle internalized_data_pickle = pickle;
  CHECK_NE(internalized_data_pickle.capacity_after_header_, kCapacityReadOnly);
  return internalized_data_pickle;
}

Pickle Pickle::WithUnownedBuffer(span<const uint8_t> data) {
  // This uses return value optimization to return a Pickle without copying
  // which will preserve the unowned-ness of the data.
  return Pickle(kUnownedData, data);
}

Pickle::Pickle(UnownedData, span<const uint8_t> data)
    : header_(reinterpret_cast<Header*>(const_cast<uint8_t*>(data.data()))),
      header_size_(0),
      capacity_after_header_(kCapacityReadOnly),
      write_offset_(0) {
  if (data.size() >= sizeof(Header)) {
    header_size_ = data.size() - header_->payload_size;
  }

  if (header_size_ > data.size()) {
    header_size_ = 0;
  }

  if (header_size_ != bits::AlignUp(header_size_, sizeof(uint32_t))) {
    header_size_ = 0;
  }

  // If there is anything wrong with the data, we're not going to use it.
  if (!header_size_) {
    header_ = nullptr;
  }
}

Pickle::Pickle(const Pickle& other)
    : header_(nullptr),
      header_size_(other.header_size_),
      capacity_after_header_(0),
      write_offset_(other.write_offset_) {
  if (other.header_) {
    Resize(other.header_->payload_size);
    UNSAFE_TODO(memcpy(header_, other.header_,
                       header_size_ + other.header_->payload_size));
  }
}

Pickle::~Pickle() {
  if (capacity_after_header_ != kCapacityReadOnly) {
    free(header_);
  }
}

Pickle& Pickle::operator=(const Pickle& other) {
  if (this == &other) {
    return *this;
  }
  if (capacity_after_header_ == kCapacityReadOnly) {
    header_ = nullptr;
    capacity_after_header_ = 0;
  }
  if (header_size_ != other.header_size_) {
    free(header_);
    header_ = nullptr;
    header_size_ = other.header_size_;
  }
  if (other.header_) {
    Resize(other.header_->payload_size);
    UNSAFE_TODO(memcpy(header_, other.header_,
                       other.header_size_ + other.header_->payload_size));
    write_offset_ = other.write_offset_;
  }
  return *this;
}

void Pickle::WriteString(std::string_view value) {
  WriteData(value.data(), value.size());
}

void Pickle::WriteString16(std::u16string_view value) {
  WriteInt(checked_cast<int>(value.size()));
  WriteBytes(as_byte_span(value));
}

void Pickle::WriteData(const char* data, size_t length) {
  WriteData(as_bytes(UNSAFE_TODO(span(data, length))));
}

void Pickle::WriteData(std::string_view data) {
  WriteData(as_byte_span(data));
}

void Pickle::WriteData(base::span<const uint8_t> data) {
  WriteInt(checked_cast<int>(data.size()));
  WriteBytes(data);
}

void Pickle::WriteBytes(span<const uint8_t> data) {
  WriteBytesCommon(data);
}

void Pickle::Reserve(size_t length) {
  size_t data_len = bits::AlignUp(length, sizeof(uint32_t));
  DCHECK_GE(data_len, length);
#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(data_len, std::numeric_limits<uint32_t>::max());
#endif
  DCHECK_LE(write_offset_, std::numeric_limits<uint32_t>::max() - data_len);
  size_t new_size = write_offset_ + data_len;
  if (new_size > capacity_after_header_) {
    Resize(capacity_after_header_ * 2 + new_size);
  }
}

bool Pickle::WriteAttachment(scoped_refptr<Attachment> attachment) {
  return false;
}

bool Pickle::ReadAttachment(base::PickleIterator* iter,
                            scoped_refptr<Attachment>* attachment) const {
  return false;
}

bool Pickle::HasAttachments() const {
  return false;
}

void Pickle::Resize(size_t new_capacity) {
  CHECK_NE(capacity_after_header_, kCapacityReadOnly);
  capacity_after_header_ = bits::AlignUp(new_capacity, kPayloadUnit);
  void* p = realloc(header_, GetTotalAllocatedSize());
  CHECK(p);
  header_ = reinterpret_cast<Header*>(p);
}

void* Pickle::ClaimBytes(size_t num_bytes) {
  void* p = ClaimUninitializedBytesInternal(num_bytes);
  CHECK(p);
  UNSAFE_TODO(memset(p, 0, num_bytes));
  return p;
}

size_t Pickle::GetTotalAllocatedSize() const {
  if (capacity_after_header_ == kCapacityReadOnly) {
    return 0;
  }
  return header_size_ + capacity_after_header_;
}

span<uint8_t> Pickle::AsWritableBytes() {
  CHECK(header_);
  CHECK_NE(kCapacityReadOnly, capacity_after_header_)
      << "oops: pickle is readonly";
  // SAFETY: `header_` always points to at least `size()` valid bytes if
  // non-null, and otherwise `size()` returns zero.
  return UNSAFE_BUFFERS(span(reinterpret_cast<uint8_t*>(header_), size()));
}

// static
bool Pickle::PeekNext(size_t header_size,
                      const char* start,
                      const char* end,
                      size_t* pickle_size) {
  DCHECK_EQ(header_size, bits::AlignUp(header_size, sizeof(uint32_t)));
  DCHECK_GE(header_size, sizeof(Header));
  DCHECK_LE(header_size, static_cast<size_t>(kPayloadUnit));

  size_t length = static_cast<size_t>(end - start);
  if (length < sizeof(Header)) {
    return false;
  }

  const Header* hdr = reinterpret_cast<const Header*>(start);
  if (length < header_size) {
    return false;
  }

  // If payload_size causes an overflow, we return maximum possible
  // pickle size to indicate that.
  *pickle_size = ClampAdd(header_size, hdr->payload_size);
  return true;
}

template <size_t length>
void Pickle::WriteBytesStatic(const void* data) {
  WriteBytesCommon(
      UNSAFE_TODO(span(static_cast<const uint8_t*>(data), length)));
}

template void Pickle::WriteBytesStatic<2>(const void* data);
template void Pickle::WriteBytesStatic<4>(const void* data);
template void Pickle::WriteBytesStatic<8>(const void* data);

inline void* Pickle::ClaimUninitializedBytesInternal(size_t length) {
  DCHECK_NE(kCapacityReadOnly, capacity_after_header_)
      << "oops: pickle is readonly";
  size_t data_len = bits::AlignUp(length, sizeof(uint32_t));
  DCHECK_GE(data_len, length);
#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(data_len, std::numeric_limits<uint32_t>::max());
#endif
  DCHECK_LE(write_offset_, std::numeric_limits<uint32_t>::max() - data_len);
  size_t new_size = write_offset_ + data_len;
  if (new_size > capacity_after_header_) {
    size_t new_capacity = capacity_after_header_ * 2;
    const size_t kPickleHeapAlign = 4096;
    if (new_capacity > kPickleHeapAlign) {
      new_capacity =
          bits::AlignUp(new_capacity, kPickleHeapAlign) - kPayloadUnit;
    }
    Resize(std::max(new_capacity, new_size));
  }

  char* write = UNSAFE_TODO(reinterpret_cast<char*>(header_) + header_size_ +
                            write_offset_);
  std::fill(UNSAFE_TODO(write + length), UNSAFE_TODO(write + data_len),
            0);  // Always initialize padding
  header_->payload_size = static_cast<uint32_t>(new_size);
  write_offset_ = new_size;
  return write;
}

inline void Pickle::WriteBytesCommon(span<const uint8_t> data) {
  DCHECK_NE(kCapacityReadOnly, capacity_after_header_)
      << "oops: pickle is readonly";
  MSAN_CHECK_MEM_IS_INITIALIZED(data.data(), data.size());
  void* write = ClaimUninitializedBytesInternal(data.size());
  std::copy(data.data(), UNSAFE_TODO(data.data() + data.size()),
            static_cast<char*>(write));
}

}  // namespace base
