// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_FLAT_SET_H_
#define BASE_CONTAINERS_FLAT_SET_H_

#include <algorithm>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <vector>

#include "base/containers/flat_tree.h"

namespace base {

// flat_set is a container with a std::set-like interface that stores its
// contents in a sorted container, by default a vector.
//
// Its implementation mostly tracks the corresponding standardization proposal
// https://wg21.link/P1222.
//
// Please see //base/containers/README.md for an overview of which container
// to select.
//
// PROS
//
//  - Good memory locality.
//  - Low overhead, especially for smaller sets.
//  - Performance is good for more workloads than you might expect (see
//    overview link above).
//  - Supports C++14 set interface.
//
// CONS
//
//  - Inserts and removals are O(n).
//
// IMPORTANT NOTES
//
//  - Iterators are invalidated across mutations.
//  - If possible, construct a flat_set in one operation by inserting into
//    a container and moving that container into the flat_set constructor.
//  - For multiple removals use base::EraseIf() which is O(n) rather than
//    O(n * removed_items).
//
// QUICK REFERENCE
//
// Most of the core functionality is inherited from flat_tree. Please see
// flat_tree.h for more details for most of these functions. As a quick
// reference, the functions available are:
//
// Constructors (inputs need not be sorted):
//   flat_set(const flat_set&);
//   flat_set(flat_set&&);
//   flat_set(InputIterator first, InputIterator last,
//            const Compare& compare = Compare());
//   flat_set(std::from_range_t,
//            Range&& range,
//            const Compare& comp = Compare());
//   flat_set(const container_type& items,
//            const Compare& compare = Compare());
//   flat_set(container_type&& items,
//            const Compare& compare = Compare());  // Re-use storage.
//   flat_set(std::initializer_list<value_type> ilist,
//            const Compare& comp = Compare());
//
// Constructors (inputs need to be sorted):
//   flat_set(sorted_unique_t,
//            InputIterator first, InputIterator last,
//            const Compare& compare = Compare());
//   flat_set(std::from_range_t,
//            sorted_unique_t,
//            Range&& range,
//            const Compare& comp = Compare());
//   flat_set(sorted_unique_t,
//            const container_type& items,
//            const Compare& compare = Compare());
//   flat_set(sorted_unique_t,
//            container_type&& items,
//            const Compare& compare = Compare());  // Re-use storage.
//   flat_set(sorted_unique_t,
//            std::initializer_list<value_type> ilist,
//            const Compare& comp = Compare());
//
// Assignment functions:
//   flat_set& operator=(const flat_set&);
//   flat_set& operator=(flat_set&&);
//   flat_set& operator=(initializer_list<Key>);
//
// Memory management functions:
//   void   reserve(size_t);
//   size_t capacity() const;
//   void   shrink_to_fit();
//
// Size management functions:
//   void   clear();
//   size_t size() const;
//   size_t max_size() const;
//   bool   empty() const;
//
// Iterator functions:
//   iterator               begin();
//   const_iterator         begin() const;
//   const_iterator         cbegin() const;
//   iterator               end();
//   const_iterator         end() const;
//   const_iterator         cend() const;
//   reverse_iterator       rbegin();
//   const reverse_iterator rbegin() const;
//   const_reverse_iterator crbegin() const;
//   reverse_iterator       rend();
//   const_reverse_iterator rend() const;
//   const_reverse_iterator crend() const;
//
// Insert and accessor functions:
//   pair<iterator, bool> insert(const key_type&);
//   pair<iterator, bool> insert(key_type&&);
//   void                 insert(InputIterator first, InputIterator last);
//   iterator             insert(const_iterator hint, const key_type&);
//   iterator             insert(const_iterator hint, key_type&&);
//   pair<iterator, bool> emplace(Args&&...);
//   iterator             emplace_hint(const_iterator, Args&&...);
//
// Underlying type functions:
//   container_type       extract() &&;
//   void                 replace(container_type&&);
//
// Erase functions:
//   iterator erase(iterator);
//   iterator erase(const_iterator);
//   iterator erase(const_iterator first, const_iterator& last);
//   template <typename K> size_t erase(const K& key);
//
// Comparators (see std::set documentation).
//   key_compare   key_comp() const;
//   value_compare value_comp() const;
//
// Search functions:
//   template <typename K> size_t                   count(const K&) const;
//   template <typename K> iterator                 find(const K&);
//   template <typename K> const_iterator           find(const K&) const;
//   template <typename K> bool                     contains(const K&) const;
//   template <typename K> pair<iterator, iterator> equal_range(K&);
//   template <typename K> iterator                 lower_bound(const K&);
//   template <typename K> const_iterator           lower_bound(const K&) const;
//   template <typename K> iterator                 upper_bound(const K&);
//   template <typename K> const_iterator           upper_bound(const K&) const;
//
// General functions:
//   void swap(flat_set&);
//
// Non-member operators:
//   bool operator==(const flat_set&, const flat_set);
//   bool operator!=(const flat_set&, const flat_set);
//   bool operator<(const flat_set&, const flat_set);
//   bool operator>(const flat_set&, const flat_set);
//   bool operator>=(const flat_set&, const flat_set);
//   bool operator<=(const flat_set&, const flat_set);
//
template <class Key,
          class Compare = std::less<>,
          class Container = std::vector<Key>>
using flat_set = typename ::base::internal::
    flat_tree<Key, std::identity, Compare, Container>;

namespace internal {

// Deduction guides for `flat_set`. They are modeled after the guides for
// `std::flat_set`, but default to `std::less<>` like `flat_set` itself. Since
// `flat_set` is an alias template, they have to be declared for `flat_tree`
// (see https://wg21.link/P1814). They always deduce `std::identity` as the key
// projection, which is what makes them apply to `flat_set` and nothing else.
//
// Example usage:
//   std::vector<int> v = {3, 1, 2};
//   base::flat_set s1(v);                              // flat_set<int>
//   base::flat_set s2(v.begin(), v.end());             // flat_set<int>
//   base::flat_set s3(std::from_range, v);             // flat_set<int>
//   base::flat_set s4 = {1, 2, 3};                     // flat_set<int>
//   base::flat_set s5(base::sorted_unique, {1, 2, 3}); // flat_set<int>
//
// Note: As for `std::flat_set`, `base::flat_set{v}` deduces a set of vectors.
//
// Unlike `std::flat_set`, the container guides return the container as
// `std::type_identity_t<Container>`, a non-deduced context. With a plain
// `Container`, alias CTAD matches it against `flat_set`'s `Container =
// std::vector<Key>` parameter, pulls in `Key` through that default
// ([over.match.class.deduct]/3.2), and then can't deduce `Key`. The guides
// also reject views such as `base::span` or `std::string_view`, so that CTAD
// never silently deduces a `flat_set` that doesn't own its elements.
template <class Container, class Compare = std::less<>>
  requires(std::ranges::input_range<Container> &&
           !std::ranges::view<Container> &&
           std::invocable<const Compare&,
                          const typename Container::value_type&,
                          const typename Container::value_type&>)
flat_tree(Container, Compare = Compare())
    -> flat_tree<typename Container::value_type,
                 std::identity,
                 Compare,
                 std::type_identity_t<Container>>;

template <class Container, class Compare = std::less<>>
  requires(std::ranges::input_range<Container> &&
           !std::ranges::view<Container> &&
           std::invocable<const Compare&,
                          const typename Container::value_type&,
                          const typename Container::value_type&>)
flat_tree(sorted_unique_t, Container, Compare = Compare())
    -> flat_tree<typename Container::value_type,
                 std::identity,
                 Compare,
                 std::type_identity_t<Container>>;

template <class InputIterator, class Compare = std::less<>>
  requires(std::input_iterator<InputIterator>)
flat_tree(InputIterator, InputIterator, Compare = Compare())
    -> flat_tree<std::iter_value_t<InputIterator>,
                 std::identity,
                 Compare,
                 std::vector<std::iter_value_t<InputIterator>>>;

template <class InputIterator, class Compare = std::less<>>
  requires(std::input_iterator<InputIterator>)
flat_tree(sorted_unique_t, InputIterator, InputIterator, Compare = Compare())
    -> flat_tree<std::iter_value_t<InputIterator>,
                 std::identity,
                 Compare,
                 std::vector<std::iter_value_t<InputIterator>>>;

template <class Range, class Compare = std::less<>>
  requires(std::ranges::input_range<Range>)
flat_tree(std::from_range_t, Range&&, Compare = Compare())
    -> flat_tree<std::ranges::range_value_t<Range>,
                 std::identity,
                 Compare,
                 std::vector<std::ranges::range_value_t<Range>>>;

template <class Range, class Compare = std::less<>>
  requires(std::ranges::input_range<Range>)
flat_tree(std::from_range_t, sorted_unique_t, Range&&, Compare = Compare())
    -> flat_tree<std::ranges::range_value_t<Range>,
                 std::identity,
                 Compare,
                 std::vector<std::ranges::range_value_t<Range>>>;

template <class Key, class Compare = std::less<>>
flat_tree(std::initializer_list<Key>, Compare = Compare())
    -> flat_tree<Key, std::identity, Compare, std::vector<Key>>;

template <class Key, class Compare = std::less<>>
flat_tree(sorted_unique_t, std::initializer_list<Key>, Compare = Compare())
    -> flat_tree<Key, std::identity, Compare, std::vector<Key>>;

}  // namespace internal

// Utility function to simplify constructing a flat_set from a fixed list
// of keys. The keys are obtained by applying the projection |proj| to the
// |unprojected_elements|. The set's keys are sorted by |comp|.
//
// Example usage (creates a set {16, 9, 4, 1}):
//   auto set = base::MakeFlatSet<int>(
//       std::vector<int>{1, 2, 3, 4}, [](int i, int j) { return i > j; },
//       [](int i) { return i * i; });
template <class Key,
          class Compare = std::less<>,
          class Container = std::vector<Key>,
          class InputContainer,
          class Projection = std::identity>
constexpr flat_set<Key, Compare, Container> MakeFlatSet(
    const InputContainer& unprojected_elements,
    const Compare& comp = Compare(),
    const Projection& proj = Projection()) {
  Container elements;
  internal::ReserveIfSupported(elements, unprojected_elements);
  std::ranges::transform(unprojected_elements, std::back_inserter(elements),
                         proj);
  return flat_set<Key, Compare, Container>(std::move(elements), comp);
}

}  // namespace base

#endif  // BASE_CONTAINERS_FLAT_SET_H_
