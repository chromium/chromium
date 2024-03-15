// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Protected memory is memory holding security-sensitive data intended to be
// left read-only for the majority of its lifetime to avoid being overwritten
// by attackers. ProtectedMemory is a simple wrapper around platform-specific
// APIs to set memory read-write and read-only when required. Protected memory
// should be set read-write for the minimum amount of time required.
//
// Normally mutable variables are held in read-write memory and constant data
// is held in read-only memory to ensure it is not accidentally overwritten.
// In some cases we want to hold mutable variables in read-only memory, except
// when they are being written to, to ensure that they are not tampered with.
//
// ProtectedMemory is a container class intended to hold a single variable in
// read-only memory, except when explicitly set read-write. The variable can be
// set read-write by creating a scoped AutoWritableMemory object by calling
// AutoWritableMemory::Create(), the memory stays writable until the returned
// object goes out of scope and is destructed. The wrapped variable can be
// accessed using operator* and operator->.
//
// Instances of ProtectedMemory must be declared in the PROTECTED_MEMORY_SECTION
// and as global variables. Because protected memory variables are globals, the
// the same rules apply disallowing non-trivial constructors and destructors.
// Global definitions are required to avoid the linker placing statics in
// inlinable functions into a comdat section and setting the protected memory
// section read-write when they are merged.
//
// EXAMPLE:
//
//  struct Items { void* item1; };
//  static PROTECTED_MEMORY_SECTION base::ProtectedMemory<Items> items;
//  void InitializeItems() {
//    // Explicitly set items read-write before writing to it.
//    auto writer = base::AutoWritableMemory::Create(items);
//    items->item1 = /* ... */;
//    assert(items->item1 != nullptr);
//    // items is set back to read-only on the destruction of writer
//  }
//
//  using FnPtr = void (*)(void);
//  PROTECTED_MEMORY_SECTION base::ProtectedMemory<FnPtr> fnPtr;
//  FnPtr ResolveFnPtr(void) {
//    // The Initializer nested class is a helper class for creating a static
//    // initializer for a ProtectedMemory variable. It implicitly sets the
//    // variable read-write during initialization.
//    static base::ProtectedMemory<FnPtr>::Initializer I(&fnPtr,
//      reinterpret_cast<FnPtr>(dlsym(/* ... */)));
//    return *fnPtr;
//  }

#ifndef BASE_MEMORY_PROTECTED_MEMORY_H_
#define BASE_MEMORY_PROTECTED_MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/gtest_prod_util.h"
#include "base/memory/protected_memory_buildflags.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
#if BUILDFLAG(IS_WIN)
// Define a read-write prot section. The $a, $mem, and $z 'sub-sections' are
// merged alphabetically so $a and $z are used to define the start and end of
// the protected memory section, and $mem holds protected variables.
// (Note: Sections in Portable Executables are equivalent to segments in other
// executable formats, so this section is mapped into its own pages.)
#pragma section("prot$a", read, write)
#pragma section("prot$mem", read, write)
#pragma section("prot$z", read, write)

// We want the protected memory section to be read-only, not read-write so we
// instruct the linker to set the section read-only at link time. We do this
// at link time instead of compile time, because defining the prot section
// read-only would cause mis-compiles due to optimizations assuming that the
// section contents are constant.
#pragma comment(linker, "/SECTION:prot,R")

__declspec(allocate("prot$a"))
__declspec(selectany) char __start_protected_memory;
__declspec(allocate("prot$z"))
__declspec(selectany) char __stop_protected_memory;

#define PROTECTED_MEMORY_SECTION __declspec(allocate("prot$mem"))
#else
#error "Protected Memory is currently only supported on Windows."
#endif  // BUILDFLAG(IS_WIN)

#else
#define PROTECTED_MEMORY_SECTION
#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)

namespace base {

template <typename T>
class AutoWritableMemory;

FORWARD_DECLARE_TEST(ProtectedMemoryDeathTest, VerifyTerminationOnAccess);

template <typename T>
class ProtectedMemory {
 public:
  ProtectedMemory() = default;
  ProtectedMemory(const ProtectedMemory&) = delete;
  ProtectedMemory& operator=(const ProtectedMemory&) = delete;

  // Expose direct access to the encapsulated variable
  const T& operator*() const { return data_; }
  const T* operator->() const { return &data_; }

  // Helper class for creating simple ProtectedMemory static initializers.
  class Initializer {
   public:
    // Defined out-of-line below to break circular definition dependency between
    // ProtectedMemory and AutoWritableMemory.
    Initializer(ProtectedMemory<T>* PM, T&& Init);

    Initializer() = delete;
    Initializer(const Initializer&) = delete;
    Initializer& operator=(const Initializer&) = delete;
  };

 private:
  friend class AutoWritableMemory<T>;
  FRIEND_TEST_ALL_PREFIXES(ProtectedMemoryDeathTest, VerifyTerminationOnAccess);

  T data_;
};

#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
namespace internal {
// Checks that the byte at `ptr` is read-only.
BASE_EXPORT bool IsMemoryReadOnly(const void* ptr);

// Abstract out platform-specific methods to get the beginning and end of the
// PROTECTED_MEMORY_SECTION. ProtectedMemoryEnd returns a pointer to the byte
// past the end of the PROTECTED_MEMORY_SECTION.
inline constexpr void* kProtectedMemoryStart = &__start_protected_memory;
inline constexpr void* kProtectedMemoryEnd = &__stop_protected_memory;
}  // namespace internal
#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)

// Provide some common functionality for `AutoWritableMemory<T>`.
class BASE_EXPORT AutoWritableMemoryBase {
 protected:
#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
  // Checks that `object` is located within the interval
  // (internal::kProtectedMemoryStart, internal::kProtectedMemoryEnd).
  template <typename T>
  static bool IsObjectInProtectedSection(const T& object) {
    const T* const ptr = std::addressof(object);
    const T* const ptr_end = ptr + 1;
    return (ptr > internal::kProtectedMemoryStart) &&
           (ptr_end <= internal::kProtectedMemoryEnd);
  }

  template <typename T>
  static bool IsObjectReadOnly(const T& object) {
    return internal::IsMemoryReadOnly(std::addressof(object));
  }

  template <typename T>
  static bool SetObjectReadWrite(T& object) {
    T* const ptr = std::addressof(object);
    T* const ptr_end = ptr + 1;
    return SetMemoryReadWrite(ptr, ptr_end);
  }

  static bool SetProtectedSectionReadOnly() {
    return SetMemoryReadOnly(internal::kProtectedMemoryStart,
                             internal::kProtectedMemoryEnd);
  }

  // When linking, each DSO will have its own protected section. We can't keep
  // track of each section, yet we have to ensure to always unlock and re-lock
  // the correct section.
  //
  // We solve this by defining a separate global writers variable (explained
  // below) in every dynamic shared object (DSO) that includes this header. To
  // do that we use this structure to define global writer data without
  // duplicate symbol errors.
  //
  // Storing the data in a substructure is required to store `writers` within
  // the protected subsection. If `writers` and `writers_lock()` are located
  // directly in `AutoWritableMemoryBase`, for unknown reasons `writers` is not
  // placed into the protected section.
  struct WriterData {
    // `writers` is a global holding the number of ProtectedMemory instances set
    // writable, used to avoid races setting protected memory readable/writable.
    // When this reaches zero the protected memory region is set read only.
    // Access is controlled by writers_lock.
    //
    // Declare writers in the protected memory section to avoid the scenario
    // where an attacker could overwrite it with a large value and invoke code
    // that constructs and destructs an AutoWritableMemory. After such a call
    // protected memory would still be set writable because writers > 0.
    PROTECTED_MEMORY_SECTION
    static inline size_t writers GUARDED_BY(writers_lock()) = 0;

    // Synchronizes access to the writers variable and the simultaneous actions
    // that need to happen alongside writers changes, e.g. setting the protected
    // memory region readable when writers is decremented to 0.
    static Lock& writers_lock() {
      static NoDestructor<Lock> writers_lock;
      return *writers_lock;
    }
  };

 private:
  // Abstract out platform-specific memory APIs. |end| points to the byte
  // past the end of the region of memory having its memory protections
  // changed.
  static bool SetMemoryReadWrite(void* start, void* end);
  static bool SetMemoryReadOnly(void* start, void* end);
#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)
};

// A class that sets a given ProtectedMemory variable writable while the
// AutoWritableMemory is in scope. This class implements the logic for setting
// the protected memory region read-only/read-write in a thread-safe manner.
//
// |AutoWritableMemory| affects the write-permissions of _all_ protected data
// for a DSO, not just of the instance that it's being passed! All protected
// data is stored within the same binary section. At the same time, the OS-level
// support enforcing write protection can only be changed at page level. To
// allow a more fine grained control a dedicated page per instance of protected
// data would be required.
template <typename T>
class AutoWritableMemory : public AutoWritableMemoryBase {
 public:
  explicit AutoWritableMemory(ProtectedMemory<T>& protected_memory)
#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
      LOCKS_EXCLUDED(WriterData::writers_lock())
#endif
      : protected_memory_(protected_memory) {
#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)

    // Check that the data is located in the protected section to
    // ensure consistency of data.
    CHECK(IsObjectInProtectedSection(protected_memory_->data_));
    CHECK(IsObjectInProtectedSection(WriterData::writers));

    {
      base::AutoLock auto_lock(WriterData::writers_lock());

      if (WriterData::writers == 0) {
        CHECK(IsObjectReadOnly(protected_memory_->data_));
        CHECK(IsObjectReadOnly(WriterData::writers));
        CHECK(SetObjectReadWrite(WriterData::writers));
      }

      ++WriterData::writers;
    }

    CHECK(SetObjectReadWrite(protected_memory_->data_));
#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)
  }

  ~AutoWritableMemory()
#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
      LOCKS_EXCLUDED(WriterData::writers_lock())
#endif
  {
#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
    base::AutoLock auto_lock(WriterData::writers_lock());
    CHECK_GT(WriterData::writers, 0u);
    --WriterData::writers;

    if (WriterData::writers == 0) {
      // Lock the whole section of protected memory and set _all_ instances of
      // base::ProtectedMemory to non-writeable.
      CHECK(SetProtectedSectionReadOnly());
      CHECK(IsObjectReadOnly(
          *static_cast<const char*>(internal::kProtectedMemoryStart)));
      CHECK(IsObjectReadOnly(WriterData::writers));
    }
#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)
  }

  AutoWritableMemory(AutoWritableMemory& original) = delete;
  AutoWritableMemory& operator=(AutoWritableMemory& original) = delete;
  AutoWritableMemory(AutoWritableMemory&& original) = delete;
  AutoWritableMemory& operator=(AutoWritableMemory&& original) = delete;

  T& GetProtectedData() { return protected_memory_->data_; }
  T* GetProtectedDataPtr() { return &(protected_memory_->data_); }

 private:

  const raw_ref<ProtectedMemory<T>> protected_memory_;
};

template <typename T>
ProtectedMemory<T>::Initializer::Initializer(
    ProtectedMemory<T>* protected_memory,
    T&& initial_value) {
  AutoWritableMemory<T> writer(*protected_memory);
  writer.GetProtectedData() = std::forward<T>(initial_value);
}

}  // namespace base

#endif  // BASE_MEMORY_PROTECTED_MEMORY_H_
