// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <tuple>

#include "base/check_deref.h"
#include "base/check_version_internal.h"
#include "base/dcheck_is_on.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/macros/concat.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {

int g_dump_without_crashing_count = 0;

constexpr base::NotFatalUntil kNextMilestone =
    BASE_CONCAT(base::NotFatalUntil::M, BASE_CHECK_NEXT_VERSION_INTERNAL);
constexpr base::NotFatalUntil kCurrentMilestone =
    BASE_CONCAT(base::NotFatalUntil::M, BASE_CHECK_VERSION_INTERNAL);

class ScopedExpectDumpWithoutCrashing {
 public:
  ScopedExpectDumpWithoutCrashing() {
    g_dump_without_crashing_count = 0;
    base::debug::SetDumpWithoutCrashingFunction(&DumpWithoutCrashing);
  }

  ~ScopedExpectDumpWithoutCrashing() {
    EXPECT_EQ(1, g_dump_without_crashing_count);
    base::debug::SetDumpWithoutCrashingFunction(nullptr);
  }

 private:
  static void DumpWithoutCrashing() { ++g_dump_without_crashing_count; }
};

MATCHER_P2(LogErrorMatches, line, expected_msg, "") {
  EXPECT_THAT(arg, testing::HasSubstr(
                       base::StringPrintf("check_unittest.cc(%d)] ", line)));
  if (std::string(expected_msg).find("=~") == 0) {
    EXPECT_THAT(std::string(arg),
                testing::ContainsRegex(std::string(expected_msg).substr(2)));
  } else {
    EXPECT_THAT(std::string(arg), testing::HasSubstr(expected_msg));
  }
  return true;
}

// TODO(pbos): Upstream support for ignoring matchers in gtest when death
// testing is not available.
// Without this we get a compile failure on iOS because
// GTEST_UNSUPPORTED_DEATH_TEST does not compile with a MATCHER as parameter.
#if GTEST_HAS_DEATH_TEST
#define CHECK_MATCHER(line, msg) LogErrorMatches(line, msg)
#else
#define CHECK_MATCHER(line, msg) msg
#endif

// Macro which expects a CHECK to fire with a certain message. If msg starts
// with "=~", it's interpreted as a regular expression.
// Example: EXPECT_CHECK("Check failed: false.", CHECK(false));
//
// Note: Please use the `CheckDeathTest` fixture when using this check.
#if !CHECK_WILL_STREAM()
#define EXPECT_CHECK(msg, check_expr) \
  do {                                \
    EXPECT_CHECK_DEATH(check_expr);   \
  } while (0)
#else
#define EXPECT_CHECK(msg, check_expr) \
  BASE_EXPECT_DEATH(check_expr, CHECK_MATCHER(__LINE__, msg))
#endif  // !CHECK_WILL_STREAM()

// Macro which expects a DCHECK to fire if DCHECKs are enabled.
//
// Note: Please use the `CheckDeathTest` fixture when using this check.
#define EXPECT_DCHECK(msg, check_expr)                                         \
  do {                                                                         \
    if (DCHECK_IS_ON() && logging::LOGGING_DCHECK == logging::LOGGING_FATAL) { \
      BASE_EXPECT_DEATH(check_expr, CHECK_MATCHER(__LINE__, msg));             \
    } else if (DCHECK_IS_ON()) {                                               \
      ScopedExpectDumpWithoutCrashing expect_dump;                             \
      check_expr;                                                              \
    } else {                                                                   \
      check_expr;                                                              \
    }                                                                          \
  } while (0)

#define EXPECT_LOG_ERROR_WITH_FILENAME(expected_file, expected_line, expr,     \
                                       msg)                                    \
  do {                                                                         \
    static bool got_log_message = false;                                       \
    ASSERT_EQ(logging::GetLogMessageHandler(), nullptr);                       \
    logging::SetLogMessageHandler([](int severity, const char* file, int line, \
                                     size_t message_start,                     \
                                     const std::string& str) {                 \
      EXPECT_FALSE(got_log_message);                                           \
      got_log_message = true;                                                  \
      EXPECT_EQ(severity, logging::LOGGING_ERROR);                             \
      EXPECT_EQ(str.substr(message_start), (msg));                             \
      if (std::string_view(expected_file) != "") {                             \
        EXPECT_STREQ(expected_file, file);                                     \
      }                                                                        \
      if (expected_line != -1) {                                               \
        EXPECT_EQ(expected_line, line);                                        \
      }                                                                        \
      return true;                                                             \
    });                                                                        \
    expr;                                                                      \
    EXPECT_TRUE(got_log_message);                                              \
    logging::SetLogMessageHandler(nullptr);                                    \
  } while (0)

#define EXPECT_LOG_ERROR(expected_line, expr, msg) \
  EXPECT_LOG_ERROR_WITH_FILENAME(__FILE__, expected_line, expr, msg)

#define EXPECT_NO_LOG(expr)                                                    \
  do {                                                                         \
    ASSERT_EQ(logging::GetLogMessageHandler(), nullptr);                       \
    logging::SetLogMessageHandler([](int severity, const char* file, int line, \
                                     size_t message_start,                     \
                                     const std::string& str) {                 \
      EXPECT_TRUE(false) << "Unexpected log: " << str;                         \
      return true;                                                             \
    });                                                                        \
    expr;                                                                      \
    logging::SetLogMessageHandler(nullptr);                                    \
  } while (0)

#if defined(OFFICIAL_BUILD)
#if DCHECK_IS_ON()
#define EXPECT_DUMP_WILL_BE_CHECK EXPECT_DCHECK
#else
#define EXPECT_DUMP_WILL_BE_CHECK(expected_string, statement)               \
  do {                                                                      \
    ScopedExpectDumpWithoutCrashing expect_dump;                            \
    EXPECT_LOG_ERROR_WITH_FILENAME(base::Location::Current().file_name(),   \
                                   base::Location::Current().line_number(), \
                                   statement, expected_string "\n");        \
  } while (0)
#endif  // DCHECK_IS_ON()
#else
#define EXPECT_DUMP_WILL_BE_CHECK EXPECT_CHECK
#endif  // defined(OFFICIAL_BUILD)

TEST(CheckDeathTest, Basics) {
  EXPECT_CHECK("Check failed: false. ", CHECK(false));

  EXPECT_CHECK("Check failed: false. foo", CHECK(false) << "foo");

  double a = 2, b = 1;
  EXPECT_CHECK("Check failed: a < b (2.000000 vs. 1.000000)", CHECK_LT(a, b));

  EXPECT_CHECK("Check failed: a < b (2.000000 vs. 1.000000)custom message",
               CHECK_LT(a, b) << "custom message");
}

TEST(CheckDeathTest, PCheck) {
  const char file[] = "/nonexistentfile123";
  std::ignore = fopen(file, "r");
  std::string err =
      logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode());

  EXPECT_CHECK(
      "Check failed: fopen(file, \"r\") != nullptr."
      " : " +
          err,
      PCHECK(fopen(file, "r") != nullptr));

  EXPECT_CHECK(
      "Check failed: fopen(file, \"r\") != nullptr."
      " foo: " +
          err,
      PCHECK(fopen(file, "r") != nullptr) << "foo");

  EXPECT_DCHECK(
      "Check failed: fopen(file, \"r\") != nullptr."
      " : " +
          err,
      DPCHECK(fopen(file, "r") != nullptr));

  EXPECT_DCHECK(
      "Check failed: fopen(file, \"r\") != nullptr."
      " foo: " +
          err,
      DPCHECK(fopen(file, "r") != nullptr) << "foo");
}

TEST(CheckDeathTest, CheckOp) {
  const int a = 1, b = 2;
  // clang-format off
  EXPECT_CHECK("Check failed: a == b (1 vs. 2)", CHECK_EQ(a, b));
  EXPECT_CHECK("Check failed: a != a (1 vs. 1)", CHECK_NE(a, a));
  EXPECT_CHECK("Check failed: b <= a (2 vs. 1)", CHECK_LE(b, a));
  EXPECT_CHECK("Check failed: b < a (2 vs. 1)",  CHECK_LT(b, a));
  EXPECT_CHECK("Check failed: a >= b (1 vs. 2)", CHECK_GE(a, b));
  EXPECT_CHECK("Check failed: a > b (1 vs. 2)",  CHECK_GT(a, b));

  EXPECT_DCHECK("Check failed: a == b (1 vs. 2)", DCHECK_EQ(a, b));
  EXPECT_DCHECK("Check failed: a != a (1 vs. 1)", DCHECK_NE(a, a));
  EXPECT_DCHECK("Check failed: b <= a (2 vs. 1)", DCHECK_LE(b, a));
  EXPECT_DCHECK("Check failed: b < a (2 vs. 1)",  DCHECK_LT(b, a));
  EXPECT_DCHECK("Check failed: a >= b (1 vs. 2)", DCHECK_GE(a, b));
  EXPECT_DCHECK("Check failed: a > b (1 vs. 2)",  DCHECK_GT(a, b));
  // clang-format on

  EXPECT_DUMP_WILL_BE_CHECK("Check failed: a == b (1 vs. 2)",
                            DUMP_WILL_BE_CHECK_EQ(a, b));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: a != a (1 vs. 1)",
                            DUMP_WILL_BE_CHECK_NE(a, a));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: b <= a (2 vs. 1)",
                            DUMP_WILL_BE_CHECK_LE(b, a));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: b < a (2 vs. 1)",
                            DUMP_WILL_BE_CHECK_LT(b, a));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: a >= b (1 vs. 2)",
                            DUMP_WILL_BE_CHECK_GE(a, b));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: a > b (1 vs. 2)",
                            DUMP_WILL_BE_CHECK_GT(a, b));
}

TEST(CheckDeathTest, CheckOpStrings) {
  std::string_view sv = "1";
  base::cstring_view csv = "2";
  std::string s = "3";

  EXPECT_CHECK("Check failed: sv == csv (1 vs. 2)", CHECK_EQ(sv, csv));
  EXPECT_CHECK("Check failed: csv == s (2 vs. 3)", CHECK_EQ(csv, s));
  EXPECT_CHECK("Check failed: sv == s (1 vs. 3)", CHECK_EQ(sv, s));

  EXPECT_DCHECK("Check failed: sv == csv (1 vs. 2)", DCHECK_EQ(sv, csv));
  EXPECT_DCHECK("Check failed: csv == s (2 vs. 3)", DCHECK_EQ(csv, s));
  EXPECT_DCHECK("Check failed: sv == s (1 vs. 3)", DCHECK_EQ(sv, s));
}

TEST(CheckDeathTest, CheckOpPointers) {
  uint8_t arr[] = {3, 2, 1, 0};
  uint8_t* arr_start = &arr[0];
  // Print pointers and not the binary data in `arr`.
#if BUILDFLAG(IS_WIN)
  EXPECT_CHECK(
      "=~Check failed: arr_start != arr_start \\([0-9A-F]+ vs. "
      "[0-9A-F]+\\)",
      CHECK_NE(arr_start, arr_start));
#else
  EXPECT_CHECK(
      "=~Check failed: arr_start != arr_start \\(0x[0-9a-f]+ vs. "
      "0x[0-9a-f]+\\)",
      CHECK_NE(arr_start, arr_start));
#endif
}

TEST(CheckTest, CheckStreamsAreLazy) {
  int called_count = 0;
  int not_called_count = 0;

  auto Called = [&] {
    ++called_count;
    // This returns a non-constant because returning 42 here directly triggers a
    // dead-code warning when streaming to *CHECK(Called()) << NotCalled();
    return called_count >= 0;
  };
  auto NotCalled = [&] {
    ++not_called_count;
    return 42;
  };

  CHECK(Called()) << NotCalled();
  CHECK_EQ(Called(), Called()) << NotCalled();
  PCHECK(Called()) << NotCalled();

  DCHECK(Called()) << NotCalled();
  DCHECK_EQ(Called(), Called()) << NotCalled();
  DPCHECK(Called()) << NotCalled();

  EXPECT_EQ(not_called_count, 0);
#if DCHECK_IS_ON()
  EXPECT_EQ(called_count, 8);
#else
  EXPECT_EQ(called_count, 4);
#endif
}

void DcheckEmptyFunction1() {
  // Provide a body so that Release builds do not cause the compiler to
  // optimize DcheckEmptyFunction1 and DcheckEmptyFunction2 as a single
  // function, which breaks the Dcheck tests below.
  LOG(INFO) << "DcheckEmptyFunction1";
}
void DcheckEmptyFunction2() {}

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
class ScopedDcheckSeverity {
 public:
  explicit ScopedDcheckSeverity(logging::LogSeverity new_severity)
      : old_severity_(logging::LOGGING_DCHECK) {
    logging::LOGGING_DCHECK = new_severity;
  }

  ~ScopedDcheckSeverity() { logging::LOGGING_DCHECK = old_severity_; }

 private:
  logging::LogSeverity old_severity_;
};
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

TEST(CheckDeathTest, Dcheck) {
#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  // DCHECKs are enabled, and LOGGING_DCHECK is mutable, but defaults to
  // non-fatal. Set it to LOGGING_FATAL to get the expected behavior from the
  // rest of this test.
  ScopedDcheckSeverity dcheck_severity(logging::LOGGING_FATAL);
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
  // Release build.
  EXPECT_FALSE(DCHECK_IS_ON());
#elif defined(NDEBUG) && defined(DCHECK_ALWAYS_ON)
  // Release build with real DCHECKS.
  EXPECT_TRUE(DCHECK_IS_ON());
#else
  // Debug build.
  EXPECT_TRUE(DCHECK_IS_ON());
#endif

  EXPECT_DCHECK("Check failed: false. ", DCHECK(false));

  // Produce a consistent error code so that both the main instance of this test
  // and the EXPECT_DEATH invocation below get the same error codes for DPCHECK.
  const char file[] = "/nonexistentfile123";
  std::ignore = fopen(file, "r");
  std::string err =
      logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode());
  EXPECT_DCHECK("Check failed: false. : " + err, DPCHECK(false));
  EXPECT_DCHECK("Check failed: 0 == 1 (0 vs. 1)", DCHECK_EQ(0, 1));

  // Test DCHECK on std::nullptr_t
  const void* p_null = nullptr;
  const void* p_not_null = &p_null;
  DCHECK_EQ(p_null, nullptr);
  DCHECK_EQ(nullptr, p_null);
  DCHECK_NE(p_not_null, nullptr);
  DCHECK_NE(nullptr, p_not_null);

  // Test DCHECK on a scoped enum.
  enum class Animal { DOG, CAT };
  DCHECK_EQ(Animal::DOG, Animal::DOG);
  EXPECT_DCHECK("Check failed: Animal::DOG == Animal::CAT (0 vs. 1)",
                DCHECK_EQ(Animal::DOG, Animal::CAT));

  // Test DCHECK on functions and function pointers.
  struct MemberFunctions {
    void MemberFunction1() {
      // See the comment in DcheckEmptyFunction1().
      LOG(INFO) << "Do not merge with MemberFunction2.";
    }
    void MemberFunction2() {}
  };
  void (MemberFunctions::*mp1)() = &MemberFunctions::MemberFunction1;
  void (MemberFunctions::*mp2)() = &MemberFunctions::MemberFunction2;
  void (*fp1)() = DcheckEmptyFunction1;
  void (*fp2)() = DcheckEmptyFunction2;
  void (*fp3)() = DcheckEmptyFunction1;
  DCHECK_EQ(fp1, fp3);
  DCHECK_EQ(mp1, &MemberFunctions::MemberFunction1);
  DCHECK_EQ(mp2, &MemberFunctions::MemberFunction2);
  EXPECT_DCHECK("=~Check failed: fp1 == fp2 \\(\\w+ vs. \\w+\\)",
                DCHECK_EQ(fp1, fp2));
  EXPECT_DCHECK(
      "Check failed: mp2 == &MemberFunctions::MemberFunction1 (1 vs. 1)",
      DCHECK_EQ(mp2, &MemberFunctions::MemberFunction1));
}

TEST(CheckTest, DcheckReleaseBehavior) {
  int var1 = 1;
  int var2 = 2;
  int var3 = 3;
  int var4 = 4;

  // No warnings about unused variables even though no check fires and DCHECK
  // may or may not be enabled.
  DCHECK(var1) << var2;
  DPCHECK(var1) << var3;
  DCHECK_EQ(var1, 1) << var4;
}

TEST(CheckTest, DCheckEqStatements) {
  bool reached = false;
  if (false)
    DCHECK_EQ(false, true);  // Unreached.
  else
    DCHECK_EQ(true, reached = true);  // Reached, passed.
  ASSERT_EQ(DCHECK_IS_ON() ? true : false, reached);

  if (false)
    DCHECK_EQ(false, true);  // Unreached.
}

TEST(CheckTest, CheckEqStatements) {
  bool reached = false;
  if (false)
    CHECK_EQ(false, true);  // Unreached.
  else
    CHECK_EQ(true, reached = true);  // Reached, passed.
  ASSERT_TRUE(reached);

  if (false)
    CHECK_EQ(false, true);  // Unreached.
}

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
TEST(CheckDeathTest, ConfigurableDCheck) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "gtest_internal_run_death_test")) {
    // This specific test relies on LOGGING_DCHECK not starting out as FATAL,
    // even when run part of death tests (should die only after LOGGING_DCHECK
    // gets reconfigured to FATAL below).
    logging::LOGGING_DCHECK = logging::LOGGING_ERROR;
  } else {
    // Verify that DCHECKs default to non-fatal in configurable-DCHECK builds.
    // Note that we require only that DCHECK is non-fatal by default, rather
    // than requiring that it be exactly INFO, ERROR, etc level.
    EXPECT_LT(logging::LOGGING_DCHECK, logging::LOGGING_FATAL);
  }
  DCHECK(false);

  // Verify that DCHECK* aren't hard-wired to crash on failure.
  logging::LOGGING_DCHECK = logging::LOGGING_ERROR;
  DCHECK(false);
  DCHECK_EQ(1, 2);

  // Verify that DCHECK does crash if LOGGING_DCHECK is set to LOGGING_FATAL.
  logging::LOGGING_DCHECK = logging::LOGGING_FATAL;
  EXPECT_CHECK("Check failed: false. ", DCHECK(false));
  EXPECT_CHECK("Check failed: 1 == 2 (1 vs. 2)", DCHECK_EQ(1, 2));
}

TEST(CheckTest, ConfigurableDCheckFeature) {
  // Initialize FeatureList with and without DcheckIsFatal, and verify the
  // value of LOGGING_DCHECK. Note that we don't require that DCHECK take a
  // specific value when the feature is off, only that it is non-fatal.

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine("DcheckIsFatal", "");
    EXPECT_EQ(logging::LOGGING_DCHECK, logging::LOGGING_FATAL);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine("", "DcheckIsFatal");
    EXPECT_LT(logging::LOGGING_DCHECK, logging::LOGGING_FATAL);
  }

  // The default case is last, so we leave LOGGING_DCHECK in the default state.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine("", "");
    EXPECT_LT(logging::LOGGING_DCHECK, logging::LOGGING_FATAL);
  }
}
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

struct StructWithOstream {
  bool operator==(const StructWithOstream& o) const { return &o == this; }
};
#if CHECK_WILL_STREAM()
std::ostream& operator<<(std::ostream& out, const StructWithOstream&) {
  return out << "ostream";
}
#endif  // CHECK_WILL_STREAM()

struct StructWithToString {
  bool operator==(const StructWithToString& o) const { return &o == this; }
  std::string ToString() const { return "ToString"; }
};

struct StructWithToStringAndOstream {
  bool operator==(const StructWithToStringAndOstream& o) const {
    return &o == this;
  }
  std::string ToString() const { return "ToString"; }
};
#if CHECK_WILL_STREAM()
std::ostream& operator<<(std::ostream& out,
                         const StructWithToStringAndOstream&) {
  return out << "ostream";
}
#endif  // CHECK_WILL_STREAM()

struct StructWithToStringNotStdString {
  struct PseudoString {};

  bool operator==(const StructWithToStringNotStdString& o) const {
    return &o == this;
  }
  PseudoString ToString() const { return PseudoString(); }
};
#if CHECK_WILL_STREAM()
std::ostream& operator<<(std::ostream& out,
                         const StructWithToStringNotStdString::PseudoString&) {
  return out << "ToString+ostream";
}
#endif  // CHECK_WILL_STREAM()

TEST(CheckDeathTest, OstreamVsToString) {
  StructWithOstream a, b;
  EXPECT_CHECK("Check failed: a == b (ostream vs. ostream)", CHECK_EQ(a, b));

  StructWithToString c, d;
  EXPECT_CHECK("Check failed: c == d (ToString vs. ToString)", CHECK_EQ(c, d));

  StructWithToStringAndOstream e, f;
  EXPECT_CHECK("Check failed: e == f (ostream vs. ostream)", CHECK_EQ(e, f));

  StructWithToStringNotStdString g, h;
  EXPECT_CHECK("Check failed: g == h (ToString+ostream vs. ToString+ostream)",
               CHECK_EQ(g, h));
}

// This non-void function is here to make sure that NOTREACHED() is properly
// annotated as [[noreturn]] and does not require a return statement.
int NotReachedInFunction() {
  NOTREACHED();
  // No return statement here.
}

TEST(CheckDeathTest, NotReached) {
  // Expect to be CHECK fatal but with a different error message.
  EXPECT_CHECK("NOTREACHED hit. foo", NOTREACHED() << "foo");
  // This call can't use EXPECT_CHECK as the NOTREACHED happens on a different
  // line.
  EXPECT_DEATH_IF_SUPPORTED(NotReachedInFunction(),
                            CHECK_WILL_STREAM() ? "NOTREACHED hit. " : "");
}

TEST(CheckDeathTest, NotReachedInMigration) {
  EXPECT_CHECK_DEATH(NOTREACHED_IN_MIGRATION());
}

TEST(CheckDeathTest, DumpWillBeCheck) {
  DUMP_WILL_BE_CHECK(true);

  EXPECT_DUMP_WILL_BE_CHECK("Check failed: false. foo",
                            DUMP_WILL_BE_CHECK(false) << "foo");
}

TEST(CheckDeathTest, DumpWillBeNotReachedNoreturn) {
  EXPECT_DUMP_WILL_BE_CHECK("NOTREACHED hit. foo", DUMP_WILL_BE_NOTREACHED()
                                                       << "foo");
}

static const std::string kNotImplementedMessage = "Not implemented reached in ";

TEST(CheckTest, NotImplemented) {
  static const std::string expected_msg =
      kNotImplementedMessage + __PRETTY_FUNCTION__;

#if DCHECK_IS_ON()
  // Expect LOG(ERROR) with streamed params intact.
  EXPECT_LOG_ERROR_WITH_FILENAME(base::Location::Current().file_name(),
                                 base::Location::Current().line_number(),
                                 NOTIMPLEMENTED() << "foo",
                                 expected_msg + "foo\n");
#else
  // Expect nothing.
  EXPECT_NO_LOG(NOTIMPLEMENTED() << "foo");
#endif
}

void NiLogOnce() {
  NOTIMPLEMENTED_LOG_ONCE();
}

TEST(CheckTest, NotImplementedLogOnce) {
  static const std::string expected_msg =
      kNotImplementedMessage + "void (anonymous namespace)::NiLogOnce()\n";

#if DCHECK_IS_ON()
  EXPECT_LOG_ERROR_WITH_FILENAME(base::Location::Current().file_name(),
                                 base::Location::Current().line_number() - 10,
                                 NiLogOnce(), expected_msg);
  EXPECT_NO_LOG(NiLogOnce());
#else
  EXPECT_NO_LOG(NiLogOnce());
  EXPECT_NO_LOG(NiLogOnce());
#endif
}

void NiLogTenTimesWithStream() {
  for (int i = 0; i < 10; ++i) {
    NOTIMPLEMENTED_LOG_ONCE() << " iteration: " << i;
  }
}

TEST(CheckTest, NotImplementedLogOnceWithStreamedParams) {
  static const std::string expected_msg1 =
      kNotImplementedMessage +
      "void (anonymous namespace)::NiLogTenTimesWithStream() iteration: 0\n";

#if DCHECK_IS_ON()
  // Expect LOG(ERROR) with streamed params intact, exactly once.
  EXPECT_LOG_ERROR_WITH_FILENAME(base::Location::Current().file_name(),
                                 base::Location::Current().line_number() - 13,
                                 NiLogTenTimesWithStream(), expected_msg1);
  // A different NOTIMPLEMENTED_LOG_ONCE() call is still logged.
  static const std::string expected_msg2 =
      kNotImplementedMessage + __PRETTY_FUNCTION__ + "tree fish\n";
  EXPECT_LOG_ERROR_WITH_FILENAME(base::Location::Current().file_name(),
                                 base::Location::Current().line_number(),
                                 NOTIMPLEMENTED_LOG_ONCE() << "tree fish",
                                 expected_msg2);

#else
  // Expect nothing.
  EXPECT_NO_LOG(NiLogTenTimesWithStream());
  EXPECT_NO_LOG(NOTIMPLEMENTED_LOG_ONCE() << "tree fish");
#endif
}

// Test CHECK_DEREF of `T*`
TEST(CheckTest, CheckDerefOfPointer) {
  std::string pointee = "not-null";
  std::string* value_pointer = &pointee;

  auto& deref_result = CHECK_DEREF(value_pointer);
  static_assert(std::is_lvalue_reference_v<decltype(deref_result)>);
  // Compare the pointers to ensure they are the same object (and not a copy)
  EXPECT_EQ(&deref_result, &pointee);
  static_assert(std::is_same_v<decltype(deref_result), std::string&>);
}

TEST(CheckDeathTest, CheckDerefOfNullPointer) {
  std::string* null_pointer = nullptr;
  EXPECT_CHECK("Check failed: null_pointer != nullptr. ",
               std::ignore = CHECK_DEREF(null_pointer));
}

// Test CHECK_DEREF of `const T*`
TEST(CheckTest, CheckDerefOfConstPointer) {
  std::string pointee = "not-null";
  const std::string* const_value_pointer = &pointee;

  auto& deref_result = CHECK_DEREF(const_value_pointer);
  static_assert(std::is_lvalue_reference_v<decltype(deref_result)>);
  // Compare the pointers to ensure they are the same object (and not a copy)
  EXPECT_EQ(&deref_result, &pointee);
  static_assert(std::is_same_v<decltype(deref_result), const std::string&>);
}

TEST(CheckDeathTest, CheckDerefOfConstNullPointer) {
  std::string* const_null_pointer = nullptr;
  EXPECT_CHECK("Check failed: const_null_pointer != nullptr. ",
               std::ignore = CHECK_DEREF(const_null_pointer));
}

TEST(CheckDeathTest, CheckNotFatalUntil) {
#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  // This specific death test relies on LOGGING_DCHECK not being FATAL, even
  // when run as part of a death test, as CHECK with a milestone acts like a
  // DCHECK.
  ScopedDcheckSeverity dcheck_severity(logging::LOGGING_ERROR);
#endif

  // Next milestone not yet fatal.
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: false. foo",
                            CHECK(false, kNextMilestone) << "foo");

  // Fatal in current major version.
  EXPECT_CHECK("Check failed: false. foo", CHECK(false, kCurrentMilestone)
                                               << "foo");
}

TEST(CheckDeathTest, CheckOpNotFatalUntil) {
#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  // This specific death test relies on LOGGING_DCHECK not being FATAL, even
  // when run as part of a death test, as CHECK with a milestone acts like a
  // DCHECK.
  ScopedDcheckSeverity dcheck_severity(logging::LOGGING_ERROR);
#endif
  const int a = 1, b = 2;

  // Next milestone not yet fatal.
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: a == b (1 vs. 2)",
                            CHECK_EQ(a, b, kNextMilestone));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: a != a (1 vs. 1)",
                            CHECK_NE(a, a, kNextMilestone));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: b <= a (2 vs. 1)",
                            CHECK_LE(b, a, kNextMilestone));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: b < a (2 vs. 1)",
                            CHECK_LT(b, a, kNextMilestone));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: a >= b (1 vs. 2)",
                            CHECK_GE(a, b, kNextMilestone));
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: a > b (1 vs. 2)",
                            CHECK_GT(a, b, kNextMilestone));

  // Fatal in current major version.
  EXPECT_CHECK("Check failed: a == b (1 vs. 2)",
               CHECK_EQ(a, b, kCurrentMilestone));
  EXPECT_CHECK("Check failed: a != a (1 vs. 1)",
               CHECK_NE(a, a, kCurrentMilestone));
  EXPECT_CHECK("Check failed: b <= a (2 vs. 1)",
               CHECK_LE(b, a, kCurrentMilestone));
  EXPECT_CHECK("Check failed: b < a (2 vs. 1)",
               CHECK_LT(b, a, kCurrentMilestone));
  EXPECT_CHECK("Check failed: a >= b (1 vs. 2)",
               CHECK_GE(a, b, kCurrentMilestone));
  EXPECT_CHECK("Check failed: a > b (1 vs. 2)",
               CHECK_GT(a, b, kCurrentMilestone));
}

TEST(CheckDeathTest, NotReachedNotFatalUntil) {
#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  // This specific death test relies on LOGGING_DCHECK not being FATAL, even
  // when run as part of a death test, as CHECK with a milestone acts like a
  // DCHECK.
  ScopedDcheckSeverity dcheck_severity(logging::LOGGING_ERROR);
#endif

  // Next milestone not yet fatal.
  EXPECT_DUMP_WILL_BE_CHECK("Check failed: false. foo",
                            NOTREACHED(kNextMilestone) << "foo");

  // Fatal in current major version.
  EXPECT_CHECK("Check failed: false. foo", NOTREACHED(kCurrentMilestone)
                                               << "foo");
}

TEST(CheckDeathTest, CorrectSystemErrorUsed) {
#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  // DCHECKs are enabled, and LOGGING_DCHECK is mutable, but defaults to
  // non-fatal. Set it to LOGGING_FATAL to get the expected behavior from the
  // rest of this test.
  ScopedDcheckSeverity dcheck_severity(logging::LOGGING_FATAL);
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  const logging::SystemErrorCode kTestError = 28;
  const std::string kExpectedCheckMessageRegex = base::StrCat(
      {" Check failed: false. ", base::NumberToString(kTestError)});
  const std::string kExpectedPCheckMessageRegex =
      base::StrCat({" Check failed: false. ", base::NumberToString(kTestError),
                    ": ", logging::SystemErrorCodeToString(kTestError)});
  const std::string kExpectedNotreachedMessageRegex =
      base::StrCat({" NOTREACHED hit. ", base::NumberToString(kTestError)});

  auto set_last_error = [](logging::SystemErrorCode error) {
#if BUILDFLAG(IS_WIN)
    ::SetLastError(error);
#else
    errno = error;
#endif
  };

  // Test that the last system error code was used as expected.
  set_last_error(kTestError);
  EXPECT_CHECK(kExpectedCheckMessageRegex,
               CHECK(false) << logging::GetLastSystemErrorCode());

  set_last_error(kTestError);
  EXPECT_DCHECK(kExpectedCheckMessageRegex,
                DCHECK(false) << logging::GetLastSystemErrorCode());

  set_last_error(kTestError);
  EXPECT_CHECK(kExpectedPCheckMessageRegex,
               PCHECK(false) << logging::GetLastSystemErrorCode());

  set_last_error(kTestError);
  EXPECT_DCHECK(kExpectedPCheckMessageRegex,
                DPCHECK(false) << logging::GetLastSystemErrorCode());

  set_last_error(kTestError);
  EXPECT_CHECK(kExpectedNotreachedMessageRegex,
               NOTREACHED() << logging::GetLastSystemErrorCode());
}

}  // namespace
