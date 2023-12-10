// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_REF_COUNTED_MEMORY_H_
#define BASE_MEMORY_REF_COUNTED_MEMORY_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"

namespace base {

class ReadOnlySharedMemoryRegion;

// A generic interface to memory. This object is reference counted because most
// of its subclasses own the data they carry, and this interface needs to
// support heterogeneous containers of these different types of memory.
class BASE_EXPORT RefCountedMemory
    : public RefCountedThreadSafe<RefCountedMemory> {
 public:
  // Retrieves a pointer to the beginning of the data we point to. If the data
  // is empty, this will return NULL.
  virtual const unsigned char* front() const = 0;

  // Size of the memory pointed to.
  virtual size_t size() const = 0;

  // Returns true if |other| is byte for byte equal.
  bool Equals(const scoped_refptr<RefCountedMemory>& other) const;

  // Handy method to simplify calling front() with a reinterpret_cast.
  template<typename T> const T* front_as() const {
    return reinterpret_cast<const T*>(front());
  }

  const unsigned char* data() const { return front(); }

  const unsigned char* begin() const { return data(); }
  const unsigned char* end() const { return data() + size(); }

 protected:
  friend class RefCountedThreadSafe<RefCountedMemory>;
  RefCountedMemory();
  virtual ~RefCountedMemory();
};

// An implementation of RefCountedMemory, where the ref counting does not
// matter.
class BASE_EXPORT RefCountedStaticMemory : public RefCountedMemory {
 public:
  RefCountedStaticMemory() : data_(nullptr), length_(0) {}
  RefCountedStaticMemory(const void* data, size_t length)
      : data_(static_cast<const unsigned char*>(length ? data : nullptr)),
        length_(length) {}

  RefCountedStaticMemory(const RefCountedStaticMemory&) = delete;
  RefCountedStaticMemory& operator=(const RefCountedStaticMemory&) = delete;

  // RefCountedMemory:
  const unsigned char* front() const override;
  size_t size() const override;

 private:
  ~RefCountedStaticMemory() override;

  const unsigned char* data_;
  size_t length_;
};

// An implementation of RefCountedMemory, where the data is stored in a STL
// vector.
class BASE_EXPORT RefCountedBytes : public RefCountedMemory {
 public:
  RefCountedBytes();

  // Constructs a RefCountedBytes object by copying from |initializer|.
  explicit RefCountedBytes(const std::vector<unsigned char>& initializer);
  explicit RefCountedBytes(base::span<const unsigned char> initializer);

  // Constructs a RefCountedBytes object by copying |size| bytes from |p|.
  RefCountedBytes(const unsigned char* p, size_t size);

  // Constructs a RefCountedBytes object by zero-initializing a new vector of
  // |size| bytes.
  explicit RefCountedBytes(size_t size);

  RefCountedBytes(const RefCountedBytes&) = delete;
  RefCountedBytes& operator=(const RefCountedBytes&) = delete;

  // Constructs a RefCountedBytes object by performing a swap. (To non
  // destructively build a RefCountedBytes, use the constructor that takes a
  // vector.)
  static scoped_refptr<RefCountedBytes> TakeVector(
      std::vector<unsigned char>* to_destroy);

  // RefCountedMemory:
  const unsigned char* front() const override;
  size_t size() const override;

  const std::vector<unsigned char>& data() const { return data_; }
  std::vector<unsigned char>& data() { return data_; }

  // Non-const versions of front() and front_as() that are simply shorthand for
  // data().data().
  unsigned char* front() { return data_.data(); }
  template <typename T>
  T* front_as() {
    return reinterpret_cast<T*>(front());
  }

 private:
  ~RefCountedBytes() override;

  std::vector<unsigned char> data_;
};

// An implementation of RefCountedMemory, where the bytes are stored in a STL
// string. Use this if your data naturally arrives in that format.
class BASE_EXPORT RefCountedString : public RefCountedMemory {
 public:
  RefCountedString();
  explicit RefCountedString(std::string value);

  RefCountedString(const RefCountedString&) = delete;
  RefCountedString& operator=(const RefCountedString&) = delete;

  // RefCountedMemory:
  const unsigned char* front() const override;
  size_t size() const override;

  const std::string& data() const { return data_; }
  std::string& data() { return data_; }

 private:
  ~RefCountedString() override;

  std::string data_;
};

// An implementation of RefCountedMemory, where the bytes are stored in a
// std::u16string.
class BASE_EXPORT RefCountedString16 : public base::RefCountedMemory {
 public:
  RefCountedString16();
  explicit RefCountedString16(std::u16string value);

  RefCountedString16(const RefCountedString16&) = delete;
  RefCountedString16& operator=(const RefCountedString16&) = delete;

  // RefCountedMemory:
  const unsigned char* front() const override;
  size_t size() const override;

 protected:
  ~RefCountedString16() override;

 private:
  std::u16string data_;
};

// An implementation of RefCountedMemory, where the bytes are stored in
// ReadOnlySharedMemoryMapping.
class BASE_EXPORT RefCountedSharedMemoryMapping : public RefCountedMemory {
 public:
  // Constructs a RefCountedMemory object by taking ownership of an already
  // mapped ReadOnlySharedMemoryMapping object.
  explicit RefCountedSharedMemoryMapping(ReadOnlySharedMemoryMapping mapping);

  RefCountedSharedMemoryMapping(const RefCountedSharedMemoryMapping&) = delete;
  RefCountedSharedMemoryMapping& operator=(
      const RefCountedSharedMemoryMapping&) = delete;

  // Convenience method to map all of |region| and take ownership of the
  // mapping. Returns an empty scoped_refptr if the map operation fails.
  static scoped_refptr<RefCountedSharedMemoryMapping> CreateFromWholeRegion(
      const ReadOnlySharedMemoryRegion& region);

  // RefCountedMemory:
  const unsigned char* front() const override;
  size_t size() const override;

 private:
  ~RefCountedSharedMemoryMapping() override;

  const ReadOnlySharedMemoryMapping mapping_;
  const size_t size_;
};

}  // namespace base

#endif  // BASE_MEMORY_REF_COUNTED_MEMORY_H_
