// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_FLAT_MAP_H_
#define BASE_CONTAINERS_FLAT_MAP_H_

#include <concepts>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_tree.h"

namespace base {

namespace internal {

// An implementation of the flat_tree GetKeyFromValue template parameter that
// extracts the key as the first element of a pair.
struct GetFirst {
  template <class Key, class Mapped>
  constexpr const Key& operator()(const std::pair<Key, Mapped>& p) const {
    return p.first;
  }
};

}  // namespace internal

// flat_map is a container with a std::map-like interface that stores its
// contents in a sorted container, by default a vector.
//
// Its implementation mostly tracks the corresponding standardization proposal
// https://wg21.link/P0429, except that the storage of keys and values is not
// split.
//
// Please see //base/containers/README.md for an overview of which container
// to select.
//
// PROS
//
//  - Good memory locality.
//  - Low overhead, especially for smaller maps.
//  - Performance is good for more workloads than you might expect (see
//    overview link above).
//  - Supports C++14 map interface.
//
// CONS
//
//  - Inserts and removals are O(n).
//
// IMPORTANT NOTES
//
//  - Iterators are invalidated across mutations. This means that the following
//    line of code has undefined behavior since adding a new element could
//    resize the container, invalidating all iterators:
//      container["new element"] = it.second;
//  - If possible, construct a flat_map in one operation by inserting into
//    a container and moving that container into the flat_map constructor.
//
// QUICK REFERENCE
//
// Most of the core functionality is inherited from flat_tree. Please see
// flat_tree.h for more details for most of these functions. As a quick
// reference, the functions available are:
//
// Constructors (inputs need not be sorted):
//   flat_map(const flat_map&);
//   flat_map(flat_map&&);
//   flat_map(InputIterator first, InputIterator last,
//            const Compare& compare = Compare());
//   flat_map(std::from_range_t,
//            Range&& range,
//            const Compare& comp = Compare());
//   flat_map(const container_type& items,
//            const Compare& compare = Compare());
//   flat_map(container_type&& items,
//            const Compare& compare = Compare()); // Re-use storage.
//   flat_map(std::initializer_list<value_type> ilist,
//            const Compare& comp = Compare());
//
// Constructors (inputs need to be sorted):
//   flat_map(sorted_unique_t,
//            InputIterator first, InputIterator last,
//            const Compare& compare = Compare());
//   flat_map(std::from_range_t,
//            sorted_unique_t,
//            Range&& range,
//            const Compare& comp = Compare());
//   flat_map(sorted_unique_t,
//            const container_type& items,
//            const Compare& compare = Compare());
//   flat_map(sorted_unique_t,
//            container_type&& items,
//            const Compare& compare = Compare());  // Re-use storage.
//   flat_map(sorted_unique_t,
//            std::initializer_list<value_type> ilist,
//            const Compare& comp = Compare());
//
// Assignment functions:
//   flat_map& operator=(const flat_map&);
//   flat_map& operator=(flat_map&&);
//   flat_map& operator=(initializer_list<value_type>);
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
//   mapped_type&         operator[](const K&);
//   mapped_type&         operator[](K&&);
//   mapped_type&         at(const K&);
//   const mapped_type&   at(const K&) const;
//   pair<iterator, bool> insert(const value_type&);
//   pair<iterator, bool> insert(value_type&&);
//   iterator             insert(const_iterator hint, const value_type&);
//   iterator             insert(const_iterator hint, value_type&&);
//   void                 insert(InputIterator first, InputIterator last);
//   pair<iterator, bool> insert_or_assign(const K&, M&&);
//   pair<iterator, bool> insert_or_assign(K&&, M&&);
//   iterator             insert_or_assign(const_iterator hint, const K&, M&&);
//   iterator             insert_or_assign(const_iterator hint, K&&, M&&);
//   pair<iterator, bool> emplace(Args&&...);
//   iterator             emplace_hint(const_iterator, Args&&...);
//   pair<iterator, bool> try_emplace(const K&, Args&&...);
//   pair<iterator, bool> try_emplace(K&&, Args&&...);
//   iterator             try_emplace(const_iterator hint, const K&, Args&&...);
//   iterator             try_emplace(const_iterator hint, K&&, Args&&...);

// Underlying type functions:
//   container_type       extract() &&;
//   void                 replace(container_type&&);
//
// Erase functions:
//   iterator erase(iterator);
//   iterator erase(const_iterator);
//   iterator erase(const_iterator first, const_iterator& last);
//   template <class K> size_t erase(const K& key);
//
// Comparators (see std::map documentation).
//   key_compare   key_comp() const;
//   value_compare value_comp() const;
//
// Search functions:
//   template <typename K> size_t                   count(const K&) const;
//   template <typename K> iterator                 find(const K&);
//   template <typename K> const_iterator           find(const K&) const;
//   template <typename K> bool                     contains(const K&) const;
//   template <typename K> pair<iterator, iterator> equal_range(const K&);
//   template <typename K> iterator                 lower_bound(const K&);
//   template <typename K> const_iterator           lower_bound(const K&) const;
//   template <typename K> iterator                 upper_bound(const K&);
//   template <typename K> const_iterator           upper_bound(const K&) const;
//
// General functions:
//   void swap(flat_map&);
//
// Non-member operators:
//   bool operator==(const flat_map&, const flat_map);
//   bool operator!=(const flat_map&, const flat_map);
//   bool operator<(const flat_map&, const flat_map);
//   bool operator>(const flat_map&, const flat_map);
//   bool operator>=(const flat_map&, const flat_map);
//   bool operator<=(const flat_map&, const flat_map);
//
template <class Key,
          class Mapped,
          class Compare = std::less<>,
          class Container = std::vector<std::pair<Key, Mapped>>>
class flat_map : public ::base::internal::
                     flat_tree<Key, internal::GetFirst, Compare, Container> {
 private:
  using tree = typename ::base::internal::
      flat_tree<Key, internal::GetFirst, Compare, Container>;

  // Alias supporting heterogeneous lookup if `Compare` is transparent.
  template <typename K>
  using KeyT = typename tree::template KeyT<K>;

 public:
  using key_type = typename tree::key_type;
  using mapped_type = Mapped;
  using value_type = typename tree::value_type;
  using reference = typename Container::reference;
  using const_reference = typename Container::const_reference;
  using size_type = typename Container::size_type;
  using difference_type = typename Container::difference_type;
  using iterator = typename tree::iterator;
  using const_iterator = typename tree::const_iterator;
  using reverse_iterator = typename tree::reverse_iterator;
  using const_reverse_iterator = typename tree::const_reverse_iterator;
  using container_type = typename tree::container_type;

  // --------------------------------------------------------------------------
  // Lifetime and assignments.
  //
  // Note: we explicitly bring operator= in because otherwise
  //   flat_map<...> x;
  //   x = {...};
  // Would first create a flat_map and then move assign it. This most likely
  // would be optimized away but still affects our debug builds.

  using tree::tree;
  using tree::operator=;

  // Out-of-bound calls to at() will CHECK.
  template <class K = Key>
  constexpr mapped_type& at(const KeyT<K>& key);
  template <class K = Key>
  constexpr const mapped_type& at(const KeyT<K>& key) const;

  // --------------------------------------------------------------------------
  // Map-specific insert operations.
  //
  // Normal insert() functions are inherited from flat_tree.
  //
  // Assume that every operation invalidates iterators and references.
  // Insertion of one element can take O(size).

  template <class K = Key>
  constexpr mapped_type& operator[](const KeyT<K>& key);
  template <class K = Key>
    requires(std::same_as<K, std::remove_cvref_t<K>>)
  constexpr mapped_type& operator[](KeyT<K>&& key);

  template <class K = Key, class M>
  std::pair<iterator, bool> insert_or_assign(const KeyT<K>& key, M&& obj);
  template <class K = Key, class M>
    requires(std::same_as<K, std::remove_cvref_t<K>>)
  std::pair<iterator, bool> insert_or_assign(KeyT<K>&& key, M&& obj);
  template <class K = Key, class M>
  iterator insert_or_assign(const_iterator hint, const KeyT<K>& key, M&& obj);
  template <class K = Key, class M>
    requires(std::same_as<K, std::remove_cvref_t<K>>)
  iterator insert_or_assign(const_iterator hint, KeyT<K>&& key, M&& obj);

  template <class K = Key, class... Args>
    requires(!std::convertible_to<K, const_iterator>)
  constexpr std::pair<iterator, bool> try_emplace(const KeyT<K>& key,
                                                  Args&&... args);
  template <class K = Key, class... Args>
    requires(std::same_as<K, std::remove_cvref_t<K>> &&
             !std::convertible_to<K, const_iterator>)
  constexpr std::pair<iterator, bool> try_emplace(KeyT<K>&& key,
                                                  Args&&... args);

  template <class K = Key, class... Args>
  constexpr iterator try_emplace(const_iterator hint,
                                 const KeyT<K>& key,
                                 Args&&... args);
  template <class K = Key, class... Args>
    requires(std::same_as<K, std::remove_cvref_t<K>>)
  constexpr iterator try_emplace(const_iterator hint,
                                 KeyT<K>&& key,
                                 Args&&... args);

  // --------------------------------------------------------------------------
  // General operations.
  //
  // Assume that swap invalidates iterators and references.

  REINITIALIZES_AFTER_MOVE void swap(flat_map& other) noexcept;

  friend void swap(flat_map& lhs, flat_map& rhs) noexcept { lhs.swap(rhs); }
};

// ----------------------------------------------------------------------------
// Lookups.

template <class Key, class Mapped, class Compare, class Container>
template <class K>
constexpr auto flat_map<Key, Mapped, Compare, Container>::at(const KeyT<K>& key)
    -> mapped_type& {
  iterator found = tree::find(key);
  CHECK(found != tree::end());
  return found->second;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K>
constexpr auto flat_map<Key, Mapped, Compare, Container>::at(
    const KeyT<K>& key) const -> const mapped_type& {
  const_iterator found = tree::find(key);
  CHECK(found != tree::cend());
  return found->second;
}

// ----------------------------------------------------------------------------
// Insert operations.

template <class Key, class Mapped, class Compare, class Container>
template <class K>
constexpr auto flat_map<Key, Mapped, Compare, Container>::operator[](
    const KeyT<K>& key) -> mapped_type& {
  iterator found = tree::lower_bound(key);
  if (found == tree::end() || tree::key_comp()(key, found->first)) {
    found = tree::unsafe_emplace(found, key, mapped_type());
  }
  return found->second;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K>
  requires(std::same_as<K, std::remove_cvref_t<K>>)
constexpr auto flat_map<Key, Mapped, Compare, Container>::operator[](
    KeyT<K>&& key) -> mapped_type& {
  iterator found = tree::lower_bound(key);
  if (found == tree::end() || tree::key_comp()(key, found->first)) {
    found = tree::unsafe_emplace(found, std::move(key), mapped_type());
  }
  return found->second;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class M>
auto flat_map<Key, Mapped, Compare, Container>::insert_or_assign(
    const KeyT<K>& key,
    M&& obj) -> std::pair<iterator, bool> {
  auto result = tree::emplace_key_args(key, key, std::forward<M>(obj));
  if (!result.second) {
    result.first->second = std::forward<M>(obj);
  }
  return result;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class M>
  requires(std::same_as<K, std::remove_cvref_t<K>>)
auto flat_map<Key, Mapped, Compare, Container>::insert_or_assign(KeyT<K>&& key,
                                                                 M&& obj)
    -> std::pair<iterator, bool> {
  auto result =
      tree::emplace_key_args(key, std::move(key), std::forward<M>(obj));
  if (!result.second) {
    result.first->second = std::forward<M>(obj);
  }
  return result;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class M>
auto flat_map<Key, Mapped, Compare, Container>::insert_or_assign(
    const_iterator hint,
    const KeyT<K>& key,
    M&& obj) -> iterator {
  auto result =
      tree::emplace_hint_key_args(hint, key, key, std::forward<M>(obj));
  if (!result.second) {
    result.first->second = std::forward<M>(obj);
  }
  return result.first;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class M>
  requires(std::same_as<K, std::remove_cvref_t<K>>)
auto flat_map<Key, Mapped, Compare, Container>::insert_or_assign(
    const_iterator hint,
    KeyT<K>&& key,
    M&& obj) -> iterator {
  auto result = tree::emplace_hint_key_args(hint, key, std::move(key),
                                            std::forward<M>(obj));
  if (!result.second) {
    result.first->second = std::forward<M>(obj);
  }
  return result.first;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class... Args>
  requires(!std::convertible_to<
           K,
           typename flat_map<Key, Mapped, Compare, Container>::const_iterator>)
constexpr auto flat_map<Key, Mapped, Compare, Container>::try_emplace(
    const KeyT<K>& key,
    Args&&... args) -> std::pair<iterator, bool> {
  return tree::emplace_key_args(
      key, std::piecewise_construct, std::forward_as_tuple(key),
      std::forward_as_tuple(std::forward<Args>(args)...));
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class... Args>
  requires(
      std::same_as<K, std::remove_cvref_t<K>> &&
      !std::convertible_to<
          K,
          typename flat_map<Key, Mapped, Compare, Container>::const_iterator>)
constexpr auto flat_map<Key, Mapped, Compare, Container>::try_emplace(
    KeyT<K>&& key,
    Args&&... args) -> std::pair<iterator, bool> {
  return tree::emplace_key_args(
      key, std::piecewise_construct, std::forward_as_tuple(std::move(key)),
      std::forward_as_tuple(std::forward<Args>(args)...));
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class... Args>
constexpr auto flat_map<Key, Mapped, Compare, Container>::try_emplace(
    const_iterator hint,
    const KeyT<K>& key,
    Args&&... args) -> iterator {
  return tree::emplace_hint_key_args(
             hint, key, std::piecewise_construct, std::forward_as_tuple(key),
             std::forward_as_tuple(std::forward<Args>(args)...))
      .first;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class... Args>
  requires(std::same_as<K, std::remove_cvref_t<K>>)
constexpr auto flat_map<Key, Mapped, Compare, Container>::try_emplace(
    const_iterator hint,
    KeyT<K>&& key,
    Args&&... args) -> iterator {
  return tree::emplace_hint_key_args(
             hint, key, std::piecewise_construct,
             std::forward_as_tuple(std::move(key)),
             std::forward_as_tuple(std::forward<Args>(args)...))
      .first;
}

// ----------------------------------------------------------------------------
// General operations.

template <class Key, class Mapped, class Compare, class Container>
void flat_map<Key, Mapped, Compare, Container>::swap(flat_map& other) noexcept {
  tree::swap(other);
}

// ----------------------------------------------------------------------------
// Utility functions.

// Utility function to simplify constructing a flat_set from a fixed list of
// keys and values. The key/value pairs are obtained by applying |proj| to the
// |unprojected_elements|. The map's keys are sorted by |comp|.
//
// Example usage (creates a set {{16, "4"}, {9, "3"}, {4, "2"}, {1, "1"}}):
//   auto map = base::MakeFlatMap<int, std::string>(
//       std::vector<int>{1, 2, 3, 4},
//       [](int i, int j) { return i > j; },
//       [](int i) { return std::make_pair(i * i, base::NumberToString(i)); });
template <class Key,
          class Mapped,
          class KeyCompare = std::less<>,
          class Container = std::vector<std::pair<Key, Mapped>>,
          class InputContainer,
          class Projection = std::identity>
constexpr flat_map<Key, Mapped, KeyCompare, Container> MakeFlatMap(
    const InputContainer& unprojected_elements,
    const KeyCompare& comp = KeyCompare(),
    const Projection& proj = Projection()) {
  Container elements;
  internal::ReserveIfSupported(elements, unprojected_elements);
  std::ranges::transform(unprojected_elements, std::back_inserter(elements),
                         proj);
  return flat_map<Key, Mapped, KeyCompare, Container>(std::move(elements),
                                                      comp);
}

namespace flat_map_internal {

// Helpers for the iterator and range deduction guides below, named after the
// exposition-only aliases of `std::flat_map`'s deduction guides, see
// https://en.cppreference.com/cpp/container/flat_map/deduction_guides. The key
// drops `const`, so e.g. `std::map` iterators deduce a `flat_map<Key, Mapped>`.
// Unlike the standard's, `IterValT` uses `std::iter_value_t`, since the guides
// accept any C++20 `std::input_iterator`, not just Cpp17 input iterators.
template <class InputIterator>
using IterValT = std::iter_value_t<InputIterator>;

template <class InputIterator>
using IterKeyT =
    std::remove_const_t<std::tuple_element_t<0, IterValT<InputIterator>>>;

template <class InputIterator>
using IterMappedT = std::tuple_element_t<1, IterValT<InputIterator>>;

template <class Range>
using RangeKeyT =
    std::remove_const_t<typename std::ranges::range_value_t<Range>::first_type>;

template <class Range>
using RangeMappedT = typename std::ranges::range_value_t<Range>::second_type;

}  // namespace flat_map_internal

// Deduction guides for `flat_map`. They are modeled after the guides for
// `std::flat_map`, but take a single container of pairs and default to
// `std::less<>` like `flat_map` itself. As for `std::flat_map`, a const key in
// the input's pair type (e.g. from `std::map` iterators) is dropped.
//
// Example usage:
//   std::vector<std::pair<int, std::string>> v = ...;
//   base::flat_map m1(v);                    // flat_map<int, std::string>
//   base::flat_map m2(v.begin(), v.end());   // flat_map<int, std::string>
//   base::flat_map m3(std::from_range, v);   // flat_map<int, std::string>
//   base::flat_map m4 = {std::pair(1, 'a')}; // flat_map<int, char>
//
// Construction from a container, an iterator pair, a range or an initializer
// list does not require sorted or unique input; duplicate keys are discarded,
// keeping only the first. The container guides reject views such as
// `base::span`, so that CTAD never silently deduces a `flat_map` that doesn't
// own its elements.
//
// As for `flat_set`, the container guides return the container as
// `std::type_identity_t<Container>`. This changes nothing for `flat_map`
// itself, but keeps CTAD working through alias templates whose container
// defaults to one built from the key and mapped types (see flat_set.h).
template <class Container, class Compare = std::less<>>
  requires(std::ranges::input_range<Container> &&
           !std::ranges::view<Container> &&
           std::invocable<const Compare&,
                          const typename Container::value_type::first_type&,
                          const typename Container::value_type::first_type&>)
flat_map(Container, Compare = Compare())
    -> flat_map<typename Container::value_type::first_type,
                typename Container::value_type::second_type,
                Compare,
                std::type_identity_t<Container>>;

template <class Container, class Compare = std::less<>>
  requires(std::ranges::input_range<Container> &&
           !std::ranges::view<Container> &&
           std::invocable<const Compare&,
                          const typename Container::value_type::first_type&,
                          const typename Container::value_type::first_type&>)
flat_map(sorted_unique_t, Container, Compare = Compare())
    -> flat_map<typename Container::value_type::first_type,
                typename Container::value_type::second_type,
                Compare,
                std::type_identity_t<Container>>;

template <class InputIterator, class Compare = std::less<>>
  requires(std::input_iterator<InputIterator>)
flat_map(InputIterator, InputIterator, Compare = Compare())
    -> flat_map<flat_map_internal::IterKeyT<InputIterator>,
                flat_map_internal::IterMappedT<InputIterator>,
                Compare>;

template <class InputIterator, class Compare = std::less<>>
  requires(std::input_iterator<InputIterator>)
flat_map(sorted_unique_t, InputIterator, InputIterator, Compare = Compare())
    -> flat_map<flat_map_internal::IterKeyT<InputIterator>,
                flat_map_internal::IterMappedT<InputIterator>,
                Compare>;

template <class Range, class Compare = std::less<>>
  requires(std::ranges::input_range<Range>)
flat_map(std::from_range_t, Range&&, Compare = Compare())
    -> flat_map<flat_map_internal::RangeKeyT<Range>,
                flat_map_internal::RangeMappedT<Range>,
                Compare>;

template <class Range, class Compare = std::less<>>
  requires(std::ranges::input_range<Range>)
flat_map(std::from_range_t, sorted_unique_t, Range&&, Compare = Compare())
    -> flat_map<flat_map_internal::RangeKeyT<Range>,
                flat_map_internal::RangeMappedT<Range>,
                Compare>;

template <class Key, class Mapped, class Compare = std::less<>>
flat_map(std::initializer_list<std::pair<Key, Mapped>>, Compare = Compare())
    -> flat_map<Key, Mapped, Compare>;

template <class Key, class Mapped, class Compare = std::less<>>
flat_map(sorted_unique_t,
         std::initializer_list<std::pair<Key, Mapped>>,
         Compare = Compare()) -> flat_map<Key, Mapped, Compare>;

}  // namespace base

#endif  // BASE_CONTAINERS_FLAT_MAP_H_
