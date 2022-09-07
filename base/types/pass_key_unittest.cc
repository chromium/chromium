// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/pass_key.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class Manager;

// May not be created without a PassKey.
class Restricted {
 public:
  Restricted(base::PassKey<Manager>) {}
};

class Manager {
 public:
  enum class ExplicitConstruction { kTag };
  enum class UniformInitialization { kTag };

  Manager(ExplicitConstruction) : restricted_(base::PassKey<Manager>()) {}
  Manager(UniformInitialization) : restricted_({}) {}

 private:
  Restricted restricted_;
};

// If this file compiles, then these test will run and pass. This is useful
// for verifying that the file actually was compiled into the unit test binary.

TEST(PassKeyTest, ExplicitConstruction) {
  Manager manager(Manager::ExplicitConstruction::kTag);
}

TEST(PassKeyTest, UniformInitialization) {
  Manager manager(Manager::UniformInitialization::kTag);
}

}  // namespace
}  // namespace base
