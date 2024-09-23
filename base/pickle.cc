// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/pickle.h"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <limits>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "base/bits.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "build/build_config.h"

namespace base {

// static
const size_t Pickle::kPayloadUnit = 64;

static const size_t kCapacityReadOnly = static_cast<size_t>(-1);

PickleIterator::PickleIterator(const Pickle& pickle)
    : payload_(pickle.payload()),
      read_index_(0),
      end_index_(pickle.payload_size()) {}

template <typename Type>
inline bool PickleIterator::ReadBuiltinType(Type* result) {
  static_assert(
      std::is_integral_v<Type> && !std::is_same_v<Type, bool>,
      "This method is only safe with to use with types without padding bits.");
  const char* read_from = GetReadPointerAndAdvance<Type>();
  if (!read_from)
    return false;
  memcpy(result, read_from, sizeof(*result));
  return true;
}

inline void PickleIterator::Advance(size_t size) {
  size_t aligned_size = bits::AlignUp(size, sizeof(uint32_t));
  if (end_index_ - read_index_ < aligned_size) {
    read_index_ = end_index_;
  } else {
    read_index_ += aligned_size;
  }
}

template <typename Type>
inline const char* PickleIterator::GetReadPointerAndAdvance() {
  if (sizeof(Type) > end_index_ - read_index_) {
    read_index_ = end_index_;
    return nullptr;
  }
  const char* current_read_ptr = payload_ + read_index_;
  Advance(sizeof(Type));
  return current_read_ptr;
}

const char* PickleIterator::GetReadPointerAndAdvance(size_t num_bytes) {
  if (num_bytes > end_index_ - read_index_) {
    read_index_ = end_index_;
    return nullptr;
  }
  const char* current_read_ptr = payload_ + read_index_;
  Advance(num_bytes);
  return current_read_ptr;
}

inline const char* PickleIterator::GetReadPointerAndAdvance(
    size_t num_elements,
    size_t size_element) {
  // Check for size_t overflow.
  size_t num_bytes;
  if (!CheckMul(num_elements, size_element).AssignIfValid(&num_bytes))
    return nullptr;
  return GetReadPointerAndAdvance(num_bytes);
}

bool PickleIterator::ReadBool(bool* result) {
  // Not all bit patterns are valid bools. Avoid undefined behavior by reading a
  // type with no padding bits, then converting to bool.
  uint8_t v;
  if (!ReadBuiltinType(&v)) {
    return false;
  }
  *result = v != 0;
  return true;
}

bool PickleIterator::ReadInt(int* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadLong(long* result) {
  // Always read long as a 64-bit value to ensure compatibility between 32-bit
  // and 64-bit processes.
  int64_t result_int64 = 0;
  if (!ReadBuiltinType(&result_int64))
    return false;
  if (!IsValueInRangeForNumericType<long>(result_int64))
    return false;
  *result = static_cast<long>(result_int64);
  return true;
}

bool PickleIterator::ReadUInt16(uint16_t* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadUInt32(uint32_t* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadInt64(int64_t* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadUInt64(uint64_t* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadFloat(float* result) {
  // crbug.com/315213
  // The source data may not be properly aligned, and unaligned float reads
  // cause SIGBUS on some ARM platforms, so force using memcpy to copy the data
  // into the result.
  const char* read_from = GetReadPointerAndAdvance<float>();
  if (!read_from)
    return false;
  memcpy(result, read_from, sizeof(*result));
  return true;
}

bool PickleIterator::ReadDouble(double* result) {
  // crbug.com/315213
  // The source data may not be properly aligned, and unaligned double reads
  // cause SIGBUS on some ARM platforms, so force using memcpy to copy the data
  // into the result.
  const char* read_from = GetReadPointerAndAdvance<double>();
  if (!read_from)
    return false;
  memcpy(result, read_from, sizeof(*result));
  return true;
}

bool PickleIterator::ReadString(std::string* result) {
  size_t len;
  if (!ReadLength(&len))
    return false;
  const char* read_from = GetReadPointerAndAdvance(len);
  if (!read_from)
    return false;

  result->assign(read_from, len);
  return true;
}

bool PickleIterator::ReadStringPiece(std::string_view* result) {
  size_t len;
  if (!ReadLength(&len))
    return false;
  const char* read_from = GetReadPointerAndAdvance(len);
  if (!read_from)
    return false;

  *result = std::string_view(read_from, len);
  return true;
}

bool PickleIterator::ReadString16(std::u16string* result) {
  size_t len;
  if (!ReadLength(&len))
    return false;
  const char* read_from = GetReadPointerAndAdvance(len, sizeof(char16_t));
  if (!read_from)
    return false;

  result->assign(reinterpret_cast<const char16_t*>(read_from), len);
  return true;
}

bool PickleIterator::ReadStringPiece16(std::u16string_view* result) {
  size_t len;
  if (!ReadLength(&len))
    return false;
  const char* read_from = GetReadPointerAndAdvance(len, sizeof(char16_t));
  if (!read_from)
    return false;

  *result =
      std::u16string_view(reinterpret_cast<const char16_t*>(read_from), len);
  return true;
}

bool PickleIterator::ReadData(const char** data, size_t* length) {
  *length = 0;
  *data = nullptr;

  if (!ReadLength(length))
    return false;

  return ReadBytes(data, *length);
}

std::optional<base::span<const uint8_t>> PickleIterator::ReadData() {
  const char* ptr;
  size_t length;

  if (!ReadData(&ptr, &length))
    return std::nullopt;

  return base::as_bytes(base::make_span(ptr, length));
}

bool PickleIterator::ReadBytes(const char** data, size_t length) {
  const char* read_from = GetReadPointerAndAdvance(length);
  if (!read_from)
    return false;
  *data = read_from;
  return true;
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
    memcpy(header_, other.header_, header_size_ + other.header_->payload_size);
  }
}

Pickle::~Pickle() {
  if (capacity_after_header_ != kCapacityReadOnly)
    free(header_);
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
    memcpy(header_, other.header_,
           other.header_size_ + other.header_->payload_size);
    write_offset_ = other.write_offset_;
  }
  return *this;
}

void Pickle::WriteString(std::string_view value) {
  WriteData(value.data(), value.size());
}

void Pickle::WriteString16(std::u16string_view value) {
  WriteInt(checked_cast<int>(value.size()));
  WriteBytes(value.data(), value.size() * sizeof(char16_t));
}

void Pickle::WriteData(const char* data, size_t length) {
  WriteData(as_bytes(span(data, length)));
}

void Pickle::WriteData(std::string_view data) {
  WriteData(as_byte_span(data));
}

void Pickle::WriteData(base::span<const uint8_t> data) {
  WriteInt(checked_cast<int>(data.size()));
  WriteBytes(data);
}

void Pickle::WriteBytes(const void* data, size_t length) {
  WriteBytesCommon(make_span(static_cast<const uint8_t*>(data), length));
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
  if (new_size > capacity_after_header_)
    Resize(capacity_after_header_ * 2 + new_size);
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
  memset(p, 0, num_bytes);
  return p;
}

size_t Pickle::GetTotalAllocatedSize() const {
  if (capacity_after_header_ == kCapacityReadOnly)
    return 0;
  return header_size_ + capacity_after_header_;
}

// static
const char* Pickle::FindNext(size_t header_size,
                             const char* start,
                             const char* end) {
  size_t pickle_size = 0;
  if (!PeekNext(header_size, start, end, &pickle_size))
    return nullptr;

  if (pickle_size > static_cast<size_t>(end - start))
    return nullptr;

  return start + pickle_size;
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
  if (length < sizeof(Header))
    return false;

  const Header* hdr = reinterpret_cast<const Header*>(start);
  if (length < header_size)
    return false;

  // If payload_size causes an overflow, we return maximum possible
  // pickle size to indicate that.
  *pickle_size = ClampAdd(header_size, hdr->payload_size);
  return true;
}

template <size_t length>
void Pickle::WriteBytesStatic(const void* data) {
  WriteBytesCommon(make_span(static_cast<const uint8_t*>(data), length));
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

  char* write = mutable_payload() + write_offset_;
  std::fill(write + length, write + data_len, 0);  // Always initialize padding
  header_->payload_size = static_cast<uint32_t>(new_size);
  write_offset_ = new_size;
  return write;
}

inline void Pickle::WriteBytesCommon(span<const uint8_t> data) {
  DCHECK_NE(kCapacityReadOnly, capacity_after_header_)
      << "oops: pickle is readonly";
  MSAN_CHECK_MEM_IS_INITIALIZED(data.data(), data.size());
  void* write = ClaimUninitializedBytesInternal(data.size());
  std::copy(data.data(), data.data() + data.size(), static_cast<char*>(write));
}

}  // namespace base
