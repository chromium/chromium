// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest EnvironmentTest;

namespace base {

namespace {

// PATH env variable is not set on Fuchsia by default, while PWD is not set on
// Windows.
#if BUILDFLAG(IS_FUCHSIA)
constexpr char kValidEnvironmentVariable[] = "PWD";
#else
constexpr char kValidEnvironmentVariable[] = "PATH";
#endif

}  // namespace

TEST_F(EnvironmentTest, GetVar) {
  std::unique_ptr<Environment> env(Environment::Create());
  std::string env_value;
  EXPECT_TRUE(env->GetVar(kValidEnvironmentVariable, &env_value));
  EXPECT_NE(env_value, "");
}

TEST_F(EnvironmentTest, GetVarReverse) {
  std::unique_ptr<Environment> env(Environment::Create());
  const char kFooUpper[] = "FOO";
  const char kFooLower[] = "foo";

  // Set a variable in UPPER case.
  EXPECT_TRUE(env->SetVar(kFooUpper, kFooLower));

  // And then try to get this variable passing the lower case.
  std::string env_value;
  EXPECT_TRUE(env->GetVar(kFooLower, &env_value));

  EXPECT_STREQ(env_value.c_str(), kFooLower);

  EXPECT_TRUE(env->UnSetVar(kFooUpper));

  const char kBar[] = "bar";
  // Now do the opposite, set the variable in the lower case.
  EXPECT_TRUE(env->SetVar(kFooLower, kBar));

  // And then try to get this variable passing the UPPER case.
  EXPECT_TRUE(env->GetVar(kFooUpper, &env_value));

  EXPECT_STREQ(env_value.c_str(), kBar);

  EXPECT_TRUE(env->UnSetVar(kFooLower));
}

TEST_F(EnvironmentTest, HasVar) {
  std::unique_ptr<Environment> env(Environment::Create());
  EXPECT_TRUE(env->HasVar(kValidEnvironmentVariable));
}

TEST_F(EnvironmentTest, SetVar) {
  std::unique_ptr<Environment> env(Environment::Create());

  const char kFooUpper[] = "FOO";
  const char kFooLower[] = "foo";
  EXPECT_TRUE(env->SetVar(kFooUpper, kFooLower));

  // Now verify that the environment has the new variable.
  EXPECT_TRUE(env->HasVar(kFooUpper));

  std::string var_value;
  EXPECT_TRUE(env->GetVar(kFooUpper, &var_value));
  EXPECT_EQ(var_value, kFooLower);
}

TEST_F(EnvironmentTest, UnSetVar) {
  std::unique_ptr<Environment> env(Environment::Create());

  const char kFooUpper[] = "FOO";
  const char kFooLower[] = "foo";
  // First set some environment variable.
  EXPECT_TRUE(env->SetVar(kFooUpper, kFooLower));

  // Now verify that the environment has the new variable.
  EXPECT_TRUE(env->HasVar(kFooUpper));

  // Finally verify that the environment variable was erased.
  EXPECT_TRUE(env->UnSetVar(kFooUpper));

  // And check that the variable has been unset.
  EXPECT_FALSE(env->HasVar(kFooUpper));
}

}  // namespace base
