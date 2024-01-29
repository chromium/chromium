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
  Restricted(base::PassKey<Manager>) {}
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

  Manager(ExplicitConstruction) : restricted_(base::PassKey<Manager>()) {}
  Manager(UniformInitialization) : restricted_({}) {}
  Manager(CopiedKey) : restricted_(ConstructWithCopiedPassKey({})) {}
  Manager(MovedKey) : restricted_(ConstructWithMovedPassKey({})) {}

 private:
  Restricted restricted_;
};

static_assert(std::constructible_from<Manager, Manager::ExplicitConstruction>);
static_assert(std::constructible_from<Manager, Manager::UniformInitialization>);
static_assert(std::constructible_from<Manager, Manager::CopiedKey>);
static_assert(std::constructible_from<Manager, Manager::MovedKey>);

}  // namespace
}  // namespace base
