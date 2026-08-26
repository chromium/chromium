#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
import unittest

# Ensure tools directory is in sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_glic_api_test_registration as checker


class CheckGlicApiTestRegistrationTypeScriptExtractorTest(unittest.TestCase):
    """Tests for extracting test method names from TypeScript test files."""

    def test_extract_ts_tests_basic_sync_and_async(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  async testAsyncMethod() {
    await this.advanceToNextStep();
  }

  testSyncMethod() {
    return 42;
  }
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testAsyncMethod', 'testSyncMethod'},
        )

    def test_extract_ts_tests_with_return_types(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  async testWithPromiseVoid(): Promise<void> {}
  testWithBoolean(): boolean { return true; }
  async testWithCustomType(): Promise<{result: string}> {}
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testWithPromiseVoid', 'testWithBoolean', 'testWithCustomType'},
        )

    def test_extract_ts_tests_with_access_modifiers(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  public async testPublicAsync() {}
  override async testOverrideAsync() {}
  protected testProtectedSync() {}
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testPublicAsync', 'testOverrideAsync', 'testProtectedSync'},
        )

    def test_extract_ts_tests_with_generics(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  async testGenericMethod<T>() {}
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testGenericMethod'},
        )

    def test_extract_ts_tests_arrow_function_properties(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  testArrowAsync = async () => {};
  public testArrowSync = () => {};
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testArrowAsync', 'testArrowSync'},
        )

    def test_extract_ts_tests_single_line_comments_ignored(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  // async testCommentedOut1() {}
  // testCommentedOut2() {}
  const notATest = 1; // testCommentedOut3()
  async testRealMethod() {}
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testRealMethod'},
        )

    def test_extract_ts_tests_multi_line_comments_ignored(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  /*
  async testBlockCommentedOut1() {}
  testBlockCommentedOut2() {}
  */
  /* async testInlineBlock() {} */
  async testRealMethod() {}
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testRealMethod'},
        )

    def test_extract_ts_tests_string_literals_ignored(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  async testRealMethod() {
    const s1 = "testInsideStringLiteral()";
    const s2 = 'testInsideSingleQuotes()';
    const s3 = `testInsideTemplateLiteral()`;
  }
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testRealMethod'},
        )

    def test_extract_ts_tests_ignored_framework_methods(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  async testStepper() {}
  async testRealMethod() {}
}

testMain([MyApiTests]);
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testRealMethod'},
        )

    def test_extract_ts_tests_non_test_methods_ignored(self):
        ts = """
class MyApiTests extends ApiTestFixtureBase {
  override async setUpTest() {}
  override async tearDownTest() {}
  async advanceToNextStep() {}
  createWebClient() {}
  latestValue() {}
  contestWinner() {}
  async testActualTestCase() {}
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testActualTestCase'},
        )

    def test_extract_ts_tests_multiple_classes_in_file(self):
        ts = """
class PrimaryApiTests extends ApiTestFixtureBase {
  async testPrimaryOne() {}
  async testPrimaryTwo() {}
}

class SecondaryDesktopOnlyApiTests extends PrimaryApiTests {
  async testDesktopSpecific() {}
}

class HelperClass {
  helperMethod() {}
}
"""
        self.assertEqual(
            checker.extract_ts_tests(ts),
            {'testPrimaryOne', 'testPrimaryTwo', 'testDesktopSpecific'},
        )


class CheckGlicApiTestRegistrationCppExtractorTest(unittest.TestCase):
    """Tests for extracting test declarations from C++ browser test files."""

    def test_extract_cc_tests_in_proc_browser_test_p(self):
        cc = """
IN_PROC_BROWSER_TEST_P(GlicApiTest, testFoo) {
  ExecuteJsTest();
}
"""
        self.assertEqual(checker.extract_cc_tests(cc), {'testFoo'})

    def test_extract_cc_tests_in_proc_browser_test_f(self):
        cc = """
IN_PROC_BROWSER_TEST_F(GlicFocusBrowserTest, testFocus) {
  ExecuteJsTest();
}
"""
        self.assertEqual(checker.extract_cc_tests(cc), {'testFocus'})

    def test_extract_cc_tests_in_proc_browser_test(self):
        cc = """
IN_PROC_BROWSER_TEST(SimpleBrowserTest, testSimple) {
  ExecuteJsTest();
}
"""
        self.assertEqual(checker.extract_cc_tests(cc), {'testSimple'})

    def test_extract_cc_tests_gtest_macros(self):
        cc = """
TEST(UnitTest, testOne) {}
TEST_F(UnitTestFixture, testTwo) {}
TEST_P(ParameterizedUnitTest, testThree) {}
"""
        self.assertEqual(
            checker.extract_cc_tests(cc),
            {'testOne', 'testTwo', 'testThree'},
        )

    def test_extract_cc_tests_maybe_prefix_stripped(self):
        cc = """
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testConditionallyDisabled DISABLED_testConditionallyDisabled
#else
#define MAYBE_testConditionallyDisabled testConditionallyDisabled
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testConditionallyDisabled) {
  ExecuteJsTest();
}
"""
        self.assertEqual(
            checker.extract_cc_tests(cc),
            {'testConditionallyDisabled'},
        )

    def test_extract_cc_tests_disabled_prefix_stripped(self):
        cc = """
IN_PROC_BROWSER_TEST_P(GlicApiTest, DISABLED_testDisabled) {
  ExecuteJsTest();
}
"""
        self.assertEqual(checker.extract_cc_tests(cc), {'testDisabled'})

    def test_extract_cc_tests_manual_prefix_stripped(self):
        cc = """
IN_PROC_BROWSER_TEST_P(GlicApiTest, MANUAL_testManualOnly) {
  ExecuteJsTest();
}
"""
        self.assertEqual(checker.extract_cc_tests(cc), {'testManualOnly'})

    def test_extract_cc_tests_multiline_formatting(self):
        cc = """
IN_PROC_BROWSER_TEST_P(
    GlicApiTestWithSkillsDisabled,
    testSkillsEnabledToggledAtRuntime) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(
    GlicFocusBrowserTest,
    testMultilineFocus)
{
  ExecuteJsTest();
}
"""
        self.assertEqual(
            checker.extract_cc_tests(cc),
            {'testSkillsEnabledToggledAtRuntime', 'testMultilineFocus'},
        )

    def test_extract_cc_tests_comments_ignored(self):
        cc = """
// IN_PROC_BROWSER_TEST_P(GlicApiTest, testSingleLineCommented) {}
/*
IN_PROC_BROWSER_TEST_P(GlicApiTest, testBlockCommented) {}
*/
IN_PROC_BROWSER_TEST_P(GlicApiTest, testActiveTest) {}
"""
        self.assertEqual(checker.extract_cc_tests(cc), {'testActiveTest'})

    def test_extract_cc_tests_indented_macros(self):
        cc = """
namespace {
  IN_PROC_BROWSER_TEST_F(GlicFocusBrowserTest, testIndentedTwoSpaces) {}
    IN_PROC_BROWSER_TEST_P(GlicApiTest, testIndentedFourSpaces) {}
}
"""
        self.assertEqual(
            checker.extract_cc_tests(cc),
            {'testIndentedTwoSpaces', 'testIndentedFourSpaces'},
        )


class CheckGlicApiTestRegistrationRepoRootTest(unittest.TestCase):
    """Tests for repository root discovery."""

    def test_find_repo_root_with_explicit_path(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            self.assertEqual(checker.find_repo_root(temp_dir), temp_dir)

    def test_find_repo_root_raises_when_no_repo_marker(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            orig_file = checker.__file__
            try:
                checker.__file__ = os.path.join(temp_dir, 'sub', 'script.py')
                with self.assertRaises(RuntimeError) as ctx:
                    checker.find_repo_root()
                self.assertIn('Could not locate Chromium repo root',
                              str(ctx.exception))
            finally:
                checker.__file__ = orig_file


class CheckGlicApiTestRegistrationPairMatchingTest(unittest.TestCase):
    """Tests for matching and comparing TypeScript and C++ test pairs."""

    def setUp(self):
        super().setUp()
        checker.clear_caches()

    def test_memoization_caches_path_mappings(self):
        with tempfile.TemporaryDirectory() as repo_root:
            cc_dir = os.path.join(repo_root, "chrome", "browser", "glic")
            ts_dir = os.path.join(repo_root, "chrome", "test", "data", "webui",
                                  "glic", "browser_tests")
            os.makedirs(cc_dir, exist_ok=True)
            os.makedirs(ts_dir, exist_ok=True)

            cc_file = os.path.join(cc_dir, "sample_test.cc")
            ts_file = os.path.join(ts_dir, "test_browsertest.ts")
            with open(ts_file, "w", encoding="utf-8") as f:
                f.write(
                    '// cc_file_path: chrome/browser/glic/sample_test.cc\n'
                    'class T extends ApiTestFixtureBase { async testA() {} }\n'
                )
            with open(cc_file, "w", encoding="utf-8") as f:
                f.write('class T : public GlicApiBrowserTest {\n'
                        '  T() : GlicApiBrowserTest('
                        'GlicTestJsPath("./test_browsertest.js")) {}\n'
                        '};\n')

            # Initial resolution caches the result.
            self.assertEqual(checker.find_ts_path_from_cc(cc_file, repo_root),
                             (ts_file, None))
            self.assertEqual(checker.find_cc_path_from_ts(ts_file, repo_root),
                             (cc_file, None))

            # Modify files on disk.
            with open(cc_file, "w", encoding="utf-8") as f:
                f.write('// empty')

            # Memoized call returns the cached mapping without re-parsing the file.
            self.assertEqual(checker.find_ts_path_from_cc(cc_file, repo_root),
                             (ts_file, None))

            # Clearing the cache causes re-evaluation from disk.
            checker.clear_caches()
            self.assertEqual(checker.find_ts_path_from_cc(cc_file, repo_root),
                             (None, None))

    def test_check_test_pair_perfect_match(self):
        ts = """class T extends ApiTestFixtureBase {
  async testA() {}
  async testB() {}
}"""
        cc = """IN_PROC_BROWSER_TEST_P(T, testA) {}
IN_PROC_BROWSER_TEST_P(T, testB) {}"""
        with tempfile.TemporaryDirectory() as temp_dir:
            ts_file = os.path.join(temp_dir, "test.ts")
            cc_file = os.path.join(temp_dir, "test.cc")
            with open(ts_file, "w", encoding="utf-8") as f:
                f.write(ts)
            with open(cc_file, "w", encoding="utf-8") as f:
                f.write(cc)
            missing, ts_tests, cc_tests = checker.check_test_pair(
                cc_file, ts_file)
            self.assertEqual(missing, set())
            self.assertEqual(ts_tests, {"testA", "testB"})
            self.assertEqual(cc_tests, {"testA", "testB"})

    def test_check_test_pair_missing_registration_detected(self):
        ts = """class T extends ApiTestFixtureBase {
  async testA() {}
  async testB() {}
  async testC() {}
}"""
        cc = """IN_PROC_BROWSER_TEST_P(T, testA) {}"""
        with tempfile.TemporaryDirectory() as temp_dir:
            ts_file = os.path.join(temp_dir, "test.ts")
            cc_file = os.path.join(temp_dir, "test.cc")
            with open(ts_file, "w", encoding="utf-8") as f:
                f.write(ts)
            with open(cc_file, "w", encoding="utf-8") as f:
                f.write(cc)
            missing, _, _ = checker.check_test_pair(cc_file, ts_file)
            self.assertEqual(missing, {"testB", "testC"})

    def test_check_test_pair_cc_has_additional_tests_is_allowed(self):
        ts = """class T extends ApiTestFixtureBase {
  async testA() {}
}"""
        cc = """IN_PROC_BROWSER_TEST_P(T, testA) {}
IN_PROC_BROWSER_TEST_P(T, testCppOnly) {}"""
        with tempfile.TemporaryDirectory() as temp_dir:
            ts_file = os.path.join(temp_dir, "test.ts")
            cc_file = os.path.join(temp_dir, "test.cc")
            with open(ts_file, "w", encoding="utf-8") as f:
                f.write(ts)
            with open(cc_file, "w", encoding="utf-8") as f:
                f.write(cc)
            missing, ts_tests, cc_tests = checker.check_test_pair(
                cc_file, ts_file)
            self.assertEqual(missing, set())
            self.assertEqual(ts_tests, {"testA"})
            self.assertEqual(cc_tests, {"testA", "testCppOnly"})

    def test_find_ts_path_from_cc_various_constructor_formats(self):
        with tempfile.TemporaryDirectory() as mock_repo_root:
            webui_dir = os.path.join(mock_repo_root, "chrome", "test", "data",
                                     "webui", "glic", "browser_tests")
            os.makedirs(webui_dir, exist_ok=True)
            fake_ts = os.path.join(webui_dir, "fake_sample_browsertest.ts")
            with open(fake_ts, "w", encoding="utf-8") as f:
                f.write("// fake")

            cc_dir = os.path.join(mock_repo_root, "chrome", "browser", "glic")
            os.makedirs(cc_dir, exist_ok=True)

            cc_single_line = (
                'class F : public GlicApiBrowserTest {\n'
                '  F() : GlicApiBrowserTest('
                'GlicTestJsPath("./fake_sample_browsertest.js")) {}\n'
                '};\n')
            cc_multiline = (
                'class F : public GlicApiBrowserTest {\n'
                '  F() : GlicApiBrowserTest(\n'
                '      GlicTestJsPath("./fake_sample_browsertest.js")) {}\n'
                '};\n')
            cc_actor_base = (
                'class F : public GlicActorFunctionalBrowserTestBase {\n'
                '  F() : GlicActorFunctionalBrowserTestBase('
                'GlicTestJsPath("./fake_sample_browsertest.js")) {}\n'
                '};\n')

            for idx, cc_content in enumerate(
                (cc_single_line, cc_multiline, cc_actor_base)):
                temp_cc = os.path.join(cc_dir, f"sample_{idx}_test.cc")
                with open(temp_cc, "w", encoding="utf-8") as f:
                    f.write(cc_content)
                ts_path, err = checker.find_ts_path_from_cc(
                    temp_cc, mock_repo_root)
                self.assertIsNone(err)
                self.assertEqual(ts_path, fake_ts)

    def test_find_ts_path_from_cc_ignores_non_cpp_files(self):
        with tempfile.TemporaryDirectory() as mock_repo_root:
            py_file = os.path.join(mock_repo_root, "script.py")
            with open(py_file, "w", encoding="utf-8") as f:
                f.write('# GlicTestJsPath("./fake.js")\n')
            self.assertEqual(
                checker.find_ts_path_from_cc(py_file, mock_repo_root),
                (None, None))

    def test_find_ts_path_from_cc_ignores_non_test_cpp_files(self):
        with tempfile.TemporaryDirectory() as mock_repo_root:
            service_cc = os.path.join(mock_repo_root, "glic_service.cc")
            with open(service_cc, "w", encoding="utf-8") as f:
                f.write('GlicTestJsPath("./fake_sample_browsertest.js")\n')
            self.assertEqual(
                checker.find_ts_path_from_cc(service_cc, mock_repo_root),
                (None, None))

    def test_find_ts_path_from_cc_ignores_commented_bundles(self):
        with tempfile.TemporaryDirectory() as mock_repo_root:
            webui_dir = os.path.join(mock_repo_root, "chrome", "test", "data",
                                     "webui", "glic", "browser_tests")
            os.makedirs(webui_dir, exist_ok=True)
            fake_ts = os.path.join(webui_dir, "fake.ts")
            with open(fake_ts, "w", encoding="utf-8") as f:
                f.write("// fake")

            cc_file = os.path.join(mock_repo_root, "sample_test.cc")
            with open(cc_file, "w", encoding="utf-8") as f:
                f.write('// GlicTestJsPath("./fake.js")\n'
                        '/* GlicTestJsPath("./fake.js") */\n')
            self.assertEqual(
                checker.find_ts_path_from_cc(cc_file, mock_repo_root),
                (None, None))

    def test_find_ts_path_from_cc_returns_error_on_missing_ts_file(self):
        with tempfile.TemporaryDirectory() as mock_repo_root:
            cc_file = os.path.join(mock_repo_root, "sample_test.cc")
            with open(cc_file, "w", encoding="utf-8") as f:
                f.write('GlicTestJsPath("./non_existent_browsertest.js")\n')
            ts_path, err = checker.find_ts_path_from_cc(
                cc_file, mock_repo_root)
            self.assertIsNone(ts_path)
            self.assertIn("corresponding TypeScript file does not exist", err)

    def test_find_cc_path_from_ts_ignores_support_files(self):
        with tempfile.TemporaryDirectory() as mock_repo_root:
            webui_dir = os.path.join(mock_repo_root, "chrome", "test", "data",
                                     "webui", "glic", "browser_tests")
            os.makedirs(webui_dir, exist_ok=True)
            support_file = os.path.join(webui_dir, "browser_test_base.ts")
            with open(support_file, "w", encoding="utf-8") as f:
                f.write("export function helper() {}\n"
                        "export function testStepper() {}\n")
            self.assertEqual(
                checker.find_cc_path_from_ts(support_file, mock_repo_root),
                (None, None))

    def test_resolve_test_pair_from_cpp_and_ts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = os.path.join(temp_dir, "repo")
            cc_dir = os.path.join(repo_root, "chrome", "browser", "glic")
            ts_dir = os.path.join(repo_root, "chrome", "test", "data", "webui",
                                  "glic", "browser_tests")
            os.makedirs(cc_dir, exist_ok=True)
            os.makedirs(ts_dir, exist_ok=True)

            cc_path = os.path.normpath(
                os.path.join(cc_dir, "test_browsertest.cc"))
            ts_path = os.path.normpath(
                os.path.join(ts_dir, "test_browsertest.ts"))

            with open(cc_path, "w", encoding="utf-8") as f:
                f.write(
                    'class T : public GlicApiBrowserTest {\n'
                    '  T() : GlicApiBrowserTest(GlicTestJsPath("./test_browsertest.js")) {}\n'
                    '};\n'
                    'IN_PROC_BROWSER_TEST_F(T, testFoo) {}\n')
            with open(ts_path, "w", encoding="utf-8") as f:
                f.write(
                    '// cc_file_path: chrome/browser/glic/test_browsertest.cc\n'
                    'class T extends ApiTestFixtureBase {\n'
                    '  async testFoo() {}\n'
                    '}\n')

            # Resolve from absolute C++ and TS paths.
            self.assertEqual(
                checker.resolve_test_pair(cc_path, repo_root),
                (cc_path, ts_path),
            )
            self.assertEqual(
                checker.resolve_test_pair(ts_path, repo_root),
                (cc_path, ts_path),
            )

            # Resolve from relative paths (native and Unix-style).
            rel_cc = os.path.relpath(cc_path, repo_root)
            rel_ts = os.path.relpath(ts_path, repo_root)
            self.assertEqual(
                checker.resolve_test_pair(rel_cc, repo_root),
                (cc_path, ts_path),
            )
            self.assertEqual(
                checker.resolve_test_pair(rel_cc.replace('\\', '/'),
                                          repo_root),
                (cc_path, ts_path),
            )
            self.assertEqual(
                checker.resolve_test_pair(rel_ts, repo_root),
                (cc_path, ts_path),
            )
            self.assertEqual(
                checker.resolve_test_pair(rel_ts.replace('\\', '/'),
                                          repo_root),
                (cc_path, ts_path),
            )

            # Non-matching path returns None.
            other_cc = os.path.join(repo_root, "chrome", "browser", "other.cc")
            self.assertIsNone(checker.resolve_test_pair(other_cc, repo_root))


class CheckGlicApiTestRegistrationFileTypeTest(unittest.TestCase):
    """Tests for is_ts_test_file and is_cc_test_file helpers."""

    def test_is_ts_test_file(self):
        self.assertTrue(checker.is_ts_test_file("glic_api_browsertest.ts"))
        self.assertTrue(checker.is_ts_test_file("glic_focus_test.ts"))
        self.assertTrue(checker.is_ts_test_file("path/to/my_unittest.ts"))
        self.assertTrue(checker.is_ts_test_file("test.ts"))

        self.assertFalse(checker.is_ts_test_file("browser_test_base.ts"))
        self.assertFalse(checker.is_ts_test_file("minimal_client.ts"))
        self.assertFalse(
            checker.is_ts_test_file("glic_browser_test_android.ts"))
        self.assertFalse(checker.is_ts_test_file("test.html"))
        self.assertFalse(checker.is_ts_test_file("test.js"))

    def test_is_cc_test_file(self):
        self.assertTrue(checker.is_cc_test_file("glic_api_browsertest.cc"))
        self.assertTrue(checker.is_cc_test_file("glic_focus_unittest.cc"))
        self.assertTrue(checker.is_cc_test_file("path/to/my_test.cc"))
        self.assertTrue(checker.is_cc_test_file("test.cc"))

        self.assertFalse(checker.is_cc_test_file("glic_service.cc"))
        self.assertFalse(checker.is_cc_test_file("glic_api_browsertest.h"))
        self.assertFalse(checker.is_cc_test_file("test.cpp"))
        self.assertFalse(checker.is_cc_test_file("test.h"))


class CheckGlicApiTestRegistrationCppHandlingTest(unittest.TestCase):
    """Tests for handling C++ test files in get_test_pairs_to_check."""

    def setUp(self):
        super().setUp()
        checker.clear_caches()

    def test_get_test_pairs_when_only_cpp_test_file_edited(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, "repo")
            cc_dir = os.path.join(repo_mock, "chrome", "browser", "glic")
            ts_dir = os.path.join(repo_mock, "chrome", "test", "data", "webui",
                                  "glic", "browser_tests")
            os.makedirs(cc_dir, exist_ok=True)
            os.makedirs(ts_dir, exist_ok=True)

            cc_file = os.path.join(cc_dir, "glic_focus_browsertest.cc")
            ts_file = os.path.join(ts_dir, "glic_focus_browsertest.ts")

            with open(cc_file, "w", encoding="utf-8") as f:
                f.write('class F : public GlicApiBrowserTest {\n'
                        '  F() : GlicApiBrowserTest('
                        'GlicTestJsPath("./glic_focus_browsertest.js")) {}\n'
                        '};\n'
                        'IN_PROC_BROWSER_TEST_F(F, testA) {}\n')
            with open(ts_file, "w", encoding="utf-8") as f:
                f.write(
                    '// cc_file_path: chrome/browser/glic/glic_focus_browsertest.cc\n'
                    'class F extends ApiTestFixtureBase {\n'
                    '  async testA() {}\n'
                    '}\n')

            # Pass only the C++ file.
            pairs, errors = checker.get_test_pairs_to_check([cc_file],
                                                            repo_mock)
            self.assertEqual(errors, [])
            self.assertEqual(pairs, [(cc_file, ts_file)])

    def test_get_test_pairs_when_non_test_cpp_file_edited(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, "repo")
            cc_dir = os.path.join(repo_mock, "chrome", "browser", "glic")
            os.makedirs(cc_dir, exist_ok=True)

            service_cc = os.path.join(cc_dir, "glic_service.cc")
            with open(service_cc, "w", encoding="utf-8") as f:
                f.write('void InitializeGlic() {}\n')

            pairs, errors = checker.get_test_pairs_to_check([service_cc],
                                                            repo_mock)
            self.assertEqual(errors, [])
            self.assertEqual(pairs, [])

    def test_get_test_pairs_when_cpp_test_without_glic_bundle_edited(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, "repo")
            cc_dir = os.path.join(repo_mock, "chrome", "browser", "glic")
            os.makedirs(cc_dir, exist_ok=True)

            unit_test_cc = os.path.join(cc_dir, "glic_profile_unittest.cc")
            with open(unit_test_cc, "w", encoding="utf-8") as f:
                f.write('TEST(GlicProfileTest, SimpleTest) {}\n')

            pairs, errors = checker.get_test_pairs_to_check([unit_test_cc],
                                                            repo_mock)
            self.assertEqual(errors, [])
            self.assertEqual(pairs, [])


class CheckGlicApiTestRegistrationTsHandlingTest(unittest.TestCase):
    """Tests for handling TypeScript test and support files."""

    def setUp(self):
        super().setUp()
        checker.clear_caches()

    def test_get_test_pairs_ignores_support_ts_file(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, "repo")
            ts_dir = os.path.join(repo_mock, "chrome", "test", "data", "webui",
                                  "glic", "browser_tests")
            os.makedirs(ts_dir, exist_ok=True)

            support_file = os.path.join(ts_dir, "browser_test_base.ts")
            with open(support_file, "w", encoding="utf-8") as f:
                f.write('export function testStepper() {}\n')

            pairs, errors = checker.get_test_pairs_to_check([support_file],
                                                            repo_mock)
            self.assertEqual(errors, [])
            self.assertEqual(pairs, [])

    def test_find_cc_path_from_ts_empty_test_file_requires_comment(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, "repo")
            ts_dir = os.path.join(repo_mock, "chrome", "test", "data", "webui",
                                  "glic", "browser_tests")
            os.makedirs(ts_dir, exist_ok=True)

            empty_test_ts = os.path.join(ts_dir, "empty_browsertest.ts")
            with open(empty_test_ts, "w", encoding="utf-8") as f:
                f.write('// Empty test file with 0 test methods\n')

            cc_path, err = checker.find_cc_path_from_ts(
                empty_test_ts, repo_mock)
            self.assertIsNone(cc_path)
            self.assertIn("missing the required C++ test reference comment",
                          err)


class CheckGlicApiTestRegistrationCliAndRepoTest(unittest.TestCase):
    """Tests CLI execution and repository-wide integration."""

    def setUp(self):
        super().setUp()
        checker.clear_caches()

    def test_cli_success_on_valid_files(self):
        repo_root = checker.find_repo_root()
        focus_cc = os.path.join(repo_root, "chrome", "browser", "glic", "host",
                                "glic_focus_browsertest.cc")
        ret = checker.main(
            ["--check-only", "--quiet", "--repo-root", repo_root, focus_cc])
        self.assertEqual(ret, 0)

    def test_cli_when_checker_script_itself_is_passed(self):
        repo_root = checker.find_repo_root()
        checker_py = os.path.join(repo_root, "chrome", "browser", "glic",
                                  "tools",
                                  "check_glic_api_test_registration.py")
        ret = checker.main(
            ["--check-only", "--quiet", "--repo-root", repo_root, checker_py])
        self.assertEqual(ret, 0)

    def test_cli_failure_on_missing_registration(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, "repo")
            cc_dir = os.path.join(repo_mock, "chrome", "browser", "glic")
            ts_dir = os.path.join(repo_mock, "chrome", "test", "data", "webui",
                                  "glic", "browser_tests")
            os.makedirs(cc_dir, exist_ok=True)
            os.makedirs(ts_dir, exist_ok=True)

            cc_file = os.path.join(cc_dir, "mock_browsertest.cc")
            ts_file = os.path.join(ts_dir, "mock_browsertest.ts")

            with open(cc_file, "w", encoding="utf-8") as f:
                f.write(
                    'class M : public GlicApiBrowserTest {\n'
                    '  M() : GlicApiBrowserTest(GlicTestJsPath("./mock_browsertest.js")) {}\n'
                    '};\n'
                    'IN_PROC_BROWSER_TEST_F(M, testRegistered) {}\n')
            with open(ts_file, "w", encoding="utf-8") as f:
                f.write(
                    '// cc_file_path: chrome/browser/glic/mock_browsertest.cc\n'
                    'class M extends ApiTestFixtureBase {\n'
                    '  async testRegistered() {}\n'
                    '  async testMissing() {}\n'
                    '}\n')

            ret = checker.main(
                ["--check-only", "--repo-root", repo_mock, cc_file])
            self.assertEqual(ret, 1)

    def test_cli_failure_on_ts_missing_cpp_comment(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, "repo")
            ts_dir = os.path.join(repo_mock, "chrome", "test", "data", "webui",
                                  "glic", "browser_tests")
            os.makedirs(ts_dir, exist_ok=True)
            ts_file = os.path.join(ts_dir, "mock_browsertest.ts")

            with open(ts_file, "w", encoding="utf-8") as f:
                f.write('class M extends ApiTestFixtureBase {\n'
                        '  async testFoo() {}\n'
                        '}\n')

            ret = checker.main(
                ["--check-only", "--repo-root", repo_mock, ts_file])
            self.assertEqual(ret, 1)

    def test_cli_failure_on_ts_invalid_cpp_path(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, "repo")
            ts_dir = os.path.join(repo_mock, "chrome", "test", "data", "webui",
                                  "glic", "browser_tests")
            os.makedirs(ts_dir, exist_ok=True)
            ts_file = os.path.join(ts_dir, "mock_browsertest.ts")

            with open(ts_file, "w", encoding="utf-8") as f:
                f.write(
                    '// cc_file_path: chrome/browser/glic/does_not_exist.cc\n'
                    'class M extends ApiTestFixtureBase {\n'
                    '  async testFoo() {}\n'
                    '}\n')

            ret = checker.main(
                ["--check-only", "--repo-root", repo_mock, ts_file])
            self.assertEqual(ret, 1)

    def test_all_repository_test_pairs_are_fully_registered(self):
        """Integration test verifying every actual test pair passes."""
        repo_root = checker.find_repo_root()
        pairs, errors = checker.discover_all_test_pairs(repo_root)
        self.assertEqual(errors, [])
        self.assertGreaterEqual(len(pairs), 9,
                                "Expected at least 9 Glic test pairs in repo")
        for cc_path, ts_path in pairs:
            missing, ts_tests, cc_tests = checker.check_test_pair(
                cc_path, ts_path)
            self.assertEqual(
                missing,
                set(),
                f"Missing C++ registration in {cc_path} for tests in "
                f"{ts_path}: {missing}",
            )


if __name__ == "__main__":
    unittest.main()
