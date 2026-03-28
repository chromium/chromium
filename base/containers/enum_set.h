// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_ENUM_SET_H_
#define BASE_CONTAINERS_ENUM_SET_H_

#include <bitset>
#include <compare>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"

namespace base {

// Forward declarations needed for friend declarations.
template <typename E, E MinEnumValue, E MaxEnumValue>
class EnumSet;

template <typename E, E Min, E Max>
constexpr EnumSet<E, Min, Max> Union(EnumSet<E, Min, Max> set1,
                                     EnumSet<E, Min, Max> set2);

template <typename E, E Min, E Max>
constexpr EnumSet<E, Min, Max> Intersection(EnumSet<E, Min, Max> set1,
                                            EnumSet<E, Min, Max> set2);

template <typename E, E Min, E Max>
constexpr EnumSet<E, Min, Max> Difference(EnumSet<E, Min, Max> set1,
                                          EnumSet<E, Min, Max> set2);

// An EnumSet is a set that can hold enum values between a min and a
// max value (inclusive of both).  It's essentially a wrapper around
// std::bitset<> with stronger type enforcement, more descriptive
// member function names, and an iterator interface.
//
// If you're working with enums with a small number of possible values
// (say, fewer than 64), you can efficiently pass around an EnumSet
// for that enum around by value.

template <typename E, E MinEnumValue, E MaxEnumValue>
class EnumSet {
 private:
  static_assert(
      std::is_enum_v<E>,
      "First template parameter of EnumSet must be an enumeration type");

  static constexpr bool InRange(E value) {
    return (value >= MinEnumValue) && (value <= MaxEnumValue);
  }

 public:
  using EnumType = E;
  static constexpr E kMinValue = MinEnumValue;
  static constexpr E kMaxValue = MaxEnumValue;
  static constexpr size_t kValueCount =
      std::to_underlying(kMaxValue) - std::to_underlying(kMinValue) + 1;

  static_assert(kMinValue <= kMaxValue,
                "min value must be no greater than max value");

  // Allow use with ::testing::ValuesIn, which expects a value_type defined.
  using value_type = EnumType;

 private:
  // Declaration needed by Iterator.
  using EnumBitSet = std::bitset<kValueCount>;

 public:
  // Iterator is a forward-only read-only iterator for EnumSet. It follows the
  // common STL input iterator interface (like std::unordered_set).
  //
  // Example usage, using a range-based for loop:
  //
  // EnumSet<SomeType> enums;
  // for (SomeType val : enums) {
  //   Process(val);
  // }
  //
  // Or using an explicit iterator (not recommended):
  //
  // for (EnumSet<...>::Iterator it = enums.begin(); it != enums.end(); it++) {
  //   Process(*it);
  // }
  //
  // The iterator must not outlive the set. In particular, the following
  // is an error:
  //
  // EnumSet<...> SomeFn() { ... }
  //
  // /* ERROR */
  // for (EnumSet<...>::Iterator it = SomeFun().begin(); ...
  //
  // Also, there are no guarantees as to what will happen if you
  // modify an EnumSet while traversing it with an iterator.
  class Iterator {
   public:
    using value_type = EnumType;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = EnumType*;
    using reference = EnumType&;
    using iterator_category = std::forward_iterator_tag;

    constexpr Iterator() : enums_(nullptr), i_(kValueCount) {}
    constexpr ~Iterator() = default;

    constexpr Iterator(const Iterator&) = default;
    constexpr Iterator& operator=(const Iterator&) = default;

    constexpr Iterator(Iterator&&) = default;
    constexpr Iterator& operator=(Iterator&&) = default;

    friend constexpr bool operator==(const Iterator& lhs, const Iterator& rhs) {
      return lhs.i_ == rhs.i_;
    }

    constexpr value_type operator*() const {
      DCHECK(Good());
      return FromIndex(i_);
    }

    constexpr Iterator& operator++() {
      DCHECK(Good());
      // If there are no more set elements in the bitset, this will result in an
      // index equal to kValueCount, which is equivalent to EnumSet.end().
      i_ = FindNext(i_ + 1);

      return *this;
    }

    constexpr Iterator operator++(int) {
      DCHECK(Good());
      Iterator old(*this);

      // If there are no more set elements in the bitset, this will result in an
      // index equal to kValueCount, which is equivalent to EnumSet.end().
      i_ = FindNext(i_ + 1);

      return old;
    }

   private:
    friend constexpr Iterator EnumSet::begin() const;

    explicit constexpr Iterator(const EnumBitSet& enums)
        : enums_(&enums), i_(FindNext(0)) {}

    // Returns true iff the iterator points to an EnumSet and it
    // hasn't yet traversed the EnumSet entirely.
    constexpr bool Good() const {
      return enums_ && i_ < kValueCount && enums_->test(i_);
    }

    constexpr size_t FindNext(size_t i) const {
      while ((i < kValueCount) && !enums_->test(i)) {
        ++i;
      }
      return i;
    }

    raw_ptr<const EnumBitSet> enums_;
    size_t i_;
  };

  constexpr EnumSet() = default;

  constexpr ~EnumSet() = default;

  constexpr EnumSet(std::initializer_list<E> values) {
    for (E value : values) {
      Put(value);
    }
  }

  // Returns an EnumSet with all values between kMinValue and kMaxValue, which
  // also contains undefined enum values if the enum in question has gaps
  // between kMinValue and kMaxValue.
  static consteval EnumSet All() {
    EnumBitSet enums;
    enums.set();
    return EnumSet(enums);
  }

  // Returns an EnumSet with all the values from start to end, inclusive.
  static constexpr EnumSet FromRange(E start, E end) {
    EnumSet result;
    result.PutRange(start, end);
    return result;
  }

  constexpr EnumSet(const EnumSet&) = default;
  constexpr EnumSet& operator=(const EnumSet&) = default;

  constexpr EnumSet(EnumSet&&) = default;
  constexpr EnumSet& operator=(EnumSet&&) = default;

  // Bitmask operations.
  //
  // This bitmask is 0-based and the value of the Nth bit depends on whether
  // the set contains an enum element of integer value N.
  //
  // These may only be used if Min >= 0 and Max < 64.

  // Returns an EnumSet constructed from |bitmask|.
  static constexpr EnumSet FromEnumBitmask(const uint64_t bitmask) {
    static_assert(std::to_underlying(kMaxValue) < 64,
                  "The highest enum value must be < 64 for FromEnumBitmask ");
    static_assert(std::to_underlying(kMinValue) >= 0,
                  "The lowest enum value must be >= 0 for FromEnumBitmask ");
    return EnumSet(EnumBitSet(bitmask >> std::to_underlying(kMinValue)));
  }
  // Returns a bitmask for the EnumSet.
  constexpr uint64_t ToEnumBitmask() const {
    static_assert(std::to_underlying(kMaxValue) < 64,
                  "The highest enum value must be < 64 for ToEnumBitmask ");
    static_assert(std::to_underlying(kMinValue) >= 0,
                  "The lowest enum value must be >= 0 for FromEnumBitmask ");
    return enums_.to_ullong() << std::to_underlying(kMinValue);
  }

  // Returns a uint64_t bit mask representing the values within the range
  // [64*n, 64*n + 63] of the EnumSet.
  constexpr std::optional<uint64_t> GetNth64bitWordBitmask(size_t n) const {
    // If the EnumSet contains less than n 64-bit masks, return std::nullopt.
    if (std::to_underlying(kMaxValue) / 64 < n) {
      return std::nullopt;
    }

    std::bitset<kValueCount> mask = ~uint64_t{0};
    std::bitset<kValueCount> bits = enums_;
    if (std::to_underlying(kMinValue) < n * 64) {
      bits >>= n * 64 - std::to_underlying(kMinValue);
    }
    uint64_t result = (bits & mask).to_ullong();
    if (std::to_underlying(kMinValue) > n * 64) {
      result <<= std::to_underlying(kMinValue) - n * 64;
    }
    return result;
  }

  // Set operations.  Put, Retain, and Remove are basically
  // self-mutating versions of Union, Intersection, and Difference
  // (defined below).

  // Adds the given value (which must be in range) to our set.
  constexpr void Put(E value) { enums_.set(ToIndex(value)); }

  // Adds all values in the given set to our set.
  constexpr void PutAll(EnumSet other) { enums_ |= other.enums_; }

  // Adds all values in the given range to our set, inclusive.
  constexpr void PutRange(E start, E end) {
    CHECK_LE(start, end);
    size_t end_index_inclusive = ToIndex(end);
    for (size_t current = ToIndex(start); current <= end_index_inclusive;
         ++current) {
      enums_.set(current);
    }
  }

  // There's no real need for a Retain(E) member function.

  // Removes all values not in the given set from our set.
  constexpr void RetainAll(EnumSet other) { enums_ &= other.enums_; }

  // If the given value is in range, removes it from our set.
  constexpr void Remove(E value) {
    if (InRange(value)) {
      enums_.reset(ToIndex(value));
    }
  }

  // Removes all values in the given set from our set.
  constexpr void RemoveAll(EnumSet other) { enums_ &= ~other.enums_; }

  // Removes all values from our set.
  constexpr void Clear() { enums_.reset(); }

  // Conditionally puts or removes `value`, based on `should_be_present`.
  constexpr void PutOrRemove(E value, bool should_be_present) {
    if (should_be_present) {
      Put(value);
    } else {
      Remove(value);
    }
  }

  // Returns true iff the given value is in range and a member of our set.
  constexpr bool Has(E value) const {
    return InRange(value) && enums_[ToIndex(value)];
  }

  // Returns true iff the given set is a subset of our set.
  constexpr bool HasAll(EnumSet other) const {
    return (enums_ & other.enums_) == other.enums_;
  }

  // Returns true if the given set contains any value of our set.
  constexpr bool HasAny(EnumSet other) const {
    return (enums_ & other.enums_).any();
  }

  // Returns true iff our set is empty.
  constexpr bool empty() const { return enums_.none(); }

  // Returns how many values our set has.
  constexpr size_t size() const { return enums_.count(); }

  // Returns an iterator pointing to the first element (if any).
  constexpr Iterator begin() const { return Iterator(enums_); }

  // Returns an iterator that does not point to any element, but to the position
  // that follows the last element in the set.
  constexpr Iterator end() const { return Iterator(); }

  // Returns true iff `a` and `b` contain exactly the same values.
  friend constexpr bool operator==(const EnumSet& a,
                                   const EnumSet& b) = default;

  // Compares `a` and `b` by their integer representation.
  friend constexpr auto operator<=>(const EnumSet& a, const EnumSet& b) {
    return a.ToEnumBitmask() <=> b.ToEnumBitmask();
  }

  std::string ToString() const { return enums_.to_string(); }

  // Allows an EnumSet to be used in absl hash containers.
  template <typename H>
  friend H AbslHashValue(H h, EnumSet e) {
    return H::combine(std::move(h), e.enums_);
  }

 private:
  friend constexpr EnumSet Union<E, MinEnumValue, MaxEnumValue>(EnumSet set1,
                                                                EnumSet set2);
  friend constexpr EnumSet Intersection<E, MinEnumValue, MaxEnumValue>(
      EnumSet set1,
      EnumSet set2);
  friend constexpr EnumSet Difference<E, MinEnumValue, MaxEnumValue>(
      EnumSet set1,
      EnumSet set2);

  explicit constexpr EnumSet(EnumBitSet enums) : enums_(enums) {}

  // Converts a value to/from an index into |enums_|.
  static constexpr size_t ToIndex(E value) {
    CHECK(InRange(value));
    return static_cast<size_t>(std::to_underlying(value)) -
           static_cast<size_t>(std::to_underlying(MinEnumValue));
  }

  static constexpr E FromIndex(size_t i) {
    DCHECK_LT(i, kValueCount);
    return static_cast<E>(std::to_underlying(MinEnumValue) + i);
  }

  EnumBitSet enums_;
};

template <typename E, E Min, E Max>
constexpr EnumSet<E, Min, Max> Union(EnumSet<E, Min, Max> set1,
                                     EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ | set2.enums_);
}

template <typename E, E Min, E Max>
constexpr EnumSet<E, Min, Max> Intersection(EnumSet<E, Min, Max> set1,
                                            EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ & set2.enums_);
}

template <typename E, E Min, E Max>
constexpr EnumSet<E, Min, Max> Difference(EnumSet<E, Min, Max> set1,
                                          EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ & ~set2.enums_);
}

}  // namespace base

#endif  // BASE_CONTAINERS_ENUM_SET_H_
