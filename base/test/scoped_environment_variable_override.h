// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_ENVIRONMENT_VARIABLE_OVERRIDE_H_
#define BASE_TEST_SCOPED_ENVIRONMENT_VARIABLE_OVERRIDE_H_

#include <memory>
#include <string>

namespace base {

class Environment;

namespace test {

// Helper class to override |variable_name| environment variable to |value| for
// the lifetime of this class. Upon destruction, the previous value is restored.
class ScopedEnvironmentVariableOverride final {
 public:
  ScopedEnvironmentVariableOverride(const std::string& variable_name,
                                    const std::string& value);
  // Unset the variable.
  explicit ScopedEnvironmentVariableOverride(const std::string& variable_name);
  ~ScopedEnvironmentVariableOverride();

  base::Environment* GetEnv() { return environment_.get(); }
  bool IsOverridden() { return overridden_; }
  bool WasSet() { return was_set_; }

 private:
  ScopedEnvironmentVariableOverride(const std::string& variable_name,
                                    const std::string& value,
                                    bool unset_var);
  std::unique_ptr<Environment> environment_;
  std::string variable_name_;
  bool overridden_;
  bool was_set_;
  std::string old_value_;
};

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_SCOPED_ENVIRONMENT_VARIABLE_OVERRIDE_H_
