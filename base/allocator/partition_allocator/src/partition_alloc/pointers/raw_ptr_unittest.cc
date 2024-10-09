// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/pointers/raw_ptr.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_alloc_support.h"
#include "base/cpu.h"
#include "base/metrics/histogram_base.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/memory/dangling_ptr_instrumentation.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/to_address.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/cpu.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/numerics/checked_math.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_hooks.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/pointers/instance_tracer.h"
#include "partition_alloc/pointers/raw_ptr_counting_impl_for_test.h"
#include "partition_alloc/pointers/raw_ptr_test_support.h"
#include "partition_alloc/pointers/raw_ref.h"
#include "partition_alloc/tagging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include <sanitizer/asan_interface.h>
#include "base/debug/asan_service.h"
#endif

using testing::AllOf;
using testing::Eq;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Ne;
using testing::SizeIs;
using testing::Test;

// The instance tracer has unavoidable per-instance overhead, but when disabled,
// there should be no size difference between raw_ptr<T> and T*.
#if !PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_INSTANCE_TRACER)
static_assert(sizeof(raw_ptr<void>) == sizeof(void*),
              "raw_ptr shouldn't add memory overhead");
static_assert(sizeof(raw_ptr<int>) == sizeof(int*),
              "raw_ptr shouldn't add memory overhead");
static_assert(sizeof(raw_ptr<std::string>) == sizeof(std::string*),
              "raw_ptr shouldn't add memory overhead");
#endif

#if !PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL) &&   \
    !PA_BUILDFLAG(USE_RAW_PTR_ASAN_UNOWNED_IMPL) && \
    !PA_BUILDFLAG(USE_RAW_PTR_HOOKABLE_IMPL) &&     \
    !PA_BUILDFLAG(RAW_PTR_ZERO_ON_MOVE) &&          \
    !PA_BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT)
// |is_trivially_copyable| assertion means that arrays/vectors of raw_ptr can
// be copied by memcpy.
static_assert(std::is_trivially_copyable_v<raw_ptr<void>>,
              "raw_ptr should be trivially copyable");
static_assert(std::is_trivially_copyable_v<raw_ptr<int>>,
              "raw_ptr should be trivially copyable");
static_assert(std::is_trivially_copyable_v<raw_ptr<std::string>>,
              "raw_ptr should be trivially copyable");
#endif  // !PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL) &&
        // !PA_BUILDFLAG(USE_RAW_PTR_ASAN_UNOWNED_IMPL) &&
        // !PA_BUILDFLAG(USE_RAW_PTR_HOOKABLE_IMPL) &&
        // !PA_BUILDFLAG(RAW_PTR_ZERO_ON_MOVE) &&
        // !PA_BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT)

#if !PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL) &&   \
    !PA_BUILDFLAG(USE_RAW_PTR_ASAN_UNOWNED_IMPL) && \
    !PA_BUILDFLAG(USE_RAW_PTR_HOOKABLE_IMPL) &&     \
    !PA_BUILDFLAG(RAW_PTR_ZERO_ON_CONSTRUCT) &&     \
    !PA_BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT)
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
static_assert(std::is_trivially_default_constructible_v<raw_ptr<void>>,
              "raw_ptr should be trivially default constructible");
static_assert(std::is_trivially_default_constructible_v<raw_ptr<int>>,
              "raw_ptr should be trivially default constructible");
static_assert(std::is_trivially_default_constructible_v<raw_ptr<std::string>>,
              "raw_ptr should be trivially default constructible");
#endif  // !PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL) &&
        // !PA_BUILDFLAG(USE_RAW_PTR_ASAN_UNOWNED_IMPL) &&
        // !PA_BUILDFLAG(USE_RAW_PTR_HOOKABLE_IMPL) &&
        // !PA_BUILDFLAG(RAW_PTR_ZERO_ON_CONSTRUCT) &&
        // !PA_BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT)

// Verify that raw_ptr is a literal type, and its entire interface is constexpr.
//
// Constexpr destructors were introduced in C++20. PartitionAlloc's minimum
// supported C++ version is C++17, so raw_ptr is not a literal type in C++17.
// Thus we only test for constexpr in C++20.
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
static_assert([]() constexpr {
  struct IntBase {};
  struct Int : public IntBase {
    int i = 0;
  };

  Int* i = new Int();
  {
    raw_ptr<Int> r(i);              // raw_ptr(T*)
    raw_ptr<Int> r2(r);             // raw_ptr(const raw_ptr&)
    raw_ptr<Int> r3(std::move(r));  // raw_ptr(raw_ptr&&)
    r = r2;                         // operator=(const raw_ptr&)
    r = std::move(r3);              // operator=(raw_ptr&&)
    raw_ptr<Int, base::RawPtrTraits::kMayDangle> r4(
        r);   // raw_ptr(const raw_ptr<DifferentTraits>&)
    r4 = r2;  // operator=(const raw_ptr<DifferentTraits>&)
    // (There is no move-version of DifferentTraits.)
    [[maybe_unused]] raw_ptr<IntBase> r5(
        r2);  // raw_ptr(const raw_ptr<Convertible>&)
    [[maybe_unused]] raw_ptr<IntBase> r6(
        std::move(r2));  // raw_ptr(raw_ptr<Convertible>&&)
    r2 = r;              // Reset after move...
    r5 = r2;             // operator=(const raw_ptr<Convertible>&)
    r5 = std::move(r2);  // operator=(raw_ptr<Convertible>&&)
    [[maybe_unused]] raw_ptr<Int> r7(nullptr);  // raw_ptr(nullptr)
    r4 = nullptr;                               // operator=(nullptr)
    r4 = i;                                     // operator=(T*)
    r5 = r4;                                    // operator=(const Upcast&)
    r5 = std::move(r4);                         // operator=(Upcast&&)
    r.get()->i += 1;                            // get()
    [[maybe_unused]] bool b = r;                // operator bool
    (*r).i += 1;                                // operator*()
    r->i += 1;                                  // operator->()
    [[maybe_unused]] Int* i2 = r;               // operator T*()
    [[maybe_unused]] IntBase* i3 = r;           // operator Convertible*()

    auto func_taking_ptr_to_ptr = [](Int**) {};
    auto func_taking_ref_to_ptr = [](Int*&) {};
    func_taking_ptr_to_ptr(&r.AsEphemeralRawAddr());
    func_taking_ref_to_ptr(r.AsEphemeralRawAddr());

    Int* array = new Int[4]();
    {
      raw_ptr<Int, base::RawPtrTraits::kAllowPtrArithmetic> ra(array);
      ++ra;      // operator++()
      --ra;      // operator--()
      ra++;      // operator++(int)
      ra--;      // operator--(int)
      ra += 1u;  // operator+=()
      ra -= 1u;  // operator-=()
      ra = ra + 1;                             // operator+(raw_ptr,int)
      ra = 1 + ra;                             // operator+(int,raw_ptr)
      ra = ra - 2;                             // operator-(raw_ptr,int)
      [[maybe_unused]] ptrdiff_t d = ra - ra;  // operator-(raw_ptr,raw_ptr)
      d = ra - array;                          // operator-(raw_ptr,T*)
      d = array - ra;                          // operator-(T*,raw_ptr)

      ra[0] = ra[1];  // operator[]()

      b = ra < ra;      // operator<(raw_ptr,raw_ptr)
      b = ra < array;   // operator<(raw_ptr,T*)
      b = array < ra;   // operator<(T*,raw_ptr)
      b = ra <= ra;     // operator<=(raw_ptr,raw_ptr)
      b = ra <= array;  // operator<=(raw_ptr,T*)
      b = array <= ra;  // operator<=(T*,raw_ptr)
      b = ra > ra;      // operator>(raw_ptr,raw_ptr)
      b = ra > array;   // operator>(raw_ptr,T*)
      b = array > ra;   // operator>(T*,raw_ptr)
      b = ra >= ra;     // operator>=(raw_ptr,raw_ptr)
      b = ra >= array;  // operator>=(raw_ptr,T*)
      b = array >= ra;  // operator>=(T*,raw_ptr)
      b = ra == ra;     // operator==(raw_ptr,raw_ptr)
      b = ra == array;  // operator==(raw_ptr,T*)
      b = array == ra;  // operator==(T*,raw_ptr)
      b = ra != ra;     // operator!=(raw_ptr,raw_ptr)
      b = ra != array;  // operator!=(raw_ptr,T*)
      b = array != ra;  // operator!=(T*,raw_ptr)
    }
    delete[] array;
  }
  delete i;
  return true;
}());
#endif

struct StructWithoutTypeBasedTraits {};
struct BaseWithTypeBasedTraits {};
struct DerivedWithTypeBasedTraits : BaseWithTypeBasedTraits {};

namespace base::raw_ptr_traits {
// `BaseWithTypeBasedTraits` and any derived classes have
// `RawPtrTraits::kDummyForTest`.
template <typename T>
constexpr auto kTypeTraits<
    T,
    std::enable_if_t<std::is_base_of_v<BaseWithTypeBasedTraits, T>>> =
    RawPtrTraits::kDummyForTest;
}  // namespace base::raw_ptr_traits

// `raw_ptr<T>` should have traits based on specialization of `kTypeTraits<T>`.
static_assert(!ContainsFlags(raw_ptr<StructWithoutTypeBasedTraits>::Traits,
                             base::RawPtrTraits::kDummyForTest));
static_assert(ContainsFlags(raw_ptr<BaseWithTypeBasedTraits>::Traits,
                            base::RawPtrTraits::kDummyForTest));
static_assert(ContainsFlags(raw_ptr<DerivedWithTypeBasedTraits>::Traits,
                            base::RawPtrTraits::kDummyForTest));

// Don't use base::internal for testing raw_ptr API, to test if code outside
// this namespace calls the correct functions from this namespace.
namespace {

// Shorter name for expected test impl.
using RawPtrCountingImpl = base::test::RawPtrCountingImplForTest;

template <typename T>
using CountingRawPtr = raw_ptr<T,
                               base::RawPtrTraits::kUseCountingImplForTest |
                                   base::RawPtrTraits::kAllowPtrArithmetic>;

// Ensure that the `kUseCountingImplForTest` flag selects the test impl.
static_assert(std::is_same_v<CountingRawPtr<int>::Impl, RawPtrCountingImpl>);

template <typename T>
using CountingRawPtrMayDangle =
    raw_ptr<T,
            base::RawPtrTraits::kMayDangle |
                base::RawPtrTraits::kUseCountingImplForTest |
                base::RawPtrTraits::kAllowPtrArithmetic>;

// Ensure that the `kUseCountingImplForTest` flag selects the test impl.
static_assert(
    std::is_same_v<CountingRawPtrMayDangle<int>::Impl, RawPtrCountingImpl>);

template <typename T>
using CountingRawPtrUninitialized =
    raw_ptr<T,
            base::RawPtrTraits::kUseCountingImplForTest |
                base::RawPtrTraits::kAllowUninitialized>;

// Ensure that the `kUseCountingImplForTest` flag selects the test impl.
static_assert(
    std::is_same_v<CountingRawPtrUninitialized<int>::Impl, RawPtrCountingImpl>);

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
  void SetUp() override {
    RawPtrCountingImpl::ClearCounters();
  }
};

// Use this instead of std::ignore, to prevent the instruction from getting
// optimized out by the compiler.
volatile int g_volatile_int_to_ignore;

TEST_F(RawPtrTest, NullStarDereference) {
  raw_ptr<int> ptr = nullptr;
  EXPECT_DEATH_IF_SUPPORTED(g_volatile_int_to_ignore = *ptr, "");
}

TEST_F(RawPtrTest, NullArrowDereference) {
  raw_ptr<MyStruct> ptr = nullptr;
  EXPECT_DEATH_IF_SUPPORTED(g_volatile_int_to_ignore = ptr->x, "");
}

TEST_F(RawPtrTest, NullExtractNoDereference) {
  CountingRawPtr<int> ptr = nullptr;
  // No dereference hence shouldn't crash.
  int* raw = ptr;
  std::ignore = raw;
  EXPECT_THAT((CountingRawPtrExpectations{.get_for_dereference_cnt = 0,
                                          .get_for_extraction_cnt = 1,
                                          .get_for_comparison_cnt = 0}),
              CountersMatch());
}

TEST_F(RawPtrTest, InvalidExtractNoDereference) {
  // Some code uses invalid pointer values as indicators, so those values must
  // be accepted by raw_ptr and passed through unchanged during extraction.
  int* inv_ptr = reinterpret_cast<int*>(~static_cast<uintptr_t>(0));
  CountingRawPtr<int> ptr = inv_ptr;
  int* raw = ptr;
  EXPECT_EQ(raw, inv_ptr);
  EXPECT_THAT((CountingRawPtrExpectations{.get_for_dereference_cnt = 0,
                                          .get_for_extraction_cnt = 1,
                                          .get_for_comparison_cnt = 0}),
              CountersMatch());
}

TEST_F(RawPtrTest, NullCmpExplicit) {
  CountingRawPtr<int> ptr = nullptr;
  EXPECT_TRUE(ptr == nullptr);
  EXPECT_TRUE(nullptr == ptr);
  EXPECT_FALSE(ptr != nullptr);
  EXPECT_FALSE(nullptr != ptr);
  // No need to unwrap pointer, just compare against 0.
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, NullCmpBool) {
  CountingRawPtr<int> ptr = nullptr;
  EXPECT_FALSE(ptr);
  EXPECT_TRUE(!ptr);
  // No need to unwrap pointer, just compare against 0.
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
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
  if (ptr) {
    is_valid = true;
  }
  [[maybe_unused]] bool is_not_valid = !ptr;
  if (!ptr) {
    is_not_valid = true;
  }
  std::ignore = IsValidNoCast(ptr);
  std::ignore = IsValidNoCast2(ptr);
  FuncThatAcceptsBool(!ptr);
  // No need to unwrap pointer, just compare against 0.
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
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
  [[maybe_unused]] bool is_valid = ptr;
  is_valid = IsValidWithCast(ptr);
  FuncThatAcceptsBool(ptr);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 3,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, StarDereference) {
  int foo = 42;
  CountingRawPtr<int> ptr = &foo;
  EXPECT_EQ(*ptr, 42);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 1,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, ArrowDereference) {
  MyStruct foo = {42};
  CountingRawPtr<MyStruct> ptr = &foo;
  EXPECT_EQ(ptr->x, 42);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 1,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, Delete) {
  CountingRawPtr<int> ptr = new int(42);
  delete ptr.ExtractAsDangling();
  // The pointer is first internally converted to MayDangle kind, then extracted
  // using implicit cast before passing to |delete|.
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 1,
                  .get_for_comparison_cnt = 0,
                  .wrap_raw_ptr_for_dup_cnt = 1,
                  .get_for_duplication_cnt = 1,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, ClearAndDelete) {
  CountingRawPtr<int> ptr(new int);
  ptr.ClearAndDelete();

  EXPECT_THAT((CountingRawPtrExpectations{
                .wrap_raw_ptr_cnt = 1,
                .release_wrapped_ptr_cnt = 1,
                .get_for_dereference_cnt = 0,
                .get_for_extraction_cnt = 1,
                .wrapped_ptr_swap_cnt = 0,
              }),
              CountersMatch());
  EXPECT_EQ(ptr.get(), nullptr);
}

TEST_F(RawPtrTest, ClearAndDeleteArray) {
  CountingRawPtr<int> ptr(new int[8]);
  ptr.ClearAndDeleteArray();

  EXPECT_THAT((CountingRawPtrExpectations{
                .wrap_raw_ptr_cnt = 1,
                .release_wrapped_ptr_cnt = 1,
                .get_for_dereference_cnt = 0,
                .get_for_extraction_cnt = 1,
                .wrapped_ptr_swap_cnt = 0,
              }),
              CountersMatch());
  EXPECT_EQ(ptr.get(), nullptr);
}

TEST_F(RawPtrTest, ExtractAsDangling) {
  CountingRawPtr<int> ptr(new int);

  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 1,
                  .release_wrapped_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .wrapped_ptr_swap_cnt = 0,
                  .wrap_raw_ptr_for_dup_cnt = 0,
                  .get_for_duplication_cnt = 0,
              }),
              CountersMatch());

  EXPECT_TRUE(ptr.get());

  CountingRawPtrMayDangle<int> dangling = ptr.ExtractAsDangling();

  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 1,
                  .release_wrapped_ptr_cnt = 1,
                  .get_for_dereference_cnt = 0,
                  .wrapped_ptr_swap_cnt = 0,
                  .wrap_raw_ptr_for_dup_cnt = 1,
                  .get_for_duplication_cnt = 1,
              }),
              CountersMatch());

  EXPECT_FALSE(ptr.get());
  EXPECT_TRUE(dangling.get());

  dangling.ClearAndDelete();
}

TEST_F(RawPtrTest, ExtractAsDanglingFromDangling) {
  CountingRawPtrMayDangle<int> ptr(new int);

  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 1,
                  .release_wrapped_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .wrapped_ptr_swap_cnt = 0,
                  .wrap_raw_ptr_for_dup_cnt = 0,
                  .get_for_duplication_cnt = 0,
              }),
              CountersMatch());

  CountingRawPtrMayDangle<int> dangling = ptr.ExtractAsDangling();

  // wrap_raw_ptr_cnt remains `1` because, as `ptr` is already a dangling
  // pointer, we are only moving `ptr` to `dangling` here to avoid extra cost.
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 1,
                  .release_wrapped_ptr_cnt = 1,
                  .get_for_dereference_cnt = 0,
                  .wrapped_ptr_swap_cnt = 0,
                  .wrap_raw_ptr_for_dup_cnt = 0,
                  .get_for_duplication_cnt = 0,
              }),
              CountersMatch());

  dangling.ClearAndDelete();
}

TEST_F(RawPtrTest, ConstVolatileVoidPtr) {
  int32_t foo[] = {1234567890};
  CountingRawPtr<const volatile void> ptr = foo;
  EXPECT_EQ(*static_cast<const volatile int32_t*>(ptr), 1234567890);
  // Because we're using a cast, the extraction API kicks in, which doesn't
  // know if the extracted pointer will be dereferenced or not.
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 1,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, VoidPtr) {
  int32_t foo[] = {1234567890};
  CountingRawPtr<void> ptr = foo;
  EXPECT_EQ(*static_cast<int32_t*>(ptr), 1234567890);
  // Because we're using a cast, the extraction API kicks in, which doesn't
  // know if the extracted pointer will be dereferenced or not.
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 1,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
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

  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 12,
              }),
              CountersMatch());
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

  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 12,
              }),
              CountersMatch());
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
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 16,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, OperatorEQCastHierarchy) {
  Derived derived_val(42, 84, 1024);
  Derived* raw_derived_ptr = &derived_val;
  const Base1* raw_base1_ptr = &derived_val;
  volatile Base2* raw_base2_ptr = &derived_val;
  // Double check the basic understanding of pointers: Even though the numeric
  // value (i.e. the address) isn't equal, the pointers are still equal. That's
  // because from derived to base adjusts the address.
  // raw_ptr must behave the same, which is checked below.
  ASSERT_NE(reinterpret_cast<uintptr_t>(raw_base2_ptr),
            reinterpret_cast<uintptr_t>(raw_derived_ptr));
  ASSERT_TRUE(raw_base2_ptr == raw_derived_ptr);

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
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 4,
                  .get_for_comparison_cnt = 20,
              }),
              CountersMatch());
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
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 16,
              }),
              CountersMatch());
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
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 4,
                  .get_for_comparison_cnt = 20,
              }),
              CountersMatch());
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
  EXPECT_FALSE((std::is_convertible_v<raw_ptr<Derived>, raw_ptr<Base>>));
  EXPECT_FALSE((std::is_convertible_v<raw_ptr<Unrelated>, raw_ptr<Base>>));
  EXPECT_FALSE((std::is_convertible_v<raw_ptr<Unrelated>, raw_ptr<void>>));
  EXPECT_FALSE((std::is_convertible_v<raw_ptr<void>, raw_ptr<Unrelated>>));
  EXPECT_FALSE((std::is_convertible_v<raw_ptr<int64_t>, raw_ptr<int32_t>>));
  EXPECT_FALSE((std::is_convertible_v<raw_ptr<int16_t>, raw_ptr<int32_t>>));
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

  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
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
  EXPECT_EQ(RawPtrCountingImpl::wrapped_ptr_swap_cnt, 1);
}

TEST_F(RawPtrTest, StdSwap) {
  int foo1, foo2;
  CountingRawPtr<int> ptr1(&foo1);
  CountingRawPtr<int> ptr2(&foo2);
  std::swap(ptr1, ptr2);
  EXPECT_EQ(ptr1.get(), &foo2);
  EXPECT_EQ(ptr2.get(), &foo1);
  EXPECT_EQ(RawPtrCountingImpl::wrapped_ptr_swap_cnt, 0);
}

TEST_F(RawPtrTest, PostIncrementOperator) {
  std::vector<int> foo({42, 43, 44, 45});
  CountingRawPtr<int> ptr = &foo[0];
  for (int i = 0; i < 4; ++i) {
    ASSERT_EQ(*ptr++, 42 + i);
  }
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 4,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, PostDecrementOperator) {
  std::vector<int> foo({42, 43, 44, 45});
  CountingRawPtr<int> ptr = &foo[3];
  // Avoid decrementing out of the slot holding the vector's backing store.
  for (int i = 3; i > 0; --i) {
    ASSERT_EQ(*ptr--, 42 + i);
  }
  ASSERT_EQ(*ptr, 42);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 4,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, PreIncrementOperator) {
  std::vector<int> foo({42, 43, 44, 45});
  CountingRawPtr<int> ptr = &foo[0];
  for (int i = 0; i < 4; ++i, ++ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 4,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, PreDecrementOperator) {
  std::vector<int> foo({42, 43, 44, 45});
  CountingRawPtr<int> ptr = &foo[3];
  // Avoid decrementing out of the slot holding the vector's backing store.
  for (int i = 3; i > 0; --i, --ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  ASSERT_EQ(*ptr, 42);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 4,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, PlusEqualOperator) {
  std::vector<int> foo({42, 43, 44, 45});
  CountingRawPtr<int> ptr = &foo[0];
  for (int i = 0; i < 4; i += 2, ptr += 2) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 2,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, PlusEqualOperatorTypes) {
  std::vector<int> foo({42, 43, 44, 45});
  CountingRawPtr<int> ptr = &foo[0];
  ASSERT_EQ(*ptr, 42);
  ptr += 2;  // Positive literal.
  ASSERT_EQ(*ptr, 44);
  ptr -= 2;  // Negative literal.
  ASSERT_EQ(*ptr, 42);
  ptr += ptrdiff_t{1};  // ptrdiff_t.
  ASSERT_EQ(*ptr, 43);
  ptr += size_t{2};  // size_t.
  ASSERT_EQ(*ptr, 45);
}

TEST_F(RawPtrTest, MinusEqualOperator) {
  std::vector<int> foo({42, 43, 44, 45});
  CountingRawPtr<int> ptr = &foo[3];
  ASSERT_EQ(*ptr, 45);
  ptr -= 2;
  ASSERT_EQ(*ptr, 43);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 2,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, MinusEqualOperatorTypes) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = &foo[3];
  ASSERT_EQ(*ptr, 45);
  ptr -= 2;  // Positive literal.
  ASSERT_EQ(*ptr, 43);
  ptr -= -2;  // Negative literal.
  ASSERT_EQ(*ptr, 45);
  ptr -= ptrdiff_t{2};  // ptrdiff_t.
  ASSERT_EQ(*ptr, 43);
  ptr -= size_t{1};  // size_t.
  ASSERT_EQ(*ptr, 42);
}

TEST_F(RawPtrTest, PlusOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = foo;
  for (int i = 0; i < 4; ++i) {
    ASSERT_EQ(*(ptr + i), 42 + i);
  }
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 4,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, MinusOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptr = &foo[4];
  for (int i = 1; i <= 4; ++i) {
    ASSERT_EQ(*(ptr - i), 46 - i);
  }
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 4,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, MinusDeltaOperator) {
  int foo[] = {42, 43, 44, 45};
  CountingRawPtr<int> ptrs[] = {&foo[0], &foo[1], &foo[2], &foo[3], &foo[4]};
  for (int i = 0; i <= 4; ++i) {
    for (int j = 0; j <= 4; ++j) {
      ASSERT_EQ(ptrs[i] - ptrs[j], i - j);
      ASSERT_EQ(ptrs[i] - &foo[j], i - j);
      ASSERT_EQ(&foo[i] - ptrs[j], i - j);
    }
  }
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, AdvanceString) {
  const char kChars[] = "Hello";
  std::string str = kChars;
  CountingRawPtr<const char> ptr = str.c_str();
  for (size_t i = 0; i < str.size(); ++i, ++ptr) {
    ASSERT_EQ(*ptr, kChars[i]);
  }
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 5,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, AssignmentFromNullptr) {
  CountingRawPtr<int> wrapped_ptr;
  wrapped_ptr = nullptr;
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());
}

void FunctionWithRawPtrParameter(raw_ptr<int> actual_ptr, int* expected_ptr) {
  EXPECT_EQ(actual_ptr.get(), expected_ptr);
  EXPECT_EQ(*actual_ptr, *expected_ptr);
}

// This test checks that raw_ptr<T> can be passed by value into function
// parameters.  This is mostly a smoke test for TRIVIAL_ABI attribute.
TEST_F(RawPtrTest, FunctionParameters_ImplicitlyMovedTemporary) {
  int x = 123;
  FunctionWithRawPtrParameter(
      raw_ptr<int>(&x),  // Temporary that will be moved into the function.
      &x);
}

// This test checks that raw_ptr<T> can be passed by value into function
// parameters.  This is mostly a smoke test for TRIVIAL_ABI attribute.
TEST_F(RawPtrTest, FunctionParameters_ExplicitlyMovedLValue) {
  int x = 123;
  raw_ptr<int> ptr(&x);
  FunctionWithRawPtrParameter(std::move(ptr), &x);
}

// This test checks that raw_ptr<T> can be passed by value into function
// parameters.  This is mostly a smoke test for TRIVIAL_ABI attribute.
TEST_F(RawPtrTest, FunctionParameters_Copy) {
  int x = 123;
  raw_ptr<int> ptr(&x);
  FunctionWithRawPtrParameter(ptr,  // `ptr` will be copied into the function.
                              &x);
}

TEST_F(RawPtrTest, SetLookupUsesGetForComparison) {
  int x = 123;
  CountingRawPtr<int> ptr(&x);
  std::set<CountingRawPtr<int>> set;

  RawPtrCountingImpl::ClearCounters();
  set.emplace(&x);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 1,
                  // Nothing to compare to yet.
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 0,
                  .wrapped_ptr_less_cnt = 0,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();
  set.emplace(ptr);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  // 2 items to compare to => 4 calls.
                  .get_for_comparison_cnt = 4,
                  // 1 element to compare to => 2 calls.
                  .wrapped_ptr_less_cnt = 2,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();
  set.count(&x);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  // 2 comparisons => 2 extractions. Less than before, because
                  // this time a raw pointer is one side of the comparison.
                  .get_for_comparison_cnt = 2,
                  // 2 items to compare to => 4 calls.
                  .wrapped_ptr_less_cnt = 2,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();
  set.count(ptr);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  // 2 comparisons => 4 extractions.
                  .get_for_comparison_cnt = 4,
                  // 2 items to compare to => 4 calls.
                  .wrapped_ptr_less_cnt = 2,
              }),
              CountersMatch());
}

TEST_F(RawPtrTest, ComparisonOperatorUsesGetForComparison) {
  int x = 123;
  CountingRawPtr<int> ptr(&x);

  RawPtrCountingImpl::ClearCounters();
  EXPECT_FALSE(ptr < ptr);
  EXPECT_FALSE(ptr > ptr);
  EXPECT_TRUE(ptr <= ptr);
  EXPECT_TRUE(ptr >= ptr);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 8,
                  // < is used directly, not std::less().
                  .wrapped_ptr_less_cnt = 0,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();
  EXPECT_FALSE(ptr < &x);
  EXPECT_FALSE(ptr > &x);
  EXPECT_TRUE(ptr <= &x);
  EXPECT_TRUE(ptr >= &x);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 4,
                  .wrapped_ptr_less_cnt = 0,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();
  EXPECT_FALSE(&x < ptr);
  EXPECT_FALSE(&x > ptr);
  EXPECT_TRUE(&x <= ptr);
  EXPECT_TRUE(&x >= ptr);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .wrap_raw_ptr_cnt = 0,
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 4,
                  .wrapped_ptr_less_cnt = 0,
              }),
              CountersMatch());
}

// Two `raw_ptr`s with different Traits should still hit `GetForComparison()`
// (as opposed to `GetForExtraction()`) in their comparison operators. We use
// `CountingRawPtr` and `CountingRawPtrMayDangle` to contrast two different
// Traits.
TEST_F(RawPtrTest, OperatorsUseGetForComparison) {
  int x = 123;
  CountingRawPtr<int> ptr1 = &x;
  CountingRawPtrMayDangle<int> ptr2 = &x;

  RawPtrCountingImpl::ClearCounters();

  EXPECT_TRUE(ptr1 == ptr2);
  EXPECT_FALSE(ptr1 != ptr2);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 4,
              }),
              CountersMatch());

  EXPECT_FALSE(ptr1 < ptr2);
  EXPECT_FALSE(ptr1 > ptr2);
  EXPECT_TRUE(ptr1 <= ptr2);
  EXPECT_TRUE(ptr1 >= ptr2);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = 12,
              }),
              CountersMatch());
}

// This test checks how the std library handles collections like
// std::vector<raw_ptr<T>>.
TEST_F(RawPtrTest, TrivialRelocability) {
  std::vector<CountingRawPtr<int>> vector;
  int x = 123;

  // See how many times raw_ptr's destructor is called when std::vector
  // needs to increase its capacity and reallocate the internal vector
  // storage (moving the raw_ptr elements).
  RawPtrCountingImpl::ClearCounters();
  size_t number_of_capacity_changes = 0;
  do {
    size_t previous_capacity = vector.capacity();
    while (vector.capacity() == previous_capacity) {
      vector.emplace_back(&x);
    }
    number_of_capacity_changes++;
  } while (number_of_capacity_changes < 10);
  EXPECT_EQ(0, RawPtrCountingImpl::release_wrapped_ptr_cnt);
  // Basic smoke test that raw_ptr elements in a vector work okay.
  for (const auto& elem : vector) {
    EXPECT_EQ(elem.get(), &x);
    EXPECT_EQ(*elem, x);
  }

  // Verification that release_wrapped_ptr_cnt does capture how many times the
  // destructors are called (e.g. that it is not always zero).
  RawPtrCountingImpl::ClearCounters();
  size_t number_of_cleared_elements = vector.size();
  vector.clear();
#if PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL) ||   \
    PA_BUILDFLAG(USE_RAW_PTR_ASAN_UNOWNED_IMPL) || \
    PA_BUILDFLAG(USE_RAW_PTR_HOOKABLE_IMPL) ||     \
    PA_BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT)
  EXPECT_EQ((int)number_of_cleared_elements,
            RawPtrCountingImpl::release_wrapped_ptr_cnt);
#else
  // TODO(lukasza): NoOpImpl has a default destructor that, unless zeroing is
  // requested, doesn't go through RawPtrCountingImpl::ReleaseWrappedPtr.  So we
  // can't really depend on `g_release_wrapped_ptr_cnt`.  This #else branch
  // should be deleted once USE_BACKUP_REF_PTR is removed (e.g. once
  // BackupRefPtr ships to the Stable channel).
  EXPECT_EQ(0, RawPtrCountingImpl::release_wrapped_ptr_cnt);
  std::ignore = number_of_cleared_elements;
#endif  // PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL) ||
        // PA_BUILDFLAG(USE_RAW_PTR_ASAN_UNOWNED_IMPL) ||
        // PA_BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT)
}

struct BaseStruct {
  explicit BaseStruct(int a) : a(a) {}
  virtual ~BaseStruct() = default;

  int a;
};

struct DerivedType1 : public BaseStruct {
  explicit DerivedType1(int a, int b) : BaseStruct(a), b(b) {}
  int b;
};

struct DerivedType2 : public BaseStruct {
  explicit DerivedType2(int a, int c) : BaseStruct(a), c(c) {}
  int c;
};

TEST_F(RawPtrTest, DerivedStructsComparison) {
  DerivedType1 derived_1(42, 84);
  raw_ptr<DerivedType1> checked_derived1_ptr = &derived_1;
  DerivedType2 derived_2(21, 10);
  raw_ptr<DerivedType2> checked_derived2_ptr = &derived_2;

  // Make sure that comparing a |DerivedType2*| to a |DerivedType1*| casted
  // as a |BaseStruct*| doesn't cause CFI errors.
  EXPECT_NE(checked_derived1_ptr,
            static_cast<BaseStruct*>(checked_derived2_ptr.get()));
  EXPECT_NE(static_cast<BaseStruct*>(checked_derived1_ptr.get()),
            checked_derived2_ptr);
}

class PmfTestBase {
 public:
  int MemFunc(char, double) const { return 11; }
};

class PmfTestDerived : public PmfTestBase {
 public:
  using PmfTestBase::MemFunc;
  int MemFunc(float, double) { return 22; }
};

TEST_F(RawPtrTest, PointerToMemberFunction) {
  PmfTestDerived object;
  int (PmfTestBase::*pmf_base_base)(char, double) const = &PmfTestBase::MemFunc;
  int (PmfTestDerived::*pmf_derived_base)(char, double) const =
      &PmfTestDerived::MemFunc;
  int (PmfTestDerived::*pmf_derived_derived)(float, double) =
      &PmfTestDerived::MemFunc;

  // Test for `derived_ptr`
  CountingRawPtr<PmfTestDerived> derived_ptr = &object;

  EXPECT_EQ((derived_ptr->*pmf_base_base)(0, 0), 11);
  EXPECT_EQ((derived_ptr->*pmf_derived_base)(0, 0), 11);
  EXPECT_EQ((derived_ptr->*pmf_derived_derived)(0, 0), 22);

  // Test for `derived_ptr_const`
  const CountingRawPtr<PmfTestDerived> derived_ptr_const = &object;

  EXPECT_EQ((derived_ptr_const->*pmf_base_base)(0, 0), 11);
  EXPECT_EQ((derived_ptr_const->*pmf_derived_base)(0, 0), 11);
  EXPECT_EQ((derived_ptr_const->*pmf_derived_derived)(0, 0), 22);

  // Test for `const_derived_ptr`
  CountingRawPtr<const PmfTestDerived> const_derived_ptr = &object;

  EXPECT_EQ((const_derived_ptr->*pmf_base_base)(0, 0), 11);
  EXPECT_EQ((const_derived_ptr->*pmf_derived_base)(0, 0), 11);
  // const_derived_ptr->*pmf_derived_derived is not a const member function,
  // so it's not possible to test it.
}

TEST_F(RawPtrTest, WorksWithOptional) {
  int x = 0;
  std::optional<raw_ptr<int>> maybe_int;
  EXPECT_FALSE(maybe_int.has_value());

  maybe_int = nullptr;
  ASSERT_TRUE(maybe_int.has_value());
  EXPECT_EQ(nullptr, maybe_int.value());

  maybe_int = &x;
  ASSERT_TRUE(maybe_int.has_value());
  EXPECT_EQ(&x, maybe_int.value());
}

TEST_F(RawPtrTest, WorksWithVariant) {
  int x = 100;
  std::variant<int, raw_ptr<int>> vary;
  ASSERT_EQ(0u, vary.index());
  EXPECT_EQ(0, std::get<int>(vary));

  vary = x;
  ASSERT_EQ(0u, vary.index());
  EXPECT_EQ(100, std::get<int>(vary));

  vary = nullptr;
  ASSERT_EQ(1u, vary.index());
  EXPECT_EQ(nullptr, std::get<raw_ptr<int>>(vary));

  vary = &x;
  ASSERT_EQ(1u, vary.index());
  EXPECT_EQ(&x, std::get<raw_ptr<int>>(vary));
}

TEST_F(RawPtrTest, CrossKindConversion) {
  int x = 123;
  CountingRawPtr<int> ptr1 = &x;

  RawPtrCountingImpl::ClearCounters();

  CountingRawPtrMayDangle<int> ptr2(ptr1);
  CountingRawPtrMayDangle<int> ptr3(std::move(ptr1));  // Falls back to copy.

  EXPECT_THAT((CountingRawPtrExpectations{.wrap_raw_ptr_cnt = 0,
                                          .get_for_dereference_cnt = 0,
                                          .get_for_extraction_cnt = 0,
                                          .wrap_raw_ptr_for_dup_cnt = 2,
                                          .get_for_duplication_cnt = 2}),
              CountersMatch());
}

TEST_F(RawPtrTest, CrossKindAssignment) {
  int x = 123;
  CountingRawPtr<int> ptr1 = &x;

  RawPtrCountingImpl::ClearCounters();

  CountingRawPtrMayDangle<int> ptr2;
  CountingRawPtrMayDangle<int> ptr3;
  ptr2 = ptr1;
  ptr3 = std::move(ptr1);  // Falls back to copy.

  EXPECT_THAT((CountingRawPtrExpectations{.wrap_raw_ptr_cnt = 0,
                                          .get_for_dereference_cnt = 0,
                                          .get_for_extraction_cnt = 0,
                                          .wrap_raw_ptr_for_dup_cnt = 2,
                                          .get_for_duplication_cnt = 2}),
              CountersMatch());
}

// Without the explicitly customized `raw_ptr::to_address()`,
// `std::to_address()` will use the dereference operator. This is not
// what we want; this test enforces extraction semantics for
// `to_address()`.
TEST_F(RawPtrTest, ToAddressDoesNotDereference) {
  CountingRawPtr<int> ptr = nullptr;
  int* raw = base::to_address(ptr);
  std::ignore = raw;
  EXPECT_THAT((CountingRawPtrExpectations{.get_for_dereference_cnt = 0,
                                          .get_for_extraction_cnt = 1,
                                          .get_for_comparison_cnt = 0,
                                          .get_for_duplication_cnt = 0}),
              CountersMatch());
}

TEST_F(RawPtrTest, ToAddressGivesBackRawAddress) {
  int* raw = nullptr;
  raw_ptr<int> miracle = raw;
  EXPECT_EQ(base::to_address(raw), base::to_address(miracle));
}

void InOutParamFuncWithPointer(int* in, int** out) {
  *out = in;
}

TEST_F(RawPtrTest, EphemeralRawAddrPointerPointer) {
  int v1 = 123;
  int v2 = 456;
  raw_ptr<int> ptr = &v1;
  // Pointer pointer should point to a pointer other than one inside raw_ptr.
  EXPECT_NE(&ptr.AsEphemeralRawAddr(),
            reinterpret_cast<int**>(std::addressof(ptr)));
  // But inner pointer should point to the same address.
  EXPECT_EQ(*&ptr.AsEphemeralRawAddr(), &v1);

  // Inner pointer can be rewritten via the pointer pointer.
  *&ptr.AsEphemeralRawAddr() = &v2;
  EXPECT_EQ(ptr.get(), &v2);
  InOutParamFuncWithPointer(&v1, &ptr.AsEphemeralRawAddr());
  EXPECT_EQ(ptr.get(), &v1);
}

void InOutParamFuncWithReference(int* in, int*& out) {
  out = in;
}

TEST_F(RawPtrTest, EphemeralRawAddrPointerReference) {
  int v1 = 123;
  int v2 = 456;
  raw_ptr<int> ptr = &v1;
  // Pointer reference should refer to a pointer other than one inside raw_ptr.
  EXPECT_NE(&static_cast<int*&>(ptr.AsEphemeralRawAddr()),
            reinterpret_cast<int**>(std::addressof(ptr)));
  // But inner pointer should point to the same address.
  EXPECT_EQ(static_cast<int*&>(ptr.AsEphemeralRawAddr()), &v1);

  // Inner pointer can be rewritten via the pointer pointer.
  static_cast<int*&>(ptr.AsEphemeralRawAddr()) = &v2;
  EXPECT_EQ(ptr.get(), &v2);
  InOutParamFuncWithReference(&v1, ptr.AsEphemeralRawAddr());
  EXPECT_EQ(ptr.get(), &v1);
}

// InstanceTracer has additional fields, so just skip this test when instance
// tracing is enabled.
#if !PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_INSTANCE_TRACER)
#if PA_BUILDFLAG(PA_COMPILER_GCC) && !defined(__clang__)
// In GCC this test will optimize the return value of the constructor, so
// assert fails. Disable optimizations to verify uninitialized attribute works
// as expected.
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif
TEST_F(RawPtrTest, AllowUninitialized) {
  constexpr uintptr_t kPattern = 0x12345678;
  uintptr_t storage = kPattern;
  // Placement new over stored pattern must not change it.
  new (&storage) CountingRawPtrUninitialized<int>;
  EXPECT_EQ(storage, kPattern);
}
#if PA_BUILDFLAG(PA_COMPILER_GCC) && !defined(__clang__)
#pragma GCC pop_options
#endif
#endif  // !PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_INSTANCE_TRACER)

}  // namespace

namespace base::internal {

#if PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL) && \
    !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

void HandleOOM(size_t unused_size) {
  LOG(FATAL) << "Out of memory";
}

class BackupRefPtrTest : public testing::Test {
 protected:
  void SetUp() override {
    // TODO(bartekn): Avoid using PartitionAlloc API directly. Switch to
    // new/delete once PartitionAlloc Everywhere is fully enabled.
    partition_alloc::PartitionAllocGlobalInit(HandleOOM);
  }

  size_t GetRequestSizeThatFills512BSlot() {
    // This requires some internal PartitionAlloc knowledge, but for the test to
    // work well the allocation + extras have to fill out the entire slot.
    // That's because PartitionAlloc doesn't know exact allocation size and
    // bases the guards on the slot size.
    //
    // A power of two is a safe choice for a slot size, then adjust it for
    // extras.
    size_t slot_size = 512;
    size_t requested_size =
        allocator_.root()->AdjustSizeForExtrasSubtract(slot_size);
    // Verify that we're indeed filling up the slot.
    // (ASSERT_EQ is more appropriate here, because it verifies test setup, but
    // it doesn't compile.)
    EXPECT_EQ(
        requested_size,
        allocator_.root()->AllocationCapacityFromRequestedSize(requested_size));
    return requested_size;
  }

  partition_alloc::PartitionAllocator allocator_ =
      partition_alloc::PartitionAllocator([] {
        partition_alloc::PartitionOptions opts;
        opts.backup_ref_ptr = partition_alloc::PartitionOptions::kEnabled;
        opts.memory_tagging = {
            .enabled = base::CPU::GetInstanceNoAllocation().has_mte()
                           ? partition_alloc::PartitionOptions::kEnabled
                           : partition_alloc::PartitionOptions::kDisabled};
        return opts;
      }());
};

TEST_F(BackupRefPtrTest, Basic) {
  base::CPU cpu;

  int* raw_ptr1 =
      reinterpret_cast<int*>(allocator_.root()->Alloc(sizeof(int), ""));
  // Use the actual raw_ptr implementation, not a test substitute, to
  // exercise real PartitionAlloc paths.
  raw_ptr<int, DisableDanglingPtrDetection> wrapped_ptr1 = raw_ptr1;

  *raw_ptr1 = 42;
  EXPECT_EQ(*raw_ptr1, *wrapped_ptr1);

  allocator_.root()->Free(raw_ptr1);
#if DCHECK_IS_ON() || PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  // In debug builds, the use-after-free should be caught immediately.
  EXPECT_DEATH_IF_SUPPORTED(g_volatile_int_to_ignore = *wrapped_ptr1, "");
#else   // DCHECK_IS_ON() || PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  // The allocation should be poisoned since there's a raw_ptr alive.
  EXPECT_NE(*wrapped_ptr1, 42);

  // The allocator should not be able to reuse the slot at this point.
  void* raw_ptr2 = allocator_.root()->Alloc(sizeof(int), "");
  EXPECT_NE(partition_alloc::UntagPtr(raw_ptr1),
            partition_alloc::UntagPtr(raw_ptr2));
  allocator_.root()->Free(raw_ptr2);

  // When the last reference is released, the slot should become reusable.
  wrapped_ptr1 = nullptr;
  void* raw_ptr3 = allocator_.root()->Alloc(sizeof(int), "");
  EXPECT_EQ(partition_alloc::UntagPtr(raw_ptr1),
            partition_alloc::UntagPtr(raw_ptr3));
  allocator_.root()->Free(raw_ptr3);
#endif  // DCHECK_IS_ON() || PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
}

TEST_F(BackupRefPtrTest, ZeroSized) {
  std::vector<raw_ptr<void>> ptrs;
  // Use a reasonable number of elements to fill up the slot span.
  for (int i = 0; i < 128 * 1024; ++i) {
    // Constructing a raw_ptr instance from a zero-sized allocation should
    // not result in a crash.
    ptrs.emplace_back(allocator_.root()->Alloc(0));
  }
}

TEST_F(BackupRefPtrTest, EndPointer) {
  // This test requires a fresh partition with an empty free list.
  // Check multiple size buckets and levels of slot filling.
  for (int size = 0; size < 1024; size += sizeof(void*)) {
    // Creating a raw_ptr from an address right past the end of an allocation
    // should not result in a crash or corrupt the free list.
    char* raw_ptr1 = reinterpret_cast<char*>(allocator_.root()->Alloc(size));
    raw_ptr<char, AllowPtrArithmetic> wrapped_ptr = raw_ptr1 + size;
    wrapped_ptr = nullptr;
    // We need to make two more allocations to turn the possible free list
    // corruption into an observable crash.
    char* raw_ptr2 = reinterpret_cast<char*>(allocator_.root()->Alloc(size));
    char* raw_ptr3 = reinterpret_cast<char*>(allocator_.root()->Alloc(size));

    // Similarly for operator+=.
    char* raw_ptr4 = reinterpret_cast<char*>(allocator_.root()->Alloc(size));
    wrapped_ptr = raw_ptr4;
    wrapped_ptr += size;
    wrapped_ptr = nullptr;
    char* raw_ptr5 = reinterpret_cast<char*>(allocator_.root()->Alloc(size));
    char* raw_ptr6 = reinterpret_cast<char*>(allocator_.root()->Alloc(size));

    allocator_.root()->Free(raw_ptr1);
    allocator_.root()->Free(raw_ptr2);
    allocator_.root()->Free(raw_ptr3);
    allocator_.root()->Free(raw_ptr4);
    allocator_.root()->Free(raw_ptr5);
    allocator_.root()->Free(raw_ptr6);
  }
}

TEST_F(BackupRefPtrTest, QuarantinedBytes) {
  uint64_t* raw_ptr1 = reinterpret_cast<uint64_t*>(
      allocator_.root()->Alloc(sizeof(uint64_t), ""));
  raw_ptr<uint64_t, DisableDanglingPtrDetection> wrapped_ptr1 = raw_ptr1;
  EXPECT_EQ(allocator_.root()->total_size_of_brp_quarantined_bytes.load(
                std::memory_order_relaxed),
            0U);
  EXPECT_EQ(allocator_.root()->total_count_of_brp_quarantined_slots.load(
                std::memory_order_relaxed),
            0U);

  // Memory should get quarantined.
  allocator_.root()->Free(raw_ptr1);
  EXPECT_GT(allocator_.root()->total_size_of_brp_quarantined_bytes.load(
                std::memory_order_relaxed),
            0U);
  EXPECT_EQ(allocator_.root()->total_count_of_brp_quarantined_slots.load(
                std::memory_order_relaxed),
            1U);

  // Non quarantined free should not effect total_size_of_brp_quarantined_bytes
  void* raw_ptr2 = allocator_.root()->Alloc(sizeof(uint64_t), "");
  allocator_.root()->Free(raw_ptr2);

  // Freeing quarantined memory should bring the size back down to zero.
  wrapped_ptr1 = nullptr;
  EXPECT_EQ(allocator_.root()->total_size_of_brp_quarantined_bytes.load(
                std::memory_order_relaxed),
            0U);
  EXPECT_EQ(allocator_.root()->total_count_of_brp_quarantined_slots.load(
                std::memory_order_relaxed),
            0U);
}

TEST_F(BackupRefPtrTest, SameSlotAssignmentWhenDangling) {
  uint64_t* ptr = reinterpret_cast<uint64_t*>(
      allocator_.root()->Alloc(sizeof(uint64_t), ""));
  raw_ptr<uint64_t, DisableDanglingPtrDetection> wrapped_ptr = ptr;
  ASSERT_EQ(allocator_.root()->total_count_of_brp_quarantined_slots.load(
                std::memory_order_relaxed),
            0U);

  // Make the pointer dangle. Memory will get quarantined.
  allocator_.root()->Free(ptr);
  ASSERT_EQ(allocator_.root()->total_count_of_brp_quarantined_slots.load(
                std::memory_order_relaxed),
            1U);

  // Test for crbug.com/347461704 which caused the ref-count to be first
  // dropped, leading to dequarantining and releasing the memory, then
  // increasing ref-count on an already released memory.
  wrapped_ptr = ptr;

  // Many things may go wrong after the above instruction (particularly on
  // DCHECK builds), but just in case check that memory continues to be
  // quarantined.
  EXPECT_EQ(allocator_.root()->total_count_of_brp_quarantined_slots.load(
                std::memory_order_relaxed),
            1U);
}

void RunBackupRefPtrImplAdvanceTest(
    partition_alloc::PartitionAllocator& allocator,
    size_t requested_size) {
#if PA_BUILDFLAG(BACKUP_REF_PTR_EXTRA_OOB_CHECKS)
  char* ptr = static_cast<char*>(allocator.root()->Alloc(requested_size));
  raw_ptr<char, AllowPtrArithmetic> protected_ptr = ptr;
  protected_ptr += 123;
  protected_ptr -= 123;
  protected_ptr = protected_ptr + 123;
  protected_ptr = protected_ptr - 123;
  protected_ptr += requested_size / 2;
  // end-of-allocation address should not cause an error immediately, but it may
  // result in the pointer being poisoned.
  protected_ptr = protected_ptr + (requested_size + 1) / 2;
#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  EXPECT_DEATH_IF_SUPPORTED(*protected_ptr = ' ', "");
  protected_ptr -= 1;  // This brings the pointer back within
                       // bounds, which causes the poison to be removed.
  *protected_ptr = ' ';
  protected_ptr += 1;  // Reposition pointer back past end of allocation.
#endif
  EXPECT_CHECK_DEATH(protected_ptr = protected_ptr + 1);
  EXPECT_CHECK_DEATH(protected_ptr += 1);
  EXPECT_CHECK_DEATH(++protected_ptr);

  // Even though |protected_ptr| is already pointing to the end of the
  // allocation, assign it explicitly to make sure the underlying implementation
  // doesn't "switch" to the next slot.
  protected_ptr = ptr + requested_size;
  protected_ptr -= (requested_size + 1) / 2;
  protected_ptr = protected_ptr - requested_size / 2;
  EXPECT_CHECK_DEATH(protected_ptr = protected_ptr - 1);
  EXPECT_CHECK_DEATH(protected_ptr -= 1);
  EXPECT_CHECK_DEATH(--protected_ptr);

#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  // An array of a size that doesn't cleanly fit into the allocation. This is to
  // check that one can't access elements that don't fully fit in the
  // allocation.
  const size_t kArraySize = 199;
  ASSERT_LT(kArraySize, requested_size);
  ASSERT_NE(requested_size % kArraySize, 0U);
  typedef char FunkyArray[kArraySize];
  raw_ptr<FunkyArray, AllowPtrArithmetic> protected_arr_ptr =
      reinterpret_cast<FunkyArray*>(ptr);

  **protected_arr_ptr = 4;
  protected_arr_ptr += requested_size / kArraySize;
  EXPECT_CHECK_DEATH(** protected_arr_ptr = 4);
  protected_arr_ptr--;
  **protected_arr_ptr = 4;
  protected_arr_ptr++;
  EXPECT_CHECK_DEATH(** protected_arr_ptr = 4);
  protected_arr_ptr = nullptr;
#endif  // PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)

  protected_ptr = nullptr;
  allocator.root()->Free(ptr);
#endif  // PA_BUILDFLAG(BACKUP_REF_PTR_EXTRA_OOB_CHECKS)
}

TEST_F(BackupRefPtrTest, Advance) {
  size_t requested_size = GetRequestSizeThatFills512BSlot();
  RunBackupRefPtrImplAdvanceTest(allocator_, requested_size);

  // We don't have the same worry for single-slot spans, as PartitionAlloc knows
  // exactly where the allocation ends.
  size_t raw_size = 300003;
  ASSERT_GT(raw_size, partition_alloc::internal::MaxRegularSlotSpanSize());
  ASSERT_LE(raw_size, partition_alloc::internal::kMaxBucketed);
  requested_size = allocator_.root()->AdjustSizeForExtrasSubtract(raw_size);
  RunBackupRefPtrImplAdvanceTest(allocator_, requested_size);

  // Same for direct map.
  raw_size = 1001001;
  ASSERT_GT(raw_size, partition_alloc::internal::kMaxBucketed);
  requested_size = allocator_.root()->AdjustSizeForExtrasSubtract(raw_size);
  RunBackupRefPtrImplAdvanceTest(allocator_, requested_size);
}

TEST_F(BackupRefPtrTest, AdvanceAcrossPools) {
  char array1[1000];
  char array2[1000];

  char* in_pool_ptr = static_cast<char*>(allocator_.root()->Alloc(123));

  raw_ptr<char, AllowPtrArithmetic> protected_ptr = array1;
  // Nothing bad happens. Both pointers are outside of the BRP pool, so no
  // checks are triggered.
  protected_ptr += (array2 - array1);
  // A pointer is shifted from outside of the BRP pool into the BRP pool. This
  // should trigger death to avoid
  EXPECT_CHECK_DEATH(protected_ptr += (in_pool_ptr - array2));

  protected_ptr = in_pool_ptr;
  // Same when a pointer is shifted from inside the BRP pool out of it.
  EXPECT_CHECK_DEATH(protected_ptr += (array1 - in_pool_ptr));

  protected_ptr = nullptr;
  allocator_.root()->Free(in_pool_ptr);
}

TEST_F(BackupRefPtrTest, GetDeltaElems) {
  size_t requested_size = allocator_.root()->AdjustSizeForExtrasSubtract(512);
  char* ptr1 = static_cast<char*>(allocator_.root()->Alloc(requested_size));
  char* ptr2 = static_cast<char*>(allocator_.root()->Alloc(requested_size));
  ASSERT_LT(partition_alloc::UntagPtr(ptr1),
            partition_alloc::UntagPtr(
                ptr2));  // There should be a ref-count between slots.
  raw_ptr<char, AllowPtrArithmetic> protected_ptr1 = ptr1;
  raw_ptr<char, AllowPtrArithmetic> protected_ptr1_2 = ptr1 + 1;
  raw_ptr<char, AllowPtrArithmetic> protected_ptr1_3 =
      ptr1 + requested_size - 1;
  raw_ptr<char, AllowPtrArithmetic> protected_ptr1_4 = ptr1 + requested_size;
  raw_ptr<char, AllowPtrArithmetic> protected_ptr2 = ptr2;
  raw_ptr<char, AllowPtrArithmetic> protected_ptr2_2 = ptr2 + 1;

  EXPECT_EQ(protected_ptr1_2 - protected_ptr1, 1);
  EXPECT_EQ(protected_ptr1 - protected_ptr1_2, -1);
  EXPECT_EQ(protected_ptr1_3 - protected_ptr1,
            checked_cast<ptrdiff_t>(requested_size) - 1);
  EXPECT_EQ(protected_ptr1 - protected_ptr1_3,
            -checked_cast<ptrdiff_t>(requested_size) + 1);
  EXPECT_EQ(protected_ptr1_4 - protected_ptr1,
            checked_cast<ptrdiff_t>(requested_size));
  EXPECT_EQ(protected_ptr1 - protected_ptr1_4,
            -checked_cast<ptrdiff_t>(requested_size));
#if PA_BUILDFLAG(ENABLE_POINTER_SUBTRACTION_CHECK)
  EXPECT_CHECK_DEATH(protected_ptr2 - protected_ptr1);
  EXPECT_CHECK_DEATH(protected_ptr1 - protected_ptr2);
  EXPECT_CHECK_DEATH(protected_ptr2 - protected_ptr1_4);
  EXPECT_CHECK_DEATH(protected_ptr1_4 - protected_ptr2);
  EXPECT_CHECK_DEATH(protected_ptr2_2 - protected_ptr1);
  EXPECT_CHECK_DEATH(protected_ptr1 - protected_ptr2_2);
  EXPECT_CHECK_DEATH(protected_ptr2_2 - protected_ptr1_4);
  EXPECT_CHECK_DEATH(protected_ptr1_4 - protected_ptr2_2);
#endif  // PA_BUILDFLAG(ENABLE_POINTER_SUBTRACTION_CHECK)
  EXPECT_EQ(protected_ptr2_2 - protected_ptr2, 1);
  EXPECT_EQ(protected_ptr2 - protected_ptr2_2, -1);

  protected_ptr1 = nullptr;
  protected_ptr1_2 = nullptr;
  protected_ptr1_3 = nullptr;
  protected_ptr1_4 = nullptr;
  protected_ptr2 = nullptr;
  protected_ptr2_2 = nullptr;

  allocator_.root()->Free(ptr1);
  allocator_.root()->Free(ptr2);
}

volatile char g_volatile_char_to_ignore;

TEST_F(BackupRefPtrTest, IndexOperator) {
#if PA_BUILDFLAG(BACKUP_REF_PTR_EXTRA_OOB_CHECKS)
  size_t requested_size = GetRequestSizeThatFills512BSlot();
  char* ptr = static_cast<char*>(allocator_.root()->Alloc(requested_size));
  {
    raw_ptr<char, AllowPtrArithmetic> array = ptr;
    std::ignore = array[0];
    std::ignore = array[requested_size - 1];
    EXPECT_CHECK_DEATH(std::ignore = array[-1]);
    EXPECT_CHECK_DEATH(std::ignore = array[requested_size + 1]);
#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
    EXPECT_DEATH_IF_SUPPORTED(g_volatile_char_to_ignore = array[requested_size],
                              "");
#endif
  }
  allocator_.root()->Free(ptr);
#endif  // PA_BUILDFLAG(BACKUP_REF_PTR_EXTRA_OOB_CHECKS)
}

bool IsQuarantineEmpty(partition_alloc::PartitionAllocator& allocator) {
  return allocator.root()->total_size_of_brp_quarantined_bytes.load(
             std::memory_order_relaxed) == 0;
}

struct BoundRawPtrTestHelper {
  static BoundRawPtrTestHelper* Create(
      partition_alloc::PartitionAllocator& allocator) {
    return new (allocator.root()->Alloc(sizeof(BoundRawPtrTestHelper), ""))
        BoundRawPtrTestHelper(allocator);
  }

  explicit BoundRawPtrTestHelper(partition_alloc::PartitionAllocator& allocator)
      : owning_allocator(allocator),
        once_callback(
            BindOnce(&BoundRawPtrTestHelper::DeleteItselfAndCheckIfInQuarantine,
                     Unretained(this))),
        repeating_callback(BindRepeating(
            &BoundRawPtrTestHelper::DeleteItselfAndCheckIfInQuarantine,
            Unretained(this))) {}

  void DeleteItselfAndCheckIfInQuarantine() {
    auto& allocator = *owning_allocator;
    EXPECT_TRUE(IsQuarantineEmpty(allocator));

    // Since we use a non-default partition, `delete` has to be simulated.
    this->~BoundRawPtrTestHelper();
    allocator.root()->Free(this);

    EXPECT_FALSE(IsQuarantineEmpty(allocator));
  }

  const raw_ref<partition_alloc::PartitionAllocator> owning_allocator;
  OnceClosure once_callback;
  RepeatingClosure repeating_callback;
};

// Check that bound callback arguments remain protected by BRP for the
// entire duration of a callback invocation.
TEST_F(BackupRefPtrTest, Bind) {
  // This test requires a separate partition; otherwise, unrelated allocations
  // might interfere with `IsQuarantineEmpty`.
  auto* object_for_once_callback1 = BoundRawPtrTestHelper::Create(allocator_);
  std::move(object_for_once_callback1->once_callback).Run();
  EXPECT_TRUE(IsQuarantineEmpty(allocator_));

  auto* object_for_repeating_callback1 =
      BoundRawPtrTestHelper::Create(allocator_);
  std::move(object_for_repeating_callback1->repeating_callback).Run();
  EXPECT_TRUE(IsQuarantineEmpty(allocator_));

  // `RepeatingCallback` has both lvalue and rvalue versions of `Run`.
  auto* object_for_repeating_callback2 =
      BoundRawPtrTestHelper::Create(allocator_);
  object_for_repeating_callback2->repeating_callback.Run();
  EXPECT_TRUE(IsQuarantineEmpty(allocator_));
}

#if PA_CONFIG(IN_SLOT_METADATA_CHECK_COOKIE)
TEST_F(BackupRefPtrTest, ReinterpretCast) {
  void* ptr = allocator_.root()->Alloc(16);
  allocator_.root()->Free(ptr);

  raw_ptr<void>* wrapped_ptr = reinterpret_cast<raw_ptr<void>*>(&ptr);
  // The reference count cookie check should detect that the allocation has
  // been already freed.
  BASE_EXPECT_DEATH(*wrapped_ptr = nullptr, "");
}
#endif  // PA_CONFIG(IN_SLOT_METADATA_CHECK_COOKIE)

// Tests that ref-count management is correct, despite `std::optional` may be
// using `union` underneath.
TEST_F(BackupRefPtrTest, WorksWithOptional) {
  void* ptr = allocator_.root()->Alloc(16);
  auto* ref_count =
      allocator_.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  std::optional<raw_ptr<void>> opt = ptr;
  ASSERT_TRUE(opt.has_value());
  EXPECT_TRUE(ref_count->IsAlive() && !ref_count->IsAliveWithNoKnownRefs());

  opt.reset();
  ASSERT_TRUE(!opt.has_value());
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  opt = ptr;
  ASSERT_TRUE(opt.has_value());
  EXPECT_TRUE(ref_count->IsAlive() && !ref_count->IsAliveWithNoKnownRefs());

  opt = nullptr;
  ASSERT_TRUE(opt.has_value());
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  {
    std::optional<raw_ptr<void>> opt2 = ptr;
    ASSERT_TRUE(opt2.has_value());
    EXPECT_TRUE(ref_count->IsAlive() && !ref_count->IsAliveWithNoKnownRefs());
  }
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  allocator_.root()->Free(ptr);
}

// Tests that ref-count management is correct, despite `std::variant` may be
// using `union` underneath.
TEST_F(BackupRefPtrTest, WorksWithVariant) {
  void* ptr = allocator_.root()->Alloc(16);
  auto* ref_count =
      allocator_.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  std::variant<uintptr_t, raw_ptr<void>> vary = ptr;
  ASSERT_EQ(1u, vary.index());
  EXPECT_TRUE(ref_count->IsAlive() && !ref_count->IsAliveWithNoKnownRefs());

  vary = 42u;
  ASSERT_EQ(0u, vary.index());
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  vary = ptr;
  ASSERT_EQ(1u, vary.index());
  EXPECT_TRUE(ref_count->IsAlive() && !ref_count->IsAliveWithNoKnownRefs());

  vary = nullptr;
  ASSERT_EQ(1u, vary.index());
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  {
    std::variant<uintptr_t, raw_ptr<void>> vary2 = ptr;
    ASSERT_EQ(1u, vary2.index());
    EXPECT_TRUE(ref_count->IsAlive() && !ref_count->IsAliveWithNoKnownRefs());
  }
  EXPECT_TRUE(ref_count->IsAliveWithNoKnownRefs());

  allocator_.root()->Free(ptr);
}

namespace {

// Install dangling raw_ptr handlers and restore them when going out of scope.
class ScopedInstallDanglingRawPtrChecks {
 public:
  ScopedInstallDanglingRawPtrChecks() {
    enabled_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPartitionAllocDanglingPtr, {{"mode", "crash"}}}},
        {/* disabled_features */});
    old_detected_fn_ = partition_alloc::GetDanglingRawPtrDetectedFn();
    old_dereferenced_fn_ = partition_alloc::GetDanglingRawPtrReleasedFn();
    allocator::InstallDanglingRawPtrChecks();
  }
  ~ScopedInstallDanglingRawPtrChecks() {
    partition_alloc::SetDanglingRawPtrDetectedFn(old_detected_fn_);
    partition_alloc::SetDanglingRawPtrReleasedFn(old_dereferenced_fn_);
  }

 private:
  test::ScopedFeatureList enabled_feature_list_;
  partition_alloc::DanglingRawPtrDetectedFn* old_detected_fn_;
  partition_alloc::DanglingRawPtrReleasedFn* old_dereferenced_fn_;
};

}  // namespace

TEST_F(BackupRefPtrTest, RawPtrMayDangle) {
  ScopedInstallDanglingRawPtrChecks enable_dangling_raw_ptr_checks;

  void* ptr = allocator_.root()->Alloc(16);
  raw_ptr<void, DisableDanglingPtrDetection> dangling_ptr = ptr;
  allocator_.root()->Free(ptr);  // No dangling raw_ptr reported.
  dangling_ptr = nullptr;        // No dangling raw_ptr reported.
}

TEST_F(BackupRefPtrTest, RawPtrNotDangling) {
  ScopedInstallDanglingRawPtrChecks enable_dangling_raw_ptr_checks;

  void* ptr = allocator_.root()->Alloc(16);
  raw_ptr<void> dangling_ptr = ptr;
#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  BASE_EXPECT_DEATH(
      {
        allocator_.root()->Free(ptr);  // Dangling raw_ptr detected.
        dangling_ptr = nullptr;        // Dangling raw_ptr released.
      },
      AllOf(HasSubstr("Detected dangling raw_ptr"),
            HasSubstr("The memory was freed at:"),
            HasSubstr("The dangling raw_ptr was released at:")));
#else
  allocator_.root()->Free(ptr);
  dangling_ptr = nullptr;
#endif
}

// Check the comparator operators work, even across raw_ptr with different
// dangling policies.
TEST_F(BackupRefPtrTest, DanglingPtrComparison) {
  ScopedInstallDanglingRawPtrChecks enable_dangling_raw_ptr_checks;

  void* ptr_1 = allocator_.root()->Alloc(16);
  void* ptr_2 = allocator_.root()->Alloc(16);

  if (ptr_1 > ptr_2) {
    std::swap(ptr_1, ptr_2);
  }

  raw_ptr<void, DisableDanglingPtrDetection> dangling_ptr_1 = ptr_1;
  raw_ptr<void, DisableDanglingPtrDetection> dangling_ptr_2 = ptr_2;
  raw_ptr<void> not_dangling_ptr_1 = ptr_1;
  raw_ptr<void> not_dangling_ptr_2 = ptr_2;

  EXPECT_EQ(dangling_ptr_1, not_dangling_ptr_1);
  EXPECT_EQ(dangling_ptr_2, not_dangling_ptr_2);
  EXPECT_NE(dangling_ptr_1, not_dangling_ptr_2);
  EXPECT_NE(dangling_ptr_2, not_dangling_ptr_1);
  EXPECT_LT(dangling_ptr_1, not_dangling_ptr_2);
  EXPECT_GT(dangling_ptr_2, not_dangling_ptr_1);
  EXPECT_LT(not_dangling_ptr_1, dangling_ptr_2);
  EXPECT_GT(not_dangling_ptr_2, dangling_ptr_1);

  not_dangling_ptr_1 = nullptr;
  not_dangling_ptr_2 = nullptr;

  allocator_.root()->Free(ptr_1);
  allocator_.root()->Free(ptr_2);
}

// Check the assignment operator works, even across raw_ptr with different
// dangling policies (only `not dangling` -> `dangling` direction is supported).
TEST_F(BackupRefPtrTest, DanglingPtrAssignment) {
  ScopedInstallDanglingRawPtrChecks enable_dangling_raw_ptr_checks;

  void* ptr = allocator_.root()->Alloc(16);

  raw_ptr<void, DisableDanglingPtrDetection> dangling_ptr;
  raw_ptr<void> not_dangling_ptr;

  not_dangling_ptr = ptr;
  dangling_ptr = not_dangling_ptr;
  not_dangling_ptr = nullptr;

  allocator_.root()->Free(ptr);

  dangling_ptr = nullptr;
}

// Check the copy constructor works, even across raw_ptr with different dangling
// policies (only `not dangling` -> `dangling` direction is supported).
TEST_F(BackupRefPtrTest, DanglingPtrCopyContructor) {
  ScopedInstallDanglingRawPtrChecks enable_dangling_raw_ptr_checks;

  void* ptr = allocator_.root()->Alloc(16);

  raw_ptr<void> not_dangling_ptr(ptr);
  raw_ptr<void, DisableDanglingPtrDetection> dangling_ptr(not_dangling_ptr);

  not_dangling_ptr = nullptr;
  dangling_ptr = nullptr;

  allocator_.root()->Free(ptr);
}

TEST_F(BackupRefPtrTest, RawPtrExtractAsDangling) {
  ScopedInstallDanglingRawPtrChecks enable_dangling_raw_ptr_checks;

  raw_ptr<int> ptr =
      static_cast<int*>(allocator_.root()->Alloc(sizeof(int), ""));
  allocator_.root()->Free(
      ptr.ExtractAsDangling());  // No dangling raw_ptr reported.
  EXPECT_EQ(ptr, nullptr);
}

TEST_F(BackupRefPtrTest, RawPtrDeleteWithoutExtractAsDangling) {
  ScopedInstallDanglingRawPtrChecks enable_dangling_raw_ptr_checks;

  raw_ptr<int> ptr =
      static_cast<int*>(allocator_.root()->Alloc(sizeof(int), ""));
#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  BASE_EXPECT_DEATH(
      {
        allocator_.root()->Free(ptr.get());  // Dangling raw_ptr detected.
        ptr = nullptr;                       // Dangling raw_ptr released.
      },
      AllOf(HasSubstr("Detected dangling raw_ptr"),
            HasSubstr("The memory was freed at:"),
            HasSubstr("The dangling raw_ptr was released at:")));
#else
  allocator_.root()->Free(ptr.get());
  ptr = nullptr;
#endif  // PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
}

TEST_F(BackupRefPtrTest, SpatialAlgoCompat) {
  size_t requested_size = GetRequestSizeThatFills512BSlot();
  size_t requested_elements = requested_size / sizeof(uint32_t);

  uint32_t* ptr =
      reinterpret_cast<uint32_t*>(allocator_.root()->Alloc(requested_size));
  uint32_t* ptr_end = ptr + requested_elements;

  CountingRawPtr<uint32_t> counting_ptr = ptr;
  CountingRawPtr<uint32_t> counting_ptr_end = counting_ptr + requested_elements;

  RawPtrCountingImpl::ClearCounters();

  uint32_t gen_val = 1;
  std::generate(counting_ptr, counting_ptr_end, [&gen_val] {
    gen_val ^= gen_val + 1;
    return gen_val;
  });

  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = requested_elements,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = (requested_elements + 1) * 2,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();

  for (CountingRawPtr<uint32_t> counting_ptr_i = counting_ptr;
       counting_ptr_i < counting_ptr_end; counting_ptr_i++) {
    *counting_ptr_i ^= *counting_ptr_i + 1;
  }

  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = requested_elements * 2,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = (requested_elements + 1) * 2,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();

  for (CountingRawPtr<uint32_t> counting_ptr_i = counting_ptr;
       counting_ptr_i < ptr_end; counting_ptr_i++) {
    *counting_ptr_i ^= *counting_ptr_i + 1;
  }

  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = requested_elements * 2,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = requested_elements + 1,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();

  for (uint32_t* ptr_i = ptr; ptr_i < counting_ptr_end; ptr_i++) {
    *ptr_i ^= *ptr_i + 1;
  }

  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 0,
                  .get_for_comparison_cnt = requested_elements + 1,
              }),
              CountersMatch());

  RawPtrCountingImpl::ClearCounters();

  size_t iter_cnt = 0;
  for (uint32_t *ptr_i = counting_ptr, *ptr_i_end = counting_ptr_end;
       ptr_i < ptr_i_end; ptr_i++) {
    *ptr_i ^= *ptr_i + 1;
    iter_cnt++;
  }
  EXPECT_EQ(iter_cnt, requested_elements);

  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_dereference_cnt = 0,
                  .get_for_extraction_cnt = 2,
                  .get_for_comparison_cnt = 0,
              }),
              CountersMatch());

  counting_ptr = nullptr;
  counting_ptr_end = nullptr;
  allocator_.root()->Free(ptr);
}

#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
TEST_F(BackupRefPtrTest, Duplicate) {
  size_t requested_size = allocator_.root()->AdjustSizeForExtrasSubtract(512);
  char* ptr = static_cast<char*>(allocator_.root()->Alloc(requested_size));
  raw_ptr<char, AllowPtrArithmetic> protected_ptr1 = ptr;
  protected_ptr1 += requested_size;  // Pointer should now be poisoned.

  // Duplicating a poisoned pointer should be allowed.
  raw_ptr<char, AllowPtrArithmetic> protected_ptr2 = protected_ptr1;

  // The poison bit should be propagated to the duplicate such that the OOB
  // access is disallowed:
  EXPECT_DEATH_IF_SUPPORTED(*protected_ptr2 = ' ', "");

  // Assignment from a poisoned pointer should be allowed.
  raw_ptr<char, AllowPtrArithmetic> protected_ptr3;
  protected_ptr3 = protected_ptr1;

  // The poison bit should be propagated via the assignment such that the OOB
  // access is disallowed:
  EXPECT_DEATH_IF_SUPPORTED(*protected_ptr3 = ' ', "");

  protected_ptr1 = nullptr;
  protected_ptr2 = nullptr;
  protected_ptr3 = nullptr;
  allocator_.root()->Free(ptr);
}
#endif  // PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)

#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
TEST_F(BackupRefPtrTest, WriteAfterFree) {
  constexpr uint64_t kPayload = 0x1234567890ABCDEF;

  raw_ptr<uint64_t, DisableDanglingPtrDetection> ptr =
      static_cast<uint64_t*>(allocator_.root()->Alloc(sizeof(uint64_t), ""));

  // Now |ptr| should be quarantined.
  allocator_.root()->Free(ptr);

  EXPECT_DEATH_IF_SUPPORTED(
      {
        // Write something different from |kQuarantinedByte|.
        *ptr = kPayload;
        // Write-after-Free should lead to crash
        // on |PartitionAllocFreeForRefCounting|.
        ptr = nullptr;
      },
      "");
}
#endif  // PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)

namespace {
constexpr uint8_t kCustomQuarantineByte = 0xff;
static_assert(kCustomQuarantineByte !=
              partition_alloc::internal::kQuarantinedByte);

void CustomQuarantineHook(void* address, size_t size) {
  partition_alloc::internal::SecureMemset(address, kCustomQuarantineByte, size);
}
}  // namespace

TEST_F(BackupRefPtrTest, QuarantineHook) {
  partition_alloc::PartitionAllocHooks::SetQuarantineOverrideHook(
      CustomQuarantineHook);
  uint8_t* native_ptr =
      static_cast<uint8_t*>(allocator_.root()->Alloc(sizeof(uint8_t), ""));
  *native_ptr = 0;
  {
    raw_ptr<uint8_t, DisableDanglingPtrDetection> smart_ptr = native_ptr;

    allocator_.root()->Free(smart_ptr);
    // Access the allocation through the native pointer to avoid triggering
    // dereference checks in debug builds.
    EXPECT_EQ(*partition_alloc::internal::TagPtr(native_ptr),
              kCustomQuarantineByte);

    // Leaving |smart_ptr| filled with |kCustomQuarantineByte| can
    // cause a crash because we have a DCHECK that expects it to be filled with
    // |kQuarantineByte|. We need to ensure it is unquarantined before
    // unregistering the hook.
  }  // <- unquarantined here

  partition_alloc::PartitionAllocHooks::SetQuarantineOverrideHook(nullptr);
}

TEST_F(BackupRefPtrTest, RawPtrTraits_DisableBRP) {
  // Allocate a slot so that a slot span doesn't get decommitted from memory,
  // while we allocate/deallocate/access the tested slot below.
  void* sentinel = allocator_.root()->Alloc(sizeof(unsigned int), "");
  constexpr uint32_t kQuarantined2Bytes =
      partition_alloc::internal::kQuarantinedByte |
      (partition_alloc::internal::kQuarantinedByte << 8);
  constexpr uint32_t kQuarantined4Bytes =
      kQuarantined2Bytes | (kQuarantined2Bytes << 16);

  {
    raw_ptr<unsigned int, DanglingUntriaged> ptr = static_cast<unsigned int*>(
        allocator_.root()->Alloc(sizeof(unsigned int), ""));
    *ptr = 0;
    // Freeing would  update the MTE tag so use |TagPtr()| to dereference it
    // below.
    allocator_.root()->Free(ptr);
#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    // Recreate the raw_ptr so we can use a pointer with the updated MTE tag.
    // Reassigning to |ptr| would hit the PartitionRefCount cookie check rather
    // than the |IsPointeeAlive()| check.
    raw_ptr<unsigned int, DanglingUntriaged> dangling_ptr =
        partition_alloc::internal::TagPtr(ptr.get());
    EXPECT_DEATH_IF_SUPPORTED(*dangling_ptr = 0, "");
#else
    EXPECT_EQ(kQuarantined4Bytes,
              *partition_alloc::internal::TagPtr(ptr.get()));
#endif
  }
  // raw_ptr with DisableBRP, BRP is expected to be off.
  {
    raw_ptr<unsigned int, DanglingUntriaged | RawPtrTraits::kDisableBRP> ptr =
        static_cast<unsigned int*>(
            allocator_.root()->Alloc(sizeof(unsigned int), ""));
    *ptr = 0;
    allocator_.root()->Free(ptr);
    // A tad fragile as a new allocation or free-list pointer may be there, but
    // highly unlikely it'll match 4 quarantine bytes in a row.
    // Use |TagPtr()| for this dereference because freeing would have updated
    // the MTE tag.
    EXPECT_NE(kQuarantined4Bytes,
              *partition_alloc::internal::TagPtr(ptr.get()));
  }

  allocator_.root()->Free(sentinel);
}

#endif  // PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL) &&
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

#if PA_BUILDFLAG(USE_RAW_PTR_HOOKABLE_IMPL)

namespace {
#define FOR_EACH_RAW_PTR_OPERATION(F) \
  F(wrap_ptr)                         \
  F(release_wrapped_ptr)              \
  F(safely_unwrap_for_dereference)    \
  F(safely_unwrap_for_extraction)     \
  F(unsafely_unwrap_for_comparison)   \
  F(advance)                          \
  F(duplicate)                        \
  F(wrap_ptr_for_duplication)         \
  F(unsafely_unwrap_for_duplication)

// Can't use gMock to count the number of invocations because
// gMock itself triggers raw_ptr<T> operations.
struct CountingHooks {
  void ResetCounts() {
#define F(name) name##_count = 0;
    FOR_EACH_RAW_PTR_OPERATION(F)
#undef F
  }

  static CountingHooks* Get() {
    static thread_local CountingHooks instance;
    return &instance;
  }

// The adapter method is templated to accept any number of arguments.
#define F(name)                      \
  template <typename... T>           \
  static void name##_adapter(T...) { \
    Get()->name##_count++;           \
  }                                  \
  size_t name##_count = 0;
  FOR_EACH_RAW_PTR_OPERATION(F)
#undef F
};

constexpr RawPtrHooks raw_ptr_hooks{
#define F(name) .name = CountingHooks::name##_adapter,
    FOR_EACH_RAW_PTR_OPERATION(F)
#undef F
};
}  // namespace

class HookableRawPtrImplTest : public testing::Test {
 protected:
  void SetUp() override { InstallRawPtrHooks(&raw_ptr_hooks); }
  void TearDown() override { ResetRawPtrHooks(); }
};

TEST_F(HookableRawPtrImplTest, WrapPtr) {
  // Can't call `ResetCounts` in `SetUp` because gTest triggers
  // raw_ptr<T> operations between `SetUp` and the test body.
  CountingHooks::Get()->ResetCounts();
  {
    int* ptr = new int;
    [[maybe_unused]] raw_ptr<int> interesting_ptr = ptr;
    delete ptr;
  }
  EXPECT_EQ(CountingHooks::Get()->wrap_ptr_count, 1u);
}

TEST_F(HookableRawPtrImplTest, ReleaseWrappedPtr) {
  CountingHooks::Get()->ResetCounts();
  {
    int* ptr = new int;
    [[maybe_unused]] raw_ptr<int> interesting_ptr = ptr;
    delete ptr;
  }
  EXPECT_EQ(CountingHooks::Get()->release_wrapped_ptr_count, 1u);
}

TEST_F(HookableRawPtrImplTest, SafelyUnwrapForDereference) {
  CountingHooks::Get()->ResetCounts();
  {
    int* ptr = new int;
    raw_ptr<int> interesting_ptr = ptr;
    *interesting_ptr = 1;
    delete ptr;
  }
  EXPECT_EQ(CountingHooks::Get()->safely_unwrap_for_dereference_count, 1u);
}

TEST_F(HookableRawPtrImplTest, SafelyUnwrapForExtraction) {
  CountingHooks::Get()->ResetCounts();
  {
    int* ptr = new int;
    raw_ptr<int> interesting_ptr = ptr;
    ptr = interesting_ptr;
    delete ptr;
  }
  EXPECT_EQ(CountingHooks::Get()->safely_unwrap_for_extraction_count, 1u);
}

TEST_F(HookableRawPtrImplTest, UnsafelyUnwrapForComparison) {
  CountingHooks::Get()->ResetCounts();
  {
    int* ptr = new int;
    raw_ptr<int> interesting_ptr = ptr;
    EXPECT_EQ(interesting_ptr, ptr);
    delete ptr;
  }
  EXPECT_EQ(CountingHooks::Get()->unsafely_unwrap_for_comparison_count, 1u);
}

TEST_F(HookableRawPtrImplTest, Advance) {
  CountingHooks::Get()->ResetCounts();
  {
    int* ptr = new int[10];
    raw_ptr<int, AllowPtrArithmetic> interesting_ptr = ptr;
    interesting_ptr += 1;
    delete[] ptr;
  }
  EXPECT_EQ(CountingHooks::Get()->advance_count, 1u);
}

TEST_F(HookableRawPtrImplTest, Duplicate) {
  CountingHooks::Get()->ResetCounts();
  {
    int* ptr = new int;
    raw_ptr<int> interesting_ptr = ptr;
    raw_ptr<int> interesting_ptr2 = interesting_ptr;
    delete ptr;
  }
  EXPECT_EQ(CountingHooks::Get()->duplicate_count, 1u);
}

TEST_F(HookableRawPtrImplTest, CrossKindCopyConstruction) {
  CountingHooks::Get()->ResetCounts();
  {
    int* ptr = new int;
    raw_ptr<int> non_dangling_ptr = ptr;
    raw_ptr<int, RawPtrTraits::kMayDangle> dangling_ptr(non_dangling_ptr);
    delete ptr;
  }
  EXPECT_EQ(CountingHooks::Get()->duplicate_count, 0u);
  EXPECT_EQ(CountingHooks::Get()->wrap_ptr_for_duplication_count, 1u);
  EXPECT_EQ(CountingHooks::Get()->unsafely_unwrap_for_duplication_count, 1u);
}

#endif  // PA_BUILDFLAG(USE_RAW_PTR_HOOKABLE_IMPL)

TEST(DanglingPtrTest, DetectAndReset) {
  auto instrumentation = test::DanglingPtrInstrumentation::Create();
  if (!instrumentation.has_value()) {
    GTEST_SKIP() << instrumentation.error();
  }

  auto owned_ptr = std::make_unique<int>(42);
  raw_ptr<int> dangling_ptr = owned_ptr.get();
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
  owned_ptr.reset();
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
  dangling_ptr = nullptr;
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 1u);
}

TEST(DanglingPtrTest, DetectAndDestructor) {
  auto instrumentation = test::DanglingPtrInstrumentation::Create();
  if (!instrumentation.has_value()) {
    GTEST_SKIP() << instrumentation.error();
  }

  auto owned_ptr = std::make_unique<int>(42);
  {
    [[maybe_unused]] raw_ptr<int> dangling_ptr = owned_ptr.get();
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
    owned_ptr.reset();
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
  }
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 1u);
}

TEST(DanglingPtrTest, DetectResetAndDestructor) {
  auto instrumentation = test::DanglingPtrInstrumentation::Create();
  if (!instrumentation.has_value()) {
    GTEST_SKIP() << instrumentation.error();
  }

  auto owned_ptr = std::make_unique<int>(42);
  {
    raw_ptr<int> dangling_ptr = owned_ptr.get();
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
    owned_ptr.reset();
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
    dangling_ptr = nullptr;
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 1u);
  }
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 1u);
}

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_INSTANCE_TRACER) && \
    PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL)
TEST(RawPtrInstanceTracerTest, CreateAndDestroy) {
  auto owned = std::make_unique<int>(8);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());

  {
    raw_ptr<int> ptr1 = owned.get();
    const auto stacks =
        InstanceTracer::GetStackTracesForAddressForTest(owned.get());
    EXPECT_THAT(stacks, SizeIs(1));
    {
      // A second raw_ptr to the same object should result in an additional
      // stack trace.
      raw_ptr<int> ptr2 = owned.get();
      EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                  SizeIs(2));
    }
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                Eq(stacks));
  }
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, CopyConstruction) {
  auto owned = std::make_unique<int>(8);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
  {
    raw_ptr<int> ptr1 = owned.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));
    {
      // Copying `ptr1` to `ptr2` should result in an additional stack trace.
      raw_ptr<int> ptr2 = ptr1;
      EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                  SizeIs(2));
    }
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));
  }

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, CopyAssignment) {
  auto owned1 = std::make_unique<int>(8);
  auto owned2 = std::make_unique<int>(9);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
              IsEmpty());
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
              IsEmpty());

  {
    raw_ptr<int> ptr1 = owned1.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                IsEmpty());

    raw_ptr<int> ptr2 = owned2.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                SizeIs(1));

    ptr2 = ptr1;
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(2));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                IsEmpty());
  }

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
              IsEmpty());
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, MoveConstruction) {
  auto owned = std::make_unique<int>(8);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
  {
    raw_ptr<int> ptr1 = owned.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));
    {
      // Moving `ptr1` to `ptr2` should not result in an additional stack trace.
      raw_ptr<int> ptr2 = std::move(ptr1);
      EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                  SizeIs(1));
    }
    // Once `ptr2` goes out of scope, there should be no more traces.
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                IsEmpty());
  }
}

TEST(RawPtrInstanceTracerTest, MoveAssignment) {
  auto owned1 = std::make_unique<int>(8);
  auto owned2 = std::make_unique<int>(9);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
              IsEmpty());
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
              IsEmpty());

  {
    raw_ptr<int> ptr1 = owned1.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                IsEmpty());

    raw_ptr<int> ptr2 = owned2.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                SizeIs(1));

    ptr2 = std::move(ptr1);
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                IsEmpty());
  }

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
              IsEmpty());
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, SelfCopy) {
  auto owned = std::make_unique<int>(8);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());

  {
    raw_ptr<int> ptr = owned.get();
    auto& ptr2 = ptr;  // To get around compiler self-assignment warning :)
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));

    ptr2 = ptr;
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));
  }

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, SelfMove) {
  auto owned = std::make_unique<int>(8);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());

  {
    raw_ptr<int> ptr = owned.get();
    auto& ptr2 = ptr;  // To get around compiler self-assignment warning :)
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));

    ptr2 = std::move(ptr);
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));
  }

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, ConversionCreateAndDestroy) {
  auto owned = std::make_unique<Derived>(1, 2, 3);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());

  {
    raw_ptr<Base1> ptr1 = owned.get();
    const auto stacks =
        InstanceTracer::GetStackTracesForAddressForTest(owned.get());
    EXPECT_THAT(stacks, SizeIs(1));
    {
      // A second raw_ptr to the same object should result in an additional
      // stack trace.
      raw_ptr<Base2> ptr2 = owned.get();
      EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                  SizeIs(2));
    }
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                Eq(stacks));
  }
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, CopyConversionConstruction) {
  auto owned = std::make_unique<Derived>(1, 2, 3);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
  {
    raw_ptr<Derived> ptr1 = owned.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));
    {
      // Copying `ptr1` to `ptr2` should result in an additional stack trace.
      raw_ptr<Base1> ptr2 = ptr1;
      EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                  SizeIs(2));
    }
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));
  }

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, CopyConversionAssignment) {
  auto owned1 = std::make_unique<Derived>(1, 2, 3);
  auto owned2 = std::make_unique<Derived>(4, 5, 6);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
              IsEmpty());
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
              IsEmpty());

  {
    raw_ptr<Derived> ptr1 = owned1.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                IsEmpty());

    raw_ptr<Base1> ptr2 = owned2.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                SizeIs(1));

    ptr2 = ptr1;
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(2));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                IsEmpty());
  }

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
              IsEmpty());
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
              IsEmpty());
}

TEST(RawPtrInstanceTracerTest, MoveConversionConstruction) {
  auto owned = std::make_unique<Derived>(1, 2, 3);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
              IsEmpty());
  {
    raw_ptr<Derived> ptr1 = owned.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                SizeIs(1));
    {
      // Moving `ptr1` to `ptr2` should not result in an additional stack trace.
      raw_ptr<Base1> ptr2 = std::move(ptr1);
      EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                  SizeIs(1));
    }
    // Once `ptr2` goes out of scope, there should be no more traces.
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned.get()),
                IsEmpty());
  }
}

TEST(RawPtrInstanceTracerTest, MoveConversionAssignment) {
  auto owned1 = std::make_unique<Derived>(1, 2, 3);
  auto owned2 = std::make_unique<Derived>(4, 5, 6);

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
              IsEmpty());
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
              IsEmpty());

  {
    raw_ptr<Derived> ptr1 = owned1.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                IsEmpty());

    raw_ptr<Base1> ptr2 = owned2.get();
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                SizeIs(1));

    ptr2 = std::move(ptr1);
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
                SizeIs(1));
    EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
                IsEmpty());
  }

  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned1.get()),
              IsEmpty());
  EXPECT_THAT(InstanceTracer::GetStackTracesForAddressForTest(owned2.get()),
              IsEmpty());
}
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_INSTANCE_TRACER) &&
        // PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL)

}  // namespace base::internal
