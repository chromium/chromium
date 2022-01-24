// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_ENUM_SET_H_
#define BASE_CONTAINERS_ENUM_SET_H_

#include <bitset>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "base/check_op.h"

namespace base {

// Forward declarations needed for friend declarations.
template <typename E, E MinEnumValue, E MaxEnumValue>
class EnumSet;

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Union(EnumSet<E, Min, Max> set1,
                           EnumSet<E, Min, Max> set2);

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Intersection(EnumSet<E, Min, Max> set1,
                                  EnumSet<E, Min, Max> set2);

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Difference(EnumSet<E, Min, Max> set1,
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
      std::is_enum<E>::value,
      "First template parameter of EnumSet must be an enumeration type");
  using enum_underlying_type = std::underlying_type_t<E>;

  static constexpr enum_underlying_type GetUnderlyingValue(E value) {
    return static_cast<enum_underlying_type>(value);
  }

 public:
  using EnumType = E;
  static const E kMinValue = MinEnumValue;
  static const E kMaxValue = MaxEnumValue;
  static const size_t kValueCount =
      GetUnderlyingValue(kMaxValue) - GetUnderlyingValue(kMinValue) + 1;

  static_assert(kMinValue < kMaxValue, "min value must be less than max value");

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
  // The iterator must not be outlived by the set. In particular, the following
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
    Iterator() : enums_(nullptr), i_(kValueCount) {}
    ~Iterator() {}

    bool operator==(const Iterator& other) const { return i_ == other.i_; }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    E operator*() const {
      DCHECK(Good());
      return FromIndex(i_);
    }

    Iterator& operator++() {
      DCHECK(Good());
      // If there are no more set elements in the bitset, this will result in an
      // index equal to kValueCount, which is equivalent to EnumSet.end().
      i_ = FindNext(i_ + 1);

      return *this;
    }

    Iterator operator++(int) {
      DCHECK(Good());
      Iterator old(*this);

      // If there are no more set elements in the bitset, this will result in an
      // index equal to kValueCount, which is equivalent to EnumSet.end().
      i_ = FindNext(i_ + 1);

      return std::move(old);
    }

   private:
    friend Iterator EnumSet::begin() const;

    explicit Iterator(const EnumBitSet& enums)
        : enums_(&enums), i_(FindNext(0)) {}

    // Returns true iff the iterator points to an EnumSet and it
    // hasn't yet traversed the EnumSet entirely.
    bool Good() const { return enums_ && i_ < kValueCount && enums_->test(i_); }

    size_t FindNext(size_t i) {
      while ((i < kValueCount) && !enums_->test(i)) {
        ++i;
      }
      return i;
    }

    const EnumBitSet* enums_;
    size_t i_;
  };

  EnumSet() {}

  ~EnumSet() = default;

  static constexpr uint64_t single_val_bitstring(E val) {
    return 1ULL << (ToIndex(val));
  }

  template <class... T>
  static constexpr uint64_t bitstring(T... values) {
    uint64_t converted[] = {single_val_bitstring(values)...};
    uint64_t result = 0;
    for (uint64_t e : converted)
      result |= e;
    return result;
  }

  template <class... T>
  constexpr EnumSet(E head, T... tail)
      : EnumSet(EnumBitSet(bitstring(head, tail...))) {}

  // Returns an EnumSet with all possible values.
  static constexpr EnumSet All() {
    return EnumSet(EnumBitSet((1ULL << kValueCount) - 1));
  }

  // Returns an EnumSet with all the values from start to end, inclusive.
  static constexpr EnumSet FromRange(E start, E end) {
    return EnumSet(EnumBitSet(
        ((single_val_bitstring(end)) - (single_val_bitstring(start))) |
        (single_val_bitstring(end))));
  }

  // Copy constructor and assignment welcome.

  // Bitmask operations.
  //
  // This bitmask is 0-based and the value of the Nth bit depends on whether
  // the set contains an enum element of integer value N.
  //
  // These may only be used if Min >= 0 and Max < 64.

  // Returns an EnumSet constructed from |bitmask|.
  static constexpr EnumSet FromEnumBitmask(const uint64_t bitmask) {
    static_assert(GetUnderlyingValue(kMaxValue) < 64,
                  "The highest enum value must be < 64 for FromEnumBitmask ");
    static_assert(GetUnderlyingValue(kMinValue) >= 0,
                  "The lowest enum value must be >= 0 for FromEnumBitmask ");
    return EnumSet(EnumBitSet(bitmask >> GetUnderlyingValue(kMinValue)));
  }
  // Returns a bitmask for the EnumSet.
  uint64_t ToEnumBitmask() const {
    static_assert(GetUnderlyingValue(kMaxValue) < 64,
                  "The highest enum value must be < 64 for ToEnumBitmask ");
    static_assert(GetUnderlyingValue(kMinValue) >= 0,
                  "The lowest enum value must be >= 0 for FromEnumBitmask ");
    return enums_.to_ullong() << GetUnderlyingValue(kMinValue);
  }

  // Set operations.  Put, Retain, and Remove are basically
  // self-mutating versions of Union, Intersection, and Difference
  // (defined below).

  // Adds the given value (which must be in range) to our set.
  void Put(E value) { enums_.set(ToIndex(value)); }

  // Adds all values in the given set to our set.
  void PutAll(EnumSet other) { enums_ |= other.enums_; }

  // Adds all values in the given range to our set, inclusive.
  void PutRange(E start, E end) {
    size_t endIndexInclusive = ToIndex(end);
    DCHECK_LE(ToIndex(start), endIndexInclusive);
    for (size_t current = ToIndex(start); current <= endIndexInclusive;
         ++current) {
      enums_.set(current);
    }
  }

  // There's no real need for a Retain(E) member function.

  // Removes all values not in the given set from our set.
  void RetainAll(EnumSet other) { enums_ &= other.enums_; }

  // If the given value is in range, removes it from our set.
  void Remove(E value) {
    if (InRange(value)) {
      enums_.reset(ToIndex(value));
    }
  }

  // Removes all values in the given set from our set.
  void RemoveAll(EnumSet other) { enums_ &= ~other.enums_; }

  // Removes all values from our set.
  void Clear() { enums_.reset(); }

  // Returns true iff the given value is in range and a member of our set.
  constexpr bool Has(E value) const {
    return InRange(value) && enums_[ToIndex(value)];
  }

  // Returns true iff the given set is a subset of our set.
  bool HasAll(EnumSet other) const {
    return (enums_ & other.enums_) == other.enums_;
  }

  // Returns true iff our set is empty.
  bool Empty() const { return !enums_.any(); }

  // Returns how many values our set has.
  size_t Size() const { return enums_.count(); }

  // Returns an iterator pointing to the first element (if any).
  Iterator begin() const { return Iterator(enums_); }

  // Returns an iterator that does not point to any element, but to the position
  // that follows the last element in the set.
  Iterator end() const { return Iterator(); }

  // Returns true iff our set and the given set contain exactly the same values.
  bool operator==(const EnumSet& other) const { return enums_ == other.enums_; }

  // Returns true iff our set and the given set do not contain exactly the same
  // values.
  bool operator!=(const EnumSet& other) const { return enums_ != other.enums_; }

 private:
  friend EnumSet Union<E, MinEnumValue, MaxEnumValue>(EnumSet set1,
                                                      EnumSet set2);
  friend EnumSet Intersection<E, MinEnumValue, MaxEnumValue>(EnumSet set1,
                                                             EnumSet set2);
  friend EnumSet Difference<E, MinEnumValue, MaxEnumValue>(EnumSet set1,
                                                           EnumSet set2);

  // A bitset can't be constexpr constructed if it has size > 64, since the
  // constexpr constructor uses a uint64_t. If your EnumSet has > 64 values, you
  // can safely remove the constepxr qualifiers from this file, at the cost of
  // some minor optimizations.
  explicit constexpr EnumSet(EnumBitSet enums) : enums_(enums) {
    static_assert(kValueCount <= 64,
                  "Max number of enum values is 64 for constexpr ");
  }

  static constexpr bool InRange(E value) {
    return (value >= MinEnumValue) && (value <= MaxEnumValue);
  }

  // Converts a value to/from an index into |enums_|.

  static constexpr size_t ToIndex(E value) {
    return GetUnderlyingValue(value) - GetUnderlyingValue(MinEnumValue);
  }

  static E FromIndex(size_t i) {
    DCHECK_LT(i, kValueCount);
    return static_cast<E>(GetUnderlyingValue(MinEnumValue) + i);
  }

  EnumBitSet enums_;
};

template <typename E, E MinEnumValue, E MaxEnumValue>
const E EnumSet<E, MinEnumValue, MaxEnumValue>::kMinValue;

template <typename E, E MinEnumValue, E MaxEnumValue>
const E EnumSet<E, MinEnumValue, MaxEnumValue>::kMaxValue;

template <typename E, E MinEnumValue, E MaxEnumValue>
const size_t EnumSet<E, MinEnumValue, MaxEnumValue>::kValueCount;

// The usual set operations.

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Union(EnumSet<E, Min, Max> set1,
                           EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ | set2.enums_);
}

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Intersection(EnumSet<E, Min, Max> set1,
                                  EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ & set2.enums_);
}

template <typename E, E Min, E Max>
EnumSet<E, Min, Max> Difference(EnumSet<E, Min, Max> set1,
                                EnumSet<E, Min, Max> set2) {
  return EnumSet<E, Min, Max>(set1.enums_ & ~set2.enums_);
}

}  // namespace base

#endif  // BASE_CONTAINERS_ENUM_SET_H_
