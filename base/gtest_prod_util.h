// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GTEST_PROD_UTIL_H_
#define BASE_GTEST_PROD_UTIL_H_

#include "base/base_export.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

// This is a wrapper for gtest's FRIEND_TEST macro that friends
// test with all possible prefixes. This is very helpful when changing the test
// prefix, because the friend declarations don't need to be updated.
//
// Example usage:
//
// class MyClass {
//  private:
//   void MyMethod();
//   FRIEND_TEST_ALL_PREFIXES(MyClassTest, TestName);
// };
#define FRIEND_TEST_ALL_PREFIXES(test_case_name, test_name) \
  FRIEND_TEST(test_case_name, test_name); \
  FRIEND_TEST(test_case_name, DISABLED_##test_name); \
  FRIEND_TEST(test_case_name, FLAKY_##test_name)

// C++ compilers will refuse to compile the following code:
//
// namespace foo {
// class MyClass {
//  private:
//   FRIEND_TEST_ALL_PREFIXES(MyClassTest, TestName);
//   bool private_var;
// };
// }  // namespace foo
//
// class MyClassTest::TestName() {
//   foo::MyClass foo_class;
//   foo_class.private_var = true;
// }
//
// Unless you forward declare MyClassTest::TestName outside of namespace foo.
// Use FORWARD_DECLARE_TEST to do so for all possible prefixes.
//
// Example usage:
//
// FORWARD_DECLARE_TEST(MyClassTest, TestName);
//
// namespace foo {
// class MyClass {
//  private:
//   FRIEND_TEST_ALL_PREFIXES(::MyClassTest, TestName);  // NOTE use of ::
//   bool private_var;
// };
// }  // namespace foo
//
// class MyClassTest::TestName() {
//   foo::MyClass foo_class;
//   foo_class.private_var = true;
// }

#define FORWARD_DECLARE_TEST(test_case_name, test_name) \
  class test_case_name##_##test_name##_Test; \
  class test_case_name##_##DISABLED_##test_name##_Test; \
  class test_case_name##_##FLAKY_##test_name##_Test

namespace base::internal {

// Returns true if executing within the context of a death test child process.
// This is an internal utility. You do not want to call this. This is provided
// for the sole purpose of suppressing expensive diagnostic logging in these
// child processes, as this logging is ordinarily not exposed to developers.
bool InDeathTestChild();

// Sets a function that may be called to detect whether or not execution is
// within the context of a death test child process. You do not want to call
// this. This is provided so that base::TestSuite can make Google Test's
// InDeathTestChild implementation available for very specific use in production
// code; see above.
using InDeathTestChildFn = bool (*)();
BASE_EXPORT void SetInDeathTestChildFn(
    InDeathTestChildFn in_death_test_child_fn);

}  // namespace base::internal

#endif  // BASE_GTEST_PROD_UTIL_H_
