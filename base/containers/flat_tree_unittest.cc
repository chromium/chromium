// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_tree.h"

// Following tests are ported and extended tests from libcpp for std::set.
// They can be found here:
// https://github.com/llvm-mirror/libcxx/tree/master/test/std/containers/associative/set
//
// Not ported tests:
// * No tests with PrivateConstructor and std::less<> changed to std::less<T>
//   These tests have to do with C++14 std::less<>
//   http://en.cppreference.com/w/cpp/utility/functional/less_void
//   and add support for templated versions of lookup functions.
//   Because we use same implementation, we figured that it's OK just to check
//   compilation and this is what we do in flat_set_unittest/flat_map_unittest.
// * No tests for max_size()
//   Has to do with allocator support.
// * No tests with DefaultOnly.
//   Standard containers allocate each element in the separate node on the heap
//   and then manipulate these nodes. Flat containers store their elements in
//   contiguous memory and move them around, type is required to be movable.
// * No tests for N3644.
//   This proposal suggests that all default constructed iterators compare
//   equal. Currently we use std::vector iterators and they don't implement
//   this.
// * No tests with min_allocator and no tests counting allocations.
//   Flat sets currently don't support allocators.

#include <forward_list>
#include <functional>
#include <iterator>
#include <list>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/template_util.h"
#include "base/test/move_only_int.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

template <class It>
class InputIterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = typename std::iterator_traits<It>::value_type;
  using difference_type = typename std::iterator_traits<It>::difference_type;
  using pointer = It;
  using reference = typename std::iterator_traits<It>::reference;

  InputIterator() : it_() {}
  explicit InputIterator(It it) : it_(it) {}

  reference operator*() const { return *it_; }
  pointer operator->() const { return it_; }

  InputIterator& operator++() {
    ++it_;
    return *this;
  }
  InputIterator operator++(int) {
    InputIterator tmp(*this);
    ++(*this);
    return tmp;
  }

  friend bool operator==(const InputIterator& lhs, const InputIterator& rhs) {
    return lhs.it_ == rhs.it_;
  }
  friend bool operator!=(const InputIterator& lhs, const InputIterator& rhs) {
    return !(lhs == rhs);
  }

 private:
  It it_;
};

template <typename It>
InputIterator<It> MakeInputIterator(It it) {
  return InputIterator<It>(it);
}

class Emplaceable {
 public:
  Emplaceable() : Emplaceable(0, 0.0) {}
  Emplaceable(int i, double d) : int_(i), double_(d) {}
  Emplaceable(Emplaceable&& other) : int_(other.int_), double_(other.double_) {
    other.int_ = 0;
    other.double_ = 0.0;
  }

  Emplaceable& operator=(Emplaceable&& other) {
    int_ = other.int_;
    other.int_ = 0;
    double_ = other.double_;
    other.double_ = 0.0;
    return *this;
  }

  friend bool operator==(const Emplaceable& lhs, const Emplaceable& rhs) {
    return std::tie(lhs.int_, lhs.double_) == std::tie(rhs.int_, rhs.double_);
  }

  friend bool operator<(const Emplaceable& lhs, const Emplaceable& rhs) {
    return std::tie(lhs.int_, lhs.double_) < std::tie(rhs.int_, rhs.double_);
  }

 private:
  int int_;
  double double_;

  DISALLOW_COPY_AND_ASSIGN(Emplaceable);
};

struct TemplateConstructor {
  template <typename T>
  TemplateConstructor(const T&) {}

  friend bool operator<(const TemplateConstructor&,
                        const TemplateConstructor&) {
    return false;
  }
};

class NonDefaultConstructibleCompare {
 public:
  explicit NonDefaultConstructibleCompare(int) {}

  template <typename T>
  bool operator()(const T& lhs, const T& rhs) const {
    return std::less<T>()(lhs, rhs);
  }
};

template <class PairType>
struct LessByFirst {
  bool operator()(const PairType& lhs, const PairType& rhs) const {
    return lhs.first < rhs.first;
  }
};

// Common test trees.
using IntTree =
    flat_tree<int, int, GetKeyFromValueIdentity<int>, std::less<int>>;
using IntPair = std::pair<int, int>;
using IntPairTree = flat_tree<IntPair,
                              IntPair,
                              GetKeyFromValueIdentity<IntPair>,
                              LessByFirst<IntPair>>;
using MoveOnlyTree = flat_tree<MoveOnlyInt,
                               MoveOnlyInt,
                               GetKeyFromValueIdentity<MoveOnlyInt>,
                               std::less<MoveOnlyInt>>;
using EmplaceableTree = flat_tree<Emplaceable,
                                  Emplaceable,
                                  GetKeyFromValueIdentity<Emplaceable>,
                                  std::less<Emplaceable>>;
using ReversedTree =
    flat_tree<int, int, GetKeyFromValueIdentity<int>, std::greater<int>>;

using TreeWithStrangeCompare = flat_tree<int,
                                         int,
                                         GetKeyFromValueIdentity<int>,
                                         NonDefaultConstructibleCompare>;

using ::testing::ElementsAre;

}  // namespace

TEST(FlatTree, IsMultipass) {
  static_assert(!is_multipass<std::istream_iterator<int>>(),
                "InputIterator is not multipass");
  static_assert(!is_multipass<std::ostream_iterator<int>>(),
                "OutputIterator is not multipass");

  static_assert(is_multipass<std::forward_list<int>::iterator>(),
                "ForwardIterator is multipass");
  static_assert(is_multipass<std::list<int>::iterator>(),
                "BidirectionalIterator is multipass");
  static_assert(is_multipass<std::vector<int>::iterator>(),
                "RandomAccessIterator is multipass");
}

// ----------------------------------------------------------------------------
// Class.

// Check that flat_tree and its iterators can be instantiated with an
// incomplete type.

TEST(FlatTree, IncompleteType) {
  struct A {
    using Tree = flat_tree<A, A, GetKeyFromValueIdentity<A>, std::less<A>>;
    int data;
    Tree set_with_incomplete_type;
    Tree::iterator it;
    Tree::const_iterator cit;

    // We do not declare operator< because clang complains that it's unused.
  };

  A a;
}

TEST(FlatTree, Stability) {
  using Pair = std::pair<int, int>;

  using Tree =
      flat_tree<Pair, Pair, GetKeyFromValueIdentity<Pair>, LessByFirst<Pair>>;

  // Constructors are stable.
  Tree cont({{0, 0}, {1, 0}, {0, 1}, {2, 0}, {0, 2}, {1, 1}});

  auto AllOfSecondsAreZero = [&cont] {
    return std::all_of(cont.begin(), cont.end(),
                       [](const Pair& elem) { return elem.second == 0; });
  };

  EXPECT_TRUE(AllOfSecondsAreZero()) << "constructor should be stable";

  // Should not replace existing.
  cont.insert(Pair(0, 2));
  cont.insert(Pair(1, 2));
  cont.insert(Pair(2, 2));

  EXPECT_TRUE(AllOfSecondsAreZero()) << "insert should be stable";

  cont.insert(Pair(3, 0));
  cont.insert(Pair(3, 2));

  EXPECT_TRUE(AllOfSecondsAreZero()) << "insert should be stable";
}

// ----------------------------------------------------------------------------
// Types.

// key_type
// key_compare
// value_type
// value_compare
// pointer
// const_pointer
// reference
// const_reference
// size_type
// difference_type
// iterator
// const_iterator
// reverse_iterator
// const_reverse_iterator

TEST(FlatTree, Types) {
  // These are guaranteed to be portable.
  static_assert((std::is_same<int, IntTree::key_type>::value), "");
  static_assert((std::is_same<int, IntTree::value_type>::value), "");
  static_assert((std::is_same<std::less<int>, IntTree::key_compare>::value),
                "");
  static_assert((std::is_same<int&, IntTree::reference>::value), "");
  static_assert((std::is_same<const int&, IntTree::const_reference>::value),
                "");
  static_assert((std::is_same<int*, IntTree::pointer>::value), "");
  static_assert((std::is_same<const int*, IntTree::const_pointer>::value), "");
}

// ----------------------------------------------------------------------------
// Lifetime.

// flat_tree()
// flat_tree(const Compare& comp)

TEST(FlatTree, DefaultConstructor) {
  {
    IntTree cont;
    EXPECT_THAT(cont, ElementsAre());
  }

  {
    TreeWithStrangeCompare cont(NonDefaultConstructibleCompare(0));
    EXPECT_THAT(cont, ElementsAre());
  }
}

// flat_tree(InputIterator first,
//           InputIterator last,
//           const Compare& comp = Compare())

TEST(FlatTree, RangeConstructor) {
  {
    IntPair input_vals[] = {{1, 1}, {1, 2}, {2, 1}, {2, 2}, {1, 3},
                            {2, 3}, {3, 1}, {3, 2}, {3, 3}};

    IntPairTree first_of(MakeInputIterator(std::begin(input_vals)),
                         MakeInputIterator(std::end(input_vals)));
    EXPECT_THAT(first_of,
                ElementsAre(IntPair(1, 1), IntPair(2, 1), IntPair(3, 1)));
  }
  {
    TreeWithStrangeCompare::value_type input_vals[] = {1, 1, 1, 2, 2,
                                                       2, 3, 3, 3};

    TreeWithStrangeCompare cont(MakeInputIterator(std::begin(input_vals)),
                                MakeInputIterator(std::end(input_vals)),
                                NonDefaultConstructibleCompare(0));
    EXPECT_THAT(cont, ElementsAre(1, 2, 3));
  }
}

// flat_tree(const flat_tree& x)

TEST(FlatTree, CopyConstructor) {
  IntTree original({1, 2, 3, 4});
  IntTree copied(original);

  EXPECT_THAT(copied, ElementsAre(1, 2, 3, 4));

  EXPECT_THAT(copied, ElementsAre(1, 2, 3, 4));
  EXPECT_THAT(original, ElementsAre(1, 2, 3, 4));
  EXPECT_EQ(original, copied);
}

// flat_tree(flat_tree&& x)

TEST(FlatTree, MoveConstructor) {
  int input_range[] = {1, 2, 3, 4};

  MoveOnlyTree original(std::begin(input_range), std::end(input_range));
  MoveOnlyTree moved(std::move(original));

  EXPECT_EQ(1U, moved.count(MoveOnlyInt(1)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(2)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(3)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(4)));
}

// flat_tree(std::vector<value_type>)

TEST(FlatTree, VectorConstructor) {
  using Pair = std::pair<int, MoveOnlyInt>;

  // Construct an unsorted vector with a duplicate item in it. Sorted by the
  // first item, the second allows us to test for stability. Using a move
  // only type to ensure the vector is not copied.
  std::vector<Pair> storage;
  storage.push_back(Pair(2, MoveOnlyInt(0)));
  storage.push_back(Pair(1, MoveOnlyInt(0)));
  storage.push_back(Pair(2, MoveOnlyInt(1)));

  using Tree =
      flat_tree<Pair, Pair, GetKeyFromValueIdentity<Pair>, LessByFirst<Pair>>;
  Tree tree(std::move(storage));

  // The list should be two items long, with only the first "2" saved.
  ASSERT_EQ(2u, tree.size());
  const Pair& zeroth = *tree.begin();
  ASSERT_EQ(1, zeroth.first);
  ASSERT_EQ(0, zeroth.second.data());

  const Pair& first = *(tree.begin() + 1);
  ASSERT_EQ(2, first.first);
  ASSERT_EQ(0, first.second.data());
}

// flat_tree(std::initializer_list<value_type> ilist,
//           const Compare& comp = Compare())

TEST(FlatTree, InitializerListConstructor) {
  {
    IntTree cont({1, 2, 3, 4, 5, 6, 10, 8});
    EXPECT_THAT(cont, ElementsAre(1, 2, 3, 4, 5, 6, 8, 10));
  }
  {
    IntTree cont({1, 2, 3, 4, 5, 6, 10, 8});
    EXPECT_THAT(cont, ElementsAre(1, 2, 3, 4, 5, 6, 8, 10));
  }
  {
    TreeWithStrangeCompare cont({1, 2, 3, 4, 5, 6, 10, 8},
                                NonDefaultConstructibleCompare(0));
    EXPECT_THAT(cont, ElementsAre(1, 2, 3, 4, 5, 6, 8, 10));
  }
  {
    IntPairTree first_of({{1, 1}, {2, 1}, {1, 2}});
    EXPECT_THAT(first_of, ElementsAre(IntPair(1, 1), IntPair(2, 1)));
  }
}

// ----------------------------------------------------------------------------
// Assignments.

// flat_tree& operator=(const flat_tree&)

TEST(FlatTree, CopyAssignable) {
  IntTree original({1, 2, 3, 4});
  IntTree copied;
  copied = original;

  EXPECT_THAT(copied, ElementsAre(1, 2, 3, 4));
  EXPECT_THAT(original, ElementsAre(1, 2, 3, 4));
  EXPECT_EQ(original, copied);
}

// flat_tree& operator=(flat_tree&&)

TEST(FlatTree, MoveAssignable) {
  int input_range[] = {1, 2, 3, 4};

  MoveOnlyTree original(std::begin(input_range), std::end(input_range));
  MoveOnlyTree moved;
  moved = std::move(original);

  EXPECT_EQ(1U, moved.count(MoveOnlyInt(1)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(2)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(3)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(4)));
}

// flat_tree& operator=(std::initializer_list<value_type> ilist)

TEST(FlatTree, InitializerListAssignable) {
  IntTree cont({0});
  cont = {1, 2, 3, 4, 5, 6, 10, 8};

  EXPECT_EQ(0U, cont.count(0));
  EXPECT_THAT(cont, ElementsAre(1, 2, 3, 4, 5, 6, 8, 10));
}

// --------------------------------------------------------------------------
// Memory management.

// void reserve(size_type new_capacity)

TEST(FlatTree, Reserve) {
  IntTree cont({1, 2, 3});

  cont.reserve(5);
  EXPECT_LE(5U, cont.capacity());
}

// size_type capacity() const

TEST(FlatTree, Capacity) {
  IntTree cont({1, 2, 3});

  EXPECT_LE(cont.size(), cont.capacity());
  cont.reserve(5);
  EXPECT_LE(cont.size(), cont.capacity());
}

// void shrink_to_fit()

TEST(FlatTree, ShrinkToFit) {
  IntTree cont({1, 2, 3});

  IntTree::size_type capacity_before = cont.capacity();
  cont.shrink_to_fit();
  EXPECT_GE(capacity_before, cont.capacity());
}

// ----------------------------------------------------------------------------
// Size management.

// void clear()

TEST(FlatTree, Clear) {
  IntTree cont({1, 2, 3, 4, 5, 6, 7, 8});
  cont.clear();
  EXPECT_THAT(cont, ElementsAre());
}

// size_type size() const

TEST(FlatTree, Size) {
  IntTree cont;

  EXPECT_EQ(0U, cont.size());
  cont.insert(2);
  EXPECT_EQ(1U, cont.size());
  cont.insert(1);
  EXPECT_EQ(2U, cont.size());
  cont.insert(3);
  EXPECT_EQ(3U, cont.size());
  cont.erase(cont.begin());
  EXPECT_EQ(2U, cont.size());
  cont.erase(cont.begin());
  EXPECT_EQ(1U, cont.size());
  cont.erase(cont.begin());
  EXPECT_EQ(0U, cont.size());
}

// bool empty() const

TEST(FlatTree, Empty) {
  IntTree cont;

  EXPECT_TRUE(cont.empty());
  cont.insert(1);
  EXPECT_FALSE(cont.empty());
  cont.clear();
  EXPECT_TRUE(cont.empty());
}

// ----------------------------------------------------------------------------
// Iterators.

// iterator begin()
// const_iterator begin() const
// iterator end()
// const_iterator end() const
//
// reverse_iterator rbegin()
// const_reverse_iterator rbegin() const
// reverse_iterator rend()
// const_reverse_iterator rend() const
//
// const_iterator cbegin() const
// const_iterator cend() const
// const_reverse_iterator crbegin() const
// const_reverse_iterator crend() const

TEST(FlatTree, Iterators) {
  IntTree cont({1, 2, 3, 4, 5, 6, 7, 8});

  auto size = static_cast<IntTree::difference_type>(cont.size());

  EXPECT_EQ(size, std::distance(cont.begin(), cont.end()));
  EXPECT_EQ(size, std::distance(cont.cbegin(), cont.cend()));
  EXPECT_EQ(size, std::distance(cont.rbegin(), cont.rend()));
  EXPECT_EQ(size, std::distance(cont.crbegin(), cont.crend()));

  {
    auto it = cont.begin();
    auto c_it = cont.cbegin();
    EXPECT_EQ(it, c_it);
    for (int j = 1; it != cont.end(); ++it, ++c_it, ++j) {
      EXPECT_EQ(j, *it);
      EXPECT_EQ(j, *c_it);
    }
  }
  {
    auto rit = cont.rbegin();
    auto c_rit = cont.crbegin();
    EXPECT_EQ(rit, c_rit);
    for (int j = static_cast<int>(size); rit != cont.rend();
         ++rit, ++c_rit, --j) {
      EXPECT_EQ(j, *rit);
      EXPECT_EQ(j, *c_rit);
    }
  }
}

// ----------------------------------------------------------------------------
// Insert operations.

// pair<iterator, bool> insert(const value_type& val)

TEST(FlatTree, InsertLValue) {
  IntTree cont;

  int value = 2;
  std::pair<IntTree::iterator, bool> result = cont.insert(value);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(cont.begin(), result.first);
  EXPECT_EQ(1U, cont.size());
  EXPECT_EQ(2, *result.first);

  value = 1;
  result = cont.insert(value);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(cont.begin(), result.first);
  EXPECT_EQ(2U, cont.size());
  EXPECT_EQ(1, *result.first);

  value = 3;
  result = cont.insert(value);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(std::prev(cont.end()), result.first);
  EXPECT_EQ(3U, cont.size());
  EXPECT_EQ(3, *result.first);

  value = 3;
  result = cont.insert(value);
  EXPECT_FALSE(result.second);
  EXPECT_EQ(std::prev(cont.end()), result.first);
  EXPECT_EQ(3U, cont.size());
  EXPECT_EQ(3, *result.first);
}

// pair<iterator, bool> insert(value_type&& val)

TEST(FlatTree, InsertRValue) {
  MoveOnlyTree cont;

  std::pair<MoveOnlyTree::iterator, bool> result = cont.insert(MoveOnlyInt(2));
  EXPECT_TRUE(result.second);
  EXPECT_EQ(cont.begin(), result.first);
  EXPECT_EQ(1U, cont.size());
  EXPECT_EQ(2, result.first->data());

  result = cont.insert(MoveOnlyInt(1));
  EXPECT_TRUE(result.second);
  EXPECT_EQ(cont.begin(), result.first);
  EXPECT_EQ(2U, cont.size());
  EXPECT_EQ(1, result.first->data());

  result = cont.insert(MoveOnlyInt(3));
  EXPECT_TRUE(result.second);
  EXPECT_EQ(std::prev(cont.end()), result.first);
  EXPECT_EQ(3U, cont.size());
  EXPECT_EQ(3, result.first->data());

  result = cont.insert(MoveOnlyInt(3));
  EXPECT_FALSE(result.second);
  EXPECT_EQ(std::prev(cont.end()), result.first);
  EXPECT_EQ(3U, cont.size());
  EXPECT_EQ(3, result.first->data());
}

// iterator insert(const_iterator position_hint, const value_type& val)

TEST(FlatTree, InsertPositionLValue) {
  IntTree cont;

  auto result = cont.insert(cont.cend(), 2);
  EXPECT_EQ(cont.begin(), result);
  EXPECT_EQ(1U, cont.size());
  EXPECT_EQ(2, *result);

  result = cont.insert(cont.cend(), 1);
  EXPECT_EQ(cont.begin(), result);
  EXPECT_EQ(2U, cont.size());
  EXPECT_EQ(1, *result);

  result = cont.insert(cont.cend(), 3);
  EXPECT_EQ(std::prev(cont.end()), result);
  EXPECT_EQ(3U, cont.size());
  EXPECT_EQ(3, *result);

  result = cont.insert(cont.cend(), 3);
  EXPECT_EQ(std::prev(cont.end()), result);
  EXPECT_EQ(3U, cont.size());
  EXPECT_EQ(3, *result);
}

// iterator insert(const_iterator position_hint, value_type&& val)

TEST(FlatTree, InsertPositionRValue) {
  MoveOnlyTree cont;

  auto result = cont.insert(cont.cend(), MoveOnlyInt(2));
  EXPECT_EQ(cont.begin(), result);
  EXPECT_EQ(1U, cont.size());
  EXPECT_EQ(2, result->data());

  result = cont.insert(cont.cend(), MoveOnlyInt(1));
  EXPECT_EQ(cont.begin(), result);
  EXPECT_EQ(2U, cont.size());
  EXPECT_EQ(1, result->data());

  result = cont.insert(cont.cend(), MoveOnlyInt(3));
  EXPECT_EQ(std::prev(cont.end()), result);
  EXPECT_EQ(3U, cont.size());
  EXPECT_EQ(3, result->data());

  result = cont.insert(cont.cend(), MoveOnlyInt(3));
  EXPECT_EQ(std::prev(cont.end()), result);
  EXPECT_EQ(3U, cont.size());
  EXPECT_EQ(3, result->data());
}

// template <class InputIterator>
//   void insert(InputIterator first, InputIterator last);

TEST(FlatTree, InsertIterIter) {
  struct GetKeyFromIntIntPair {
    const int& operator()(const std::pair<int, int>& p) const {
      return p.first;
    }
  };

  using IntIntMap =
      flat_tree<int, IntPair, GetKeyFromIntIntPair, std::less<int>>;

  {
    IntIntMap cont;
    IntPair int_pairs[] = {{3, 1}, {1, 1}, {4, 1}, {2, 1}};
    cont.insert(std::begin(int_pairs), std::end(int_pairs));
    EXPECT_THAT(cont, ElementsAre(IntPair(1, 1), IntPair(2, 1), IntPair(3, 1),
                                  IntPair(4, 1)));
  }

  {
    IntIntMap cont({{1, 1}, {2, 1}, {3, 1}, {4, 1}});
    std::vector<IntPair> int_pairs;
    cont.insert(std::begin(int_pairs), std::end(int_pairs));
    EXPECT_THAT(cont, ElementsAre(IntPair(1, 1), IntPair(2, 1), IntPair(3, 1),
                                  IntPair(4, 1)));
  }

  {
    IntIntMap cont({{1, 1}, {2, 1}, {3, 1}, {4, 1}});
    IntPair int_pairs[] = {{1, 1}};
    cont.insert(std::begin(int_pairs), std::end(int_pairs));
    EXPECT_THAT(cont, ElementsAre(IntPair(1, 1), IntPair(2, 1), IntPair(3, 1),
                                  IntPair(4, 1)));
  }

  {
    IntIntMap cont({{1, 1}, {2, 1}, {3, 1}, {4, 1}});
    IntPair int_pairs[] = {{5, 1}};
    cont.insert(std::begin(int_pairs), std::end(int_pairs));
    EXPECT_THAT(cont, ElementsAre(IntPair(1, 1), IntPair(2, 1), IntPair(3, 1),
                                  IntPair(4, 1), IntPair(5, 1)));
  }

  {
    IntIntMap cont({{1, 1}, {2, 1}, {3, 1}, {4, 1}});
    IntPair int_pairs[] = {{3, 2}, {1, 2}, {4, 2}, {2, 2}};
    cont.insert(std::begin(int_pairs), std::end(int_pairs));
    EXPECT_THAT(cont, ElementsAre(IntPair(1, 1), IntPair(2, 1), IntPair(3, 1),
                                  IntPair(4, 1)));
  }

  {
    IntIntMap cont({{1, 1}, {2, 1}, {3, 1}, {4, 1}});
    IntPair int_pairs[] = {{3, 2}, {1, 2}, {4, 2}, {2, 2}, {7, 2}, {6, 2},
                           {8, 2}, {5, 2}, {5, 3}, {6, 3}, {7, 3}, {8, 3}};
    cont.insert(std::begin(int_pairs), std::end(int_pairs));
    EXPECT_THAT(cont, ElementsAre(IntPair(1, 1), IntPair(2, 1), IntPair(3, 1),
                                  IntPair(4, 1), IntPair(5, 2), IntPair(6, 2),
                                  IntPair(7, 2), IntPair(8, 2)));
  }
}

// template <class... Args>
// pair<iterator, bool> emplace(Args&&... args)

TEST(FlatTree, Emplace) {
  {
    EmplaceableTree cont;

    std::pair<EmplaceableTree::iterator, bool> result = cont.emplace();
    EXPECT_TRUE(result.second);
    EXPECT_EQ(cont.begin(), result.first);
    EXPECT_EQ(1U, cont.size());
    EXPECT_EQ(Emplaceable(), *cont.begin());

    result = cont.emplace(2, 3.5);
    EXPECT_TRUE(result.second);
    EXPECT_EQ(std::next(cont.begin()), result.first);
    EXPECT_EQ(2U, cont.size());
    EXPECT_EQ(Emplaceable(2, 3.5), *result.first);

    result = cont.emplace(2, 3.5);
    EXPECT_FALSE(result.second);
    EXPECT_EQ(std::next(cont.begin()), result.first);
    EXPECT_EQ(2U, cont.size());
    EXPECT_EQ(Emplaceable(2, 3.5), *result.first);
  }
  {
    IntTree cont;

    std::pair<IntTree::iterator, bool> result = cont.emplace(2);
    EXPECT_TRUE(result.second);
    EXPECT_EQ(cont.begin(), result.first);
    EXPECT_EQ(1U, cont.size());
    EXPECT_EQ(2, *result.first);
  }
}

// template <class... Args>
// iterator emplace_hint(const_iterator position_hint, Args&&... args)

TEST(FlatTree, EmplacePosition) {
  {
    EmplaceableTree cont;

    auto result = cont.emplace_hint(cont.cend());
    EXPECT_EQ(cont.begin(), result);
    EXPECT_EQ(1U, cont.size());
    EXPECT_EQ(Emplaceable(), *cont.begin());

    result = cont.emplace_hint(cont.cend(), 2, 3.5);
    EXPECT_EQ(std::next(cont.begin()), result);
    EXPECT_EQ(2U, cont.size());
    EXPECT_EQ(Emplaceable(2, 3.5), *result);

    result = cont.emplace_hint(cont.cbegin(), 2, 3.5);
    EXPECT_EQ(std::next(cont.begin()), result);
    EXPECT_EQ(2U, cont.size());
    EXPECT_EQ(Emplaceable(2, 3.5), *result);
  }
  {
    IntTree cont;

    auto result = cont.emplace_hint(cont.cend(), 2);
    EXPECT_EQ(cont.begin(), result);
    EXPECT_EQ(1U, cont.size());
    EXPECT_EQ(2, *result);
  }
}

// ----------------------------------------------------------------------------
// Erase operations.

// iterator erase(const_iterator position_hint)

TEST(FlatTree, ErasePosition) {
  {
    IntTree cont({1, 2, 3, 4, 5, 6, 7, 8});

    auto it = cont.erase(std::next(cont.cbegin(), 3));
    EXPECT_EQ(std::next(cont.begin(), 3), it);
    EXPECT_THAT(cont, ElementsAre(1, 2, 3, 5, 6, 7, 8));

    it = cont.erase(std::next(cont.cbegin(), 0));
    EXPECT_EQ(cont.begin(), it);
    EXPECT_THAT(cont, ElementsAre(2, 3, 5, 6, 7, 8));

    it = cont.erase(std::next(cont.cbegin(), 5));
    EXPECT_EQ(cont.end(), it);
    EXPECT_THAT(cont, ElementsAre(2, 3, 5, 6, 7));

    it = cont.erase(std::next(cont.cbegin(), 1));
    EXPECT_EQ(std::next(cont.begin()), it);
    EXPECT_THAT(cont, ElementsAre(2, 5, 6, 7));

    it = cont.erase(std::next(cont.cbegin(), 2));
    EXPECT_EQ(std::next(cont.begin(), 2), it);
    EXPECT_THAT(cont, ElementsAre(2, 5, 7));

    it = cont.erase(std::next(cont.cbegin(), 2));
    EXPECT_EQ(std::next(cont.begin(), 2), it);
    EXPECT_THAT(cont, ElementsAre(2, 5));

    it = cont.erase(std::next(cont.cbegin(), 0));
    EXPECT_EQ(std::next(cont.begin(), 0), it);
    EXPECT_THAT(cont, ElementsAre(5));

    it = cont.erase(cont.cbegin());
    EXPECT_EQ(cont.begin(), it);
    EXPECT_EQ(cont.end(), it);
  }
  //  This is LWG #2059.
  //  There is a potential ambiguity between erase with an iterator and erase
  //  with a key, if key has a templated constructor.
  {
    using T = TemplateConstructor;

    flat_tree<T, T, GetKeyFromValueIdentity<T>, std::less<>> cont;
    T v(0);

    auto it = cont.find(v);
    if (it != cont.end())
      cont.erase(it);
  }
}

// iterator erase(const_iterator first, const_iterator last)

TEST(FlatTree, EraseRange) {
  IntTree cont({1, 2, 3, 4, 5, 6, 7, 8});

  auto it =
      cont.erase(std::next(cont.cbegin(), 5), std::next(cont.cbegin(), 5));
  EXPECT_EQ(std::next(cont.begin(), 5), it);
  EXPECT_THAT(cont, ElementsAre(1, 2, 3, 4, 5, 6, 7, 8));

  it = cont.erase(std::next(cont.cbegin(), 3), std::next(cont.cbegin(), 4));
  EXPECT_EQ(std::next(cont.begin(), 3), it);
  EXPECT_THAT(cont, ElementsAre(1, 2, 3, 5, 6, 7, 8));

  it = cont.erase(std::next(cont.cbegin(), 2), std::next(cont.cbegin(), 5));
  EXPECT_EQ(std::next(cont.begin(), 2), it);
  EXPECT_THAT(cont, ElementsAre(1, 2, 7, 8));

  it = cont.erase(std::next(cont.cbegin(), 0), std::next(cont.cbegin(), 2));
  EXPECT_EQ(std::next(cont.begin(), 0), it);
  EXPECT_THAT(cont, ElementsAre(7, 8));

  it = cont.erase(cont.cbegin(), cont.cend());
  EXPECT_EQ(cont.begin(), it);
  EXPECT_EQ(cont.end(), it);
}

// size_type erase(const key_type& key)

TEST(FlatTree, EraseKey) {
  IntTree cont({1, 2, 3, 4, 5, 6, 7, 8});

  EXPECT_EQ(0U, cont.erase(9));
  EXPECT_THAT(cont, ElementsAre(1, 2, 3, 4, 5, 6, 7, 8));

  EXPECT_EQ(1U, cont.erase(4));
  EXPECT_THAT(cont, ElementsAre(1, 2, 3, 5, 6, 7, 8));

  EXPECT_EQ(1U, cont.erase(1));
  EXPECT_THAT(cont, ElementsAre(2, 3, 5, 6, 7, 8));

  EXPECT_EQ(1U, cont.erase(8));
  EXPECT_THAT(cont, ElementsAre(2, 3, 5, 6, 7));

  EXPECT_EQ(1U, cont.erase(3));
  EXPECT_THAT(cont, ElementsAre(2, 5, 6, 7));

  EXPECT_EQ(1U, cont.erase(6));
  EXPECT_THAT(cont, ElementsAre(2, 5, 7));

  EXPECT_EQ(1U, cont.erase(7));
  EXPECT_THAT(cont, ElementsAre(2, 5));

  EXPECT_EQ(1U, cont.erase(2));
  EXPECT_THAT(cont, ElementsAre(5));

  EXPECT_EQ(1U, cont.erase(5));
  EXPECT_THAT(cont, ElementsAre());
}

// ----------------------------------------------------------------------------
// Comparators.

// key_compare key_comp() const

TEST(FlatTree, KeyComp) {
  ReversedTree cont({1, 2, 3, 4, 5});

  EXPECT_TRUE(std::is_sorted(cont.begin(), cont.end(), cont.key_comp()));
  int new_elements[] = {6, 7, 8, 9, 10};
  std::copy(std::begin(new_elements), std::end(new_elements),
            std::inserter(cont, cont.end()));
  EXPECT_TRUE(std::is_sorted(cont.begin(), cont.end(), cont.key_comp()));
}

// value_compare value_comp() const

TEST(FlatTree, ValueComp) {
  ReversedTree cont({1, 2, 3, 4, 5});

  EXPECT_TRUE(std::is_sorted(cont.begin(), cont.end(), cont.value_comp()));
  int new_elements[] = {6, 7, 8, 9, 10};
  std::copy(std::begin(new_elements), std::end(new_elements),
            std::inserter(cont, cont.end()));
  EXPECT_TRUE(std::is_sorted(cont.begin(), cont.end(), cont.value_comp()));
}

// ----------------------------------------------------------------------------
// Search operations.

// size_type count(const key_type& key) const

TEST(FlatTree, Count) {
  const IntTree cont({5, 6, 7, 8, 9, 10, 11, 12});

  EXPECT_EQ(1U, cont.count(5));
  EXPECT_EQ(1U, cont.count(6));
  EXPECT_EQ(1U, cont.count(7));
  EXPECT_EQ(1U, cont.count(8));
  EXPECT_EQ(1U, cont.count(9));
  EXPECT_EQ(1U, cont.count(10));
  EXPECT_EQ(1U, cont.count(11));
  EXPECT_EQ(1U, cont.count(12));
  EXPECT_EQ(0U, cont.count(4));
}

// iterator find(const key_type& key)
// const_iterator find(const key_type& key) const

TEST(FlatTree, Find) {
  {
    IntTree cont({5, 6, 7, 8, 9, 10, 11, 12});

    EXPECT_EQ(cont.begin(), cont.find(5));
    EXPECT_EQ(std::next(cont.begin()), cont.find(6));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.find(7));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.find(8));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.find(9));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.find(10));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.find(11));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.find(12));
    EXPECT_EQ(std::next(cont.begin(), 8), cont.find(4));
  }
  {
    const IntTree cont({5, 6, 7, 8, 9, 10, 11, 12});

    EXPECT_EQ(cont.begin(), cont.find(5));
    EXPECT_EQ(std::next(cont.begin()), cont.find(6));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.find(7));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.find(8));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.find(9));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.find(10));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.find(11));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.find(12));
    EXPECT_EQ(std::next(cont.begin(), 8), cont.find(4));
  }
}

// bool contains(const key_type& key) const

TEST(FlatTree, Contains) {
  const IntTree cont({5, 6, 7, 8, 9, 10, 11, 12});

  EXPECT_TRUE(cont.contains(5));
  EXPECT_TRUE(cont.contains(6));
  EXPECT_TRUE(cont.contains(7));
  EXPECT_TRUE(cont.contains(8));
  EXPECT_TRUE(cont.contains(9));
  EXPECT_TRUE(cont.contains(10));
  EXPECT_TRUE(cont.contains(11));
  EXPECT_TRUE(cont.contains(12));
  EXPECT_FALSE(cont.contains(4));
}

// pair<iterator, iterator> equal_range(const key_type& key)
// pair<const_iterator, const_iterator> equal_range(const key_type& key) const

TEST(FlatTree, EqualRange) {
  {
    IntTree cont({5, 7, 9, 11, 13, 15, 17, 19});

    std::pair<IntTree::iterator, IntTree::iterator> result =
        cont.equal_range(5);
    EXPECT_EQ(std::next(cont.begin(), 0), result.first);
    EXPECT_EQ(std::next(cont.begin(), 1), result.second);
    result = cont.equal_range(7);
    EXPECT_EQ(std::next(cont.begin(), 1), result.first);
    EXPECT_EQ(std::next(cont.begin(), 2), result.second);
    result = cont.equal_range(9);
    EXPECT_EQ(std::next(cont.begin(), 2), result.first);
    EXPECT_EQ(std::next(cont.begin(), 3), result.second);
    result = cont.equal_range(11);
    EXPECT_EQ(std::next(cont.begin(), 3), result.first);
    EXPECT_EQ(std::next(cont.begin(), 4), result.second);
    result = cont.equal_range(13);
    EXPECT_EQ(std::next(cont.begin(), 4), result.first);
    EXPECT_EQ(std::next(cont.begin(), 5), result.second);
    result = cont.equal_range(15);
    EXPECT_EQ(std::next(cont.begin(), 5), result.first);
    EXPECT_EQ(std::next(cont.begin(), 6), result.second);
    result = cont.equal_range(17);
    EXPECT_EQ(std::next(cont.begin(), 6), result.first);
    EXPECT_EQ(std::next(cont.begin(), 7), result.second);
    result = cont.equal_range(19);
    EXPECT_EQ(std::next(cont.begin(), 7), result.first);
    EXPECT_EQ(std::next(cont.begin(), 8), result.second);
    result = cont.equal_range(4);
    EXPECT_EQ(std::next(cont.begin(), 0), result.first);
    EXPECT_EQ(std::next(cont.begin(), 0), result.second);
    result = cont.equal_range(6);
    EXPECT_EQ(std::next(cont.begin(), 1), result.first);
    EXPECT_EQ(std::next(cont.begin(), 1), result.second);
    result = cont.equal_range(8);
    EXPECT_EQ(std::next(cont.begin(), 2), result.first);
    EXPECT_EQ(std::next(cont.begin(), 2), result.second);
    result = cont.equal_range(10);
    EXPECT_EQ(std::next(cont.begin(), 3), result.first);
    EXPECT_EQ(std::next(cont.begin(), 3), result.second);
    result = cont.equal_range(12);
    EXPECT_EQ(std::next(cont.begin(), 4), result.first);
    EXPECT_EQ(std::next(cont.begin(), 4), result.second);
    result = cont.equal_range(14);
    EXPECT_EQ(std::next(cont.begin(), 5), result.first);
    EXPECT_EQ(std::next(cont.begin(), 5), result.second);
    result = cont.equal_range(16);
    EXPECT_EQ(std::next(cont.begin(), 6), result.first);
    EXPECT_EQ(std::next(cont.begin(), 6), result.second);
    result = cont.equal_range(18);
    EXPECT_EQ(std::next(cont.begin(), 7), result.first);
    EXPECT_EQ(std::next(cont.begin(), 7), result.second);
    result = cont.equal_range(20);
    EXPECT_EQ(std::next(cont.begin(), 8), result.first);
    EXPECT_EQ(std::next(cont.begin(), 8), result.second);
  }
  {
    const IntTree cont({5, 7, 9, 11, 13, 15, 17, 19});

    std::pair<IntTree::const_iterator, IntTree::const_iterator> result =
        cont.equal_range(5);
    EXPECT_EQ(std::next(cont.begin(), 0), result.first);
    EXPECT_EQ(std::next(cont.begin(), 1), result.second);
    result = cont.equal_range(7);
    EXPECT_EQ(std::next(cont.begin(), 1), result.first);
    EXPECT_EQ(std::next(cont.begin(), 2), result.second);
    result = cont.equal_range(9);
    EXPECT_EQ(std::next(cont.begin(), 2), result.first);
    EXPECT_EQ(std::next(cont.begin(), 3), result.second);
    result = cont.equal_range(11);
    EXPECT_EQ(std::next(cont.begin(), 3), result.first);
    EXPECT_EQ(std::next(cont.begin(), 4), result.second);
    result = cont.equal_range(13);
    EXPECT_EQ(std::next(cont.begin(), 4), result.first);
    EXPECT_EQ(std::next(cont.begin(), 5), result.second);
    result = cont.equal_range(15);
    EXPECT_EQ(std::next(cont.begin(), 5), result.first);
    EXPECT_EQ(std::next(cont.begin(), 6), result.second);
    result = cont.equal_range(17);
    EXPECT_EQ(std::next(cont.begin(), 6), result.first);
    EXPECT_EQ(std::next(cont.begin(), 7), result.second);
    result = cont.equal_range(19);
    EXPECT_EQ(std::next(cont.begin(), 7), result.first);
    EXPECT_EQ(std::next(cont.begin(), 8), result.second);
    result = cont.equal_range(4);
    EXPECT_EQ(std::next(cont.begin(), 0), result.first);
    EXPECT_EQ(std::next(cont.begin(), 0), result.second);
    result = cont.equal_range(6);
    EXPECT_EQ(std::next(cont.begin(), 1), result.first);
    EXPECT_EQ(std::next(cont.begin(), 1), result.second);
    result = cont.equal_range(8);
    EXPECT_EQ(std::next(cont.begin(), 2), result.first);
    EXPECT_EQ(std::next(cont.begin(), 2), result.second);
    result = cont.equal_range(10);
    EXPECT_EQ(std::next(cont.begin(), 3), result.first);
    EXPECT_EQ(std::next(cont.begin(), 3), result.second);
    result = cont.equal_range(12);
    EXPECT_EQ(std::next(cont.begin(), 4), result.first);
    EXPECT_EQ(std::next(cont.begin(), 4), result.second);
    result = cont.equal_range(14);
    EXPECT_EQ(std::next(cont.begin(), 5), result.first);
    EXPECT_EQ(std::next(cont.begin(), 5), result.second);
    result = cont.equal_range(16);
    EXPECT_EQ(std::next(cont.begin(), 6), result.first);
    EXPECT_EQ(std::next(cont.begin(), 6), result.second);
    result = cont.equal_range(18);
    EXPECT_EQ(std::next(cont.begin(), 7), result.first);
    EXPECT_EQ(std::next(cont.begin(), 7), result.second);
    result = cont.equal_range(20);
    EXPECT_EQ(std::next(cont.begin(), 8), result.first);
    EXPECT_EQ(std::next(cont.begin(), 8), result.second);
  }
}

//       iterator lower_bound(const key_type& key);
// const_iterator lower_bound(const key_type& key) const;

TEST(FlatTree, LowerBound) {
  {
    IntTree cont({5, 7, 9, 11, 13, 15, 17, 19});

    EXPECT_EQ(cont.begin(), cont.lower_bound(5));
    EXPECT_EQ(std::next(cont.begin()), cont.lower_bound(7));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.lower_bound(9));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.lower_bound(11));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.lower_bound(13));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.lower_bound(15));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.lower_bound(17));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.lower_bound(19));
    EXPECT_EQ(std::next(cont.begin(), 0), cont.lower_bound(4));
    EXPECT_EQ(std::next(cont.begin(), 1), cont.lower_bound(6));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.lower_bound(8));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.lower_bound(10));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.lower_bound(12));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.lower_bound(14));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.lower_bound(16));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.lower_bound(18));
    EXPECT_EQ(std::next(cont.begin(), 8), cont.lower_bound(20));
  }
  {
    const IntTree cont({5, 7, 9, 11, 13, 15, 17, 19});

    EXPECT_EQ(cont.begin(), cont.lower_bound(5));
    EXPECT_EQ(std::next(cont.begin()), cont.lower_bound(7));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.lower_bound(9));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.lower_bound(11));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.lower_bound(13));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.lower_bound(15));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.lower_bound(17));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.lower_bound(19));
    EXPECT_EQ(std::next(cont.begin(), 0), cont.lower_bound(4));
    EXPECT_EQ(std::next(cont.begin(), 1), cont.lower_bound(6));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.lower_bound(8));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.lower_bound(10));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.lower_bound(12));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.lower_bound(14));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.lower_bound(16));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.lower_bound(18));
    EXPECT_EQ(std::next(cont.begin(), 8), cont.lower_bound(20));
  }
}

// iterator upper_bound(const key_type& key)
// const_iterator upper_bound(const key_type& key) const

TEST(FlatTree, UpperBound) {
  {
    IntTree cont({5, 7, 9, 11, 13, 15, 17, 19});

    EXPECT_EQ(std::next(cont.begin(), 1), cont.upper_bound(5));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.upper_bound(7));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.upper_bound(9));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.upper_bound(11));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.upper_bound(13));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.upper_bound(15));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.upper_bound(17));
    EXPECT_EQ(std::next(cont.begin(), 8), cont.upper_bound(19));
    EXPECT_EQ(std::next(cont.begin(), 0), cont.upper_bound(4));
    EXPECT_EQ(std::next(cont.begin(), 1), cont.upper_bound(6));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.upper_bound(8));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.upper_bound(10));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.upper_bound(12));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.upper_bound(14));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.upper_bound(16));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.upper_bound(18));
    EXPECT_EQ(std::next(cont.begin(), 8), cont.upper_bound(20));
  }
  {
    const IntTree cont({5, 7, 9, 11, 13, 15, 17, 19});

    EXPECT_EQ(std::next(cont.begin(), 1), cont.upper_bound(5));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.upper_bound(7));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.upper_bound(9));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.upper_bound(11));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.upper_bound(13));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.upper_bound(15));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.upper_bound(17));
    EXPECT_EQ(std::next(cont.begin(), 8), cont.upper_bound(19));
    EXPECT_EQ(std::next(cont.begin(), 0), cont.upper_bound(4));
    EXPECT_EQ(std::next(cont.begin(), 1), cont.upper_bound(6));
    EXPECT_EQ(std::next(cont.begin(), 2), cont.upper_bound(8));
    EXPECT_EQ(std::next(cont.begin(), 3), cont.upper_bound(10));
    EXPECT_EQ(std::next(cont.begin(), 4), cont.upper_bound(12));
    EXPECT_EQ(std::next(cont.begin(), 5), cont.upper_bound(14));
    EXPECT_EQ(std::next(cont.begin(), 6), cont.upper_bound(16));
    EXPECT_EQ(std::next(cont.begin(), 7), cont.upper_bound(18));
    EXPECT_EQ(std::next(cont.begin(), 8), cont.upper_bound(20));
  }
}

// ----------------------------------------------------------------------------
// General operations.

// void swap(flat_tree& other)
// void swap(flat_tree& lhs, flat_tree& rhs)

TEST(FlatTreeOurs, Swap) {
  IntTree x({1, 2, 3});
  IntTree y({4});
  swap(x, y);
  EXPECT_THAT(x, ElementsAre(4));
  EXPECT_THAT(y, ElementsAre(1, 2, 3));

  y.swap(x);
  EXPECT_THAT(x, ElementsAre(1, 2, 3));
  EXPECT_THAT(y, ElementsAre(4));
}

// bool operator==(const flat_tree& lhs, const flat_tree& rhs)
// bool operator!=(const flat_tree& lhs, const flat_tree& rhs)
// bool operator<(const flat_tree& lhs, const flat_tree& rhs)
// bool operator>(const flat_tree& lhs, const flat_tree& rhs)
// bool operator<=(const flat_tree& lhs, const flat_tree& rhs)
// bool operator>=(const flat_tree& lhs, const flat_tree& rhs)

TEST(FlatTree, Comparison) {
  // Provided comparator does not participate in comparison.
  ReversedTree biggest({3});
  ReversedTree smallest({1});
  ReversedTree middle({1, 2});

  EXPECT_EQ(biggest, biggest);
  EXPECT_NE(biggest, smallest);
  EXPECT_LT(smallest, middle);
  EXPECT_LE(smallest, middle);
  EXPECT_LE(middle, middle);
  EXPECT_GT(biggest, middle);
  EXPECT_GE(biggest, middle);
  EXPECT_GE(biggest, biggest);
}

TEST(FlatSet, EraseIf) {
  IntTree x;
  EraseIf(x, [](int) { return false; });
  EXPECT_THAT(x, ElementsAre());

  x = {1, 2, 3};
  EraseIf(x, [](int elem) { return !(elem & 1); });
  EXPECT_THAT(x, ElementsAre(1, 3));

  x = {1, 2, 3, 4};
  EraseIf(x, [](int elem) { return elem & 1; });
  EXPECT_THAT(x, ElementsAre(2, 4));
}

}  // namespace internal
}  // namespace base
