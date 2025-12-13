// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_VARIANT_MAP_H_
#define BASE_CONTAINERS_VARIANT_MAP_H_

#include <map>
#include <type_traits>

#include "base/check_op.h"
#include "base/features.h"
#include "base/gtest_prod_util.h"
#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace metrics {
class SubprocessMetricsProvider;
}

namespace mojo {
template <typename ContextType>
class BinderMapWithContext;

class ReceiverSetState;

template <typename ReceiverType, typename ContextType>
class ReceiverSetBase;
}

namespace resource_attribution {
class CPUMeasurementMonitor;
class FakeMemoryMeasurementDelegateFactory;
class MemoryMeasurementDelegate;
class QueryResultMap;
}  // namespace resource_attribution

namespace base {

// Enum to specify which underlying map implementation to use.
enum class MapType {
  // See StdMapVariant
  kStdMap,
  // See FlatHashMapVariant
  kFlatHashMap,
};

// Whether the AbslFlatMapInVariantMap feature is enabled.
BASE_EXPORT bool IsAbslFlatMapInVariantMapEnabled();

// Initializes VariantMap features. See `base::features::Init()`.
BASE_EXPORT void InitializeVariantMapFeatures();

// Class used to evaluate the performance of switching from std::map to
// absl::flat_hash_map in place. This class is used exactly like the underlying
// map implementation for the implemented subset of operations. Constructing can
// be done with a chosen `MapType` or if the default constructor is used the
// variant is chosen automatically with a base::Feature.
//
// Example:
//   VariantMap<int, int> map(MapType::kFlatHashMap);
//   map[4] = 5;
//
// Since this class supports backing map implementations with different
// guarantees users have to assume that the least permissive guarantees apply.
// This includes but is not limited to:
// 1) No specific entry ordering
// 2) No iterator stability through modifications
// 3) No storage stability through modifications
//
// TODO(crbug.com/433462519): Remove this entire class by M145.
template <typename Key, typename Value>
class VariantMap {
 public:
  using StdMapVariant = std::map<Key, Value>;
  using FlatHashMapVariant = absl::flat_hash_map<Key, Value>;
  using value_type = std::pair<const Key, Value>;

  // Iterator class used to erase the difference in backend variant. This should
  // mostly be a drop-in replacement for the iterator types of the underlying
  // map and user code that already uses `auto` for iterator variables should
  // not need to be updated.
  template <bool is_const>
  class IteratorImpl {
   public:
    using StdMapIter =
        std::conditional_t<is_const,
                           typename StdMapVariant::const_iterator,
                           typename StdMapVariant::iterator>;
    using FlatHashMapIter =
        std::conditional_t<is_const,
                           typename FlatHashMapVariant::const_iterator,
                           typename FlatHashMapVariant::iterator>;

    using pointer =
        std::conditional_t<is_const, const value_type*, value_type*>;
    using reference =
        std::conditional_t<is_const, const value_type&, value_type&>;

    explicit IteratorImpl(StdMapIter it) : iter_variant_(it) {}
    explicit IteratorImpl(FlatHashMapIter it) : iter_variant_(it) {}

    // Allow narrowing access from a non-const iterator to a const iterator but
    // not the opposite.
    template <bool other_is_const>
      requires(is_const && !other_is_const)
    explicit IteratorImpl(IteratorImpl<other_is_const> other)
        : iter_variant_(std::visit(
              [](const auto& it) { return decltype(iter_variant_)(it); },
              other.iter_variant_)) {}

    IteratorImpl& operator++() {
      std::visit([](auto& it) { ++it; }, iter_variant_);
      return *this;
    }

    reference operator*() const {
      return std::visit([](auto& it) -> reference { return *it; },
                        iter_variant_);
    }

    pointer operator->() const {
      // Tie the implementation of this operator to operator*.
      // *this accesses this instance * of that uses the * operator.
      // & of that result is a pointer to the value.
      return &(**this);
    }

    bool operator==(const IteratorImpl& other) const {
      // Comparing iterators from different variants should never happen.
      CHECK_EQ(iter_variant_.index(), other.iter_variant_.index());

      return iter_variant_ == other.iter_variant_;
    }

   private:
    template <bool>
    friend class IteratorImpl;

    // For access to `iter_variant_`.
    friend class VariantMap;

    // The variant holds the specific iterator from the underlying map.
    std::variant<StdMapIter, FlatHashMapIter> iter_variant_;
  };

  using iterator = IteratorImpl<false>;
  using const_iterator = IteratorImpl<true>;

  // Protected by PassKey because not intended for general use but only
  // experimenting.
  template <typename ContextType>
  explicit VariantMap(
      base::PassKey<mojo::BinderMapWithContext<ContextType>> passkey)
      : VariantMap() {}

  template <typename ReceiverType, typename ContextType>
  explicit VariantMap(
      base::PassKey<mojo::ReceiverSetBase<ReceiverType, ContextType>> passkey)
      : VariantMap() {}

  explicit VariantMap(base::PassKey<metrics::SubprocessMetricsProvider> passkey)
      : VariantMap() {}

  explicit VariantMap(base::PassKey<mojo::ReceiverSetState> passkey)
      : VariantMap() {}

  explicit VariantMap(
      base::PassKey<resource_attribution::CPUMeasurementMonitor> passkey)
      : VariantMap() {}

  explicit VariantMap(
      base::PassKey<resource_attribution::FakeMemoryMeasurementDelegateFactory>
          passkey)
      : VariantMap() {}

  explicit VariantMap(
      base::PassKey<resource_attribution::MemoryMeasurementDelegate> passkey)
      : VariantMap() {}

  explicit VariantMap(
      base::PassKey<resource_attribution::QueryResultMap> passkey)
      : VariantMap() {}

  size_t size() const {
    return std::visit([](const auto& map) { return map.size(); }, data_);
  }

  bool empty() const {
    return std::visit([](const auto& map) { return map.empty(); }, data_);
  }

  void clear() {
    return std::visit([](auto& map) { return map.clear(); }, data_);
  }

  Value& operator[](const Key& key) {
    return std::visit([&key](auto& map) -> Value& { return map[key]; }, data_);
  }

  Value& at(const Key& key) {
    return std::visit([&key](auto& map) -> Value& { return map.at(key); },
                      data_);
  }

  const Value& at(const Key& key) const {
    return std::visit(
        [&key](const auto& map) -> const Value& { return map.at(key); }, data_);
  }

  template <class... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return std::visit(
        [&](auto& map) {
          auto result = map.emplace(std::forward<Args>(args)...);
          return std::make_pair(iterator(result.first), result.second);
        },
        data_);
  }

  template <class... Args>
  std::pair<iterator, bool> try_emplace(Args&&... args) {
    return std::visit(
        [&](auto& map) {
          auto result = map.try_emplace(std::forward<Args>(args)...);
          return std::make_pair(iterator(result.first), result.second);
        },
        data_);
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    return std::visit(
        [&](auto& map) {
          auto result = map.insert(std::move(value));
          return std::make_pair(iterator(result.first), result.second);
        },
        data_);
  }

  size_t erase(const Key& key) {
    return std::visit([&](auto& map) { return map.erase(key); }, data_);
  }

  void erase(iterator pos) {
    std::visit(
        [&](auto& map) {
          // Get the correct native_iterator out.
          using MapType = typename std::decay<decltype(map)>::type;
          auto native_it =
              std::get<typename MapType::iterator>(pos.iter_variant_);

          map.erase(native_it);
        },
        data_);
  }

  iterator begin() {
    return std::visit([](auto& map) { return iterator(map.begin()); }, data_);
  }
  const_iterator begin() const {
    return const_iterator(const_cast<VariantMap*>(this)->begin());
  }

  iterator end() {
    return std::visit([](auto& map) { return iterator(map.end()); }, data_);
  }
  const_iterator end() const {
    return const_iterator(const_cast<VariantMap*>(this)->end());
  }

  iterator find(const Key& key) {
    return std::visit([&key](auto& map) { return iterator(map.find(key)); },
                      data_);
  }
  const_iterator find(const Key& key) const {
    return const_iterator(const_cast<VariantMap*>(this)->find(key));
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(VariantMapTest, Construction);
  FRIEND_TEST_ALL_PREFIXES(VariantMapTest, Insertion);
  FRIEND_TEST_ALL_PREFIXES(VariantMapTest, At);
  FRIEND_TEST_ALL_PREFIXES(VariantMapTest, Find);
  FRIEND_TEST_ALL_PREFIXES(VariantMapTest, Iteration);
  FRIEND_TEST_ALL_PREFIXES(VariantMapTest, Empty);
  FRIEND_TEST_ALL_PREFIXES(VariantMapTest, Clear);

  using Map = std::variant<StdMapVariant, FlatHashMapVariant>;

  // Constructors private because protected via PassKey.

  explicit VariantMap(MapType type)
      : data_([&]() {
          switch (type) {
            case MapType::kStdMap:
              return Map(std::in_place_index_t<0>());
            case MapType::kFlatHashMap:
              return Map(std::in_place_index_t<1>());

              NOTREACHED();
          }
        }()) {}

  VariantMap()
      : VariantMap(base::IsAbslFlatMapInVariantMapEnabled()
                       ? MapType::kFlatHashMap
                       : MapType::kStdMap) {}

  // The variant that holds one of the two map types.
  Map data_;
};

}  // namespace base

#endif  // BASE_CONTAINERS_VARIANT_MAP_H_
