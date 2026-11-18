// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"

#include <memory>
#include <optional>
#include <string>

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if BUILDFLAG(IS_POSIX)
#include <vector>

#include "base/strings/strcat.h"
#endif

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
  std::optional<std::string> env_value = env->GetVar(kValidEnvironmentVariable);
  EXPECT_THAT(env_value, testing::Optional(testing::Not(testing::IsEmpty())));
}

TEST_F(EnvironmentTest, GetVarReverse) {
  std::unique_ptr<Environment> env(Environment::Create());
  const char kFooUpper[] = "FOO";
  const char kFooLower[] = "foo";

  // Set a variable in UPPER case.
  EXPECT_TRUE(env->SetVar(kFooUpper, kFooLower));

  // And then try to get this variable passing the lower case.
  EXPECT_THAT(env->GetVar(kFooLower),
              testing::Optional(testing::Eq(kFooLower)));

  EXPECT_TRUE(env->UnSetVar(kFooUpper));

  const char kBar[] = "bar";
  // Now do the opposite, set the variable in the lower case.
  EXPECT_TRUE(env->SetVar(kFooLower, kBar));

  // And then try to get this variable passing the UPPER case.
  EXPECT_THAT(env->GetVar(kFooUpper), testing::Optional(testing::Eq(kBar)));

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

  EXPECT_THAT(env->GetVar(kFooUpper),
              testing::Optional(testing::Eq(kFooLower)));
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

#if BUILDFLAG(IS_POSIX)
TEST_F(EnvironmentTest, GetAllVars) {
  Environment env;
  std::optional<std::string> env_value = env.GetVar(kValidEnvironmentVariable);
  ASSERT_TRUE(env_value.has_value());
  EXPECT_THAT(env.GetAllVars(),
              testing::Contains(
                  StrCat({kValidEnvironmentVariable, "=", env_value.value()})));

  constexpr char kTestVar[] = "BASE_ENVIRONMENT_GET_ALL_VARS_TEST";
  EXPECT_FALSE(env.HasVar(kTestVar));

  ASSERT_TRUE(env.SetVar(kTestVar, "foo"));
  EXPECT_THAT(env.GetAllVars(),
              testing::Contains("BASE_ENVIRONMENT_GET_ALL_VARS_TEST=foo"));

  ASSERT_TRUE(env.SetVar(kTestVar, ""));
  EXPECT_THAT(env.GetAllVars(),
              testing::Contains("BASE_ENVIRONMENT_GET_ALL_VARS_TEST="));
  EXPECT_THAT(env.GetAllVars(), testing::Not(testing::Contains(
                                    "BASE_ENVIRONMENT_GET_ALL_VARS_TEST=foo")));

  ASSERT_TRUE(env.UnSetVar(kTestVar));
  EXPECT_THAT(
      env.GetAllVars(),
      testing::Not(testing::Contains("BASE_ENVIRONMENT_GET_ALL_VARS_TEST=")));
}
#endif

}  // namespace base
