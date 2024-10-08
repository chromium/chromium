// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/intrusive_heap.h"

#include <array>

#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using IntrusiveHeapInt = IntrusiveHeap<WithHeapHandle<int>>;

// Validates whether or not the given heap satisfies the heap invariant.
template <class H>
void ExpectHeap(const H& heap) {
  const auto& less = heap.value_comp();
  const auto& handle_access = heap.heap_handle_access();

  for (size_t i = 0; i < heap.size(); ++i) {
    size_t left = intrusive_heap::LeftIndex(i);
    size_t right = left + 1;

    if (left < heap.size())
      EXPECT_FALSE(less(heap[i], heap[left]));
    if (right < heap.size())
      EXPECT_FALSE(less(heap[i], heap[right]));

    intrusive_heap::CheckInvalidOrEqualTo(handle_access.GetHeapHandle(&heap[i]),
                                          i);
  }
}

// A small set of canonical elements, and the a function for validating the
// heap that should be created by those elements. This is used in various
// constructor/insertion tests.
#define CANONICAL_ELEMENTS 3, 1, 2, 4, 5, 6, 7, 0
void ExpectCanonical(const IntrusiveHeapInt& heap) {
  ExpectHeap(heap);

  // Manual implementation of a max-heap inserting the elements defined by
  // CANONICAL_ELEMENTS:
  // 3
  // 3 1
  // 3 1 2
  // 3 1 2 4 -> 3 4 2 1 -> 4 3 2 1
  // 4 3 2 1 5 -> 4 5 2 1 3 -> 5 4 2 1 3
  // 5 4 2 1 3 6 -> 5 4 6 1 3 2 -> 6 4 5 1 3 2
  // 6 4 5 1 3 2 7 -> 6 4 7 1 3 2 5 -> 7 4 6 1 3 2 5
  // 7 4 6 1 3 2 5 0
  std::vector<int> expected{7, 4, 6, 1, 3, 2, 5, 0};
  std::vector<int> actual;
  for (const auto& element : heap)
    actual.push_back(element.value());
  ASSERT_THAT(actual, testing::ContainerEq(expected));
}

// Initializes the given heap to be the "canonical" heap from the point of view
// of these tests.
void MakeCanonical(IntrusiveHeapInt* heap) {
  static constexpr int kInts[] = {CANONICAL_ELEMENTS};
  heap->clear();
  heap->insert_range(kInts);
  ExpectCanonical(*heap);
}

// A handful of helper functions and classes related to building an exhaustive
// stress test for IntrusiveHeap, with all combinations of default-constructible
// supports-move-operations and supports-copy-operations value types.

// IntrusiveHeap supports 3 types of operations: those that cause the heap to
// get smaller (deletions), those that keep the heap the same size (updates,
// replaces, etc), and those that cause the heap to get bigger (insertions).
enum OperationTypes : int {
  kGrowing,
  kShrinking,
  kSameSize,
  kOperationTypesCount
};

// The operations that cause a heap to get bigger.
enum GrowingOperations : int { kInsert, kEmplace, kGrowingOperationsCount };

// The operations that cause a heap to get smaller. Some of these are only
// supported by move-only value types.
enum ShrinkingOperations : int {
  kTake,
  kTakeTop,
  kErase,
  kPop,
  kShrinkingOperationsCount
};

// The operations that keep a heap the same size.
enum SameSizeOperations : int {
  kReplace,
  kReplaceTop,
  kUpdate,
  kSameSizeOperationsCount
};

// Randomly selects an operation for the GrowingOperations enum, applies it to
// the given heap, and validates that the operation completed as expected.
template <typename T>
void DoGrowingOperation(IntrusiveHeap<T>* heap) {
  GrowingOperations op = static_cast<GrowingOperations>(
      base::RandInt(0, kGrowingOperationsCount - 1));

  int value = base::RandInt(0, 1000);
  size_t old_size = heap->size();
  typename IntrusiveHeap<T>::const_iterator it;

  switch (op) {
    case kInsert: {
      it = heap->insert(T(value));
      break;
    }

    case kEmplace: {
      it = heap->emplace(value);
      break;
    }

    case kGrowingOperationsCount:
      NOTREACHED();
  }

  EXPECT_EQ(old_size + 1, heap->size());
  EXPECT_EQ(value, it->value());
  EXPECT_EQ(it->GetHeapHandle().index(), heap->ToIndex(it));
}

// Helper struct for determining with the given value type T is movable or not.
// Used to determine whether or not the "take" operations can be used.
template <typename T>
struct NotMovable {
  static constexpr bool value = !std::is_nothrow_move_constructible_v<T> &&
                                std::is_copy_constructible_v<T>;
};

// Invokes "take" if the type is movable, otherwise invokes erase.
template <typename T, bool kNotMovable = NotMovable<T>::value>
struct Take;
template <typename T>
struct Take<T, true> {
  static void Do(IntrusiveHeap<T>* heap, size_t index) { heap->erase(index); }
};
template <typename T>
struct Take<T, false> {
  static void Do(IntrusiveHeap<T>* heap, size_t index) {
    int value = heap->at(index).value();
    T t = heap->take(index);
    EXPECT_EQ(value, t.value());
    EXPECT_FALSE(t.GetHeapHandle().IsValid());
  }
};

// Invokes "take_top" if the type is movable, otherwise invokes pop.
template <typename T, bool kNotMovable = NotMovable<T>::value>
struct TakeTop;
template <typename T>
struct TakeTop<T, true> {
  static void Do(IntrusiveHeap<T>* heap) { heap->pop(); }
};
template <typename T>
struct TakeTop<T, false> {
  static void Do(IntrusiveHeap<T>* heap) {
    int value = heap->at(0).value();
    T t = heap->take_top();
    EXPECT_EQ(value, t.value());
    EXPECT_FALSE(t.GetHeapHandle().IsValid());
  }
};

// Randomly selects a shrinking operations, applies it to the given |heap| and
// validates that the operation completed as expected and resulted in a valid
// heap. The "take" operations will only be invoked for a value type T that
// supports move operations, otherwise they will be mapped to erase/pop.
template <typename T>
void DoShrinkingOperation(IntrusiveHeap<T>* heap) {
  ShrinkingOperations op = static_cast<ShrinkingOperations>(
      base::RandInt(0, kShrinkingOperationsCount - 1));

  size_t old_size = heap->size();
  size_t index = static_cast<size_t>(base::RandInt(0, old_size - 1));

  switch (op) {
    case kTake: {
      Take<T>::Do(heap, index);
      break;
    }

    case kTakeTop: {
      TakeTop<T>::Do(heap);
      break;
    }

    case kErase: {
      heap->erase(index);
      break;
    }

    case kPop: {
      heap->pop();
      break;
    }

    case kShrinkingOperationsCount:
      NOTREACHED();
  }

  EXPECT_EQ(old_size - 1, heap->size());
}

// Randomly selects a same size operation, applies it to the given |heap| and
// validates that the operation completed as expected and resulted in a valid
// heap.
template <typename T>
void DoSameSizeOperation(IntrusiveHeap<T>* heap) {
  SameSizeOperations op = static_cast<SameSizeOperations>(
      base::RandInt(0, kSameSizeOperationsCount - 1));

  size_t old_size = heap->size();
  size_t index = static_cast<size_t>(base::RandInt(0, old_size - 1));
  if (op == kReplaceTop)
    index = 0;
  int new_value = base::RandInt(0, 1000);
  typename IntrusiveHeap<T>::const_iterator it;

  switch (op) {
    case kReplace: {
      it = heap->Replace(index, T(new_value));
      break;
    }

    case kReplaceTop: {
      it = heap->ReplaceTop(T(new_value));
      break;
    }

    case kUpdate: {
      it = heap->Modify(
          index, [&new_value](T& element) { element.set_value(new_value); });
      break;
    }

    case kSameSizeOperationsCount:
      NOTREACHED();
  }

  EXPECT_EQ(old_size, heap->size());
  EXPECT_EQ(new_value, it->value());
  EXPECT_EQ(it->GetHeapHandle().index(), heap->ToIndex(it));
}

// Randomly selects an operation, applies it to the given |heap| and validates
// that the operation completed as expected and resulted in a valid heap.
template <typename T>
void DoRandomHeapOperation(IntrusiveHeap<T>* heap) {
  static constexpr int kMinHeapSize = 10u;
  static constexpr int kMaxHeapSize = 100u;

  OperationTypes operation_type =
      static_cast<OperationTypes>(base::RandInt(0, kOperationTypesCount - 1));

  // Keep the heap size bounded by forcing growing and shrinking operations when
  // it exceeds the bounds.
  if (heap->size() < kMinHeapSize) {
    operation_type = kGrowing;
  } else if (heap->size() > kMaxHeapSize) {
    operation_type = kShrinking;
  }

  switch (operation_type) {
    case kGrowing:
      DoGrowingOperation(heap);
      break;
    case kShrinking:
      DoShrinkingOperation(heap);
      break;
    case kSameSize:
      DoSameSizeOperation(heap);
      break;
    case kOperationTypesCount:
      NOTREACHED();
  }
}

// A stress test that is only applicable to value types T that support move
// operations.
template <typename T>
void MoveStressTest() {
  IntrusiveHeap<T> heap({2, 4, 6, 8});
  EXPECT_EQ(4u, heap.size());
  EXPECT_FALSE(heap.empty());
  ExpectHeap(heap);

  IntrusiveHeap<T> heap2(std::move(heap));
  EXPECT_EQ(4u, heap2.size());
  EXPECT_FALSE(heap2.empty());
  ExpectHeap(heap2);
  EXPECT_EQ(0u, heap.size());
  EXPECT_TRUE(heap.empty());
  ExpectHeap(heap);

  heap = std::move(heap2);
  EXPECT_EQ(4u, heap.size());
  EXPECT_FALSE(heap.empty());
  ExpectHeap(heap);
  EXPECT_EQ(0u, heap2.size());
  EXPECT_TRUE(heap2.empty());
  ExpectHeap(heap2);
}

// A stress that that is only applicable to value types T that support copy
// operations.
template <typename T>
void CopyStressTest() {
  IntrusiveHeap<T> heap({2, 4, 6, 8});
  EXPECT_EQ(4u, heap.size());
  EXPECT_FALSE(heap.empty());
  ExpectHeap(heap);

  IntrusiveHeap<T> heap2(heap);
  EXPECT_EQ(4u, heap2.size());
  EXPECT_FALSE(heap2.empty());
  ExpectHeap(heap2);
  EXPECT_EQ(4u, heap.size());
  EXPECT_FALSE(heap.empty());
  ExpectHeap(heap);

  IntrusiveHeap<T> heap3({1, 3, 5});
  heap3.clear();
  heap3 = heap;
  EXPECT_EQ(4u, heap3.size());
  EXPECT_FALSE(heap3.empty());
  ExpectHeap(heap);
  EXPECT_EQ(4u, heap.size());
  EXPECT_FALSE(heap.empty());
  ExpectHeap(heap);

  EXPECT_TRUE(heap == heap2);
  EXPECT_FALSE(heap != heap2);
}

// A stress test that is applicable to all value types, whether or not they
// support copy and/or move operations.
template <typename T>
void GeneralStressTest() {
  std::vector<int> vector{2, 4, 6, 8};
  IntrusiveHeap<T> heap(base::from_range, vector);
  EXPECT_EQ(4u, heap.size());
  EXPECT_FALSE(heap.empty());
  ExpectHeap(heap);

  heap.clear();
  EXPECT_EQ(0u, heap.size());
  EXPECT_TRUE(heap.empty());
  ExpectHeap(heap);

  // Create an element and get a handle to it.
  auto it = heap.insert(T(34));
  EXPECT_EQ(1u, heap.size());
  HeapHandle* handle = it->handle();
  EXPECT_EQ(0u, handle->index());
  ExpectHeap(heap);

  // Add some other elements.
  heap.insert(T(12));
  heap.emplace(14);
  EXPECT_EQ(3u, heap.size());
  ExpectHeap(heap);

  // The handle should have tracked the element it is associated with.
  EXPECT_EQ(34, heap[*handle].value());

  // Replace with a value that shouldn't need moving in the heap.
  size_t index = handle->index();
  handle = heap.Replace(*handle, T(40))->handle();
  EXPECT_EQ(3u, heap.size());
  ExpectHeap(heap);
  EXPECT_EQ(index, handle->index());

  // Replace with a value that should cause the entry to move.
  handle = heap.Replace(handle->index(), T(1))->handle();
  EXPECT_EQ(3u, heap.size());
  ExpectHeap(heap);
  EXPECT_NE(index, handle->index());

  // Replace the top element.
  heap.ReplaceTop(T(65));
  EXPECT_EQ(3u, heap.size());
  ExpectHeap(heap);

  // Insert several more elements.
  std::vector<int> elements({13, 17, 19, 23, 29, 31, 37, 41});
  heap.insert_range(elements);
  EXPECT_EQ(11u, heap.size());
  ExpectHeap(heap);

  // Invasively change an element that is already inside the heap, and then
  // repair the heap.
  T* element = const_cast<T*>(&heap[7]);
  element->set_value(97);
  heap.Update(7u);
  ExpectHeap(heap);

  // Safely modify an element that is already inside the heap.
  heap.Modify(7u, [](T& element) { element.set_value(128); });
  ExpectHeap(heap);

  // Do some more updates that are no-ops, just to explore all the flavours of
  // ToIndex.
  handle = heap[5].handle();
  heap.Update(*handle);
  heap.Update(heap.begin() + 6);
  heap.Update(heap.rbegin() + 8);
  ExpectHeap(heap);

  handle = heap[5].handle();
  EXPECT_TRUE(handle);
  EXPECT_EQ(5u, handle->index());
  EXPECT_EQ(5u, heap.ToIndex(*handle));
  EXPECT_EQ(5u, heap.ToIndex(heap.begin() + 5));
  EXPECT_EQ(5u, heap.ToIndex(heap.cbegin() + 5));
  EXPECT_EQ(5u, heap.ToIndex(heap.rbegin() + 5));
  EXPECT_EQ(5u, heap.ToIndex(heap.crbegin() + 5));
  EXPECT_EQ(HeapHandle::kInvalidIndex, heap.ToIndex(heap.end()));
  EXPECT_EQ(HeapHandle::kInvalidIndex, heap.ToIndex(heap.cend()));
  EXPECT_EQ(HeapHandle::kInvalidIndex, heap.ToIndex(heap.rend()));
  EXPECT_EQ(HeapHandle::kInvalidIndex, heap.ToIndex(heap.crend()));

  EXPECT_EQ(&heap[0], &heap.at(0));
  EXPECT_EQ(&heap[0], &heap.front());
  EXPECT_EQ(&heap[0], &heap.top());
  EXPECT_EQ(&heap[heap.size() - 1], &heap.back());
  EXPECT_EQ(&heap[0], heap.data());

  // Do a bunch of random operations on a heap as a stress test.
  for (size_t i = 0; i < 1000; ++i) {
    DoRandomHeapOperation(&heap);
    ExpectHeap(heap);
  }
}

// A basic value type that wraps an integer. This is default constructible, and
// supports both move and copy operations.
class Value : public InternalHeapHandleStorage {
 public:
  explicit Value(int value) : value_(value) {}
  Value() : value_(-1) {}
  Value(Value&& other) noexcept
      : InternalHeapHandleStorage(std::move(other)),
        value_(std::exchange(other.value_, -1)) {}
  Value(const Value& other) : value_(other.value_) {
    HeapHandle h = other.GetHeapHandle();
    if (h.IsValid())
      SetHeapHandle(h);
  }
  ~Value() override = default;

  Value& operator=(Value&& other) noexcept {
    InternalHeapHandleStorage::operator=(std::move(other));
    value_ = std::exchange(other.value_, -1);
    return *this;
  }
  Value& operator=(const Value& other) {
    value_ = other.value_;
    return *this;
  }

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

  bool operator==(const Value& rhs) const { return value_ == rhs.value_; }
  bool operator!=(const Value& rhs) const { return value_ != rhs.value_; }
  bool operator<=(const Value& rhs) const { return value_ <= rhs.value_; }
  bool operator>=(const Value& rhs) const { return value_ >= rhs.value_; }
  bool operator<(const Value& rhs) const { return value_ < rhs.value_; }
  bool operator>(const Value& rhs) const { return value_ > rhs.value_; }

 private:
  int value_;
};

// Macro for creating versions of Value that selectively enable/disable
// default-constructors, move-operations and copy-operations.
#define DEFINE_VALUE_TYPE(name, default_construct, move, copy) \
  class name : public Value {                                  \
   public:                                                     \
    explicit name(int value) : Value(value) {}                 \
    name() = default_construct;                                \
    name(name&&) noexcept = move;                              \
    name(const name&) = copy;                                  \
    name& operator=(name&&) noexcept = move;                   \
    name& operator=(const name&) = copy;                       \
  };

DEFINE_VALUE_TYPE(Value_DMC, default, default, default)
DEFINE_VALUE_TYPE(Value_DmC, default, delete, default)
DEFINE_VALUE_TYPE(Value_DMc, default, default, delete)
DEFINE_VALUE_TYPE(Value_dMC, delete, default, default)
DEFINE_VALUE_TYPE(Value_dmC, delete, delete, default)
DEFINE_VALUE_TYPE(Value_dMc, delete, default, delete)

// Used to validate that the generated value types work as expected wrt
// default-constructors, move-operations and copy-operations.
template <typename ValueType, bool D, bool M, bool C>
void ValidateValueType() {
  static_assert(std::is_default_constructible_v<ValueType> == D, "oops");
  static_assert(std::is_move_constructible_v<ValueType> == M, "oops");
  static_assert(std::is_move_assignable_v<ValueType> == M, "oops");
  static_assert(std::is_copy_constructible_v<ValueType> == C, "oops");
  static_assert(std::is_copy_assignable_v<ValueType> == C, "oops");
}

// A small test element that provides its own HeapHandle storage and implements
// the contract expected of the DefaultHeapHandleAccessor.
struct TestElement {
  int key;
  raw_ptr<HeapHandle> handle;

  // Make this a min-heap by return > instead of <.
  bool operator<(const TestElement& other) const { return key > other.key; }

  void SetHeapHandle(HeapHandle h) {
    if (handle)
      *handle = h;
  }

  void ClearHeapHandle() {
    if (handle)
      handle->reset();
  }

  HeapHandle GetHeapHandle() const {
    if (handle)
      return *handle;
    return HeapHandle::Invalid();
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TEST SUITE 1
//
// Explicit tests of a simple heap using WithHeapHandle<>.

TEST(IntrusiveHeapTest, Constructors) {
  {
    // Default constructor.
    IntrusiveHeapInt heap;
    EXPECT_TRUE(heap.empty());
  }

  {
    // Constructor from range.
    std::vector<int> ints{CANONICAL_ELEMENTS};
    IntrusiveHeapInt heap(base::from_range, ints);
    ExpectCanonical(heap);

    // Move constructor.
    IntrusiveHeapInt heap2(std::move(heap));
    EXPECT_TRUE(heap.empty());
    ExpectCanonical(heap2);
  }

  {
    // Constructor with iterators.
    std::vector<int> ints{CANONICAL_ELEMENTS};
    // SAFETY: begin/end always provides a valid iterator pair.
    auto heap = UNSAFE_BUFFERS(IntrusiveHeapInt(ints.begin(), ints.end()));
    ExpectCanonical(heap);
  }

  {
    // Constructor with initializer list.
    IntrusiveHeapInt heap{CANONICAL_ELEMENTS};
    ExpectCanonical(heap);
  }
}

TEST(IntrusiveHeapTest, Assignment) {
  IntrusiveHeapInt heap{CANONICAL_ELEMENTS};

  // Move assignment.
  IntrusiveHeapInt heap2;
  heap2 = std::move(heap);
  EXPECT_TRUE(heap.empty());
  ExpectCanonical(heap2);
}

TEST(IntrusiveHeapTest, Swap) {
  IntrusiveHeapInt heap{CANONICAL_ELEMENTS};
  IntrusiveHeapInt heap2;
  swap(heap, heap2);
  EXPECT_TRUE(heap.empty());
  ExpectCanonical(heap2);
  heap.swap(heap2);
  EXPECT_TRUE(heap2.empty());
  ExpectCanonical(heap);
}

TEST(IntrusiveHeapTest, ElementAccess) {
  IntrusiveHeapInt heap{CANONICAL_ELEMENTS};
  EXPECT_EQ(heap.front(), heap[0]);
  EXPECT_EQ(heap.back(), heap[7]);
  EXPECT_EQ(heap.top(), heap[0]);

  // Canonical heap is stored as: 7 4 6 1 3 2 5 0
  EXPECT_EQ(heap[0].value(), 7);
  EXPECT_EQ(heap[1].value(), 4);
  EXPECT_EQ(heap[2].value(), 6);
  EXPECT_EQ(heap[3].value(), 1);
  EXPECT_EQ(heap[4].value(), 3);
  EXPECT_EQ(heap[5].value(), 2);
  EXPECT_EQ(heap[6].value(), 5);
  EXPECT_EQ(heap[7].value(), 0);
  EXPECT_EQ(heap.at(0).value(), 7);
  EXPECT_EQ(heap.at(1).value(), 4);
  EXPECT_EQ(heap.at(2).value(), 6);
  EXPECT_EQ(heap.at(3).value(), 1);
  EXPECT_EQ(heap.at(4).value(), 3);
  EXPECT_EQ(heap.at(5).value(), 2);
  EXPECT_EQ(heap.at(6).value(), 5);
  EXPECT_EQ(heap.at(7).value(), 0);
}

TEST(IntrusiveHeapTest, SizeManagement) {
  IntrusiveHeapInt heap;
  EXPECT_TRUE(heap.empty());
  EXPECT_LE(heap.size(), heap.capacity());

  MakeCanonical(&heap);
  EXPECT_FALSE(heap.empty());
  EXPECT_LE(heap.size(), heap.capacity());
}

TEST(IntrusiveHeapTest, Iterators) {
  IntrusiveHeapInt heap;
  MakeCanonical(&heap);

  // Canonical heap is stored as: 7 4 6 1 3 2 5 0
  {
    auto it = heap.begin();
    EXPECT_EQ(heap.ToIndex(it), 0u);
    EXPECT_EQ(it->value(), 7);
    EXPECT_NE(it, heap.end());
    ++it;
    EXPECT_EQ(heap.ToIndex(it), 1u);
    EXPECT_EQ(it->value(), 4);
    EXPECT_NE(it, heap.end());
    it += 6u;
    EXPECT_EQ(heap.ToIndex(it), 7u);
    EXPECT_EQ(it->value(), 0);
    EXPECT_NE(it, heap.end());
    ++it;
    EXPECT_EQ(it, heap.end());
  }

  {
    auto it = heap.rbegin();
    EXPECT_EQ(heap.ToIndex(it), 7u);
    EXPECT_EQ(it->value(), 0);
    EXPECT_NE(it, heap.rend());
    ++it;
    EXPECT_EQ(heap.ToIndex(it), 6u);
    EXPECT_EQ(it->value(), 5);
    EXPECT_NE(it, heap.rend());
    it += 6u;
    EXPECT_EQ(heap.ToIndex(it), 0u);
    EXPECT_EQ(it->value(), 7);
    EXPECT_NE(it, heap.rend());
    ++it;
    EXPECT_EQ(it, heap.rend());
  }
}

////////////////////////////////////////////////////////////////////////////////
// TEST SUITE 2
//
// Exhaustive stress tests with different value types, exploring all
// possibilities of default-constrible, movable and copyable value types.

TEST(IntrusiveHeapTest, MoveOnlyNoDefaultConstructorTest) {
  using ValueType = Value_dMc;
  ValidateValueType<ValueType, false, true, false>();
  MoveStressTest<ValueType>();
  GeneralStressTest<ValueType>();
}

TEST(IntrusiveHeapTest, CopyOnlyNoDefaultConstructorTest) {
  using ValueType = Value_dmC;
  ValidateValueType<ValueType, false, false, true>();
  // We cannot perform CopyStressTest nor GeneralStressTest here, because
  // Value_dmC has deleted move constructor and assignment operator. See
  // crbug.com/1022576.
}

TEST(IntrusiveHeapTest, CopyAndMoveNoDefaultConstructorTest) {
  using ValueType = Value_dMC;
  ValidateValueType<ValueType, false, true, true>();
  CopyStressTest<ValueType>();
  MoveStressTest<ValueType>();
  GeneralStressTest<ValueType>();
}

TEST(IntrusiveHeapTest, MoveOnlyWithDefaultConstructorTest) {
  using ValueType = Value_DMc;
  ValidateValueType<ValueType, true, true, false>();
  MoveStressTest<ValueType>();
  GeneralStressTest<ValueType>();
}

TEST(IntrusiveHeapTest, CopyOnlyWithDefaultConstructorTest) {
  using ValueType = Value_DmC;
  ValidateValueType<ValueType, true, false, true>();
  // We cannot perform CopyStressTest nor GeneralStressTest here, because
  // Value_DmC has deleted move constructor and assignment operator. See
  // crbug.com/1022576.
}

TEST(IntrusiveHeapTest, CopyAndMoveWithDefaultConstructorTest) {
  using ValueType = Value_DMC;
  ValidateValueType<ValueType, true, true, true>();
  CopyStressTest<ValueType>();
  MoveStressTest<ValueType>();
  GeneralStressTest<ValueType>();
}

////////////////////////////////////////////////////////////////////////////////
// TEST SUITE 3
//
// Tests individual functions on a custom type that provides heap handle storage
// externally through raw pointers.

TEST(IntrusiveHeapTest, Basic) {
  IntrusiveHeap<TestElement> heap;

  EXPECT_TRUE(heap.empty());
  EXPECT_EQ(0u, heap.size());
}

TEST(IntrusiveHeapTest, Clear) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index1;

  heap.insert({11, &index1});
  EXPECT_EQ(1u, heap.size());
  EXPECT_TRUE(index1.IsValid());

  heap.clear();
  EXPECT_EQ(0u, heap.size());
  EXPECT_FALSE(index1.IsValid());
}

TEST(IntrusiveHeapTest, Destructor) {
  HeapHandle index1;

  {
    IntrusiveHeap<TestElement> heap;

    heap.insert({11, &index1});
    EXPECT_EQ(1u, heap.size());
    EXPECT_TRUE(index1.IsValid());
  }

  EXPECT_FALSE(index1.IsValid());
}

TEST(IntrusiveHeapTest, Min) {
  IntrusiveHeap<TestElement> heap;

  heap.insert({9, nullptr});
  heap.insert({10, nullptr});
  heap.insert({8, nullptr});
  heap.insert({2, nullptr});
  heap.insert({7, nullptr});
  heap.insert({15, nullptr});
  heap.insert({22, nullptr});
  heap.insert({3, nullptr});

  EXPECT_FALSE(heap.empty());
  EXPECT_EQ(8u, heap.size());
  EXPECT_EQ(2, heap.top().key);
}

TEST(IntrusiveHeapTest, MinDuplicates) {
  IntrusiveHeap<TestElement> heap;

  heap.insert({2, nullptr});
  heap.insert({2, nullptr});
  heap.insert({3, nullptr});

  EXPECT_FALSE(heap.empty());
  EXPECT_EQ(3u, heap.size());
  EXPECT_EQ(2, heap.top().key);
}

TEST(IntrusiveHeapTest, InsertAscending) {
  IntrusiveHeap<TestElement> heap;

  std::array<TestElement, 50> arr = {};
  for (int i = 0; i < 50; i++) {
    arr[i].key = i;
  }
  heap.insert_range(arr);

  EXPECT_EQ(0, heap.top().key);
  EXPECT_EQ(50u, heap.size());
}

TEST(IntrusiveHeapTest, InsertDescending) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 50; i++)
    heap.insert({50 - i, nullptr});

  EXPECT_EQ(1, heap.top().key);
  EXPECT_EQ(50u, heap.size());
}

TEST(IntrusiveHeapTest, HeapIndex) {
  HeapHandle index5;
  HeapHandle index4;
  HeapHandle index3;
  HeapHandle index2;
  HeapHandle index1;
  IntrusiveHeap<TestElement> heap;

  EXPECT_FALSE(index1.IsValid());
  EXPECT_FALSE(index2.IsValid());
  EXPECT_FALSE(index3.IsValid());
  EXPECT_FALSE(index4.IsValid());
  EXPECT_FALSE(index5.IsValid());

  heap.insert({15, &index5});
  heap.insert({14, &index4});
  heap.insert({13, &index3});
  heap.insert({12, &index2});
  heap.insert({11, &index1});

  EXPECT_TRUE(index1.IsValid());
  EXPECT_TRUE(index2.IsValid());
  EXPECT_TRUE(index3.IsValid());
  EXPECT_TRUE(index4.IsValid());
  EXPECT_TRUE(index5.IsValid());

  EXPECT_FALSE(heap.empty());
}

TEST(IntrusiveHeapTest, HeapIndexDuplicates) {
  HeapHandle index2;
  HeapHandle index1;
  IntrusiveHeap<TestElement> heap;

  EXPECT_FALSE(index1.IsValid());
  EXPECT_FALSE(index2.IsValid());

  heap.insert({2, &index2});
  heap.insert({2, &index1});

  EXPECT_TRUE(index1.IsValid());
  EXPECT_TRUE(index2.IsValid());

  EXPECT_EQ(2U, heap.size());
}

TEST(IntrusiveHeapTest, Pop) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index1;
  HeapHandle index2;

  heap.insert({11, &index1});
  heap.insert({12, &index2});
  EXPECT_EQ(2u, heap.size());
  EXPECT_TRUE(index1.IsValid());
  EXPECT_TRUE(index2.IsValid());

  heap.pop();
  EXPECT_EQ(1u, heap.size());
  EXPECT_FALSE(index1.IsValid());
  EXPECT_TRUE(index2.IsValid());

  heap.pop();
  EXPECT_EQ(0u, heap.size());
  EXPECT_FALSE(index1.IsValid());
  EXPECT_FALSE(index2.IsValid());
}

TEST(IntrusiveHeapTest, PopMany) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 500; i++)
    heap.insert({i, nullptr});

  EXPECT_FALSE(heap.empty());
  EXPECT_EQ(500u, heap.size());
  for (int i = 0; i < 500; i++) {
    EXPECT_EQ(i, heap.top().key);
    heap.pop();
  }
  EXPECT_TRUE(heap.empty());
}

TEST(IntrusiveHeapTest, Erase) {
  IntrusiveHeap<TestElement> heap;

  HeapHandle index12;

  heap.insert({15, nullptr});
  heap.insert({14, nullptr});
  heap.insert({13, nullptr});
  heap.insert({12, &index12});
  heap.insert({11, nullptr});

  EXPECT_EQ(5u, heap.size());
  EXPECT_TRUE(index12.IsValid());
  heap.erase(index12);
  EXPECT_EQ(4u, heap.size());
  EXPECT_FALSE(index12.IsValid());

  EXPECT_EQ(11, heap.top().key);
  heap.pop();
  EXPECT_EQ(13, heap.top().key);
  heap.pop();
  EXPECT_EQ(14, heap.top().key);
  heap.pop();
  EXPECT_EQ(15, heap.top().key);
  heap.pop();
  EXPECT_TRUE(heap.empty());
}

TEST(IntrusiveHeapTest, ReplaceTop) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 500; i++)
    heap.insert({500 - i, nullptr});

  EXPECT_EQ(1, heap.top().key);

  for (int i = 0; i < 500; i++)
    heap.ReplaceTop({1000 + i, nullptr});

  EXPECT_EQ(1000, heap.top().key);
}

TEST(IntrusiveHeapTest, ReplaceTopWithNonLeafNode) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 50; i++) {
    heap.insert({i, nullptr});
    heap.insert({200 + i, nullptr});
  }

  EXPECT_EQ(0, heap.top().key);

  for (int i = 0; i < 50; i++)
    heap.ReplaceTop({100 + i, nullptr});

  for (int i = 0; i < 50; i++) {
    EXPECT_EQ((100 + i), heap.top().key);
    heap.pop();
  }
  for (int i = 0; i < 50; i++) {
    EXPECT_EQ((200 + i), heap.top().key);
    heap.pop();
  }
  EXPECT_TRUE(heap.empty());
}

TEST(IntrusiveHeapTest, ReplaceTopCheckAllFinalPositions) {
  std::array<HeapHandle, 100> index;
  HeapHandle top_index;

  for (int j = -1; j <= 201; j += 2) {
    IntrusiveHeap<TestElement> heap;
    for (size_t i = 0; i < 100; i++) {
      heap.insert({static_cast<int>(i) * 2, &index[i]});
    }

    heap.ReplaceTop({j, &top_index});

    int prev = -2;
    while (!heap.empty()) {
      DCHECK_GT(heap.top().key, prev);
      DCHECK(heap.top().key == j || (heap.top().key % 2) == 0);
      DCHECK_NE(heap.top().key, 0);
      prev = heap.top().key;
      heap.pop();
    }
  }
}

TEST(IntrusiveHeapTest, ReplaceUp) {
  IntrusiveHeap<TestElement> heap;
  std::array<HeapHandle, 10> index;

  for (size_t i = 0; i < 10; i++) {
    heap.insert({static_cast<int>(i) * 2, &index[i]});
  }

  heap.Replace(index[5], {17, &index[5]});

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.top().key);
    heap.pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(0, 2, 4, 6, 8, 12, 14, 16, 17, 18));
}

TEST(IntrusiveHeapTest, ReplaceUpButDoesntMove) {
  IntrusiveHeap<TestElement> heap;
  std::array<HeapHandle, 10> index;

  for (size_t i = 0; i < 10; i++) {
    heap.insert({static_cast<int>(i) * 2, &index[i]});
  }

  heap.Replace(index[5], {11, &index[5]});

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.top().key);
    heap.pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(0, 2, 4, 6, 8, 11, 12, 14, 16, 18));
}

TEST(IntrusiveHeapTest, ReplaceDown) {
  IntrusiveHeap<TestElement> heap;
  std::array<HeapHandle, 10> index;

  for (size_t i = 0; i < 10; i++) {
    heap.insert({static_cast<int>(i) * 2, &index[i]});
  }

  heap.Replace(index[5], {1, &index[5]});

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.top().key);
    heap.pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(0, 1, 2, 4, 6, 8, 12, 14, 16, 18));
}

TEST(IntrusiveHeapTest, ReplaceDownButDoesntMove) {
  IntrusiveHeap<TestElement> heap;
  std::array<HeapHandle, 10> index;

  for (size_t i = 0; i < 10; i++) {
    heap.insert({static_cast<int>(i) * 2, &index[i]});
  }

  heap.Replace(index[5], {9, &index[5]});

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.top().key);
    heap.pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(0, 2, 4, 6, 8, 9, 12, 14, 16, 18));
}

TEST(IntrusiveHeapTest, ReplaceCheckAllFinalPositions) {
  std::array<HeapHandle, 100> index;

  for (int j = -1; j <= 201; j += 2) {
    IntrusiveHeap<TestElement> heap;
    for (size_t i = 0; i < 100; i++) {
      heap.insert({static_cast<int>(i) * 2, &index[i]});
    }

    heap.Replace(index[40], {j, &index[40]});

    int prev = -2;
    while (!heap.empty()) {
      DCHECK_GT(heap.top().key, prev);
      DCHECK(heap.top().key == j || (heap.top().key % 2) == 0);
      DCHECK_NE(heap.top().key, 80);
      prev = heap.top().key;
      heap.pop();
    }
  }
}

TEST(IntrusiveHeapTest, At) {
  std::array<HeapHandle, 10> index;
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 10; i++)
    heap.insert({static_cast<int>(i ^ (i + 1)), &index[i]});

  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(heap.at(index[i]).key, i ^ (i + 1));
    EXPECT_EQ(heap.at(index[i]).handle, &index[i]);
  }
}

bool IsEven(int i) {
  return i % 2 == 0;
}

TEST(IntrusiveHeapTest, EraseIf) {
  std::array<HeapHandle, 10> index;
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 10; i++)
    heap.insert({i, &index[i]});
  ASSERT_EQ(heap.size(), 10u);

  // Remove all even elements.
  heap.EraseIf([](const TestElement& element) { return IsEven(element.key); });
  ASSERT_EQ(heap.size(), 5u);

  // Handles were correctly updated.
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(IsEven(i), !index[i].IsValid());

  // Now iterate over all elements of the heap and check their handles.
  for (size_t i = 0; i < heap.size(); i++) {
    auto it = heap.begin();
    std::advance(it, i);

    // Retrieve the value of the element at this position.
    int value = it->key;

    // Its handle should have the correct index.
    EXPECT_EQ(index[value].index(), i);
  }

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.top().key);
    heap.pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(1, 3, 5, 7, 9));
}

// A comparator class whose sole purpose is to allow the insertion of a
// ScopedClosureRunner inside the heap. The ordering does not matter.
class Comparator {
 public:
  bool operator()(const WithHeapHandle<ScopedClosureRunner>& lhs,
                  const WithHeapHandle<ScopedClosureRunner>& rhs) {
    // Treat all closures as equal.
    return true;
  }
};

// Tests that inserting another element from the destructor of an object removed
// during EraseIf() doesn't crash.
TEST(IntrusiveHeapTest, EraseIf_Reentrancy) {
  IntrusiveHeap<WithHeapHandle<ScopedClosureRunner>, Comparator> heap;

  // The task that will post a new element inside the heap upon destruction of
  // the first.
  OnceClosure insert_task = BindLambdaForTesting([&] {
    // Insert a null callback so it can be differentiated.
    heap.insert(OnceClosure());
  });
  heap.insert(ScopedClosureRunner(std::move(insert_task)));

  // The heap contains the non-null closure.
  EXPECT_EQ(heap.size(), 1u);
  EXPECT_TRUE(heap.top().value());

  // Erase the only element using EraseIf().
  heap.EraseIf([](const auto& element) { return true; });

  // Now the heap contains the null closure.
  EXPECT_EQ(heap.size(), 1u);
  EXPECT_FALSE(heap.top().value());
}

}  // namespace base
