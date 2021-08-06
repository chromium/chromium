// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#include <climits>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Test;

static_assert(sizeof(raw_ptr<void>) == sizeof(void*),
              "raw_ptr shouldn't add memory overhead");
static_assert(sizeof(raw_ptr<int>) == sizeof(int*),
              "raw_ptr shouldn't add memory overhead");
static_assert(sizeof(raw_ptr<std::string>) == sizeof(std::string*),
              "raw_ptr shouldn't add memory overhead");

#if !BUILDFLAG(USE_BACKUP_REF_PTR)
// |is_trivially_copyable| assertion means that arrays/vectors of raw_ptr can
// be copied by memcpy.
static_assert(std::is_trivially_copyable<raw_ptr<void>>::value,
              "raw_ptr should be trivially copyable");
static_assert(std::is_trivially_copyable<raw_ptr<int>>::value,
              "raw_ptr should be trivially copyable");
static_assert(std::is_trivially_copyable<raw_ptr<std::string>>::value,
              "raw_ptr should be trivially copyable");

// |is_trivially_default_constructible| assertion helps retain implicit default
// constructors when raw_ptr is used as a union field.  Example of an error
// if this assertion didn't hold:
//
//     ../../base/trace_event/trace_arguments.h:249:16: error: call to
//     implicitly-deleted default constructor of 'base::trace_event::TraceValue'
//         TraceValue ret;
//                    ^
//     ../../base/trace_event/trace_arguments.h:211:26: note: default
//     constructor of 'TraceValue' is implicitly deleted because variant field
//     'as_pointer' has a non-trivial default constructor
//       raw_ptr<const void> as_pointer;
static_assert(std::is_trivially_default_constructible<raw_ptr<void>>::value,
              "raw_ptr should be trivially default constructible");
static_assert(std::is_trivially_default_constructible<raw_ptr<int>>::value,
              "raw_ptr should be trivially default constructible");
static_assert(
    std::is_trivially_default_constructible<raw_ptr<std::string>>::value,
    "raw_ptr should be trivially default constructible");
#endif  // !BUILDFLAG(USE_BACKUP_REF_PTR)

// Don't use base::internal for testing raw_ptr API, to test if code outside
// this namespace calls the correct functions from this namespace.
namespace {

static int g_wrap_raw_ptr_cnt = INT_MIN;
static int g_get_for_dereference_cnt = INT_MIN;
static int g_get_for_extraction_cnt = INT_MIN;
static int g_get_for_comparison_cnt = INT_MIN;
static int g_wrapped_ptr_swap_cnt = INT_MIN;

static void ClearCounters() {
  g_wrap_raw_ptr_cnt = 0;
  g_get_for_dereference_cnt = 0;
  g_get_for_extraction_cnt = 0;
  g_get_for_comparison_cnt = 0;
  g_wrapped_ptr_swap_cnt = 0;
}

struct RawPtrCountingNoOpImpl : base::internal::RawPtrNoOpImpl {
  using Super = base::internal::RawPtrNoOpImpl;

  static ALWAYS_INLINE void* WrapRawPtr(void* ptr) {
    ++g_wrap_raw_ptr_cnt;
    return Super::WrapRawPtr(ptr);
  }

  static ALWAYS_INLINE void* SafelyUnwrapPtrForDereference(void* wrapped_ptr) {
    ++g_get_for_dereference_cnt;
    return Super::SafelyUnwrapPtrForDereference(wrapped_ptr);
  }

  static ALWAYS_INLINE void* SafelyUnwrapPtrForExtraction(void* wrapped_ptr) {
    ++g_get_for_extraction_cnt;
    return Super::SafelyUnwrapPtrForExtraction(wrapped_ptr);
  }

  static ALWAYS_INLINE void* UnsafelyUnwrapPtrForComparison(void* wrapped_ptr) {
    ++g_get_for_comparison_cnt;
    return Super::UnsafelyUnwrapPtrForComparison(wrapped_ptr);
  }

  static ALWAYS_INLINE void IncrementSwapCountForTest() {
    ++g_wrapped_ptr_swap_cnt;
  }
};

template <typename T>
using CountingRawPtr = raw_ptr<T, RawPtrCountingNoOpImpl>;

struct MyStruct {
  int x;
};

struct Base1 {
  explicit Base1(int b1) : b1(b1) {}
  int b1;
};

struct Base2 {
  explicit Base2(int b2) : b2(b2) {}
  int b2;
};

struct Derived : Base1, Base2 {
  Derived(int b1, int b2, int d) : Base1(b1), Base2(b2), d(d) {}
  int d;
};

class RawPtrTest : public Test {
 protected:
  void SetUp() override { ClearCounters(); }
};

TEST_F(RawPtrTest, NullStarDereference) {
  raw_ptr<int> ptr = nullptr;
  EXPECT_DEATH_IF_SUPPORTED(if (*ptr == 42) return, "");
}

TEST_F(RawPtrTest, NullArrowDereference) {
  raw_ptr<MyStruct> ptr = nullptr;
  EXPECT_DEATH_IF_SUPPORTED(if (ptr->x == 42) return, "");
}

TEST_F(RawPtrTest, NullExtractNoDereference) {
  CountingRawPtr<int> ptr = nullptr;
  // No dereference hence shouldn't crash.
  int* raw = ptr;
  std::ignore = raw;
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 1);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, NullCmpExplicit) {
  CountingRawPtr<int> ptr = nullptr;
  EXPECT_TRUE(ptr == nullptr);
  EXPECT_TRUE(nullptr == ptr);
  EXPECT_FALSE(ptr != nullptr);
  EXPECT_FALSE(nullptr != ptr);
  // No need to unwrap pointer, just compare against 0.
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, NullCmpBool) {
  CountingRawPtr<int> ptr = nullptr;
  EXPECT_FALSE(ptr);
  EXPECT_TRUE(!ptr);
  // No need to unwrap pointer, just compare against 0.
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

void FuncThatAcceptsBool(bool b) {}

bool IsValidNoCast(CountingRawPtr<int> ptr) {
  return !!ptr;  // !! to avoid implicit cast
}
bool IsValidNoCast2(CountingRawPtr<int> ptr) {
  return ptr && true;
}

TEST_F(RawPtrTest, BoolOpNotCast) {
  CountingRawPtr<int> ptr = nullptr;
  volatile bool is_valid = !!ptr;  // !! to avoid implicit cast
  is_valid = ptr || is_valid;      // volatile, so won't be optimized
  if (ptr)
    is_valid = true;
  bool is_not_valid = !ptr;
  if (!ptr)
    is_not_valid = true;
  ALLOW_UNUSED_LOCAL(is_not_valid);
  std::ignore = IsValidNoCast(ptr);
  std::ignore = IsValidNoCast2(ptr);
  FuncThatAcceptsBool(!ptr);
  // No need to unwrap pointer, just compare against 0.
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

bool IsValidWithCast(CountingRawPtr<int> ptr) {
  return ptr;
}

// This test is mostly for documentation purposes. It demonstrates cases where
// |operator T*| is called first and then the pointer is converted to bool,
// as opposed to calling |operator bool| directly. The former may be more
// costly, so the caller has to be careful not to trigger this path.
TEST_F(RawPtrTest, CastNotBoolOp) {
  CountingRawPtr<int> ptr = nullptr;
  bool is_valid = ptr;
  is_valid = IsValidWithCast(ptr);
  ALLOW_UNUSED_LOCAL(is_valid);
  FuncThatAcceptsBool(ptr);
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 3);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, StarDereference) {
  int foo = 42;
  CountingRawPtr<int> ptr = &foo;
  EXPECT_EQ(*ptr, 42);
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 1);
}

TEST_F(RawPtrTest, ArrowDereference) {
  MyStruct foo = {42};
  CountingRawPtr<MyStruct> ptr = &foo;
  EXPECT_EQ(ptr->x, 42);
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 1);
}

TEST_F(RawPtrTest, Delete) {
  CountingRawPtr<int> ptr = new int(42);
  delete ptr;
  // The pointer was extracted using implicit cast before passing to |delete|.
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 1);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, ConstVolatileVoidPtr) {
  int32_t foo[] = {1234567890};
  CountingRawPtr<const volatile void> ptr = foo;
  EXPECT_EQ(*static_cast<const volatile int32_t*>(ptr), 1234567890);
  // Because we're using a cast, the extraction API kicks in, which doesn't
  // know if the extracted pointer will be dereferenced or not.
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 1);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, VoidPtr) {
  int32_t foo[] = {1234567890};
  CountingRawPtr<void> ptr = foo;
  EXPECT_EQ(*static_cast<int32_t*>(ptr), 1234567890);
  // Because we're using a cast, the extraction API kicks in, which doesn't
  // know if the extracted pointer will be dereferenced or not.
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 1);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, OperatorEQ) {
  int foo;
  CountingRawPtr<int> ptr1 = nullptr;
  EXPECT_TRUE(ptr1 == ptr1);

  CountingRawPtr<int> ptr2 = nullptr;
  EXPECT_TRUE(ptr1 == ptr2);

  CountingRawPtr<int> ptr3 = &foo;
  EXPECT_TRUE(&foo == ptr3);
  EXPECT_TRUE(ptr3 == &foo);
  EXPECT_FALSE(ptr1 == ptr3);

  ptr1 = &foo;
  EXPECT_TRUE(ptr1 == ptr3);
  EXPECT_TRUE(ptr3 == ptr1);

  EXPECT_EQ(g_get_for_comparison_cnt, 12);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, OperatorNE) {
  int foo;
  CountingRawPtr<int> ptr1 = nullptr;
  EXPECT_FALSE(ptr1 != ptr1);

  CountingRawPtr<int> ptr2 = nullptr;
  EXPECT_FALSE(ptr1 != ptr2);

  CountingRawPtr<int> ptr3 = &foo;
  EXPECT_FALSE(&foo != ptr3);
  EXPECT_FALSE(ptr3 != &foo);
  EXPECT_TRUE(ptr1 != ptr3);

  ptr1 = &foo;
  EXPECT_FALSE(ptr1 != ptr3);
  EXPECT_FALSE(ptr3 != ptr1);

  EXPECT_EQ(g_get_for_comparison_cnt, 12);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, OperatorEQCast) {
  int foo = 42;
  const int* raw_int_ptr = &foo;
  volatile void* raw_void_ptr = &foo;
  CountingRawPtr<volatile int> checked_int_ptr = &foo;
  CountingRawPtr<const void> checked_void_ptr = &foo;
  EXPECT_TRUE(checked_int_ptr == checked_int_ptr);
  EXPECT_TRUE(checked_int_ptr == raw_int_ptr);
  EXPECT_TRUE(raw_int_ptr == checked_int_ptr);
  EXPECT_TRUE(checked_void_ptr == checked_void_ptr);
  EXPECT_TRUE(checked_void_ptr == raw_void_ptr);
  EXPECT_TRUE(raw_void_ptr == checked_void_ptr);
  EXPECT_TRUE(checked_int_ptr == checked_void_ptr);
  EXPECT_TRUE(checked_int_ptr == raw_void_ptr);
  EXPECT_TRUE(raw_int_ptr == checked_void_ptr);
  EXPECT_TRUE(checked_void_ptr == checked_int_ptr);
  EXPECT_TRUE(checked_void_ptr == raw_int_ptr);
  EXPECT_TRUE(raw_void_ptr == checked_int_ptr);
  // Make sure that all cases are handled by operator== (faster) and none by the
  // cast operator (slower).
  EXPECT_EQ(g_get_for_comparison_cnt, 16);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, OperatorEQCastHierarchy) {
  Derived derived_val(42, 84, 1024);
  Derived* raw_derived_ptr = &derived_val;
  const Base1* raw_base1_ptr = &derived_val;
  volatile Base2* raw_base2_ptr = &derived_val;
  CountingRawPtr<const volatile Derived> checked_derived_ptr = &derived_val;
  CountingRawPtr<volatile Base1> checked_base1_ptr = &derived_val;
  CountingRawPtr<const Base2> checked_base2_ptr = &derived_val;
  EXPECT_TRUE(checked_derived_ptr == checked_derived_ptr);
  EXPECT_TRUE(checked_derived_ptr == raw_derived_ptr);
  EXPECT_TRUE(raw_derived_ptr == checked_derived_ptr);
  EXPECT_TRUE(checked_derived_ptr == checked_base1_ptr);
  EXPECT_TRUE(checked_derived_ptr == raw_base1_ptr);
  EXPECT_TRUE(raw_derived_ptr == checked_base1_ptr);
  EXPECT_TRUE(checked_base1_ptr == checked_derived_ptr);
  EXPECT_TRUE(checked_base1_ptr == raw_derived_ptr);
  EXPECT_TRUE(raw_base1_ptr == checked_derived_ptr);
  // |base2_ptr| points to the second base class of |derived|, so will be
  // located at an offset. While the stored raw uinptr_t values shouldn't match,
  // ensure that the internal pointer manipulation correctly offsets when
  // casting up and down the class hierarchy.
  EXPECT_NE(reinterpret_cast<uintptr_t>(checked_base2_ptr.get()),
            reinterpret_cast<uintptr_t>(checked_derived_ptr.get()));
  EXPECT_NE(reinterpret_cast<uintptr_t>(raw_base2_ptr),
            reinterpret_cast<uintptr_t>(checked_derived_ptr.get()));
  EXPECT_NE(reinterpret_cast<uintptr_t>(checked_base2_ptr.get()),
            reinterpret_cast<uintptr_t>(raw_derived_ptr));
  EXPECT_TRUE(checked_derived_ptr == checked_base2_ptr);
  EXPECT_TRUE(checked_derived_ptr == raw_base2_ptr);
  EXPECT_TRUE(raw_derived_ptr == checked_base2_ptr);
  EXPECT_TRUE(checked_base2_ptr == checked_derived_ptr);
  EXPECT_TRUE(checked_base2_ptr == raw_derived_ptr);
  EXPECT_TRUE(raw_base2_ptr == checked_derived_ptr);
  // Make sure that all cases are handled by operator== (faster) and none by the
  // cast operator (slower).
  // The 4 extractions come from .get() checks, that compare raw addresses.
  EXPECT_EQ(g_get_for_comparison_cnt, 20);
  EXPECT_EQ(g_get_for_extraction_cnt, 4);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, OperatorNECast) {
  int foo = 42;
  volatile int* raw_int_ptr = &foo;
  const void* raw_void_ptr = &foo;
  CountingRawPtr<const int> checked_int_ptr = &foo;
  CountingRawPtr<volatile void> checked_void_ptr = &foo;
  EXPECT_FALSE(checked_int_ptr != checked_int_ptr);
  EXPECT_FALSE(checked_int_ptr != raw_int_ptr);
  EXPECT_FALSE(raw_int_ptr != checked_int_ptr);
  EXPECT_FALSE(checked_void_ptr != checked_void_ptr);
  EXPECT_FALSE(checked_void_ptr != raw_void_ptr);
  EXPECT_FALSE(raw_void_ptr != checked_void_ptr);
  EXPECT_FALSE(checked_int_ptr != checked_void_ptr);
  EXPECT_FALSE(checked_int_ptr != raw_void_ptr);
  EXPECT_FALSE(raw_int_ptr != checked_void_ptr);
  EXPECT_FALSE(checked_void_ptr != checked_int_ptr);
  EXPECT_FALSE(checked_void_ptr != raw_int_ptr);
  EXPECT_FALSE(raw_void_ptr != checked_int_ptr);
  // Make sure that all cases are handled by operator== (faster) and none by the
  // cast operator (slower).
  EXPECT_EQ(g_get_for_comparison_cnt, 16);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, OperatorNECastHierarchy) {
  Derived derived_val(42, 84, 1024);
  const Derived* raw_derived_ptr = &derived_val;
  volatile Base1* raw_base1_ptr = &derived_val;
  const Base2* raw_base2_ptr = &derived_val;
  CountingRawPtr<volatile Derived> checked_derived_ptr = &derived_val;
  CountingRawPtr<const Base1> checked_base1_ptr = &derived_val;
  CountingRawPtr<const volatile Base2> checked_base2_ptr = &derived_val;
  EXPECT_FALSE(checked_derived_ptr != checked_derived_ptr);
  EXPECT_FALSE(checked_derived_ptr != raw_derived_ptr);
  EXPECT_FALSE(raw_derived_ptr != checked_derived_ptr);
  EXPECT_FALSE(checked_derived_ptr != checked_base1_ptr);
  EXPECT_FALSE(checked_derived_ptr != raw_base1_ptr);
  EXPECT_FALSE(raw_derived_ptr != checked_base1_ptr);
  EXPECT_FALSE(checked_base1_ptr != checked_derived_ptr);
  EXPECT_FALSE(checked_base1_ptr != raw_derived_ptr);
  EXPECT_FALSE(raw_base1_ptr != checked_derived_ptr);
  // |base2_ptr| points to the second base class of |derived|, so will be
  // located at an offset. While the stored raw uinptr_t values shouldn't match,
  // ensure that the internal pointer manipulation correctly offsets when
  // casting up and down the class hierarchy.
  EXPECT_NE(reinterpret_cast<uintptr_t>(checked_base2_ptr.get()),
            reinterpret_cast<uintptr_t>(checked_derived_ptr.get()));
  EXPECT_NE(reinterpret_cast<uintptr_t>(raw_base2_ptr),
            reinterpret_cast<uintptr_t>(checked_derived_ptr.get()));
  EXPECT_NE(reinterpret_cast<uintptr_t>(checked_base2_ptr.get()),
            reinterpret_cast<uintptr_t>(raw_derived_ptr));
  EXPECT_FALSE(checked_derived_ptr != checked_base2_ptr);
  EXPECT_FALSE(checked_derived_ptr != raw_base2_ptr);
  EXPECT_FALSE(raw_derived_ptr != checked_base2_ptr);
  EXPECT_FALSE(checked_base2_ptr != checked_derived_ptr);
  EXPECT_FALSE(checked_base2_ptr != raw_derived_ptr);
  EXPECT_FALSE(raw_base2_ptr != checked_derived_ptr);
  // Make sure that all cases are handled by operator== (faster) and none by the
  // cast operator (slower).
  // The 4 extractions come from .get() checks, that compare raw addresses.
  EXPECT_EQ(g_get_for_comparison_cnt, 20);
  EXPECT_EQ(g_get_for_extraction_cnt, 4);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, Cast) {
  Derived derived_val(42, 84, 1024);
  raw_ptr<Derived> checked_derived_ptr = &derived_val;
  Base1* raw_base1_ptr = checked_derived_ptr;
  EXPECT_EQ(raw_base1_ptr->b1, 42);
  Base2* raw_base2_ptr = checked_derived_ptr;
  EXPECT_EQ(raw_base2_ptr->b2, 84);

  Derived* raw_derived_ptr = static_cast<Derived*>(raw_base1_ptr);
  EXPECT_EQ(raw_derived_ptr->b1, 42);
  EXPECT_EQ(raw_derived_ptr->b2, 84);
  EXPECT_EQ(raw_derived_ptr->d, 1024);
  raw_derived_ptr = static_cast<Derived*>(raw_base2_ptr);
  EXPECT_EQ(raw_derived_ptr->b1, 42);
  EXPECT_EQ(raw_derived_ptr->b2, 84);
  EXPECT_EQ(raw_derived_ptr->d, 1024);

  raw_ptr<Base1> checked_base1_ptr = raw_derived_ptr;
  EXPECT_EQ(checked_base1_ptr->b1, 42);
  raw_ptr<Base2> checked_base2_ptr = raw_derived_ptr;
  EXPECT_EQ(checked_base2_ptr->b2, 84);

  raw_ptr<Derived> checked_derived_ptr2 =
      static_cast<Derived*>(checked_base1_ptr);
  EXPECT_EQ(checked_derived_ptr2->b1, 42);
  EXPECT_EQ(checked_derived_ptr2->b2, 84);
  EXPECT_EQ(checked_derived_ptr2->d, 1024);
  checked_derived_ptr2 = static_cast<Derived*>(checked_base2_ptr);
  EXPECT_EQ(checked_derived_ptr2->b1, 42);
  EXPECT_EQ(checked_derived_ptr2->b2, 84);
  EXPECT_EQ(checked_derived_ptr2->d, 1024);

  const Derived* raw_const_derived_ptr = checked_derived_ptr2;
  EXPECT_EQ(raw_const_derived_ptr->b1, 42);
  EXPECT_EQ(raw_const_derived_ptr->b2, 84);
  EXPECT_EQ(raw_const_derived_ptr->d, 1024);

  raw_ptr<const Derived> checked_const_derived_ptr = raw_const_derived_ptr;
  EXPECT_EQ(checked_const_derived_ptr->b1, 42);
  EXPECT_EQ(checked_const_derived_ptr->b2, 84);
  EXPECT_EQ(checked_const_derived_ptr->d, 1024);

  const Derived* raw_const_derived_ptr2 = checked_const_derived_ptr;
  EXPECT_EQ(raw_const_derived_ptr2->b1, 42);
  EXPECT_EQ(raw_const_derived_ptr2->b2, 84);
  EXPECT_EQ(raw_const_derived_ptr2->d, 1024);

  raw_ptr<const Derived> checked_const_derived_ptr2 = raw_derived_ptr;
  EXPECT_EQ(checked_const_derived_ptr2->b1, 42);
  EXPECT_EQ(checked_const_derived_ptr2->b2, 84);
  EXPECT_EQ(checked_const_derived_ptr2->d, 1024);

  raw_ptr<const Derived> checked_const_derived_ptr3 = checked_derived_ptr2;
  EXPECT_EQ(checked_const_derived_ptr3->b1, 42);
  EXPECT_EQ(checked_const_derived_ptr3->b2, 84);
  EXPECT_EQ(checked_const_derived_ptr3->d, 1024);

  volatile Derived* raw_volatile_derived_ptr = checked_derived_ptr2;
  EXPECT_EQ(raw_volatile_derived_ptr->b1, 42);
  EXPECT_EQ(raw_volatile_derived_ptr->b2, 84);
  EXPECT_EQ(raw_volatile_derived_ptr->d, 1024);

  raw_ptr<volatile Derived> checked_volatile_derived_ptr =
      raw_volatile_derived_ptr;
  EXPECT_EQ(checked_volatile_derived_ptr->b1, 42);
  EXPECT_EQ(checked_volatile_derived_ptr->b2, 84);
  EXPECT_EQ(checked_volatile_derived_ptr->d, 1024);

  void* raw_void_ptr = checked_derived_ptr;
  raw_ptr<void> checked_void_ptr = raw_derived_ptr;
  raw_ptr<Derived> checked_derived_ptr3 = static_cast<Derived*>(raw_void_ptr);
  raw_ptr<Derived> checked_derived_ptr4 =
      static_cast<Derived*>(checked_void_ptr);
  EXPECT_EQ(checked_derived_ptr3->b1, 42);
  EXPECT_EQ(checked_derived_ptr3->b2, 84);
  EXPECT_EQ(checked_derived_ptr3->d, 1024);
  EXPECT_EQ(checked_derived_ptr4->b1, 42);
  EXPECT_EQ(checked_derived_ptr4->b2, 84);
  EXPECT_EQ(checked_derived_ptr4->d, 1024);
}

TEST_F(RawPtrTest, UpcastConvertible) {
  {
    Derived derived_val(42, 84, 1024);
    raw_ptr<Derived> checked_derived_ptr = &derived_val;

    raw_ptr<Base1> checked_base1_ptr(checked_derived_ptr);
    EXPECT_EQ(checked_base1_ptr->b1, 42);
    raw_ptr<Base2> checked_base2_ptr(checked_derived_ptr);
    EXPECT_EQ(checked_base2_ptr->b2, 84);

    checked_base1_ptr = checked_derived_ptr;
    EXPECT_EQ(checked_base1_ptr->b1, 42);
    checked_base2_ptr = checked_derived_ptr;
    EXPECT_EQ(checked_base2_ptr->b2, 84);

    EXPECT_EQ(checked_base1_ptr, checked_derived_ptr);
    EXPECT_EQ(checked_base2_ptr, checked_derived_ptr);
  }

  {
    Derived derived_val(42, 84, 1024);
    raw_ptr<Derived> checked_derived_ptr1 = &derived_val;
    raw_ptr<Derived> checked_derived_ptr2 = &derived_val;
    raw_ptr<Derived> checked_derived_ptr3 = &derived_val;
    raw_ptr<Derived> checked_derived_ptr4 = &derived_val;

    raw_ptr<Base1> checked_base1_ptr(std::move(checked_derived_ptr1));
    EXPECT_EQ(checked_base1_ptr->b1, 42);
    raw_ptr<Base2> checked_base2_ptr(std::move(checked_derived_ptr2));
    EXPECT_EQ(checked_base2_ptr->b2, 84);

    checked_base1_ptr = std::move(checked_derived_ptr3);
    EXPECT_EQ(checked_base1_ptr->b1, 42);
    checked_base2_ptr = std::move(checked_derived_ptr4);
    EXPECT_EQ(checked_base2_ptr->b2, 84);
  }
}

TEST_F(RawPtrTest, UpcastNotConvertible) {
  class Base {};
  class Derived : private Base {};
  class Unrelated {};
  EXPECT_FALSE((std::is_convertible<raw_ptr<Derived>, raw_ptr<Base>>::value));
  EXPECT_FALSE((std::is_convertible<raw_ptr<Unrelated>, raw_ptr<Base>>::value));
  EXPECT_FALSE((std::is_convertible<raw_ptr<Unrelated>, raw_ptr<void>>::value));
  EXPECT_FALSE((std::is_convertible<raw_ptr<void>, raw_ptr<Unrelated>>::value));
  EXPECT_FALSE(
      (std::is_convertible<raw_ptr<int64_t>, raw_ptr<int32_t>>::value));
  EXPECT_FALSE(
      (std::is_convertible<raw_ptr<int16_t>, raw_ptr<int32_t>>::value));
}

TEST_F(RawPtrTest, UpcastPerformance) {
  {
    Derived derived_val(42, 84, 1024);
    CountingRawPtr<Derived> checked_derived_ptr = &derived_val;
    CountingRawPtr<Base1> checked_base1_ptr(checked_derived_ptr);
    CountingRawPtr<Base2> checked_base2_ptr(checked_derived_ptr);
    checked_base1_ptr = checked_derived_ptr;
    checked_base2_ptr = checked_derived_ptr;
  }

  {
    Derived derived_val(42, 84, 1024);
    CountingRawPtr<Derived> checked_derived_ptr = &derived_val;
    CountingRawPtr<Base1> checked_base1_ptr(std::move(checked_derived_ptr));
    CountingRawPtr<Base2> checked_base2_ptr(std::move(checked_derived_ptr));
    checked_base1_ptr = std::move(checked_derived_ptr);
    checked_base2_ptr = std::move(checked_derived_ptr);
  }

  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

TEST_F(RawPtrTest, CustomSwap) {
  int foo1, foo2;
  CountingRawPtr<int> ptr1(&foo1);
  CountingRawPtr<int> ptr2(&foo2);
  // Recommended use pattern.
  using std::swap;
  swap(ptr1, ptr2);
  EXPECT_EQ(ptr1.get(), &foo2);
  EXPECT_EQ(ptr2.get(), &foo1);
  EXPECT_EQ(g_wrapped_ptr_swap_cnt, 1);
}

TEST_F(RawPtrTest, StdSwap) {
  int foo1, foo2;
  CountingRawPtr<int> ptr1(&foo1);
  CountingRawPtr<int> ptr2(&foo2);
  std::swap(ptr1, ptr2);
  EXPECT_EQ(ptr1.get(), &foo2);
  EXPECT_EQ(ptr2.get(), &foo1);
  EXPECT_EQ(g_wrapped_ptr_swap_cnt, 0);
}

TEST_F(RawPtrTest, PostIncrementOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = foo;
  for (int i = 0; i < 4; ++i) {
    ASSERT_EQ(*ptr++, 42 + i);
  }
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 4);
}

TEST_F(RawPtrTest, PostDecrementOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = &foo[3];
  for (int i = 3; i >= 0; --i) {
    ASSERT_EQ(*ptr--, 42 + i);
  }
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 4);
}

TEST_F(RawPtrTest, PreIncrementOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = foo;
  for (int i = 0; i < 4; ++i, ++ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 4);
}

TEST_F(RawPtrTest, PreDecrementOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = &foo[3];
  for (int i = 3; i >= 0; --i, --ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 4);
}

TEST_F(RawPtrTest, PlusEqualOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = foo;
  for (int i = 0; i < 4; i += 2, ptr += 2) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 2);
}

TEST_F(RawPtrTest, MinusEqualOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = &foo[3];
  for (int i = 3; i >= 0; i -= 2, ptr -= 2) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 2);
}

TEST_F(RawPtrTest, AdvanceString) {
  const char kChars[] = "Hello";
  std::string str = kChars;
  CountingRawPtr<const char> ptr = str.c_str();
  for (size_t i = 0; i < str.size(); ++i, ++ptr) {
    ASSERT_EQ(*ptr, kChars[i]);
  }
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 5);
}

TEST_F(RawPtrTest, AssignmentFromNullptr) {
  CountingRawPtr<int> wrapped_ptr;
  wrapped_ptr = nullptr;
  EXPECT_EQ(g_wrap_raw_ptr_cnt, 0);
  EXPECT_EQ(g_get_for_comparison_cnt, 0);
  EXPECT_EQ(g_get_for_extraction_cnt, 0);
  EXPECT_EQ(g_get_for_dereference_cnt, 0);
}

}  // namespace

namespace base {
namespace internal {

#if BUILDFLAG(USE_BACKUP_REF_PTR) && !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

void HandleOOM(size_t unused_size) {
  LOG(FATAL) << "Out of memory";
}

static constexpr PartitionOptions kOpts = {
    PartitionOptions::AlignedAlloc::kDisallowed,
    PartitionOptions::ThreadCache::kDisabled,
    PartitionOptions::Quarantine::kDisallowed,
    PartitionOptions::Cookies::kAllowed, PartitionOptions::RefCount::kAllowed};

TEST(BackupRefPtrImpl, Basic) {
  // TODO(bartekn): Avoid using PartitionAlloc API directly. Switch to
  // new/delete once PartitionAlloc Everywhere is fully enabled.
  PartitionAllocGlobalInit(HandleOOM);
  PartitionAllocator<ThreadSafe> allocator;
  allocator.init(kOpts);
  uint64_t* raw_ptr1 = reinterpret_cast<uint64_t*>(
      allocator.root()->Alloc(sizeof(uint64_t), ""));
  // Use the actual raw_ptr implementation, not a test substitute, to
  // exercise real PartitionAlloc paths.
  raw_ptr<uint64_t> wrapped_ptr1 = raw_ptr1;

  *raw_ptr1 = 42;
  EXPECT_EQ(*raw_ptr1, *wrapped_ptr1);

  allocator.root()->Free(raw_ptr1);
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  // In debug builds, the use-after-free should be caught immediately.
  EXPECT_DEATH_IF_SUPPORTED(if (*wrapped_ptr1 == 42) return, "");
#else   // DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  // The allocation should be poisoned since there's a raw_ptr alive.
  EXPECT_NE(*wrapped_ptr1, 42ul);

  // The allocator should not be able to reuse the slot at this point.
  void* raw_ptr2 = allocator.root()->Alloc(sizeof(uint64_t), "");
  EXPECT_NE(raw_ptr1, raw_ptr2);
  allocator.root()->Free(raw_ptr2);

  // When the last reference is released, the slot should become reusable.
  wrapped_ptr1 = nullptr;
  void* raw_ptr3 = allocator.root()->Alloc(sizeof(uint64_t), "");
  EXPECT_EQ(raw_ptr1, raw_ptr3);
  allocator.root()->Free(raw_ptr3);
#endif  // DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
}

TEST(BackupRefPtrImpl, ZeroSized) {
  // TODO(bartekn): Avoid using PartitionAlloc API directly. Switch to
  // new/delete once PartitionAlloc Everywhere is fully enabled.
  PartitionAllocGlobalInit(HandleOOM);
  PartitionAllocator<ThreadSafe> allocator;
  allocator.init(kOpts);

  std::vector<raw_ptr<void>> ptrs;
  // Use a reasonable number of elements to fill up the slot span.
  for (int i = 0; i < 128 * 1024; ++i) {
    // Constructing a raw_ptr instance from a zero-sized allocation should
    // not result in a crash.
    ptrs.emplace_back(allocator.root()->Alloc(0, ""));
  }
}

TEST(BackupRefPtrImpl, EndPointer) {
  // This test requires a fresh partition with an empty free list.
  PartitionAllocGlobalInit(HandleOOM);
  PartitionAllocator<ThreadSafe> allocator;
  allocator.init(kOpts);

  // Check multiple size buckets and levels of slot filling.
  for (int size = 0; size < 1024; size += sizeof(void*)) {
    // Creating a raw_ptr from an address right past the end of an allocation
    // should not result in a crash or corrupt the free list.
    char* raw_ptr1 = reinterpret_cast<char*>(allocator.root()->Alloc(size, ""));
    raw_ptr<char> wrapped_ptr = raw_ptr1 + size;
    wrapped_ptr = nullptr;
    // We need to make two more allocations to turn the possible free list
    // corruption into an observable crash.
    char* raw_ptr2 = reinterpret_cast<char*>(allocator.root()->Alloc(size, ""));
    char* raw_ptr3 = reinterpret_cast<char*>(allocator.root()->Alloc(size, ""));

    // Similarly for operator+=.
    char* raw_ptr4 = reinterpret_cast<char*>(allocator.root()->Alloc(size, ""));
    wrapped_ptr = raw_ptr4;
    wrapped_ptr += size;
    wrapped_ptr = nullptr;
    char* raw_ptr5 = reinterpret_cast<char*>(allocator.root()->Alloc(size, ""));
    char* raw_ptr6 = reinterpret_cast<char*>(allocator.root()->Alloc(size, ""));

    allocator.root()->Free(raw_ptr1);
    allocator.root()->Free(raw_ptr2);
    allocator.root()->Free(raw_ptr3);
    allocator.root()->Free(raw_ptr4);
    allocator.root()->Free(raw_ptr5);
    allocator.root()->Free(raw_ptr6);
  }
}

#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
TEST(BackupRefPtrImpl, ReinterpretCast) {
  // TODO(bartekn): Avoid using PartitionAlloc API directly. Switch to
  // new/delete once PartitionAlloc Everywhere is fully enabled.
  PartitionAllocGlobalInit(HandleOOM);
  PartitionAllocator<ThreadSafe> allocator;
  allocator.init(kOpts);

  void* raw_ptr = allocator.root()->Alloc(16, "");
  allocator.root()->Free(raw_ptr);

  raw_ptr<void>* wrapped_ptr = reinterpret_cast<raw_ptr<void>*>(&raw_ptr);
  // The reference count cookie check should detect that the allocation has
  // been already freed.
  EXPECT_DEATH_IF_SUPPORTED(*wrapped_ptr = nullptr, "");
}
#endif

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR) &&
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

}  // namespace internal
}  // namespace base
