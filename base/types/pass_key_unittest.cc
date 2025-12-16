// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/pass_key.h"

#include <concepts>
#include <utility>

namespace base {
namespace {

class Manager;

// May not be created without a PassKey.
class Restricted {
 public:
  explicit Restricted(base::PassKey<Manager>) {}
};

Restricted ConstructWithCopiedPassKey(base::PassKey<Manager> key) {
  return Restricted(key);
}

Restricted ConstructWithMovedPassKey(base::PassKey<Manager> key) {
  return Restricted(std::move(key));
}

class Manager {
 public:
  enum class ExplicitConstruction { kTag };
  enum class UniformInitialization { kTag };
  enum class CopiedKey { kTag };
  enum class MovedKey { kTag };

  explicit Manager(ExplicitConstruction)
      : restricted_(base::PassKey<Manager>()) {}
  explicit Manager(UniformInitialization) : restricted_({}) {}
  explicit Manager(CopiedKey) : restricted_(ConstructWithCopiedPassKey({})) {}
  explicit Manager(MovedKey) : restricted_(ConstructWithMovedPassKey({})) {}

 private:
  Restricted restricted_;
};

static_assert(std::constructible_from<Manager, Manager::ExplicitConstruction>);
static_assert(std::constructible_from<Manager, Manager::UniformInitialization>);
static_assert(std::constructible_from<Manager, Manager::CopiedKey>);
static_assert(std::constructible_from<Manager, Manager::MovedKey>);

// For testing multi-arg PassKey and internal concepts.
class A;
class B;
class C;
class D;

class RestrictedMulti {
 public:
  explicit RestrictedMulti(PassKey<A, B, C>) {}
};

// Test single-to-multi conversion.
static_assert(std::is_constructible_v<RestrictedMulti, PassKey<A>>);
static_assert(std::is_constructible_v<RestrictedMulti, PassKey<B>>);
static_assert(std::is_constructible_v<RestrictedMulti, PassKey<C>>);

// Test multi-to-multi subset conversion.
static_assert(std::is_constructible_v<RestrictedMulti, PassKey<A, B>>);
static_assert(std::is_constructible_v<RestrictedMulti, PassKey<A, C>>);
static_assert(std::is_constructible_v<RestrictedMulti, PassKey<B, C>>);

// Test multi-to-multi full conversion.
static_assert(std::is_constructible_v<RestrictedMulti, PassKey<A, B, C>>);
static_assert(!std::is_constructible_v<RestrictedMulti, PassKey<A, B, C, D>>);

// Test internal concepts.
static_assert(pass_key_internal::OneOf<A, A, B, C>);
static_assert(pass_key_internal::OneOf<B, A, B, C>);
static_assert(pass_key_internal::OneOf<C, A, B, C>);
static_assert(!pass_key_internal::OneOf<D, A, B, C>);
static_assert(pass_key_internal::OneOf<A, A>);
static_assert(!pass_key_internal::OneOf<B, A>);

static_assert(pass_key_internal::PairwiseUnique<>);
static_assert(pass_key_internal::PairwiseUnique<A>);
static_assert(pass_key_internal::PairwiseUnique<A, B, C, D>);
static_assert(!pass_key_internal::PairwiseUnique<A, B, C, A>);
static_assert(!pass_key_internal::PairwiseUnique<A, A>);
static_assert(!pass_key_internal::PairwiseUnique<A, B, A>);

}  // namespace
}  // namespace base
