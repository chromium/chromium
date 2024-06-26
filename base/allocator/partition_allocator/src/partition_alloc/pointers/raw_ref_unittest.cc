// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/pointers/raw_ref.h"

#include <functional>
#include <type_traits>

#include "base/test/gtest_util.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/pointers/raw_ptr.h"
#include "partition_alloc/pointers/raw_ptr_counting_impl_for_test.h"
#include "partition_alloc/pointers/raw_ptr_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include "base/debug/asan_service.h"
#include "base/memory/raw_ptr_asan_service.h"
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

namespace {

class BaseClass {};
class SubClass : public BaseClass {};

// raw_ref just defers to the superclass for implementations, so it
// can't add more data types.
static_assert(sizeof(raw_ref<int>) == sizeof(raw_ptr<int>));

// Since it can't hold null, raw_ref is not default-constructible.
static_assert(!std::is_default_constructible_v<raw_ref<int>>);
static_assert(!std::is_default_constructible_v<raw_ref<const int>>);

// A mutable reference can only be constructed from a mutable lvalue reference.
static_assert(!std::is_constructible_v<raw_ref<int>, const int>);
static_assert(!std::is_constructible_v<raw_ref<int>, int>);
static_assert(!std::is_constructible_v<raw_ref<int>, const int&>);
static_assert(std::is_constructible_v<raw_ref<int>, int&>);
static_assert(!std::is_constructible_v<raw_ref<int>, const int*>);
static_assert(!std::is_constructible_v<raw_ref<int>, int*>);
static_assert(!std::is_constructible_v<raw_ref<int>, const int&&>);
static_assert(!std::is_constructible_v<raw_ref<int>, int&&>);
// Same for assignment.
static_assert(!std::is_assignable_v<raw_ref<int>, const int>);
static_assert(!std::is_assignable_v<raw_ref<int>, int>);
static_assert(!std::is_assignable_v<raw_ref<int>, const int&>);
static_assert(std::is_assignable_v<raw_ref<int>, int&>);
static_assert(!std::is_assignable_v<raw_ref<int>, const int*>);
static_assert(!std::is_assignable_v<raw_ref<int>, int*>);
static_assert(!std::is_assignable_v<raw_ref<int>, const int&&>);
static_assert(!std::is_assignable_v<raw_ref<int>, int&&>);

// A const reference can be constructed from a const or mutable lvalue
// reference.
static_assert(!std::is_constructible_v<raw_ref<const int>, const int>);
static_assert(!std::is_constructible_v<raw_ref<const int>, int>);
static_assert(std::is_constructible_v<raw_ref<const int>, const int&>);
static_assert(std::is_constructible_v<raw_ref<const int>, int&>);
static_assert(!std::is_constructible_v<raw_ref<const int>, const int*>);
static_assert(!std::is_constructible_v<raw_ref<const int>, int*>);
static_assert(!std::is_constructible_v<raw_ref<const int>, const int&&>);
static_assert(!std::is_constructible_v<raw_ref<const int>, int&&>);
// Same for assignment.
static_assert(!std::is_assignable_v<raw_ref<const int>, const int>);
static_assert(!std::is_assignable_v<raw_ref<const int>, int>);
static_assert(std::is_assignable_v<raw_ref<const int>, const int&>);
static_assert(std::is_assignable_v<raw_ref<const int>, int&>);
static_assert(!std::is_assignable_v<raw_ref<const int>, const int*>);
static_assert(!std::is_assignable_v<raw_ref<const int>, int*>);
static_assert(!std::is_assignable_v<raw_ref<const int>, const int&&>);
static_assert(!std::is_assignable_v<raw_ref<const int>, int&&>);

// Same trivial operations (or not) as raw_ptr<T>.
static_assert(std::is_trivially_constructible_v<raw_ref<int>, const int&> ==
              std::is_trivially_constructible_v<raw_ptr<int>, const int&>);
static_assert(std::is_trivially_destructible_v<raw_ref<int>> ==
              std::is_trivially_destructible_v<raw_ptr<int>>);
// But constructing from another raw_ref must check if it's internally null
// (which indicates use-after-move).
static_assert(!std::is_trivially_move_constructible_v<raw_ref<int>>);
static_assert(!std::is_trivially_move_assignable_v<raw_ref<int>>);
static_assert(!std::is_trivially_copy_constructible_v<raw_ref<int>>);
static_assert(!std::is_trivially_copy_assignable_v<raw_ref<int>>);

// A raw_ref can be copied or moved.
static_assert(std::is_move_constructible_v<raw_ref<int>>);
static_assert(std::is_copy_constructible_v<raw_ref<int>>);
static_assert(std::is_move_assignable_v<raw_ref<int>>);
static_assert(std::is_copy_assignable_v<raw_ref<int>>);

// A SubClass can be converted to a BaseClass.
static_assert(std::is_constructible_v<raw_ref<BaseClass>, raw_ref<SubClass>>);
static_assert(
    std::is_constructible_v<raw_ref<BaseClass>, const raw_ref<SubClass>&>);
static_assert(std::is_constructible_v<raw_ref<BaseClass>, raw_ref<SubClass>&&>);
static_assert(std::is_assignable_v<raw_ref<BaseClass>, raw_ref<SubClass>>);
static_assert(
    std::is_assignable_v<raw_ref<BaseClass>, const raw_ref<SubClass>&>);
static_assert(std::is_assignable_v<raw_ref<BaseClass>, raw_ref<SubClass>&&>);
// A BaseClass can't be implicitly downcasted.
static_assert(!std::is_constructible_v<raw_ref<SubClass>, raw_ref<BaseClass>>);
static_assert(
    !std::is_constructible_v<raw_ref<SubClass>, const raw_ref<BaseClass>&>);
static_assert(
    !std::is_constructible_v<raw_ref<SubClass>, raw_ref<BaseClass>&&>);
static_assert(!std::is_assignable_v<raw_ref<SubClass>, raw_ref<BaseClass>>);
static_assert(
    !std::is_assignable_v<raw_ref<SubClass>, const raw_ref<BaseClass>&>);
static_assert(!std::is_assignable_v<raw_ref<SubClass>, raw_ref<BaseClass>&&>);

// A raw_ref<BaseClass> can be constructed directly from a SubClass.
static_assert(std::is_constructible_v<raw_ref<BaseClass>, SubClass&>);
static_assert(std::is_assignable_v<raw_ref<BaseClass>, SubClass&>);
static_assert(std::is_constructible_v<raw_ref<const BaseClass>, SubClass&>);
static_assert(std::is_assignable_v<raw_ref<const BaseClass>, SubClass&>);
static_assert(
    std::is_constructible_v<raw_ref<const BaseClass>, const SubClass&>);
static_assert(std::is_assignable_v<raw_ref<const BaseClass>, const SubClass&>);
// But a raw_ref<SubClass> can't be constructed from an implicit downcast from a
// BaseClass.
static_assert(!std::is_constructible_v<raw_ref<SubClass>, BaseClass&>);
static_assert(!std::is_assignable_v<raw_ref<SubClass>, BaseClass&>);
static_assert(!std::is_constructible_v<raw_ref<const SubClass>, BaseClass&>);
static_assert(!std::is_assignable_v<raw_ref<const SubClass>, BaseClass&>);
static_assert(
    !std::is_constructible_v<raw_ref<const SubClass>, const BaseClass&>);
static_assert(!std::is_assignable_v<raw_ref<const SubClass>, const BaseClass&>);

// A mutable reference can be converted to const reference.
static_assert(std::is_constructible_v<raw_ref<const int>, raw_ref<int>>);
static_assert(std::is_assignable_v<raw_ref<const int>, raw_ref<int>>);
// A const reference can't be converted to mutable.
static_assert(!std::is_constructible_v<raw_ref<int>, raw_ref<const int>>);
static_assert(!std::is_assignable_v<raw_ref<int>, raw_ref<const int>>);

// The deref operator gives the internal reference.
static_assert(std::is_same_v<int&, decltype(*std::declval<raw_ref<int>>())>);
static_assert(
    std::is_same_v<int&, decltype(*std::declval<const raw_ref<int>>())>);
static_assert(std::is_same_v<int&, decltype(*std::declval<raw_ref<int>&>())>);
static_assert(
    std::is_same_v<int&, decltype(*std::declval<const raw_ref<int>&>())>);
static_assert(std::is_same_v<int&, decltype(*std::declval<raw_ref<int>&&>())>);
static_assert(
    std::is_same_v<int&, decltype(*std::declval<const raw_ref<int>&&>())>);
// A const T is always returned as const.
static_assert(
    std::is_same_v<const int&, decltype(*std::declval<raw_ref<const int>>())>);

// The arrow operator gives a (non-null) pointer to the internal reference.
static_assert(
    std::is_same_v<int*, decltype(std::declval<raw_ref<int>>().operator->())>);
static_assert(
    std::is_same_v<const int*,
                   decltype(std::declval<raw_ref<const int>>().operator->())>);

// Verify that raw_ref is a literal type, and its entire interface is constexpr.
//
// Constexpr destructors were introduced in C++20. PartitionAlloc's minimum
// supported C++ version is C++17, so raw_ref is not a literal type in C++17.
// Thus we only test for constexpr in C++20.
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
static_assert([]() constexpr {
  struct IntBase {};
  struct Int : public IntBase {
    int i = 0;
  };

  Int* i = new Int();
  {
    raw_ref<Int> r(*i);              // raw_ref(T&)
    r = *i;                          // operator=(T&)
    raw_ref<Int> r2(r);              // raw_ref(const raw_ref&)
    raw_ref<Int> r3(std::move(r2));  // raw_ref(raw_ref&&)
    r2 = r;                          // operator=(const raw_ref&)
    r3 = std::move(r2);              // operator=(raw_ref&&)
    r2 = r;                          // Reset after move.
    [[maybe_unused]] raw_ref<IntBase> r5(
        r2);  // raw_ref(const raw_ref<Convertible>&)
    [[maybe_unused]] raw_ref<IntBase> r6(
        std::move(r2));         // raw_ref(raw_ref<Convertible>&&)
    r2 = r;                     // Reset after move.
    r5 = r2;                    // operator=(const raw_ref<Convertible>&)
    r6 = std::move(r2);         // operator=(raw_ref<Convertible>&&)
    raw_ref<Int>::from_ptr(i);  // from_ptr(T*)
    (*r).i += 1;                // operator*()
    r.get().i += 1;             // get()
    r->i += 1;                  // operator->()
    r2 = r;                     // Reset after move.
    swap(r, r2);                // swap()
  }
  delete i;
  return true;
}());
#endif

struct StructWithoutTypeBasedTraits {};
struct BaseWithTypeBasedTraits {};
struct DerivedWithTypeBasedTraits : BaseWithTypeBasedTraits {};

}  // namespace

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
static_assert(!ContainsFlags(raw_ref<StructWithoutTypeBasedTraits>::Traits,
                             base::RawPtrTraits::kDummyForTest));
static_assert(ContainsFlags(raw_ref<BaseWithTypeBasedTraits>::Traits,
                            base::RawPtrTraits::kDummyForTest));
static_assert(ContainsFlags(raw_ref<DerivedWithTypeBasedTraits>::Traits,
                            base::RawPtrTraits::kDummyForTest));

namespace {

TEST(RawRef, Construct) {
  int i = 1;
  auto r = raw_ref<int>(i);
  EXPECT_EQ(&*r, &i);
  auto cr = raw_ref<const int>(i);
  EXPECT_EQ(&*cr, &i);
  const int ci = 1;
  auto cci = raw_ref<const int>(ci);
  EXPECT_EQ(&*cci, &ci);
}

TEST(RawRef, CopyConstruct) {
  {
    int i = 1;
    auto r = raw_ref<int>(i);
    EXPECT_EQ(&*r, &i);
    auto r2 = raw_ref<int>(r);
    EXPECT_EQ(&*r2, &i);
  }
  {
    int i = 1;
    auto r = raw_ref<const int>(i);
    EXPECT_EQ(&*r, &i);
    auto r2 = raw_ref<const int>(r);
    EXPECT_EQ(&*r2, &i);
  }
}

TEST(RawRef, MoveConstruct) {
  {
    int i = 1;
    auto r = raw_ref<int>(i);
    EXPECT_EQ(&*r, &i);
    auto r2 = raw_ref<int>(std::move(r));
    EXPECT_EQ(&*r2, &i);
  }
  {
    int i = 1;
    auto r = raw_ref<const int>(i);
    EXPECT_EQ(&*r, &i);
    auto r2 = raw_ref<const int>(std::move(r));
    EXPECT_EQ(&*r2, &i);
  }
}

TEST(RawRef, CopyAssign) {
  {
    int i = 1;
    int j = 2;
    auto r = raw_ref<int>(i);
    EXPECT_EQ(&*r, &i);
    auto rj = raw_ref<int>(j);
    r = rj;
    EXPECT_EQ(&*r, &j);
  }
  {
    int i = 1;
    int j = 2;
    auto r = raw_ref<const int>(i);
    EXPECT_EQ(&*r, &i);
    auto rj = raw_ref<const int>(j);
    r = rj;
    EXPECT_EQ(&*r, &j);
  }
  {
    int i = 1;
    int j = 2;
    auto r = raw_ref<const int>(i);
    EXPECT_EQ(&*r, &i);
    auto rj = raw_ref<int>(j);
    r = rj;
    EXPECT_EQ(&*r, &j);
  }
}

TEST(RawRef, CopyReassignAfterMove) {
  int i = 1;
  int j = 1;
  auto r = raw_ref<int>(i);
  auto r2 = std::move(r);
  r2 = raw_ref<int>(j);
  // Reassign to the moved-from `r` so it can be used again.
  r = r2;
  EXPECT_EQ(&*r, &j);
}

TEST(RawRef, MoveAssign) {
  {
    int i = 1;
    int j = 2;
    auto r = raw_ref<int>(i);
    EXPECT_EQ(&*r, &i);
    r = raw_ref<int>(j);
    EXPECT_EQ(&*r, &j);
  }
  {
    int i = 1;
    int j = 2;
    auto r = raw_ref<const int>(i);
    EXPECT_EQ(&*r, &i);
    r = raw_ref<const int>(j);
    EXPECT_EQ(&*r, &j);
  }
  {
    int i = 1;
    int j = 2;
    auto r = raw_ref<const int>(i);
    EXPECT_EQ(&*r, &i);
    r = raw_ref<int>(j);
    EXPECT_EQ(&*r, &j);
  }
}

TEST(RawRef, MoveReassignAfterMove) {
  int i = 1;
  int j = 1;
  auto r = raw_ref<int>(i);
  auto r2 = std::move(r);
  // Reassign to the moved-from `r` so it can be used again.
  r = raw_ref<int>(j);
  EXPECT_EQ(&*r, &j);
}

TEST(RawRef, CopyConstructUpCast) {
  {
    auto s = SubClass();
    auto r = raw_ref<SubClass>(s);
    EXPECT_EQ(&*r, &s);
    auto r2 = raw_ref<BaseClass>(r);
    EXPECT_EQ(&*r2, &s);
  }
  {
    auto s = SubClass();
    auto r = raw_ref<const SubClass>(s);
    EXPECT_EQ(&*r, &s);
    auto r2 = raw_ref<const BaseClass>(r);
    EXPECT_EQ(&*r2, &s);
  }
}

TEST(RawRef, MoveConstructUpCast) {
  {
    auto s = SubClass();
    auto r = raw_ref<SubClass>(s);
    EXPECT_EQ(&*r, &s);
    auto r2 = raw_ref<BaseClass>(std::move(r));
    EXPECT_EQ(&*r2, &s);
  }
  {
    auto s = SubClass();
    auto r = raw_ref<const SubClass>(s);
    EXPECT_EQ(&*r, &s);
    auto r2 = raw_ref<const BaseClass>(std::move(r));
    EXPECT_EQ(&*r2, &s);
  }
}

TEST(RawRef, FromPtr) {
  int i = 42;
  auto ref = raw_ref<int>::from_ptr(&i);
  EXPECT_EQ(&i, &*ref);
}

TEST(RawRef, CopyAssignUpCast) {
  {
    auto s = SubClass();
    auto r = raw_ref<SubClass>(s);
    auto t = BaseClass();
    auto rt = raw_ref<BaseClass>(t);
    rt = r;
    EXPECT_EQ(&*rt, &s);
  }
  {
    auto s = SubClass();
    auto r = raw_ref<const SubClass>(s);
    auto t = BaseClass();
    auto rt = raw_ref<const BaseClass>(t);
    rt = r;
    EXPECT_EQ(&*rt, &s);
  }
  {
    auto s = SubClass();
    auto r = raw_ref<SubClass>(s);
    auto t = BaseClass();
    auto rt = raw_ref<const BaseClass>(t);
    rt = r;
    EXPECT_EQ(&*rt, &s);
  }
}

TEST(RawRef, MoveAssignUpCast) {
  {
    auto s = SubClass();
    auto r = raw_ref<SubClass>(s);
    auto t = BaseClass();
    auto rt = raw_ref<BaseClass>(t);
    rt = std::move(r);
    EXPECT_EQ(&*rt, &s);
  }
  {
    auto s = SubClass();
    auto r = raw_ref<const SubClass>(s);
    auto t = BaseClass();
    auto rt = raw_ref<const BaseClass>(t);
    rt = std::move(r);
    EXPECT_EQ(&*rt, &s);
  }
  {
    auto s = SubClass();
    auto r = raw_ref<SubClass>(s);
    auto t = BaseClass();
    auto rt = raw_ref<const BaseClass>(t);
    rt = std::move(r);
    EXPECT_EQ(&*rt, &s);
  }
}

TEST(RawRef, Deref) {
  int i;
  auto r = raw_ref<int>(i);
  EXPECT_EQ(&*r, &i);
}

TEST(RawRef, Arrow) {
  int i;
  auto r = raw_ref<int>(i);
  EXPECT_EQ(r.operator->(), &i);
}

TEST(RawRef, Swap) {
  int i;
  int j;
  auto ri = raw_ref<int>(i);
  auto rj = raw_ref<int>(j);
  swap(ri, rj);
  EXPECT_EQ(&*ri, &j);
  EXPECT_EQ(&*rj, &i);
}

TEST(RawRef, Equals) {
  int i = 1;
  auto r1 = raw_ref<int>(i);
  auto r2 = raw_ref<int>(i);
  EXPECT_TRUE(r1 == r1);
  EXPECT_TRUE(r1 == r2);
  EXPECT_TRUE(r1 == i);
  EXPECT_TRUE(i == r1);
  int j = 1;
  auto r3 = raw_ref<int>(j);
  EXPECT_FALSE(r1 == r3);
  EXPECT_FALSE(r1 == j);
  EXPECT_FALSE(j == r1);
}

TEST(RawRef, NotEquals) {
  int i = 1;
  auto r1 = raw_ref<int>(i);
  int j = 1;
  auto r2 = raw_ref<int>(j);
  EXPECT_TRUE(r1 != r2);
  EXPECT_TRUE(r1 != j);
  EXPECT_TRUE(j != r1);
  EXPECT_FALSE(r1 != r1);
  EXPECT_FALSE(r2 != j);
  EXPECT_FALSE(j != r2);
}

TEST(RawRef, LessThan) {
  int i[] = {1, 1};
  auto r1 = raw_ref<int>(i[0]);
  auto r2 = raw_ref<int>(i[1]);
  EXPECT_TRUE(r1 < r2);
  EXPECT_TRUE(r1 < i[1]);
  EXPECT_FALSE(i[1] < r1);
  EXPECT_FALSE(r2 < r1);
  EXPECT_FALSE(r2 < i[0]);
  EXPECT_TRUE(i[0] < r2);
  EXPECT_FALSE(r1 < r1);
  EXPECT_FALSE(r1 < i[0]);
  EXPECT_FALSE(i[0] < r1);
}

TEST(RawRef, GreaterThan) {
  int i[] = {1, 1};
  auto r1 = raw_ref<int>(i[0]);
  auto r2 = raw_ref<int>(i[1]);
  EXPECT_TRUE(r2 > r1);
  EXPECT_FALSE(r1 > r2);
  EXPECT_FALSE(r1 > i[1]);
  EXPECT_TRUE(i[1] > r1);
  EXPECT_FALSE(r2 > r2);
  EXPECT_FALSE(r2 > i[1]);
  EXPECT_FALSE(i[1] > r2);
}

TEST(RawRef, LessThanOrEqual) {
  int i[] = {1, 1};
  auto r1 = raw_ref<int>(i[0]);
  auto r2 = raw_ref<int>(i[1]);
  EXPECT_TRUE(r1 <= r2);
  EXPECT_TRUE(r1 <= r1);
  EXPECT_TRUE(r2 <= r2);
  EXPECT_FALSE(r2 <= r1);
  EXPECT_TRUE(r1 <= i[1]);
  EXPECT_TRUE(r1 <= i[0]);
  EXPECT_TRUE(r2 <= i[1]);
  EXPECT_FALSE(r2 <= i[0]);
  EXPECT_FALSE(i[1] <= r1);
  EXPECT_TRUE(i[0] <= r1);
  EXPECT_TRUE(i[1] <= r2);
  EXPECT_TRUE(i[0] <= r2);
}

TEST(RawRef, GreaterThanOrEqual) {
  int i[] = {1, 1};
  auto r1 = raw_ref<int>(i[0]);
  auto r2 = raw_ref<int>(i[1]);
  EXPECT_TRUE(r2 >= r1);
  EXPECT_TRUE(r1 >= r1);
  EXPECT_TRUE(r2 >= r2);
  EXPECT_FALSE(r1 >= r2);
  EXPECT_TRUE(r2 >= i[0]);
  EXPECT_TRUE(r1 >= i[0]);
  EXPECT_TRUE(r2 >= i[1]);
  EXPECT_FALSE(r1 >= i[1]);
  EXPECT_FALSE(i[0] >= r2);
  EXPECT_TRUE(i[0] >= r1);
  EXPECT_TRUE(i[1] >= r2);
  EXPECT_TRUE(i[1] >= r1);
}

// Death Tests: If we're only using the no-op version of `raw_ptr` and
// have `!PA_BUILDFLAG(DCHECKS_ARE_ON)`, the `PA_RAW_PTR_CHECK()`s used in
// `raw_ref` evaluate to nothing. Therefore, death tests relying on
// these CHECKs firing are disabled in their absence.

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) || PA_BUILDFLAG(DCHECKS_ARE_ON)

TEST(RawRefDeathTest, CopyConstructAfterMove) {
  int i = 1;
  auto r = raw_ref<int>(i);
  auto r2 = std::move(r);
  EXPECT_CHECK_DEATH({ [[maybe_unused]] auto r3 = r; });
}

TEST(RawRefDeathTest, MoveConstructAfterMove) {
  int i = 1;
  auto r = raw_ref<int>(i);
  auto r2 = std::move(r);
  EXPECT_CHECK_DEATH({ [[maybe_unused]] auto r3 = std::move(r); });
}

TEST(RawRefDeathTest, CopyAssignAfterMove) {
  int i = 1;
  auto r = raw_ref<int>(i);
  auto r2 = std::move(r);
  EXPECT_CHECK_DEATH({ r2 = r; });
}

TEST(RawRefDeathTest, MoveAssignAfterMove) {
  int i = 1;
  auto r = raw_ref<int>(i);
  auto r2 = std::move(r);
  EXPECT_CHECK_DEATH({ r2 = std::move(r); });
}

TEST(RawRefDeathTest, CopyConstructAfterMoveUpCast) {
  auto s = SubClass();
  auto r = raw_ref<SubClass>(s);
  auto moved = std::move(r);
  EXPECT_CHECK_DEATH({ [[maybe_unused]] auto r2 = raw_ref<BaseClass>(r); });
}

TEST(RawRefDeathTest, MoveConstructAfterMoveUpCast) {
  auto s = SubClass();
  auto r = raw_ref<SubClass>(s);
  auto moved = std::move(r);
  EXPECT_CHECK_DEATH(
      { [[maybe_unused]] auto r2 = raw_ref<BaseClass>(std::move(r)); });
}

TEST(RawRefDeathTest, FromPtrWithNullptr) {
  EXPECT_CHECK_DEATH({ raw_ref<int>::from_ptr(nullptr); });
}

TEST(RawRefDeathTest, CopyAssignAfterMoveUpCast) {
  auto s = SubClass();
  auto r = raw_ref<const SubClass>(s);
  auto t = BaseClass();
  auto rt = raw_ref<const BaseClass>(t);
  auto moved = std::move(r);
  EXPECT_CHECK_DEATH({ rt = r; });
}

TEST(RawRefDeathTest, MoveAssignAfterMoveUpCast) {
  auto s = SubClass();
  auto r = raw_ref<const SubClass>(s);
  auto t = BaseClass();
  auto rt = raw_ref<const BaseClass>(t);
  auto moved = std::move(r);
  EXPECT_CHECK_DEATH({ rt = std::move(r); });
}

TEST(RawRefDeathTest, DerefAfterMove) {
  int i;
  auto r = raw_ref<int>(i);
  auto moved = std::move(r);
  EXPECT_CHECK_DEATH({ r.operator*(); });
}

TEST(RawRefDeathTest, ArrowAfterMove) {
  int i;
  auto r = raw_ref<int>(i);
  auto moved = std::move(r);
  EXPECT_CHECK_DEATH({ r.operator->(); });
}

TEST(RawRefDeathTest, SwapAfterMove) {
  {
    int i;
    auto ri = raw_ref<int>(i);
    int j;
    auto rj = raw_ref<int>(j);

    auto moved = std::move(ri);
    EXPECT_CHECK_DEATH({ swap(ri, rj); });
  }
  {
    int i;
    auto ri = raw_ref<int>(i);
    int j;
    auto rj = raw_ref<int>(j);

    auto moved = std::move(rj);
    EXPECT_CHECK_DEATH({ swap(ri, rj); });
  }
}

TEST(RawRefDeathTest, EqualsAfterMove) {
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 == r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r2);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 == r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 == r1; });
  }
}

TEST(RawRefDeathTest, NotEqualsAfterMove) {
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 != r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r2);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 != r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 != r1; });
  }
}

TEST(RawRefDeathTest, LessThanAfterMove) {
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 < r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r2);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 < r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 < r1; });
  }
}

TEST(RawRefDeathTest, GreaterThanAfterMove) {
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 > r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r2);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 > r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 > r1; });
  }
}

TEST(RawRefDeathTest, LessThanOrEqualAfterMove) {
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 <= r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r2);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 <= r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 <= r1; });
  }
}

TEST(RawRefDeathTest, GreaterThanOrEqualAfterMove) {
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 >= r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto r2 = raw_ref<int>(i);
    auto moved = std::move(r2);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 >= r2; });
  }
  {
    int i = 1;
    auto r1 = raw_ref<int>(i);
    auto moved = std::move(r1);
    EXPECT_CHECK_DEATH({ [[maybe_unused]] bool b = r1 >= r1; });
  }
}

#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) ||
        // PA_BUILDFLAG(DCHECKS_ARE_ON)

TEST(RawRef, CTAD) {
  int i = 1;
  auto r = raw_ref(i);
  EXPECT_EQ(&*r, &i);
}

TEST(RawRefPtr, CTADWithConst) {
  std::string str;
  struct S {
    const raw_ref<const std::string> r;
  };
  // Deduces as `raw_ref<std::string>`, for which the constructor call is valid
  // making a mutable reference, and then converts to
  // `raw_ref<const std::string>`.
  S s1 = {.r = raw_ref(str)};
  // Deduces as raw_ref<const std::string>, for which the constructor call is
  // valid from a const ref.
  S s2 = {.r = raw_ref(static_cast<const std::string&>(str))};
  EXPECT_EQ(&*s1.r, &str);
  EXPECT_EQ(&*s2.r, &str);
}

// Shorter name for expected test impl.
using RawPtrCountingImpl = base::test::RawPtrCountingImplForTest;

template <typename T>
using CountingRawRef = raw_ref<T, base::RawPtrTraits::kUseCountingImplForTest>;

// Ensure that the `kUseCountingImplForTest` flag selects the test impl.
static_assert(std::is_same_v<CountingRawRef<int>::Impl, RawPtrCountingImpl>);

template <typename T>
using CountingRawRefMayDangle =
    raw_ref<T,
            base::RawPtrTraits::kMayDangle |
                base::RawPtrTraits::kUseCountingImplForTest>;

// Ensure that the `kUseCountingImplForTest` flag selects the test impl.
static_assert(
    std::is_same_v<CountingRawRefMayDangle<int>::Impl, RawPtrCountingImpl>);

TEST(RawRef, StdLess) {
  int i[] = {1, 1};
  {
    RawPtrCountingImpl::ClearCounters();
    auto r1 = CountingRawRef<int>(i[0]);
    auto r2 = CountingRawRef<int>(i[1]);
    EXPECT_TRUE(std::less<CountingRawRef<int>>()(r1, r2));
    EXPECT_FALSE(std::less<CountingRawRef<int>>()(r2, r1));
    EXPECT_EQ(2, RawPtrCountingImpl::wrapped_ptr_less_cnt);
  }
  {
    RawPtrCountingImpl::ClearCounters();
    const auto r1 = CountingRawRef<int>(i[0]);
    const auto r2 = CountingRawRef<int>(i[1]);
    EXPECT_TRUE(std::less<CountingRawRef<int>>()(r1, r2));
    EXPECT_FALSE(std::less<CountingRawRef<int>>()(r2, r1));
    EXPECT_EQ(2, RawPtrCountingImpl::wrapped_ptr_less_cnt);
  }
  {
    RawPtrCountingImpl::ClearCounters();
    auto r1 = CountingRawRef<const int>(i[0]);
    auto r2 = CountingRawRef<const int>(i[1]);
    EXPECT_TRUE(std::less<CountingRawRef<const int>>()(r1, r2));
    EXPECT_FALSE(std::less<CountingRawRef<const int>>()(r2, r1));
    EXPECT_EQ(2, RawPtrCountingImpl::wrapped_ptr_less_cnt);
  }
  {
    RawPtrCountingImpl::ClearCounters();
    auto r1 = CountingRawRef<int>(i[0]);
    auto r2 = CountingRawRef<int>(i[1]);
    EXPECT_TRUE(std::less<CountingRawRef<int>>()(r1, i[1]));
    EXPECT_FALSE(std::less<CountingRawRef<int>>()(r2, i[0]));
    EXPECT_EQ(2, RawPtrCountingImpl::wrapped_ptr_less_cnt);
  }
  {
    RawPtrCountingImpl::ClearCounters();
    const auto r1 = CountingRawRef<int>(i[0]);
    const auto r2 = CountingRawRef<int>(i[1]);
    EXPECT_TRUE(std::less<CountingRawRef<int>>()(r1, i[1]));
    EXPECT_FALSE(std::less<CountingRawRef<int>>()(r2, i[0]));
    EXPECT_EQ(2, RawPtrCountingImpl::wrapped_ptr_less_cnt);
  }
  {
    RawPtrCountingImpl::ClearCounters();
    auto r1 = CountingRawRef<const int>(i[0]);
    auto r2 = CountingRawRef<const int>(i[1]);
    EXPECT_TRUE(std::less<CountingRawRef<const int>>()(r1, i[1]));
    EXPECT_FALSE(std::less<CountingRawRef<const int>>()(r2, i[0]));
    EXPECT_EQ(2, RawPtrCountingImpl::wrapped_ptr_less_cnt);
  }
}

// Verifies that comparing `raw_ref`s with different underlying Traits
// is a valid utterance and primarily uses the `GetForComparison()` methods.
TEST(RawRef, OperatorsUseGetForComparison) {
  int x = 123;
  CountingRawRef<int> ref1(x);
  CountingRawRefMayDangle<int> ref2(x);

  RawPtrCountingImpl::ClearCounters();

  EXPECT_TRUE(ref1 == ref2);
  EXPECT_FALSE(ref1 != ref2);
  // The use of `PA_RAW_PTR_CHECK()`s to catch dangling references means
  // that we can't actually readily specify whether there are 0
  // extractions (`CHECK()`s compiled out) or 2 extractions.
  EXPECT_THAT((CountingRawPtrExpectations{.get_for_comparison_cnt = 4}),
              CountersMatch());

  EXPECT_FALSE(ref1 < ref2);
  EXPECT_FALSE(ref1 > ref2);
  EXPECT_TRUE(ref1 <= ref2);
  EXPECT_TRUE(ref1 >= ref2);
  EXPECT_THAT((CountingRawPtrExpectations{
                  .get_for_comparison_cnt = 12,
              }),
              CountersMatch());
}

TEST(RawRef, CrossKindConversion) {
  int x = 123;
  CountingRawRef<int> ref1(x);

  RawPtrCountingImpl::ClearCounters();

  CountingRawRefMayDangle<int> ref2(ref1);
  CountingRawRefMayDangle<int> ref3(std::move(ref1));  // Falls back to copy.

  EXPECT_THAT((CountingRawPtrExpectations{.wrap_raw_ptr_cnt = 0,
                                          .get_for_dereference_cnt = 0,
                                          .get_for_extraction_cnt = 0,
                                          .wrap_raw_ptr_for_dup_cnt = 2,
                                          .get_for_duplication_cnt = 2}),
              CountersMatch());
}

TEST(RawRef, CrossKindAssignment) {
  int x = 123;
  CountingRawRef<int> ref1(x);

  CountingRawRefMayDangle<int> ref2(x);
  CountingRawRefMayDangle<int> ref3(x);

  RawPtrCountingImpl::ClearCounters();
  ref2 = ref1;
  ref3 = std::move(ref1);  // Falls back to copy.

  EXPECT_THAT((CountingRawPtrExpectations{.wrap_raw_ptr_cnt = 0,
                                          .get_for_dereference_cnt = 0,
                                          .get_for_extraction_cnt = 0,
                                          .wrap_raw_ptr_for_dup_cnt = 2,
                                          .get_for_duplication_cnt = 2}),
              CountersMatch());
}

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

TEST(AsanBackupRefPtrImpl, RawRefGet) {
  base::debug::AsanService::GetInstance()->Initialize();

  if (!base::RawPtrAsanService::GetInstance().IsEnabled()) {
    base::RawPtrAsanService::GetInstance().Configure(
        base::EnableDereferenceCheck(true), base::EnableExtractionCheck(true),
        base::EnableInstantiationCheck(true));
  } else {
    ASSERT_TRUE(
        base::RawPtrAsanService::GetInstance().is_dereference_check_enabled());
    ASSERT_TRUE(
        base::RawPtrAsanService::GetInstance().is_extraction_check_enabled());
    ASSERT_TRUE(base::RawPtrAsanService::GetInstance()
                    .is_instantiation_check_enabled());
  }

  auto ptr = ::std::make_unique<int>();
  raw_ref<int> safe_ref(*ptr);
  ptr.reset();

  // This test is specifically to ensure that raw_ref.get() does not cause a
  // dereference of the memory referred to by the reference. If there is a
  // dereference, then this test will crash.
  [[maybe_unused]] volatile int& ref = safe_ref.get();
}

TEST(AsanBackupRefPtrImpl, RawRefOperatorStar) {
  base::debug::AsanService::GetInstance()->Initialize();

  if (!base::RawPtrAsanService::GetInstance().IsEnabled()) {
    base::RawPtrAsanService::GetInstance().Configure(
        base::EnableDereferenceCheck(true), base::EnableExtractionCheck(true),
        base::EnableInstantiationCheck(true));
  } else {
    ASSERT_TRUE(
        base::RawPtrAsanService::GetInstance().is_dereference_check_enabled());
    ASSERT_TRUE(
        base::RawPtrAsanService::GetInstance().is_extraction_check_enabled());
    ASSERT_TRUE(base::RawPtrAsanService::GetInstance()
                    .is_instantiation_check_enabled());
  }

  auto ptr = ::std::make_unique<int>();
  raw_ref<int> safe_ref(*ptr);
  ptr.reset();

  // This test is specifically to ensure that &*raw_ref does not cause a
  // dereference of the memory referred to by the reference. If there is a
  // dereference, then this test will crash.
  [[maybe_unused]] volatile int& ref = *safe_ref;
}

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

}  // namespace
