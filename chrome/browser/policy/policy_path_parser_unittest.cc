// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_path_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyPathParserTests : public testing::Test {
 protected:
  void CheckForSubstitution(base::FilePath::StringType test_string,
                            base::FilePath::StringType var_name) {
    base::FilePath::StringType var(test_string);
    base::FilePath::StringType var_result =
        path_parser::ExpandPathVariables(var);
    ASSERT_EQ(var_result.find(var_name), base::FilePath::StringType::npos);
  }
};

TEST_F(PolicyPathParserTests, AllPlatformVariables) {
  // No vars whatsoever no substitution should occur.
  base::FilePath::StringType no_vars(FILE_PATH_LITERAL("//$C/shares"));
  base::FilePath::StringType no_vars_result =
      path_parser::ExpandPathVariables(no_vars);
  ASSERT_EQ(no_vars_result, no_vars);

  // This is unknown variable and shouldn't be substituted.
  base::FilePath::StringType unknown_vars(FILE_PATH_LITERAL("//$C/${buggy}"));
  base::FilePath::StringType unknown_vars_result =
      path_parser::ExpandPathVariables(unknown_vars);
  ASSERT_EQ(unknown_vars_result, unknown_vars);

  // Trim quotes around, but not inside paths. Test against bug 80211.
  base::FilePath::StringType no_quotes(FILE_PATH_LITERAL("//$C/\"a\"/$path"));
  base::FilePath::StringType single_quotes(
      FILE_PATH_LITERAL("'//$C/\"a\"/$path'"));
  base::FilePath::StringType double_quotes(
      FILE_PATH_LITERAL("\"//$C/\"a\"/$path\""));
  base::FilePath::StringType quotes_result =
      path_parser::ExpandPathVariables(single_quotes);
  ASSERT_EQ(quotes_result, no_quotes);
  quotes_result = path_parser::ExpandPathVariables(double_quotes);
  ASSERT_EQ(quotes_result, no_quotes);

  // Both should have been substituted.
  base::FilePath::StringType vars(
      FILE_PATH_LITERAL("${user_name}${machine_name}"));
  base::FilePath::StringType vars_result =
      path_parser::ExpandPathVariables(vars);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${user_name}")),
            base::FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}")),
            base::FilePath::StringType::npos);

  // Should substitute only one instance.
  vars = FILE_PATH_LITERAL("${machine_name}${machine_name}");
  vars_result = path_parser::ExpandPathVariables(vars);
  size_t pos = vars_result.find(FILE_PATH_LITERAL("${machine_name}"));
  ASSERT_NE(pos, base::FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}"), pos+1),
            base::FilePath::StringType::npos);

  vars =FILE_PATH_LITERAL("${user_name}${machine_name}");
  vars_result = path_parser::ExpandPathVariables(vars);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${user_name}")),
            base::FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}")),
            base::FilePath::StringType::npos);

  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${user_name}"),
                       FILE_PATH_LITERAL("${user_name}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${machine_name}"),
                       FILE_PATH_LITERAL("${machine_name}"));
}

#if BUILDFLAG(IS_MAC)

TEST_F(PolicyPathParserTests, MacVariables) {
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${users}"),
                       FILE_PATH_LITERAL("${users}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${documents}"),
                       FILE_PATH_LITERAL("${documents}"));
}

#elif BUILDFLAG(IS_WIN)

TEST_F(PolicyPathParserTests, WinVariables) {
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${documents}"),
                       FILE_PATH_LITERAL("${documents}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${local_app_data}"),
                       FILE_PATH_LITERAL("${local_app_data}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${roaming_app_data}"),
                       FILE_PATH_LITERAL("${roaming_app_data}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${profile}"),
                       FILE_PATH_LITERAL("${profile}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${global_app_data}"),
                       FILE_PATH_LITERAL("${global_app_data}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${program_files}"),
                       FILE_PATH_LITERAL("${program_files}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${windows}"),
                       FILE_PATH_LITERAL("${windows}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${client_name}"),
                       FILE_PATH_LITERAL("${client_name}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${session_name}"),
                       FILE_PATH_LITERAL("${session_name}"));
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace policy
