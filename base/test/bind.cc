// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// A helper class for MakeExpectedRunClosure() that fails if it is
// destroyed without Run() having been called.  This class may be used
// from multiple threads as long as Run() is called at most once
// before destruction.
class RunChecker {
 public:
  explicit RunChecker(const Location& location,
                      std::string_view message,
                      bool is_repeating)
      : location_(location), message_(message), is_repeating_(is_repeating) {}

  ~RunChecker() {
    if (!called_) {
      ADD_FAILURE_AT(location_.file_name(), location_.line_number())
          << message_;
    }
  }

  void Run() {
    DCHECK(is_repeating_ || !called_);
    called_ = true;
  }

 private:
  const Location location_;
  const std::string message_;
  const bool is_repeating_;
  bool called_ = false;
};

}  // namespace

OnceClosure MakeExpectedRunClosure(const Location& location,
                                   std::string_view message) {
  return BindOnce(&RunChecker::Run,
                  Owned(new RunChecker(location, message, false)));
}

RepeatingClosure MakeExpectedRunAtLeastOnceClosure(const Location& location,
                                                   std::string_view message) {
  return BindRepeating(&RunChecker::Run,
                       Owned(new RunChecker(location, message, true)));
}

RepeatingClosure MakeExpectedNotRunClosure(const Location& location,
                                           std::string_view message) {
  return BindRepeating(
      [](const Location& location, std::string_view message) {
        ADD_FAILURE_AT(location.file_name(), location.line_number()) << message;
      },
      location, std::string(message));
}

}  // namespace base
