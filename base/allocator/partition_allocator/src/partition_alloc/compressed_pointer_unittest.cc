// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/compressed_pointer.h"

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_root.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc {

namespace {

struct Base {
  double a;
};
struct Derived : Base {
  double b;
};
struct Mixin {
  double c;
};
struct DerivedWithMixin : Base, Mixin {
  double d;
};

struct PADeleter final {
  void operator()(void* ptr) const { allocator_.root()->Free(ptr); }
  PartitionAllocator& allocator_;
};

template <typename T, typename... Args>
std::unique_ptr<T, PADeleter> make_pa_unique(PartitionAllocator& alloc,
                                             Args&&... args) {
  T* result = new (alloc.root()->Alloc(sizeof(T), nullptr))
      T(std::forward<Args>(args)...);
  return std::unique_ptr<T, PADeleter>(result, PADeleter{alloc});
}

template <typename T>
std::unique_ptr<T[], PADeleter> make_pa_array_unique(PartitionAllocator& alloc,
                                                     size_t num) {
  T* result = new (alloc.root()->Alloc(sizeof(T) * num, nullptr)) T();
  return std::unique_ptr<T[], PADeleter>(result, PADeleter{alloc});
}

// Test that pointer types are trivial.
#if PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
static_assert(
    std::is_trivially_default_constructible_v<CompressedPointer<Base>>);
static_assert(std::is_trivially_copy_constructible_v<CompressedPointer<Base>>);
static_assert(std::is_trivially_move_constructible_v<CompressedPointer<Base>>);
static_assert(std::is_trivially_copy_assignable_v<CompressedPointer<Base>>);
static_assert(std::is_trivially_move_assignable_v<CompressedPointer<Base>>);
#endif  // PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
static_assert(
    std::is_trivially_default_constructible_v<UncompressedPointer<Base>>);
static_assert(
    std::is_trivially_copy_constructible_v<UncompressedPointer<Base>>);
static_assert(
    std::is_trivially_move_constructible_v<UncompressedPointer<Base>>);
static_assert(std::is_trivially_copy_assignable_v<UncompressedPointer<Base>>);
static_assert(std::is_trivially_move_assignable_v<UncompressedPointer<Base>>);

}  // namespace

struct UncompressedTypeTag {};
struct CompressedTypeTag {};

template <typename TagType>
class CompressedPointerTest : public ::testing::Test {
 public:
#if PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
  template <typename T>
  using PointerType =
      std::conditional_t<std::is_same_v<TagType, CompressedTypeTag>,
                         CompressedPointer<T>,
                         UncompressedPointer<T>>;
#else   // PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
  template <typename T>
  using PointerType = UncompressedPointer<T>;
#endif  // PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)

  CompressedPointerTest() : allocator_(PartitionOptions{}) {}

 protected:
  PartitionAllocator allocator_;
};

#if PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
using ObjectTypes = ::testing::Types<UncompressedTypeTag, CompressedTypeTag>;
#else
using ObjectTypes = ::testing::Types<UncompressedTypeTag>;
#endif

TYPED_TEST_SUITE(CompressedPointerTest, ObjectTypes);

TYPED_TEST(CompressedPointerTest, NullConstruction) {
  using DoublePointer = typename TestFixture::template PointerType<double>;
  {
    DoublePointer p = static_cast<DoublePointer>(nullptr);
    EXPECT_FALSE(p.is_nonnull());
    EXPECT_FALSE(p.get());
    EXPECT_EQ(p, nullptr);
  }
  {
    DoublePointer p1 = static_cast<DoublePointer>(nullptr);
    DoublePointer p2 = p1;
    EXPECT_FALSE(p2.is_nonnull());
    EXPECT_FALSE(p2.get());
    EXPECT_EQ(p2, nullptr);
  }
  {
    DoublePointer p1 = static_cast<DoublePointer>(nullptr);
    DoublePointer p2 = std::move(p1);
    EXPECT_FALSE(p2.is_nonnull());
    EXPECT_FALSE(p2.get());
    EXPECT_EQ(p2, nullptr);
  }
}

TYPED_TEST(CompressedPointerTest, NullAssignment) {
  using DoublePointer = typename TestFixture::template PointerType<double>;
  {
    DoublePointer p;
    p = static_cast<DoublePointer>(nullptr);
    EXPECT_FALSE(p.is_nonnull());
    EXPECT_FALSE(p.get());
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(p, nullptr);
  }
  {
    DoublePointer p1 = DoublePointer(nullptr), p2;
    p2 = p1;
    EXPECT_FALSE(p2.is_nonnull());
    EXPECT_FALSE(p2.get());
    EXPECT_EQ(p2.get(), nullptr);
    EXPECT_EQ(p2, nullptr);
  }
  {
    DoublePointer p1 = DoublePointer(nullptr), p2;
    p2 = std::move(p1);
    EXPECT_FALSE(p2.is_nonnull());
    EXPECT_FALSE(p2.get());
    EXPECT_EQ(p2.get(), nullptr);
    EXPECT_EQ(p2, nullptr);
  }
}

TYPED_TEST(CompressedPointerTest, SameTypeValueConstruction) {
  using DoublePointer = typename TestFixture::template PointerType<double>;
  auto d = make_pa_unique<double>(this->allocator_);
  {
    DoublePointer p = static_cast<DoublePointer>(d.get());
    EXPECT_TRUE(p.is_nonnull());
    EXPECT_EQ(p.get(), d.get());
    EXPECT_EQ(p, d.get());
  }
  {
    DoublePointer p1 = static_cast<DoublePointer>(d.get());
    DoublePointer p2 = p1;
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, p1);
    EXPECT_EQ(p2, d.get());
  }
  {
    DoublePointer p1 = static_cast<DoublePointer>(d.get());
    DoublePointer p2 = std::move(p1);
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, d.get());
  }
}

TYPED_TEST(CompressedPointerTest, SameTypeValueAssignment) {
  using DoublePointer = typename TestFixture::template PointerType<double>;
  auto d = make_pa_unique<double>(this->allocator_);
  {
    DoublePointer p;
    p = static_cast<DoublePointer>(d.get());
    EXPECT_TRUE(p.is_nonnull());
    EXPECT_EQ(p.get(), d.get());
    EXPECT_EQ(p, d.get());
  }
  {
    DoublePointer p1 = static_cast<DoublePointer>(d.get());
    DoublePointer p2;
    p2 = p1;
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, p1);
    EXPECT_EQ(p2, d.get());
  }
  {
    DoublePointer p1 = static_cast<DoublePointer>(d.get());
    DoublePointer p2;
    p2 = std::move(p1);
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, d.get());
  }
}

TYPED_TEST(CompressedPointerTest,
           HeterogeneousValueConstructionSamePointerValue) {
  using BasePointer = typename TestFixture::template PointerType<Base>;
  auto d = make_pa_unique<Derived>(this->allocator_);
  {
    BasePointer p = static_cast<BasePointer>(d.get());
    EXPECT_TRUE(p.is_nonnull());
    EXPECT_EQ(p.get(), d.get());
  }
  {
    BasePointer p1 = static_cast<BasePointer>(d.get());
    BasePointer p2 = p1;
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, p1);
    EXPECT_EQ(p2, d.get());
  }
  {
    BasePointer p1 = static_cast<BasePointer>(d.get());
    BasePointer p2 = std::move(p1);
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, d.get());
  }
}

TYPED_TEST(CompressedPointerTest,
           HeterogeneousValueAssignmentSamePointerValue) {
  using BasePointer = typename TestFixture::template PointerType<Base>;
  auto d = make_pa_unique<Derived>(this->allocator_);
  {
    BasePointer p;
    p = static_cast<BasePointer>(d.get());
    EXPECT_TRUE(p.is_nonnull());
    EXPECT_EQ(p.get(), d.get());
  }
  {
    BasePointer p1 = static_cast<BasePointer>(d.get());
    BasePointer p2;
    p2 = p1;
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, p1);
    EXPECT_EQ(p2, d.get());
  }
  {
    BasePointer p1 = static_cast<BasePointer>(d.get());
    BasePointer p2;
    p2 = std::move(p1);
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, d.get());
  }
}

TYPED_TEST(CompressedPointerTest,
           HeterogeneousValueConstructionDifferentPointerValues) {
  using MixinPointer = typename TestFixture::template PointerType<Mixin>;
  auto d = make_pa_unique<DerivedWithMixin>(this->allocator_);
  {
    MixinPointer p = static_cast<MixinPointer>(d.get());
    ASSERT_NE(static_cast<void*>(p.get()), static_cast<void*>(d.get()));
  }
  {
    MixinPointer p = static_cast<MixinPointer>(d.get());
    EXPECT_TRUE(p.is_nonnull());
    EXPECT_EQ(p.get(), d.get());
  }
  {
    MixinPointer p1 = static_cast<MixinPointer>(d.get());
    MixinPointer p2 = p1;
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, p1);
    EXPECT_EQ(p2, d.get());
  }
  {
    MixinPointer p1 = static_cast<MixinPointer>(d.get());
    MixinPointer p2 = std::move(p1);
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, d.get());
  }
}

TYPED_TEST(CompressedPointerTest,
           HeterogeneousValueAssignmentDifferentPointerValue) {
  using MixinPointer = typename TestFixture::template PointerType<Mixin>;
  auto d = make_pa_unique<DerivedWithMixin>(this->allocator_);
  {
    MixinPointer p;
    p = static_cast<MixinPointer>(d.get());
    ASSERT_NE(static_cast<void*>(p.get()), static_cast<void*>(d.get()));
  }
  {
    MixinPointer p;
    p = static_cast<MixinPointer>(d.get());
    EXPECT_TRUE(p.is_nonnull());
    EXPECT_EQ(p.get(), d.get());
  }
  {
    MixinPointer p1 = static_cast<MixinPointer>(d.get());
    MixinPointer p2;
    p2 = p1;
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, p1);
    EXPECT_EQ(p2, d.get());
  }
  {
    MixinPointer p1 = static_cast<MixinPointer>(d.get());
    MixinPointer p2;
    p2 = std::move(p1);
    EXPECT_TRUE(p2.is_nonnull());
    EXPECT_EQ(p2.get(), d.get());
    EXPECT_EQ(p2, d.get());
  }
}

namespace {

template <template <typename> class PointerType,
          typename T1,
          typename T2,
          typename U>
void EqualityTest(U* raw) {
  PointerType<T1> p1 = static_cast<PointerType<T1>>(raw);
  PointerType<T2> p2 = static_cast<PointerType<T2>>(raw);
  EXPECT_EQ(p1, raw);
  EXPECT_EQ(p2, raw);
  EXPECT_EQ(raw, p1);
  EXPECT_EQ(raw, p2);
  EXPECT_EQ(p1, p2);
}

template <template <typename> class PointerType,
          typename T1,
          typename T2,
          typename U>
void CompareTest(U* array) {
  PointerType<T1> p0 = static_cast<PointerType<T1>>(&array[0]);
  PointerType<T2> p1 = static_cast<PointerType<T2>>(&array[1]);
  {
    EXPECT_NE(p0, &array[1]);
    EXPECT_NE(p0, p1);
    EXPECT_NE(p1, &array[0]);
    EXPECT_NE(p1, p0);
  }
  {
    EXPECT_LT(p0, &array[1]);
    EXPECT_LT(&array[0], p1);
    EXPECT_LT(p0, p1);
  }
  {
    EXPECT_LE(p0, &array[0]);
    EXPECT_LE(p0, &array[1]);
    EXPECT_LE(&array[0], p0);

    EXPECT_LE(&array[1], p1);
    EXPECT_LE(p1, &array[1]);

    auto p2 = p0;
    EXPECT_LE(p0, p2);
    EXPECT_LE(p2, p1);
  }
  {
    EXPECT_GT(&array[1], p0);
    EXPECT_GT(p1, &array[0]);
    EXPECT_GT(p1, p0);
  }
  {
    EXPECT_GE(&array[0], p0);
    EXPECT_GE(&array[1], p0);
    EXPECT_GE(p0, &array[0]);

    EXPECT_GE(p1, &array[1]);
    EXPECT_GE(&array[1], p1);

    auto p2 = p1;
    EXPECT_GE(p1, p2);
    EXPECT_GE(p1, p0);
  }
}

}  // namespace

TYPED_TEST(CompressedPointerTest, EqualitySamePointerValue) {
  auto d = make_pa_unique<Derived>(this->allocator_);
  EqualityTest<TestFixture::template PointerType, Base, Base>(d.get());
  EqualityTest<TestFixture::template PointerType, Base, Derived>(d.get());
  EqualityTest<TestFixture::template PointerType, Derived, Base>(d.get());
  EqualityTest<TestFixture::template PointerType, Derived, Derived>(d.get());
}

TYPED_TEST(CompressedPointerTest, EqualityDifferentPointerValues) {
  auto d = make_pa_unique<DerivedWithMixin>(this->allocator_);
  EqualityTest<TestFixture::template PointerType, Mixin, Mixin>(d.get());
  EqualityTest<TestFixture::template PointerType, Mixin, DerivedWithMixin>(
      d.get());
  EqualityTest<TestFixture::template PointerType, DerivedWithMixin, Mixin>(
      d.get());
  EqualityTest<TestFixture::template PointerType, DerivedWithMixin,
               DerivedWithMixin>(d.get());
}

TYPED_TEST(CompressedPointerTest, CompareSamePointerValue) {
  auto d = make_pa_array_unique<Derived>(this->allocator_, 2);
  CompareTest<TestFixture::template PointerType, Base, Base>(d.get());
  CompareTest<TestFixture::template PointerType, Base, Derived>(d.get());
  CompareTest<TestFixture::template PointerType, Derived, Base>(d.get());
  CompareTest<TestFixture::template PointerType, Derived, Derived>(d.get());
}

TYPED_TEST(CompressedPointerTest, CompareDifferentPointerValues) {
  auto d = make_pa_array_unique<DerivedWithMixin>(this->allocator_, 2);
  CompareTest<TestFixture::template PointerType, Mixin, Mixin>(d.get());
  CompareTest<TestFixture::template PointerType, Mixin, DerivedWithMixin>(
      d.get());
  CompareTest<TestFixture::template PointerType, DerivedWithMixin, Mixin>(
      d.get());
  CompareTest<TestFixture::template PointerType, DerivedWithMixin,
              DerivedWithMixin>(d.get());
}

}  // namespace partition_alloc
