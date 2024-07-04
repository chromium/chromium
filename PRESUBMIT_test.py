#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import os.path
import subprocess
import textwrap
import unittest

import PRESUBMIT

from PRESUBMIT_test_mocks import MockFile, MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi


_TEST_DATA_DIR = 'base/test/data/presubmit'


class VersionControlConflictsTest(unittest.TestCase):

    def testTypicalConflict(self):
        lines = [
            '<<<<<<< HEAD', '  base::ScopedTempDir temp_dir_;', '=======',
            '  ScopedTempDir temp_dir_;', '>>>>>>> master'
        ]
        errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
            MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
        self.assertEqual(3, len(errors))
        self.assertTrue('1' in errors[0])
        self.assertTrue('3' in errors[1])
        self.assertTrue('5' in errors[2])

    def testIgnoresReadmes(self):
        lines = [
            'A First Level Header', '====================', '',
            'A Second Level Header', '---------------------'
        ]
        errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
            MockInputApi(), MockFile('some/polymer/README.md', lines))
        self.assertEqual(0, len(errors))


class BadExtensionsTest(unittest.TestCase):

    def testBadRejFile(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('some/path/foo.cc', ''),
            MockFile('some/path/foo.cc.rej', ''),
            MockFile('some/path2/bar.h.rej', ''),
        ]

        results = PRESUBMIT.CheckPatchFiles(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(2, len(results[0].items))
        self.assertTrue('foo.cc.rej' in results[0].items[0])
        self.assertTrue('bar.h.rej' in results[0].items[1])

    def testBadOrigFile(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('other/path/qux.h.orig', ''),
            MockFile('other/path/qux.h', ''),
            MockFile('other/path/qux.cc', ''),
        ]

        results = PRESUBMIT.CheckPatchFiles(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(1, len(results[0].items))
        self.assertTrue('qux.h.orig' in results[0].items[0])

    def testGoodFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('other/path/qux.h', ''),
            MockFile('other/path/qux.cc', ''),
        ]
        results = PRESUBMIT.CheckPatchFiles(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))


class CheckForSuperfluousStlIncludesInHeadersTest(unittest.TestCase):

    def testGoodFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            # The check is not smart enough to figure out which definitions correspond
            # to which header.
            MockFile('other/path/foo.h', ['#include <string>', 'std::vector']),
            # The check is not smart enough to do IWYU.
            MockFile('other/path/bar.h',
                     ['#include "base/check.h"', 'std::vector']),
            MockFile('other/path/qux.h',
                     ['#include "base/stl_util.h"', 'foobar']),
            MockFile('other/path/baz.h',
                     ['#include "set/vector.h"', 'bazzab']),
            # The check is only for header files.
            MockFile('other/path/not_checked.cc',
                     ['#include <vector>', 'bazbaz']),
        ]
        results = PRESUBMIT.CheckForSuperfluousStlIncludesInHeaders(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testBadFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('other/path/foo.h', ['#include <vector>', 'vector']),
            MockFile(
                'other/path/bar.h',
                ['#include <limits>', '#include <set>', 'no_std_namespace']),
        ]
        results = PRESUBMIT.CheckForSuperfluousStlIncludesInHeaders(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertTrue('foo.h: Includes STL' in results[0].message)
        self.assertTrue('bar.h: Includes STL' in results[0].message)


class CheckSingletonInHeadersTest(unittest.TestCase):

    def testSingletonInArbitraryHeader(self):
        diff_singleton_h = [
            'base::subtle::AtomicWord '
            'base::Singleton<Type, Traits, DifferentiatingType>::'
        ]
        diff_foo_h = [
            '// base::Singleton<Foo> in comment.',
            'friend class base::Singleton<Foo>'
        ]
        diff_foo2_h = ['  //Foo* bar = base::Singleton<Foo>::get();']
        diff_bad_h = ['Foo* foo = base::Singleton<Foo>::get();']
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/memory/singleton.h', diff_singleton_h),
            MockAffectedFile('foo.h', diff_foo_h),
            MockAffectedFile('foo2.h', diff_foo2_h),
            MockAffectedFile('bad.h', diff_bad_h)
        ]
        warnings = PRESUBMIT.CheckSingletonInHeaders(mock_input_api,
                                                     MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual(1, len(warnings[0].items))
        self.assertEqual('error', warnings[0].type)
        self.assertTrue('Found base::Singleton<T>' in warnings[0].message)

    def testSingletonInCC(self):
        diff_cc = ['Foo* foo = base::Singleton<Foo>::get();']
        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile('some/path/foo.cc', diff_cc)]
        warnings = PRESUBMIT.CheckSingletonInHeaders(mock_input_api,
                                                     MockOutputApi())
        self.assertEqual(0, len(warnings))


class DeprecatedOSMacroNamesTest(unittest.TestCase):

    def testDeprecatedOSMacroNames(self):
        lines = [
            '#if defined(OS_WIN)', ' #elif defined(OS_WINDOW)',
            ' # if defined(OS_MAC) || defined(OS_CHROME)'
        ]
        errors = PRESUBMIT._CheckForDeprecatedOSMacrosInFile(
            MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
        self.assertEqual(len(lines) + 1, len(errors))
        self.assertTrue(
            ':1: defined(OS_WIN) -> BUILDFLAG(IS_WIN)' in errors[0])


class InvalidIfDefinedMacroNamesTest(unittest.TestCase):

    def testInvalidIfDefinedMacroNames(self):
        lines = [
            '#if defined(TARGET_IPHONE_SIMULATOR)',
            '#if !defined(TARGET_IPHONE_SIMULATOR)',
            '#elif defined(TARGET_IPHONE_SIMULATOR)',
            '#ifdef TARGET_IPHONE_SIMULATOR',
            ' # ifdef TARGET_IPHONE_SIMULATOR',
            '# if defined(VALID) || defined(TARGET_IPHONE_SIMULATOR)',
            '# else  // defined(TARGET_IPHONE_SIMULATOR)',
            '#endif  // defined(TARGET_IPHONE_SIMULATOR)'
        ]
        errors = PRESUBMIT._CheckForInvalidIfDefinedMacrosInFile(
            MockInputApi(), MockFile('some/path/source.mm', lines))
        self.assertEqual(len(lines), len(errors))

    def testValidIfDefinedMacroNames(self):
        lines = [
            '#if defined(FOO)', '#ifdef BAR', '#if TARGET_IPHONE_SIMULATOR'
        ]
        errors = PRESUBMIT._CheckForInvalidIfDefinedMacrosInFile(
            MockInputApi(), MockFile('some/path/source.cc', lines))
        self.assertEqual(0, len(errors))


class CheckNoUNIT_TESTInSourceFilesTest(unittest.TestCase):

    def testUnitTestMacros(self):
        lines = [
            '#if defined(UNIT_TEST)', '#if defined UNIT_TEST',
            '#if !defined(UNIT_TEST)', '#elif defined(UNIT_TEST)',
            '#ifdef UNIT_TEST', ' # ifdef UNIT_TEST', '#ifndef UNIT_TEST',
            '# if defined(VALID) || defined(UNIT_TEST)',
            '# if defined(UNIT_TEST) && defined(VALID)',
            '# else  // defined(UNIT_TEST)', '#endif  // defined(UNIT_TEST)'
        ]
        errors = PRESUBMIT._CheckNoUNIT_TESTInSourceFiles(
            MockInputApi(), MockFile('some/path/source.cc', lines))
        self.assertEqual(len(lines), len(errors))

    def testNotUnitTestMacros(self):
        lines = [
            '// Comment about "#if defined(UNIT_TEST)"',
            '/* Comment about #if defined(UNIT_TEST)" */',
            '#ifndef UNIT_TEST_H', '#define UNIT_TEST_H',
            '#ifndef TEST_UNIT_TEST', '#define TEST_UNIT_TEST',
            '#if defined(_UNIT_TEST)', '#if defined(UNIT_TEST_)',
            '#ifdef _UNIT_TEST', '#ifdef UNIT_TEST_', '#ifndef _UNIT_TEST',
            '#ifndef UNIT_TEST_'
        ]
        errors = PRESUBMIT._CheckNoUNIT_TESTInSourceFiles(
            MockInputApi(), MockFile('some/path/source.cc', lines))
        self.assertEqual(0, len(errors))


class CheckEachPerfettoTestDataFileHasDepsEntry(unittest.TestCase):

    def testNewSha256FileNoDEPS(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('base/tracing/test/data_sha256/new.pftrace.sha256', []),
        ]
        results = PRESUBMIT.CheckEachPerfettoTestDataFileHasDepsEntry(
            input_api, MockOutputApi())
        self.assertEqual(
            ('You must update the DEPS file when you update a .sha256 file '
             'in base/tracing/test/data_sha256'), results[0].message)

    def testNewSha256FileSuccess(self):
        input_api = MockInputApi()
        new_deps = """deps = {
                    'src/base/tracing/test/data': {
                      'bucket': 'perfetto',
                      'objects': [
                        {
                          'object_name': 'test_data/new.pftrace-a1b2c3f4',
                          'sha256sum': 'a1b2c3f4',
                          'size_bytes': 1,
                          'generation': 1,
                          'output_file': 'new.pftrace'
                        },
                      ],
                      'dep_type': 'gcs'
                    },
                  }""".splitlines()
        input_api.files = [
            MockFile('base/tracing/test/data_sha256/new.pftrace.sha256',
                     ['a1b2c3f4']),
            MockFile('DEPS', new_deps,
                     ["deps={'src/base/tracing/test/data':{}}"]),
        ]
        results = PRESUBMIT.CheckEachPerfettoTestDataFileHasDepsEntry(
            input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNewSha256FileWrongSha256(self):
        input_api = MockInputApi()
        new_deps = """deps = {
                    'src/base/tracing/test/data': {
                      'bucket': 'perfetto',
                      'objects': [
                        {
                          'object_name': 'test_data/new.pftrace-a1b2c3f4',
                          'sha256sum': 'wrong_hash',
                          'size_bytes': 1,
                          'generation': 1,
                          'output_file': 'new.pftrace'
                        },
                      ],
                      'dep_type': 'gcs'
                    },
                  }""".splitlines()
        f = MockFile('base/tracing/test/data_sha256/new.pftrace.sha256',
                     ['a1b2c3f4'])
        input_api.files = [
            f,
            MockFile('DEPS', new_deps,
                     ["deps={'src/base/tracing/test/data':{}}"]),
        ]
        results = PRESUBMIT.CheckEachPerfettoTestDataFileHasDepsEntry(
            input_api, MockOutputApi())
        self.assertEqual(
            ('No corresponding DEPS entry found for %s. '
             'Run `base/tracing/test/test_data.py get_deps --filepath %s` '
             'to generate the DEPS entry.' % (f.LocalPath(), f.LocalPath())),
            results[0].message)

    def testDeleteSha256File(self):
        input_api = MockInputApi()
        old_deps = """deps = {
                    'src/base/tracing/test/data': {
                      'bucket': 'perfetto',
                      'objects': [
                        {
                          'object_name': 'test_data/new.pftrace-a1b2c3f4',
                          'sha256sum': 'a1b2c3f4',
                          'size_bytes': 1,
                          'generation': 1,
                          'output_file': 'new.pftrace'
                        },
                      ],
                      'dep_type': 'gcs'
                    },
                  }""".splitlines()
        f = MockFile('base/tracing/test/data_sha256/new.pftrace.sha256', [],
                     ['a1b2c3f4'],
                     action='D')
        input_api.files = [
            f,
            MockFile('DEPS', old_deps, old_deps),
        ]
        results = PRESUBMIT.CheckEachPerfettoTestDataFileHasDepsEntry(
            input_api, MockOutputApi())
        self.assertEqual((
            'You deleted %s so you must also remove the corresponding DEPS entry.'
            % f.LocalPath()), results[0].message)

    def testDeleteSha256Success(self):
        input_api = MockInputApi()
        new_deps = """deps = {
                    'src/base/tracing/test/data': {
                      'bucket': 'perfetto',
                      'objects': [],
                      'dep_type': 'gcs'
                    },
                  }""".splitlines()
        old_deps = """deps = {
                    'src/base/tracing/test/data': {
                      'bucket': 'perfetto',
                      'objects': [
                        {
                          'object_name': 'test_data/new.pftrace-a1b2c3f4',
                          'sha256sum': 'a1b2c3f4',
                          'size_bytes': 1,
                          'generation': 1,
                          'output_file': 'new.pftrace'
                        },
                      ],
                      'dep_type': 'gcs'
                    },
                  }""".splitlines()
        f = MockFile('base/tracing/test/data_sha256/new.pftrace.sha256', [],
                     ['a1b2c3f4'],
                     action='D')
        input_api.files = [
            f,
            MockFile('DEPS', new_deps, old_deps),
        ]
        results = PRESUBMIT.CheckEachPerfettoTestDataFileHasDepsEntry(
            input_api, MockOutputApi())
        self.assertEqual(0, len(results))


class CheckAddedDepsHaveTestApprovalsTest(unittest.TestCase):

    def calculate(self, old_include_rules, old_specific_include_rules,
                  new_include_rules, new_specific_include_rules):
        return PRESUBMIT._CalculateAddedDeps(
            os.path, 'include_rules = %r\nspecific_include_rules = %r' %
            (old_include_rules, old_specific_include_rules),
            'include_rules = %r\nspecific_include_rules = %r' %
            (new_include_rules, new_specific_include_rules))

    def testCalculateAddedDeps(self):
        old_include_rules = [
            '+base',
            '-chrome',
            '+content',
            '-grit',
            '-grit/",',
            '+jni/fooblat.h',
            '!sandbox',
        ]
        old_specific_include_rules = {
            'compositor\.*': {
                '+cc',
            },
        }

        new_include_rules = [
            '-ash',
            '+base',
            '+chrome',
            '+components',
            '+content',
            '+grit',
            '+grit/generated_resources.h",',
            '+grit/",',
            '+jni/fooblat.h',
            '+policy',
            '+' + os.path.join('third_party', 'WebKit'),
        ]
        new_specific_include_rules = {
            'compositor\.*': {
                '+cc',
            },
            'widget\.*': {
                '+gpu',
            },
        }

        expected = set([
            os.path.join('chrome', 'DEPS'),
            os.path.join('gpu', 'DEPS'),
            os.path.join('components', 'DEPS'),
            os.path.join('policy', 'DEPS'),
            os.path.join('third_party', 'WebKit', 'DEPS'),
        ])
        self.assertEqual(
            expected,
            self.calculate(old_include_rules, old_specific_include_rules,
                           new_include_rules, new_specific_include_rules))

    def testCalculateAddedDepsIgnoresPermutations(self):
        old_include_rules = [
            '+base',
            '+chrome',
        ]
        new_include_rules = [
            '+chrome',
            '+base',
        ]
        self.assertEqual(
            set(), self.calculate(old_include_rules, {}, new_include_rules,
                                  {}))

    class FakeOwnersClient(object):
        APPROVED = "APPROVED"
        PENDING = "PENDING"
        returns = {}

        def ListOwners(self, *args, **kwargs):
            return self.returns.get(self.ListOwners.__name__, "")

        def mockListOwners(self, owners):
            self.returns[self.ListOwners.__name__] = owners

        def GetFilesApprovalStatus(self, *args, **kwargs):
            return self.returns.get(self.GetFilesApprovalStatus.__name__, {})

        def mockGetFilesApprovalStatus(self, status):
            self.returns[self.GetFilesApprovalStatus.__name__] = status

        def SuggestOwners(self, *args, **kwargs):
            return ["eng1", "eng2", "eng3"]

    class fakeGerrit(object):

        def IsOwnersOverrideApproved(self, issue):
            return False

    def setUp(self):
        self.input_api = input_api = MockInputApi()
        input_api.environ = {}
        input_api.owners_client = self.FakeOwnersClient()
        input_api.gerrit = self.fakeGerrit()
        input_api.change.issue = 123
        self.mockOwnersAndReviewers("owner", set(["reviewer"]))
        self.mockListSubmodules([])

    def mockOwnersAndReviewers(self, owner, reviewers):

        def mock(*args, **kwargs):
            return [owner, reviewers]

        self.input_api.canned_checks.GetCodereviewOwnerAndReviewers = mock

    def mockListSubmodules(self, paths):

        def mock(*args, **kwargs):
            return paths

        self.input_api.ListSubmodules = mock

    def testApprovedAdditionalDep(self):
        old_deps = """include_rules = []""".splitlines()
        new_deps = """include_rules = ["+v8/123"]""".splitlines()
        self.input_api.files = [
            MockAffectedFile("pdf/DEPS", new_deps, old_deps)
        ]

        # mark the additional dep as approved.
        os_path = self.input_api.os_path
        self.input_api.owners_client.mockGetFilesApprovalStatus(
            {os_path.join('v8/123', 'DEPS'): self.FakeOwnersClient.APPROVED})
        results = PRESUBMIT.CheckAddedDepsHaveTargetApprovals(
            self.input_api, MockOutputApi())
        # Then, the check should pass.
        self.assertEqual([], results)

    def testUnapprovedAdditionalDep(self):
        old_deps = """include_rules = []""".splitlines()
        new_deps = """include_rules = ["+v8/123"]""".splitlines()
        self.input_api.files = [
            MockAffectedFile('pdf/DEPS', new_deps, old_deps),
        ]

        # pending.
        os_path = self.input_api.os_path
        self.input_api.owners_client.mockGetFilesApprovalStatus(
            {os_path.join('v8/123', 'DEPS'): self.FakeOwnersClient.PENDING})
        results = PRESUBMIT.CheckAddedDepsHaveTargetApprovals(
            self.input_api, MockOutputApi())
        # the check should fail
        self.assertIn('You need LGTM', results[0].message)
        self.assertIn('+v8/123', results[0].message)

        # unless the added dep is from a submodule.
        self.mockListSubmodules(['v8'])
        results = PRESUBMIT.CheckAddedDepsHaveTargetApprovals(
            self.input_api, MockOutputApi())
        self.assertEqual([], results)


class JSONParsingTest(unittest.TestCase):

    def testSuccess(self):
        input_api = MockInputApi()
        filename = 'valid_json.json'
        contents = [
            '// This is a comment.', '{', '  "key1": ["value1", "value2"],',
            '  "key2": 3  // This is an inline comment.', '}'
        ]
        input_api.files = [MockFile(filename, contents)]
        self.assertEqual(None,
                         PRESUBMIT._GetJSONParseError(input_api, filename))

    def testFailure(self):
        input_api = MockInputApi()
        test_data = [
            ('invalid_json_1.json', ['{ x }'], 'Expecting property name'),
            ('invalid_json_2.json', ['// Hello world!', '{ "hello": "world }'],
             'Unterminated string starting at:'),
            ('invalid_json_3.json', ['{ "a": "b", "c": "d", }'],
             'Expecting property name'),
            ('invalid_json_4.json', ['{ "a": "b" "c": "d" }'],
             "Expecting ',' delimiter:"),
        ]

        input_api.files = [
            MockFile(filename, contents)
            for (filename, contents, _) in test_data
        ]

        for (filename, _, expected_error) in test_data:
            actual_error = PRESUBMIT._GetJSONParseError(input_api, filename)
            self.assertTrue(
                expected_error in str(actual_error),
                "'%s' not found in '%s'" % (expected_error, actual_error))

    def testNoEatComments(self):
        input_api = MockInputApi()
        file_with_comments = 'file_with_comments.json'
        contents_with_comments = [
            '// This is a comment.', '{', '  "key1": ["value1", "value2"],',
            '  "key2": 3  // This is an inline comment.', '}'
        ]
        file_without_comments = 'file_without_comments.json'
        contents_without_comments = [
            '{', '  "key1": ["value1", "value2"],', '  "key2": 3', '}'
        ]
        input_api.files = [
            MockFile(file_with_comments, contents_with_comments),
            MockFile(file_without_comments, contents_without_comments)
        ]

        self.assertNotEqual(
            None,
            str(
                PRESUBMIT._GetJSONParseError(input_api,
                                             file_with_comments,
                                             eat_comments=False)))
        self.assertEqual(
            None,
            PRESUBMIT._GetJSONParseError(input_api,
                                         file_without_comments,
                                         eat_comments=False))


class IDLParsingTest(unittest.TestCase):

    def testSuccess(self):
        input_api = MockInputApi()
        filename = 'valid_idl_basics.idl'
        contents = [
            '// Tests a valid IDL file.', 'namespace idl_basics {',
            '  enum EnumType {', '    name1,', '    name2', '  };', '',
            '  dictionary MyType1 {', '    DOMString a;', '  };', '',
            '  callback Callback1 = void();',
            '  callback Callback2 = void(long x);',
            '  callback Callback3 = void(MyType1 arg);',
            '  callback Callback4 = void(EnumType type);', '',
            '  interface Functions {', '    static void function1();',
            '    static void function2(long x);',
            '    static void function3(MyType1 arg);',
            '    static void function4(Callback1 cb);',
            '    static void function5(Callback2 cb);',
            '    static void function6(Callback3 cb);',
            '    static void function7(Callback4 cb);', '  };', '',
            '  interface Events {', '    static void onFoo1();',
            '    static void onFoo2(long x);',
            '    static void onFoo2(MyType1 arg);',
            '    static void onFoo3(EnumType type);', '  };', '};'
        ]
        input_api.files = [MockFile(filename, contents)]
        self.assertEqual(None,
                         PRESUBMIT._GetIDLParseError(input_api, filename))

    def testFailure(self):
        input_api = MockInputApi()
        test_data = [
            ('invalid_idl_1.idl', [
                '//', 'namespace test {', '  dictionary {', '    DOMString s;',
                '  };', '};'
            ], 'Unexpected "{" after keyword "dictionary".\n'),
            # TODO(yoz): Disabled because it causes the IDL parser to hang.
            # See crbug.com/363830.
            # ('invalid_idl_2.idl',
            #  (['namespace test {',
            #    '  dictionary MissingSemicolon {',
            #    '    DOMString a',
            #    '    DOMString b;',
            #    '  };',
            #    '};'],
            #   'Unexpected symbol DOMString after symbol a.'),
            ('invalid_idl_3.idl', [
                '//', 'namespace test {', '  enum MissingComma {', '    name1',
                '    name2', '  };', '};'
            ], 'Unexpected symbol name2 after symbol name1.'),
            ('invalid_idl_4.idl', [
                '//', 'namespace test {', '  enum TrailingComma {',
                '    name1,', '    name2,', '  };', '};'
            ], 'Trailing comma in block.'),
            ('invalid_idl_5.idl',
             ['//', 'namespace test {', '  callback Callback1 = void(;',
              '};'], 'Unexpected ";" after "(".'),
            ('invalid_idl_6.idl', [
                '//', 'namespace test {',
                '  callback Callback1 = void(long );', '};'
            ], 'Unexpected ")" after symbol long.'),
            ('invalid_idl_7.idl', [
                '//', 'namespace test {', '  interace Events {',
                '    static void onFoo1();', '  };', '};'
            ], 'Unexpected symbol Events after symbol interace.'),
            ('invalid_idl_8.idl', [
                '//', 'namespace test {', '  interface NotEvent {',
                '    static void onFoo1();', '  };', '};'
            ], 'Did not process Interface Interface(NotEvent)'),
            ('invalid_idl_9.idl', [
                '//', 'namespace test {', '  interface {',
                '    static void function1();', '  };', '};'
            ], 'Interface missing name.'),
        ]

        input_api.files = [
            MockFile(filename, contents)
            for (filename, contents, _) in test_data
        ]

        for (filename, _, expected_error) in test_data:
            actual_error = PRESUBMIT._GetIDLParseError(input_api, filename)
            self.assertTrue(
                expected_error in str(actual_error),
                "'%s' not found in '%s'" % (expected_error, actual_error))


class UserMetricsActionTest(unittest.TestCase):

    def testUserMetricsActionInActions(self):
        input_api = MockInputApi()
        file_with_user_action = 'file_with_user_action.cc'
        contents_with_user_action = ['base::UserMetricsAction("AboutChrome")']

        input_api.files = [
            MockFile(file_with_user_action, contents_with_user_action)
        ]

        self.assertEqual([],
                         PRESUBMIT.CheckUserActionUpdate(
                             input_api, MockOutputApi()))

    def testUserMetricsActionNotAddedToActions(self):
        input_api = MockInputApi()
        file_with_user_action = 'file_with_user_action.cc'
        contents_with_user_action = [
            'base::UserMetricsAction("NotInActionsXml")'
        ]

        input_api.files = [
            MockFile(file_with_user_action, contents_with_user_action)
        ]

        output = PRESUBMIT.CheckUserActionUpdate(input_api, MockOutputApi())
        self.assertEqual(
            ('File %s line %d: %s is missing in '
             'tools/metrics/actions/actions.xml. Please run '
             'tools/metrics/actions/extract_actions.py to update.' %
             (file_with_user_action, 1, 'NotInActionsXml')), output[0].message)

    def testUserMetricsActionInTestFile(self):
        input_api = MockInputApi()
        file_with_user_action = 'file_with_user_action_unittest.cc'
        contents_with_user_action = [
            'base::UserMetricsAction("NotInActionsXml")'
        ]

        input_api.files = [
            MockFile(file_with_user_action, contents_with_user_action)
        ]

        self.assertEqual([],
                         PRESUBMIT.CheckUserActionUpdate(
                             input_api, MockOutputApi()))


class PydepsNeedsUpdatingTest(unittest.TestCase):

    class MockPopen:

        def __init__(self, stdout):
            self.stdout = io.StringIO(stdout)

        def wait(self):
            return 0

    class MockSubprocess:
        CalledProcessError = subprocess.CalledProcessError
        PIPE = 0

        def __init__(self):
            self._popen_func = None

        def SetPopenCallback(self, func):
            self._popen_func = func

        def Popen(self, cmd, *args, **kwargs):
            return PydepsNeedsUpdatingTest.MockPopen(self._popen_func(cmd))

    def _MockParseGclientArgs(self, is_android=True):
        return lambda: {'checkout_android': 'true' if is_android else 'false'}

    def setUp(self):
        mock_all_pydeps = ['A.pydeps', 'B.pydeps', 'D.pydeps']
        self.old_ALL_PYDEPS_FILES = PRESUBMIT._ALL_PYDEPS_FILES
        PRESUBMIT._ALL_PYDEPS_FILES = mock_all_pydeps
        mock_android_pydeps = ['D.pydeps']
        self.old_ANDROID_SPECIFIC_PYDEPS_FILES = (
            PRESUBMIT._ANDROID_SPECIFIC_PYDEPS_FILES)
        PRESUBMIT._ANDROID_SPECIFIC_PYDEPS_FILES = mock_android_pydeps
        self.old_ParseGclientArgs = PRESUBMIT._ParseGclientArgs
        PRESUBMIT._ParseGclientArgs = self._MockParseGclientArgs()
        self.mock_input_api = MockInputApi()
        self.mock_output_api = MockOutputApi()
        self.mock_input_api.subprocess = PydepsNeedsUpdatingTest.MockSubprocess(
        )
        self.checker = PRESUBMIT.PydepsChecker(self.mock_input_api,
                                               mock_all_pydeps)
        self.checker._file_cache = {
            'A.pydeps':
            '# Generated by:\n# CMD --output A.pydeps A\nA.py\nC.py\n',
            'B.pydeps':
            '# Generated by:\n# CMD --output B.pydeps B\nB.py\nC.py\n',
            'D.pydeps': '# Generated by:\n# CMD --output D.pydeps D\nD.py\n',
        }

    def tearDown(self):
        PRESUBMIT._ALL_PYDEPS_FILES = self.old_ALL_PYDEPS_FILES
        PRESUBMIT._ANDROID_SPECIFIC_PYDEPS_FILES = (
            self.old_ANDROID_SPECIFIC_PYDEPS_FILES)
        PRESUBMIT._ParseGclientArgs = self.old_ParseGclientArgs

    def _RunCheck(self):
        return PRESUBMIT.CheckPydepsNeedsUpdating(
            self.mock_input_api,
            self.mock_output_api,
            checker_for_tests=self.checker)

    def testAddedPydep(self):
        # PRESUBMIT.CheckPydepsNeedsUpdating is only implemented for Linux.
        if not self.mock_input_api.platform.startswith('linux'):
            return []

        self.mock_input_api.files = [
            MockAffectedFile('new.pydeps', [], action='A'),
        ]

        self.mock_input_api.CreateMockFileInPath([
            x.LocalPath()
            for x in self.mock_input_api.AffectedFiles(include_deletes=True)
        ])
        results = self._RunCheck()
        self.assertEqual(1, len(results))
        self.assertIn('PYDEPS_FILES', str(results[0]))

    def testPydepNotInSrc(self):
        self.mock_input_api.files = [
            MockAffectedFile('new.pydeps', [], action='A'),
        ]
        self.mock_input_api.CreateMockFileInPath([])
        results = self._RunCheck()
        self.assertEqual(0, len(results))

    def testRemovedPydep(self):
        # PRESUBMIT.CheckPydepsNeedsUpdating is only implemented for Linux.
        if not self.mock_input_api.platform.startswith('linux'):
            return []

        self.mock_input_api.files = [
            MockAffectedFile(PRESUBMIT._ALL_PYDEPS_FILES[0], [], action='D'),
        ]
        self.mock_input_api.CreateMockFileInPath([
            x.LocalPath()
            for x in self.mock_input_api.AffectedFiles(include_deletes=True)
        ])
        results = self._RunCheck()
        self.assertEqual(1, len(results))
        self.assertIn('PYDEPS_FILES', str(results[0]))

    def testRandomPyIgnored(self):
        # PRESUBMIT.CheckPydepsNeedsUpdating is only implemented for Linux.
        if not self.mock_input_api.platform.startswith('linux'):
            return []

        self.mock_input_api.files = [
            MockAffectedFile('random.py', []),
        ]

        results = self._RunCheck()
        self.assertEqual(0, len(results), 'Unexpected results: %r' % results)

    def testRelevantPyNoChange(self):
        # PRESUBMIT.CheckPydepsNeedsUpdating is only implemented for Linux.
        if not self.mock_input_api.platform.startswith('linux'):
            return []

        self.mock_input_api.files = [
            MockAffectedFile('A.py', []),
        ]

        def popen_callback(cmd):
            self.assertEqual('CMD --output A.pydeps A --output ""', cmd)
            return self.checker._file_cache['A.pydeps']

        self.mock_input_api.subprocess.SetPopenCallback(popen_callback)

        results = self._RunCheck()
        self.assertEqual(0, len(results), 'Unexpected results: %r' % results)

    def testRelevantPyOneChange(self):
        # PRESUBMIT.CheckPydepsNeedsUpdating is only implemented for Linux.
        if not self.mock_input_api.platform.startswith('linux'):
            return []

        self.mock_input_api.files = [
            MockAffectedFile('A.py', []),
        ]

        def popen_callback(cmd):
            self.assertEqual('CMD --output A.pydeps A --output ""', cmd)
            return 'changed data'

        self.mock_input_api.subprocess.SetPopenCallback(popen_callback)

        results = self._RunCheck()
        self.assertEqual(1, len(results))
        # Check that --output "" is not included.
        self.assertNotIn('""', str(results[0]))
        self.assertIn('File is stale', str(results[0]))

    def testRelevantPyTwoChanges(self):
        # PRESUBMIT.CheckPydepsNeedsUpdating is only implemented for Linux.
        if not self.mock_input_api.platform.startswith('linux'):
            return []

        self.mock_input_api.files = [
            MockAffectedFile('C.py', []),
        ]

        def popen_callback(cmd):
            return 'changed data'

        self.mock_input_api.subprocess.SetPopenCallback(popen_callback)

        results = self._RunCheck()
        self.assertEqual(2, len(results))
        self.assertIn('File is stale', str(results[0]))
        self.assertIn('File is stale', str(results[1]))

    def testRelevantAndroidPyInNonAndroidCheckout(self):
        # PRESUBMIT.CheckPydepsNeedsUpdating is only implemented for Linux.
        if not self.mock_input_api.platform.startswith('linux'):
            return []

        self.mock_input_api.files = [
            MockAffectedFile('D.py', []),
        ]

        def popen_callback(cmd):
            self.assertEqual('CMD --output D.pydeps D --output ""', cmd)
            return 'changed data'

        self.mock_input_api.subprocess.SetPopenCallback(popen_callback)
        PRESUBMIT._ParseGclientArgs = self._MockParseGclientArgs(
            is_android=False)

        results = self._RunCheck()
        self.assertEqual(1, len(results))
        self.assertIn('Android', str(results[0]))
        self.assertIn('D.pydeps', str(results[0]))

    def testGnPathsAndMissingOutputFlag(self):
        # PRESUBMIT.CheckPydepsNeedsUpdating is only implemented for Linux.
        if not self.mock_input_api.platform.startswith('linux'):
            return []

        self.checker._file_cache = {
            'A.pydeps':
            '# Generated by:\n# CMD --gn-paths A\n//A.py\n//C.py\n',
            'B.pydeps':
            '# Generated by:\n# CMD --gn-paths B\n//B.py\n//C.py\n',
            'D.pydeps': '# Generated by:\n# CMD --gn-paths D\n//D.py\n',
        }

        self.mock_input_api.files = [
            MockAffectedFile('A.py', []),
        ]

        def popen_callback(cmd):
            self.assertEqual('CMD --gn-paths A --output A.pydeps --output ""',
                             cmd)
            return 'changed data'

        self.mock_input_api.subprocess.SetPopenCallback(popen_callback)

        results = self._RunCheck()
        self.assertEqual(1, len(results))
        self.assertIn('File is stale', str(results[0]))


class IncludeGuardTest(unittest.TestCase):

    def testIncludeGuardChecks(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.files = [
            MockAffectedFile('content/browser/thing/foo.h', [
                '// Comment',
                '#ifndef CONTENT_BROWSER_THING_FOO_H_',
                '#define CONTENT_BROWSER_THING_FOO_H_',
                'struct McBoatFace;',
                '#endif  // CONTENT_BROWSER_THING_FOO_H_',
            ]),
            MockAffectedFile('content/browser/thing/bar.h', [
                '#ifndef CONTENT_BROWSER_THING_BAR_H_',
                '#define CONTENT_BROWSER_THING_BAR_H_',
                'namespace content {',
                '#endif  // CONTENT_BROWSER_THING_BAR_H_',
                '}  // namespace content',
            ]),
            MockAffectedFile('content/browser/test1.h', [
                'namespace content {',
                '}  // namespace content',
            ]),
            MockAffectedFile('content\\browser\\win.h', [
                '#ifndef CONTENT_BROWSER_WIN_H_',
                '#define CONTENT_BROWSER_WIN_H_',
                'struct McBoatFace;',
                '#endif  // CONTENT_BROWSER_WIN_H_',
            ]),
            MockAffectedFile('content/browser/test2.h', [
                '// Comment',
                '#ifndef CONTENT_BROWSER_TEST2_H_',
                'struct McBoatFace;',
                '#endif  // CONTENT_BROWSER_TEST2_H_',
            ]),
            MockAffectedFile('content/browser/internal.h', [
                '// Comment',
                '#ifndef CONTENT_BROWSER_INTERNAL_H_',
                '#define CONTENT_BROWSER_INTERNAL_H_',
                '// Comment',
                '#ifndef INTERNAL_CONTENT_BROWSER_INTERNAL_H_',
                '#define INTERNAL_CONTENT_BROWSER_INTERNAL_H_',
                'namespace internal {',
                '}  // namespace internal',
                '#endif  // INTERNAL_CONTENT_BROWSER_THING_BAR_H_',
                'namespace content {',
                '}  // namespace content',
                '#endif  // CONTENT_BROWSER_THING_BAR_H_',
            ]),
            MockAffectedFile('content/browser/thing/foo.cc', [
                '// This is a non-header.',
            ]),
            MockAffectedFile('content/browser/disabled.h', [
                '// no-include-guard-because-multiply-included',
                'struct McBoatFace;',
            ]),
            # New files don't allow misspelled include guards.
            MockAffectedFile('content/browser/spleling.h', [
                '#ifndef CONTENT_BROWSER_SPLLEING_H_',
                '#define CONTENT_BROWSER_SPLLEING_H_',
                'struct McBoatFace;',
                '#endif  // CONTENT_BROWSER_SPLLEING_H_',
            ]),
            # New files don't allow + in include guards.
            MockAffectedFile('content/browser/foo+bar.h', [
                '#ifndef CONTENT_BROWSER_FOO+BAR_H_',
                '#define CONTENT_BROWSER_FOO+BAR_H_',
                'struct McBoatFace;',
                '#endif  // CONTENT_BROWSER_FOO+BAR_H_',
            ]),
            # Old files allow misspelled include guards (for now).
            MockAffectedFile('chrome/old.h', [
                '// New contents',
                '#ifndef CHROME_ODL_H_',
                '#define CHROME_ODL_H_',
                '#endif  // CHROME_ODL_H_',
            ], [
                '// Old contents',
                '#ifndef CHROME_ODL_H_',
                '#define CHROME_ODL_H_',
                '#endif  // CHROME_ODL_H_',
            ],
                             action='M'),
            # Using a Blink style include guard outside Blink is wrong.
            MockAffectedFile('content/NotInBlink.h', [
                '#ifndef NotInBlink_h',
                '#define NotInBlink_h',
                'struct McBoatFace;',
                '#endif  // NotInBlink_h',
            ]),
            # Using a Blink style include guard in Blink is no longer ok.
            MockAffectedFile('third_party/blink/InBlink.h', [
                '#ifndef InBlink_h',
                '#define InBlink_h',
                'struct McBoatFace;',
                '#endif  // InBlink_h',
            ]),
            # Using a bad include guard in Blink is not ok.
            MockAffectedFile('third_party/blink/AlsoInBlink.h', [
                '#ifndef WrongInBlink_h',
                '#define WrongInBlink_h',
                'struct McBoatFace;',
                '#endif  // WrongInBlink_h',
            ]),
            # Using a bad include guard in Blink is not supposed to be accepted even
            # if it's an old file. However the current presubmit has accepted this
            # for a while.
            MockAffectedFile('third_party/blink/StillInBlink.h', [
                '// New contents',
                '#ifndef AcceptedInBlink_h',
                '#define AcceptedInBlink_h',
                'struct McBoatFace;',
                '#endif  // AcceptedInBlink_h',
            ], [
                '// Old contents',
                '#ifndef AcceptedInBlink_h',
                '#define AcceptedInBlink_h',
                'struct McBoatFace;',
                '#endif  // AcceptedInBlink_h',
            ],
                             action='M'),
            # Using a non-Chromium include guard in third_party
            # (outside blink) is accepted.
            MockAffectedFile('third_party/foo/some_file.h', [
                '#ifndef REQUIRED_RPCNDR_H_',
                '#define REQUIRED_RPCNDR_H_',
                'struct SomeFileFoo;',
                '#endif  // REQUIRED_RPCNDR_H_',
            ]),
            # Not having proper include guard in *_message_generator.h
            # for old IPC messages is allowed.
            MockAffectedFile('content/common/content_message_generator.h', [
                '#undef CONTENT_COMMON_FOO_MESSAGES_H_',
                '#include "content/common/foo_messages.h"',
                '#ifndef CONTENT_COMMON_FOO_MESSAGES_H_',
                '#error "Failed to include content/common/foo_messages.h"',
                '#endif',
            ]),
            MockAffectedFile('chrome/renderer/thing/qux.h', [
                '// Comment',
                '#ifndef CHROME_RENDERER_THING_QUX_H_',
                '#define CHROME_RENDERER_THING_QUX_H_',
                'struct Boaty;',
                '#endif',
            ]),
        ]
        msgs = PRESUBMIT.CheckForIncludeGuards(mock_input_api, mock_output_api)
        expected_fail_count = 10
        self.assertEqual(
            expected_fail_count, len(msgs), 'Expected %d items, found %d: %s' %
            (expected_fail_count, len(msgs), msgs))
        self.assertEqual(msgs[0].items, ['content/browser/thing/bar.h'])
        self.assertEqual(
            msgs[0].message, 'Include guard CONTENT_BROWSER_THING_BAR_H_ '
            'not covering the whole file')

        self.assertIn('content/browser/test1.h', msgs[1].message)
        self.assertIn('Recommended name: CONTENT_BROWSER_TEST1_H_',
                      msgs[1].message)

        self.assertEqual(msgs[2].items, ['content/browser/test2.h:3'])
        self.assertEqual(
            msgs[2].message, 'Missing "#define CONTENT_BROWSER_TEST2_H_" for '
            'include guard')

        self.assertIn('content/browser/internal.h', msgs[3].message)
        self.assertIn(
            'Recommended #endif comment: // CONTENT_BROWSER_INTERNAL_H_',
            msgs[3].message)

        self.assertEqual(msgs[4].items, ['content/browser/spleling.h:1'])
        self.assertEqual(
            msgs[4].message, 'Header using the wrong include guard name '
            'CONTENT_BROWSER_SPLLEING_H_')

        self.assertIn('content/browser/foo+bar.h', msgs[5].message)
        self.assertIn('Recommended name: CONTENT_BROWSER_FOO_BAR_H_',
                      msgs[5].message)

        self.assertEqual(msgs[6].items, ['content/NotInBlink.h:1'])
        self.assertEqual(
            msgs[6].message, 'Header using the wrong include guard name '
            'NotInBlink_h')

        self.assertEqual(msgs[7].items, ['third_party/blink/InBlink.h:1'])
        self.assertEqual(
            msgs[7].message, 'Header using the wrong include guard name '
            'InBlink_h')

        self.assertEqual(msgs[8].items, ['third_party/blink/AlsoInBlink.h:1'])
        self.assertEqual(
            msgs[8].message, 'Header using the wrong include guard name '
            'WrongInBlink_h')

        self.assertIn('chrome/renderer/thing/qux.h', msgs[9].message)
        self.assertIn(
            'Recommended #endif comment: // CHROME_RENDERER_THING_QUX_H_',
            msgs[9].message)


class AccessibilityRelnotesFieldTest(unittest.TestCase):

    def testRelnotesPresent(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('ui/accessibility/foo.bar', [''])
        ]
        mock_input_api.change.DescriptionText = lambda: 'Commit description'
        mock_input_api.change.footers['AX-Relnotes'] = [
            'Important user facing change'
        ]

        msgs = PRESUBMIT.CheckAccessibilityRelnotesField(
            mock_input_api, mock_output_api)
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    def testRelnotesMissingFromAccessibilityChange(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('some/file', ['']),
            MockAffectedFile('ui/accessibility/foo.bar', ['']),
            MockAffectedFile('some/other/file', [''])
        ]
        mock_input_api.change.DescriptionText = lambda: 'Commit description'

        msgs = PRESUBMIT.CheckAccessibilityRelnotesField(
            mock_input_api, mock_output_api)
        self.assertEqual(
            1, len(msgs),
            'Expected %d messages, found %d: %s' % (1, len(msgs), msgs))
        self.assertTrue(
            "Missing 'AX-Relnotes:' field" in msgs[0].message,
            'Missing AX-Relnotes field message not found in errors')

    # The relnotes footer is not required for changes which do not touch any
    # accessibility directories.
    def testIgnoresNonAccessibilityCode(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('some/file', ['']),
            MockAffectedFile('some/other/file', [''])
        ]
        mock_input_api.change.DescriptionText = lambda: 'Commit description'

        msgs = PRESUBMIT.CheckAccessibilityRelnotesField(
            mock_input_api, mock_output_api)
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that our presubmit correctly raises an error for a set of known paths.
    def testExpectedPaths(self):
        filesToTest = [
            "chrome/browser/accessibility/foo.py",
            "chrome/browser/ash/arc/accessibility/foo.cc",
            "chrome/browser/ui/views/accessibility/foo.h",
            "chrome/browser/extensions/api/automation/foo.h",
            "chrome/browser/extensions/api/automation_internal/foo.cc",
            "chrome/renderer/extensions/accessibility_foo.h",
            "chrome/tests/data/accessibility/foo.html",
            "content/browser/accessibility/foo.cc",
            "content/renderer/accessibility/foo.h",
            "content/tests/data/accessibility/foo.cc",
            "extensions/renderer/api/automation/foo.h",
            "ui/accessibility/foo/bar/baz.cc",
            "ui/views/accessibility/foo/bar/baz.h",
        ]

        for testFile in filesToTest:
            mock_input_api = MockInputApi()
            mock_output_api = MockOutputApi()

            mock_input_api.files = [MockAffectedFile(testFile, [''])]
            mock_input_api.change.DescriptionText = lambda: 'Commit description'

            msgs = PRESUBMIT.CheckAccessibilityRelnotesField(
                mock_input_api, mock_output_api)
            self.assertEqual(
                1, len(msgs),
                'Expected %d messages, found %d: %s, for file %s' %
                (1, len(msgs), msgs, testFile))
            self.assertTrue(
                "Missing 'AX-Relnotes:' field" in msgs[0].message,
                ('Missing AX-Relnotes field message not found in errors '
                 ' for file %s' % (testFile)))

    # Test that AX-Relnotes field can appear in the commit description (as long
    # as it appears at the beginning of a line).
    def testRelnotesInCommitDescription(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('ui/accessibility/foo.bar', ['']),
        ]
        mock_input_api.change.DescriptionText = lambda: (
            'Description:\n' +
            'AX-Relnotes: solves all accessibility issues forever')

        msgs = PRESUBMIT.CheckAccessibilityRelnotesField(
            mock_input_api, mock_output_api)
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that we don't match AX-Relnotes if it appears in the middle of a line.
    def testRelnotesMustAppearAtBeginningOfLine(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('ui/accessibility/foo.bar', ['']),
        ]
        mock_input_api.change.DescriptionText = lambda: (
            'Description:\n' +
            'This change has no AX-Relnotes: we should print a warning')

        msgs = PRESUBMIT.CheckAccessibilityRelnotesField(
            mock_input_api, mock_output_api)
        self.assertTrue(
            "Missing 'AX-Relnotes:' field" in msgs[0].message,
            'Missing AX-Relnotes field message not found in errors')

    # Tests that the AX-Relnotes field can be lowercase and use a '=' in place
    # of a ':'.
    def testRelnotesLowercaseWithEqualSign(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('ui/accessibility/foo.bar', ['']),
        ]
        mock_input_api.change.DescriptionText = lambda: (
            'Description:\n' +
            'ax-relnotes= this is a valid format for accessibility relnotes')

        msgs = PRESUBMIT.CheckAccessibilityRelnotesField(
            mock_input_api, mock_output_api)
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))


class AccessibilityEventsTestsAreIncludedForAndroidTest(unittest.TestCase):
    # Test that no warning is raised when the Android file is also modified.
    def testAndroidChangeIncluded(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile(
                'content/test/data/accessibility/event/foo-expected-mac.txt',
                [''],
                action='A'),
            MockAffectedFile(
                'accessibility/WebContentsAccessibilityEventsTest.java', [''],
                action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityEventsTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that Android change is not required when no html file is added/removed.
    def testIgnoreNonHtmlFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/event/foo.txt',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/event/foo.cc',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/event/foo.h',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/event/foo.py',
                             [''],
                             action='A')
        ]

        msgs = PRESUBMIT.CheckAccessibilityEventsTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that Android change is not required for unrelated html files.
    def testIgnoreNonRelatedHtmlFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/aria/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/html/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('chrome/tests/data/accessibility/foo.html', [''],
                             action='A')
        ]

        msgs = PRESUBMIT.CheckAccessibilityEventsTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that only modifying an html file will not trigger the warning.
    def testIgnoreModifiedFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile(
                'content/test/data/accessibility/event/foo-expected-win.txt',
                [''],
                action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityEventsTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))


class AccessibilityTreeTestsAreIncludedForAndroidTest(unittest.TestCase):
    # Test that no warning is raised when the Android file is also modified.
    def testAndroidChangeIncluded(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/aria/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile(
                'accessibility/WebContentsAccessibilityTreeTest.java', [''],
                action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that no warning is raised when the Android file is also modified.
    def testAndroidChangeIncludedManyFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile(
                'content/test/data/accessibility/accname/foo.html', [''],
                action='A'),
            MockAffectedFile('content/test/data/accessibility/aria/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/css/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/html/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile(
                'accessibility/WebContentsAccessibilityTreeTest.java', [''],
                action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that a warning is raised when the Android file is not modified.
    def testAndroidChangeMissing(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile(
                'content/test/data/accessibility/aria/foo-expected-win.txt',
                [''],
                action='A'),
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            1, len(msgs),
            'Expected %d messages, found %d: %s' % (1, len(msgs), msgs))

    # Test that Android change is not required when no platform expectations files are changed.
    def testAndroidChangNotMissing(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/accname/foo.txt',
                             [''],
                             action='A'),
            MockAffectedFile(
                'content/test/data/accessibility/html/foo-expected-blink.txt',
                [''],
                action='A'),
            MockAffectedFile('content/test/data/accessibility/html/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/aria/foo.cc',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/css/foo.h', [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/tree/foo.py',
                             [''],
                             action='A')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that Android change is not required for unrelated html files.
    def testIgnoreNonRelatedHtmlFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/event/foo.html',
                             [''],
                             action='A'),
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that only modifying an html file will not trigger the warning.
    def testIgnoreModifiedFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/aria/foo.html',
                             [''],
                             action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))


class AndroidDeprecatedTestAnnotationTest(unittest.TestCase):

    def testCheckAndroidTestAnnotationUsage(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('LalaLand.java', ['random stuff']),
            MockAffectedFile('CorrectUsage.java', [
                'import androidx.test.filters.LargeTest;',
                'import androidx.test.filters.MediumTest;',
                'import androidx.test.filters.SmallTest;',
            ]),
            MockAffectedFile('UsedDeprecatedLargeTestAnnotation.java', [
                'import android.test.suitebuilder.annotation.LargeTest;',
            ]),
            MockAffectedFile('UsedDeprecatedMediumTestAnnotation.java', [
                'import android.test.suitebuilder.annotation.MediumTest;',
            ]),
            MockAffectedFile('UsedDeprecatedSmallTestAnnotation.java', [
                'import android.test.suitebuilder.annotation.SmallTest;',
            ]),
            MockAffectedFile('UsedDeprecatedSmokeAnnotation.java', [
                'import android.test.suitebuilder.annotation.Smoke;',
            ])
        ]
        msgs = PRESUBMIT._CheckAndroidTestAnnotationUsage(
            mock_input_api, mock_output_api)
        self.assertEqual(
            1, len(msgs),
            'Expected %d items, found %d: %s' % (1, len(msgs), msgs))
        self.assertEqual(
            4, len(msgs[0].items), 'Expected %d items, found %d: %s' %
            (4, len(msgs[0].items), msgs[0].items))
        self.assertTrue(
            'UsedDeprecatedLargeTestAnnotation.java:1' in msgs[0].items,
            'UsedDeprecatedLargeTestAnnotation not found in errors')
        self.assertTrue(
            'UsedDeprecatedMediumTestAnnotation.java:1' in msgs[0].items,
            'UsedDeprecatedMediumTestAnnotation not found in errors')
        self.assertTrue(
            'UsedDeprecatedSmallTestAnnotation.java:1' in msgs[0].items,
            'UsedDeprecatedSmallTestAnnotation not found in errors')
        self.assertTrue(
            'UsedDeprecatedSmokeAnnotation.java:1' in msgs[0].items,
            'UsedDeprecatedSmokeAnnotation not found in errors')


class AndroidBannedImportTest(unittest.TestCase):

    def testCheckAndroidNoBannedImports(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        test_files = [
            MockAffectedFile('RandomStufff.java', ['random stuff']),
            MockAffectedFile('NoBannedImports.java', [
                'import androidx.test.filters.LargeTest;',
                'import androidx.test.filters.MediumTest;',
                'import androidx.test.filters.SmallTest;',
            ]),
            MockAffectedFile('BannedUri.java', [
                'import java.net.URI;',
            ]),
            MockAffectedFile('BannedTargetApi.java', [
                'import android.annotation.TargetApi;',
            ]),
            MockAffectedFile('BannedUiThreadTestRule.java', [
                'import androidx.test.rule.UiThreadTestRule;',
            ]),
            MockAffectedFile('BannedUiThreadTest.java', [
                'import androidx.test.annotation.UiThreadTest;',
            ]),
            MockAffectedFile('BannedActivityTestRule.java', [
                'import androidx.test.rule.ActivityTestRule;',
            ]),
            MockAffectedFile('BannedVectorDrawableCompat.java', [
                'import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;',
            ])
        ]
        msgs = []
        for file in test_files:
            mock_input_api.files = [file]
            msgs.append(
                PRESUBMIT._CheckAndroidNoBannedImports(mock_input_api,
                                                       mock_output_api))
        self.assertEqual(0, len(msgs[0]))
        self.assertEqual(0, len(msgs[1]))
        self.assertTrue(msgs[2][0].message.startswith(
            textwrap.dedent("""\
      Banned imports were used.
          BannedUri.java:1:""")))
        self.assertTrue(msgs[3][0].message.startswith(
            textwrap.dedent("""\
      Banned imports were used.
          BannedTargetApi.java:1:""")))
        self.assertTrue(msgs[4][0].message.startswith(
            textwrap.dedent("""\
      Banned imports were used.
          BannedUiThreadTestRule.java:1:""")))
        self.assertTrue(msgs[5][0].message.startswith(
            textwrap.dedent("""\
      Banned imports were used.
          BannedUiThreadTest.java:1:""")))
        self.assertTrue(msgs[6][0].message.startswith(
            textwrap.dedent("""\
      Banned imports were used.
          BannedActivityTestRule.java:1:""")))
        self.assertTrue(msgs[7][0].message.startswith(
            textwrap.dedent("""\
      Banned imports were used.
          BannedVectorDrawableCompat.java:1:""")))


class CheckNoDownstreamDepsTest(unittest.TestCase):

    def testInvalidDepFromUpstream(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('BUILD.gn',
                             ['deps = [', '   "//clank/target:test",', ']']),
            MockAffectedFile('chrome/android/BUILD.gn',
                             ['deps = [ "//clank/target:test" ]']),
            MockAffectedFile(
                'chrome/chrome_java_deps.gni',
                ['java_deps = [', '   "//clank/target:test",', ']']),
        ]
        mock_input_api.change.RepositoryRoot = lambda: 'chromium/src'
        msgs = PRESUBMIT.CheckNoUpstreamDepsOnClank(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(
            1, len(msgs),
            'Expected %d items, found %d: %s' % (1, len(msgs), msgs))
        self.assertEqual(
            3, len(msgs[0].items), 'Expected %d items, found %d: %s' %
            (3, len(msgs[0].items), msgs[0].items))
        self.assertTrue(any('BUILD.gn:2' in item for item in msgs[0].items),
                        'BUILD.gn not found in errors')
        self.assertTrue(
            any('chrome/android/BUILD.gn:1' in item for item in msgs[0].items),
            'chrome/android/BUILD.gn:1 not found in errors')
        self.assertTrue(
            any('chrome/chrome_java_deps.gni:2' in item
                for item in msgs[0].items),
            'chrome/chrome_java_deps.gni:2 not found in errors')

    def testAllowsComments(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('BUILD.gn', [
                '# real implementation in //clank/target:test',
            ]),
        ]
        mock_input_api.change.RepositoryRoot = lambda: 'chromium/src'
        msgs = PRESUBMIT.CheckNoUpstreamDepsOnClank(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(
            0, len(msgs),
            'Expected %d items, found %d: %s' % (0, len(msgs), msgs))

    def testOnlyChecksBuildFiles(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('README.md',
                             ['DEPS = [ "//clank/target:test" ]']),
            MockAffectedFile('chrome/android/java/file.java',
                             ['//clank/ only function']),
        ]
        mock_input_api.change.RepositoryRoot = lambda: 'chromium/src'
        msgs = PRESUBMIT.CheckNoUpstreamDepsOnClank(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(
            0, len(msgs),
            'Expected %d items, found %d: %s' % (0, len(msgs), msgs))

    def testValidDepFromDownstream(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('BUILD.gn',
                             ['DEPS = [', '   "//clank/target:test",', ']']),
            MockAffectedFile('java/BUILD.gn',
                             ['DEPS = [ "//clank/target:test" ]']),
        ]
        mock_input_api.change.RepositoryRoot = lambda: 'chromium/src/clank'
        msgs = PRESUBMIT.CheckNoUpstreamDepsOnClank(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(
            0, len(msgs),
            'Expected %d items, found %d: %s' % (0, len(msgs), msgs))


class AndroidDebuggableBuildTest(unittest.TestCase):

    def testCheckAndroidDebuggableBuild(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('RandomStuff.java', ['random stuff']),
            MockAffectedFile('CorrectUsage.java', [
                'import org.chromium.base.BuildInfo;',
                'some random stuff',
                'boolean isOsDebuggable = BuildInfo.isDebugAndroid();',
            ]),
            MockAffectedFile('JustCheckUserdebugBuild.java', [
                'import android.os.Build;',
                'some random stuff',
                'boolean isOsDebuggable = Build.TYPE.equals("userdebug")',
            ]),
            MockAffectedFile('JustCheckEngineeringBuild.java', [
                'import android.os.Build;',
                'some random stuff',
                'boolean isOsDebuggable = "eng".equals(Build.TYPE)',
            ]),
            MockAffectedFile('UsedBuildType.java', [
                'import android.os.Build;',
                'some random stuff',
                'boolean isOsDebuggable = Build.TYPE.equals("userdebug")'
                '|| "eng".equals(Build.TYPE)',
            ]),
            MockAffectedFile('UsedExplicitBuildType.java', [
                'some random stuff',
                'boolean isOsDebuggable = android.os.Build.TYPE.equals("userdebug")'
                '|| "eng".equals(android.os.Build.TYPE)',
            ]),
        ]

        msgs = PRESUBMIT._CheckAndroidDebuggableBuild(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(
            1, len(msgs),
            'Expected %d items, found %d: %s' % (1, len(msgs), msgs))
        self.assertEqual(
            4, len(msgs[0].items), 'Expected %d items, found %d: %s' %
            (4, len(msgs[0].items), msgs[0].items))
        self.assertTrue('JustCheckUserdebugBuild.java:3' in msgs[0].items)
        self.assertTrue('JustCheckEngineeringBuild.java:3' in msgs[0].items)
        self.assertTrue('UsedBuildType.java:3' in msgs[0].items)
        self.assertTrue('UsedExplicitBuildType.java:2' in msgs[0].items)


class LogUsageTest(unittest.TestCase):

    def testCheckAndroidCrLogUsage(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile('RandomStuff.java', ['random stuff']),
            MockAffectedFile('HasAndroidLog.java', [
                'import android.util.Log;',
                'some random stuff',
                'Log.d("TAG", "foo");',
            ]),
            MockAffectedFile('HasExplicitUtilLog.java', [
                'some random stuff',
                'android.util.Log.d("TAG", "foo");',
            ]),
            MockAffectedFile('IsInBasePackage.java', [
                'package org.chromium.base;',
                'private static final String TAG = "cr_Foo";',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('IsInBasePackageButImportsLog.java', [
                'package org.chromium.base;',
                'import android.util.Log;',
                'private static final String TAG = "cr_Foo";',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasBothLog.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "cr_Foo";',
                'Log.d(TAG, "foo");',
                'android.util.Log.d("TAG", "foo");',
            ]),
            MockAffectedFile('HasCorrectTag.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "cr_Foo";',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasOldTag.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "cr.Foo";',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasDottedTag.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "cr_foo.bar";',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasDottedTagPublic.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'public static final String TAG = "cr_foo.bar";',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasNoTagDecl.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasIncorrectTagDecl.java', [
                'import org.chromium.base.Log;',
                'private static final String TAHG = "cr_Foo";',
                'some random stuff',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasInlineTag.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "cr_Foo";',
                'Log.d("TAG", "foo");',
            ]),
            MockAffectedFile('HasInlineTagWithSpace.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "cr_Foo";',
                'Log.d("log message", "foo");',
            ]),
            MockAffectedFile('HasUnprefixedTag.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "rubbish";',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasTooLongTag.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "21_characters_long___";',
                'Log.d(TAG, "foo");',
            ]),
            MockAffectedFile('HasTooLongTagWithNoLogCallsInDiff.java', [
                'import org.chromium.base.Log;',
                'some random stuff',
                'private static final String TAG = "21_characters_long___";',
            ]),
        ]

        msgs = PRESUBMIT._CheckAndroidCrLogUsage(mock_input_api,
                                                 mock_output_api)

        self.assertEqual(
            5, len(msgs),
            'Expected %d items, found %d: %s' % (5, len(msgs), msgs))

        # Declaration format
        nb = len(msgs[0].items)
        self.assertEqual(
            2, nb, 'Expected %d items, found %d: %s' % (2, nb, msgs[0].items))
        self.assertTrue('HasNoTagDecl.java' in msgs[0].items)
        self.assertTrue('HasIncorrectTagDecl.java' in msgs[0].items)

        # Tag length
        nb = len(msgs[1].items)
        self.assertEqual(
            2, nb, 'Expected %d items, found %d: %s' % (2, nb, msgs[1].items))
        self.assertTrue('HasTooLongTag.java' in msgs[1].items)
        self.assertTrue(
            'HasTooLongTagWithNoLogCallsInDiff.java' in msgs[1].items)

        # Tag must be a variable named TAG
        nb = len(msgs[2].items)
        self.assertEqual(
            3, nb, 'Expected %d items, found %d: %s' % (3, nb, msgs[2].items))
        self.assertTrue('HasBothLog.java:5' in msgs[2].items)
        self.assertTrue('HasInlineTag.java:4' in msgs[2].items)
        self.assertTrue('HasInlineTagWithSpace.java:4' in msgs[2].items)

        # Util Log usage
        nb = len(msgs[3].items)
        self.assertEqual(
            3, nb, 'Expected %d items, found %d: %s' % (3, nb, msgs[3].items))
        self.assertTrue('HasAndroidLog.java:3' in msgs[3].items)
        self.assertTrue('HasExplicitUtilLog.java:2' in msgs[3].items)
        self.assertTrue('IsInBasePackageButImportsLog.java:4' in msgs[3].items)

        # Tag must not contain
        nb = len(msgs[4].items)
        self.assertEqual(
            3, nb, 'Expected %d items, found %d: %s' % (2, nb, msgs[4].items))
        self.assertTrue('HasDottedTag.java' in msgs[4].items)
        self.assertTrue('HasDottedTagPublic.java' in msgs[4].items)
        self.assertTrue('HasOldTag.java' in msgs[4].items)


class GoogleAnswerUrlFormatTest(unittest.TestCase):

    def testCatchAnswerUrlId(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('somewhere/file.cc', [
                'char* host = '
                '  "https://support.google.com/chrome/answer/123456";'
            ]),
            MockFile('somewhere_else/file.cc', [
                'char* host = '
                '  "https://support.google.com/chrome/a/answer/123456";'
            ]),
        ]

        warnings = PRESUBMIT.CheckGoogleSupportAnswerUrlOnUpload(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual(2, len(warnings[0].items))

    def testAllowAnswerUrlParam(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('somewhere/file.cc', [
                'char* host = '
                '  "https://support.google.com/chrome/?p=cpn_crash_reports";'
            ]),
        ]

        warnings = PRESUBMIT.CheckGoogleSupportAnswerUrlOnUpload(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))


class HardcodedGoogleHostsTest(unittest.TestCase):

    def testWarnOnAssignedLiterals(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('content/file.cc',
                     ['char* host = "https://www.google.com";']),
            MockFile('content/file.cc',
                     ['char* host = "https://www.googleapis.com";']),
            MockFile('content/file.cc',
                     ['char* host = "https://clients1.google.com";']),
        ]

        warnings = PRESUBMIT.CheckHardcodedGoogleHostsInLowerLayers(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual(3, len(warnings[0].items))

    def testAllowInComment(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('content/file.cc',
                     ['char* host = "https://www.aol.com"; // google.com'])
        ]

        warnings = PRESUBMIT.CheckHardcodedGoogleHostsInLowerLayers(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))


class ChromeOsSyncedPrefRegistrationTest(unittest.TestCase):

    def testWarnsOnChromeOsDirectories(self):
        files = [
            MockFile('ash/file.cc', ['PrefRegistrySyncable::SYNCABLE_PREF']),
            MockFile('chrome/browser/chromeos/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_PREF']),
            MockFile('chromeos/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_PREF']),
            MockFile('components/arc/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_PREF']),
            MockFile('components/exo/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_PREF']),
        ]
        input_api = MockInputApi()
        for file in files:
            input_api.files = [file]
            warnings = PRESUBMIT.CheckChromeOsSyncedPrefRegistration(
                input_api, MockOutputApi())
            self.assertEqual(1, len(warnings))

    def testDoesNotWarnOnSyncOsPref(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('chromeos/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_OS_PREF']),
        ]
        warnings = PRESUBMIT.CheckChromeOsSyncedPrefRegistration(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testDoesNotWarnOnOtherDirectories(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('chrome/browser/ui/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_PREF']),
            MockFile('components/sync/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_PREF']),
            MockFile('content/browser/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_PREF']),
            MockFile('a/notchromeos/file.cc',
                     ['PrefRegistrySyncable::SYNCABLE_PREF']),
        ]
        warnings = PRESUBMIT.CheckChromeOsSyncedPrefRegistration(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testSeparateWarningForPriorityPrefs(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('chromeos/file.cc', [
                'PrefRegistrySyncable::SYNCABLE_PREF',
                'PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF'
            ]),
        ]
        warnings = PRESUBMIT.CheckChromeOsSyncedPrefRegistration(
            input_api, MockOutputApi())
        self.assertEqual(2, len(warnings))


class ForwardDeclarationTest(unittest.TestCase):

    def testCheckHeadersOnlyOutsideThirdParty(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('somewhere/file.cc', ['class DummyClass;']),
            MockAffectedFile('third_party/header.h', ['class DummyClass;'])
        ]
        warnings = PRESUBMIT.CheckUselessForwardDeclarations(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testNoNestedDeclaration(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('somewhere/header.h', [
                'class SomeClass {', ' protected:', '  class NotAMatch;', '};'
            ])
        ]
        warnings = PRESUBMIT.CheckUselessForwardDeclarations(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testSubStrings(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('somewhere/header.h', [
                'class NotUsefulClass;', 'struct SomeStruct;',
                'UsefulClass *p1;', 'SomeStructPtr *p2;'
            ])
        ]
        warnings = PRESUBMIT.CheckUselessForwardDeclarations(
            mock_input_api, MockOutputApi())
        self.assertEqual(2, len(warnings))

    def testUselessForwardDeclaration(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('somewhere/header.h', [
                'class DummyClass;', 'struct DummyStruct;',
                'class UsefulClass;', 'std::unique_ptr<UsefulClass> p;'
            ])
        ]
        warnings = PRESUBMIT.CheckUselessForwardDeclarations(
            mock_input_api, MockOutputApi())
        self.assertEqual(2, len(warnings))

    def testBlinkHeaders(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('third_party/blink/header.h', [
                'class DummyClass;',
                'struct DummyStruct;',
            ]),
            MockAffectedFile('third_party\\blink\\header.h', [
                'class DummyClass;',
                'struct DummyStruct;',
            ])
        ]
        warnings = PRESUBMIT.CheckUselessForwardDeclarations(
            mock_input_api, MockOutputApi())
        self.assertEqual(4, len(warnings))


class RelativeIncludesTest(unittest.TestCase):

    def testThirdPartyNotWebKitIgnored(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('third_party/test.cpp', '#include "../header.h"'),
            MockAffectedFile('third_party/test/test.cpp',
                             '#include "../header.h"'),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForRelativeIncludes(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(0, len(errors))

    def testNonCppFileIgnored(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test.py', '#include "../header.h"'),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForRelativeIncludes(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(0, len(errors))

    def testInnocuousChangesAllowed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test.cpp', '#include "header.h"'),
            MockAffectedFile('test2.cpp', '../'),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForRelativeIncludes(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(0, len(errors))

    def testRelativeIncludeNonWebKitProducesError(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test.cpp', ['#include "../header.h"']),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForRelativeIncludes(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(1, len(errors))

    def testRelativeIncludeWebKitProducesError(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('third_party/blink/test.cpp',
                             ['#include "../header.h']),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForRelativeIncludes(mock_input_api,
                                                    mock_output_api)
        self.assertEqual(1, len(errors))


class CCIncludeTest(unittest.TestCase):

    def testThirdPartyNotBlinkIgnored(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('third_party/test.cpp', '#include "file.cc"'),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForCcIncludes(mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))

    def testPythonFileIgnored(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test.py', '#include "file.cc"'),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForCcIncludes(mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))

    def testIncFilesAccepted(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test.py', '#include "file.inc"'),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForCcIncludes(mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))

    def testInnocuousChangesAllowed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test.cpp', '#include "header.h"'),
            MockAffectedFile('test2.cpp', 'Something "file.cc"'),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForCcIncludes(mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))

    def testCcIncludeNonBlinkProducesError(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test.cpp', ['#include "file.cc"']),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForCcIncludes(mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))

    def testCppIncludeBlinkProducesError(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('third_party/blink/test.cpp',
                             ['#include "foo/file.cpp"']),
        ]

        mock_output_api = MockOutputApi()

        errors = PRESUBMIT.CheckForCcIncludes(mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))


class GnGlobForwardTest(unittest.TestCase):

    def testAddBareGlobs(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.gni',
                             ['forward_variables_from(invoker, "*")']),
            MockAffectedFile('base/BUILD.gn',
                             ['forward_variables_from(invoker, "*")']),
        ]
        warnings = PRESUBMIT.CheckGnGlobForward(mock_input_api,
                                                MockOutputApi())
        self.assertEqual(1, len(warnings))
        msg = '\n'.join(warnings[0].items)
        self.assertIn('base/stuff.gni', msg)
        # Should not check .gn files. Local templates don't need to care about
        # visibility / testonly.
        self.assertNotIn('base/BUILD.gn', msg)

    def testValidUses(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.gni',
                             ['forward_variables_from(invoker, "*", [])']),
            MockAffectedFile('base/stuff2.gni', [
                'forward_variables_from(invoker, "*", TESTONLY_AND_VISIBILITY)'
            ]),
            MockAffectedFile(
                'base/stuff3.gni',
                ['forward_variables_from(invoker, [ "testonly" ])']),
        ]
        warnings = PRESUBMIT.CheckGnGlobForward(mock_input_api,
                                                MockOutputApi())
        self.assertEqual([], warnings)


class GnRebasePathTest(unittest.TestCase):

    def testAddAbsolutePath(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/BUILD.gn',
                             ['rebase_path("$target_gen_dir", "//")']),
            MockAffectedFile('base/root/BUILD.gn',
                             ['rebase_path("$target_gen_dir", "/")']),
            MockAffectedFile('base/variable/BUILD.gn',
                             ['rebase_path(target_gen_dir, "/")']),
        ]
        warnings = PRESUBMIT.CheckGnRebasePath(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        msg = '\n'.join(warnings[0].items)
        self.assertIn('base/BUILD.gn', msg)
        self.assertIn('base/root/BUILD.gn', msg)
        self.assertIn('base/variable/BUILD.gn', msg)
        self.assertEqual(3, len(warnings[0].items))

    def testValidUses(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                'base/foo/BUILD.gn',
                ['rebase_path("$target_gen_dir", root_build_dir)']),
            MockAffectedFile(
                'base/bar/BUILD.gn',
                ['rebase_path("$target_gen_dir", root_build_dir, "/")']),
            MockAffectedFile('base/baz/BUILD.gn',
                             ['rebase_path(target_gen_dir, root_build_dir)']),
            MockAffectedFile(
                'base/baz/BUILD.gn',
                ['rebase_path(target_gen_dir, "//some/arbitrary/path")']),
            MockAffectedFile('base/okay_slash/BUILD.gn',
                             ['rebase_path(".", "//")']),
        ]
        warnings = PRESUBMIT.CheckGnRebasePath(mock_input_api, MockOutputApi())
        self.assertEqual([], warnings)


class NewHeaderWithoutGnChangeTest(unittest.TestCase):

    def testAddHeaderWithoutGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', ''),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertTrue('base/stuff.h' in warnings[0].items)

    def testModifyHeader(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', '', action='M'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testDeleteHeader(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', '', action='D'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testAddHeaderWithGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', ''),
            MockAffectedFile('base/BUILD.gn', 'stuff.h'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testAddHeaderWithGni(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', ''),
            MockAffectedFile('base/files.gni', 'stuff.h'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testAddHeaderWithOther(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', ''),
            MockAffectedFile('base/stuff.cc', 'stuff.h'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testAddHeaderWithWrongGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', ''),
            MockAffectedFile('base/BUILD.gn', 'stuff_h'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testAddHeadersWithGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', ''),
            MockAffectedFile('base/another.h', ''),
            MockAffectedFile('base/BUILD.gn', 'another.h\nstuff.h'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testAddHeadersWithWrongGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', ''),
            MockAffectedFile('base/another.h', ''),
            MockAffectedFile('base/BUILD.gn', 'another_h\nstuff.h'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertFalse('base/stuff.h' in warnings[0].items)
        self.assertTrue('base/another.h' in warnings[0].items)

    def testAddHeadersWithWrongGn2(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('base/stuff.h', ''),
            MockAffectedFile('base/another.h', ''),
            MockAffectedFile('base/BUILD.gn', 'another_h\nstuff_h'),
        ]
        warnings = PRESUBMIT.CheckNewHeaderWithoutGnChangeOnUpload(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertTrue('base/stuff.h' in warnings[0].items)
        self.assertTrue('base/another.h' in warnings[0].items)


class CorrectProductNameInMessagesTest(unittest.TestCase):

    def testProductNameInDesc(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/app/google_chrome_strings.grd', [
                '<message name="Foo" desc="Welcome to Chrome">',
                '  Welcome to Chrome!',
                '</message>',
            ]),
            MockAffectedFile('chrome/app/chromium_strings.grd', [
                '<message name="Bar" desc="Welcome to Chrome">',
                '  Welcome to Chromium!',
                '</message>',
            ]),
        ]
        warnings = PRESUBMIT.CheckCorrectProductNameInMessages(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testChromeInChromium(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/app/google_chrome_strings.grd', [
                '<message name="Foo" desc="Welcome to Chrome">',
                '  Welcome to Chrome!',
                '</message>',
            ]),
            MockAffectedFile('chrome/app/chromium_strings.grd', [
                '<message name="Bar" desc="Welcome to Chrome">',
                '  Welcome to Chrome!',
                '</message>',
            ]),
        ]
        warnings = PRESUBMIT.CheckCorrectProductNameInMessages(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertTrue(
            'chrome/app/chromium_strings.grd' in warnings[0].items[0])

    def testChromiumInChrome(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/app/google_chrome_strings.grd', [
                '<message name="Foo" desc="Welcome to Chrome">',
                '  Welcome to Chromium!',
                '</message>',
            ]),
            MockAffectedFile('chrome/app/chromium_strings.grd', [
                '<message name="Bar" desc="Welcome to Chrome">',
                '  Welcome to Chromium!',
                '</message>',
            ]),
        ]
        warnings = PRESUBMIT.CheckCorrectProductNameInMessages(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertTrue(
            'chrome/app/google_chrome_strings.grd:2' in warnings[0].items[0])

    def testChromeForTestingInChromium(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/app/chromium_strings.grd', [
                '<message name="Bar" desc="Welcome to Chrome">',
                '  Welcome to Chrome for Testing!',
                '</message>',
            ]),
        ]
        warnings = PRESUBMIT.CheckCorrectProductNameInMessages(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testChromeForTestingInChrome(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/app/google_chrome_strings.grd', [
                '<message name="Bar" desc="Welcome to Chrome">',
                '  Welcome to Chrome for Testing!',
                '</message>',
            ]),
        ]
        warnings = PRESUBMIT.CheckCorrectProductNameInMessages(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertTrue(
            'chrome/app/google_chrome_strings.grd:2' in warnings[0].items[0])

    def testMultipleInstances(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/app/chromium_strings.grd', [
                '<message name="Bar" desc="Welcome to Chrome">',
                '  Welcome to Chrome!',
                '</message>',
                '<message name="Baz" desc="A correct message">',
                '  Chromium is the software you are using.',
                '</message>',
                '<message name="Bat" desc="An incorrect message">',
                '  Google Chrome is the software you are using.',
                '</message>',
            ]),
        ]
        warnings = PRESUBMIT.CheckCorrectProductNameInMessages(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertTrue(
            'chrome/app/chromium_strings.grd:2' in warnings[0].items[0])
        self.assertTrue(
            'chrome/app/chromium_strings.grd:8' in warnings[0].items[1])

    def testMultipleWarnings(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/app/chromium_strings.grd', [
                '<message name="Bar" desc="Welcome to Chrome">',
                '  Welcome to Chrome!',
                '</message>',
                '<message name="Baz" desc="A correct message">',
                '  Chromium is the software you are using.',
                '</message>',
                '<message name="Bat" desc="An incorrect message">',
                '  Google Chrome is the software you are using.',
                '</message>',
            ]),
            MockAffectedFile(
                'components/components_google_chrome_strings.grd', [
                    '<message name="Bar" desc="Welcome to Chrome">',
                    '  Welcome to Chrome!',
                    '</message>',
                    '<message name="Baz" desc="A correct message">',
                    '  Chromium is the software you are using.',
                    '</message>',
                    '<message name="Bat" desc="An incorrect message">',
                    '  Google Chrome is the software you are using.',
                    '</message>',
                ]),
        ]
        warnings = PRESUBMIT.CheckCorrectProductNameInMessages(
            mock_input_api, MockOutputApi())
        self.assertEqual(2, len(warnings))
        self.assertTrue('components/components_google_chrome_strings.grd:5' in
                        warnings[0].items[0])
        self.assertTrue(
            'chrome/app/chromium_strings.grd:2' in warnings[1].items[0])
        self.assertTrue(
            'chrome/app/chromium_strings.grd:8' in warnings[1].items[1])


class _SecurityOwnersTestCase(unittest.TestCase):

    def _createMockInputApi(self):
        mock_input_api = MockInputApi()

        def FakeRepositoryRoot():
            return mock_input_api.os_path.join('chromium', 'src')

        mock_input_api.change.RepositoryRoot = FakeRepositoryRoot
        self._injectFakeOwnersClient(
            mock_input_api, ['apple@chromium.org', 'orange@chromium.org'])
        return mock_input_api

    def _setupFakeChange(self, input_api):

        class FakeGerrit(object):

            def IsOwnersOverrideApproved(self, issue):
                return False

        input_api.change.issue = 123
        input_api.gerrit = FakeGerrit()

    def _injectFakeOwnersClient(self, input_api, owners):

        class FakeOwnersClient(object):

            def ListOwners(self, f):
                return owners

        input_api.owners_client = FakeOwnersClient()

    def _injectFakeChangeOwnerAndReviewers(self, input_api, owner, reviewers):

        def MockOwnerAndReviewers(input_api,
                                  email_regexp,
                                  approval_needed=False):
            return [owner, reviewers]

        input_api.canned_checks.GetCodereviewOwnerAndReviewers = \
            MockOwnerAndReviewers


class IpcSecurityOwnerTest(_SecurityOwnersTestCase):
    _test_cases = [
        ('*_messages.cc', 'scary_messages.cc'),
        ('*_messages*.h', 'scary_messages.h'),
        ('*_messages*.h', 'scary_messages_android.h'),
        ('*_param_traits*.*', 'scary_param_traits.h'),
        ('*_param_traits*.*', 'scary_param_traits_win.h'),
        ('*.mojom', 'scary.mojom'),
        ('*_mojom_traits*.*', 'scary_mojom_traits.h'),
        ('*_mojom_traits*.*', 'scary_mojom_traits_mac.h'),
        ('*_type_converter*.*', 'scary_type_converter.h'),
        ('*_type_converter*.*', 'scary_type_converter_nacl.h'),
        ('*.aidl', 'scary.aidl'),
    ]

    def testHasCorrectPerFileRulesAndSecurityReviewer(self):
        mock_input_api = self._createMockInputApi()
        new_owners_file_path = mock_input_api.os_path.join(
            'services', 'goat', 'public', 'OWNERS')
        new_owners_file = [
            'per-file *.mojom=set noparent',
            'per-file *.mojom=file://ipc/SECURITY_OWNERS'
        ]

        def FakeReadFile(filename):
            self.assertEqual(
                mock_input_api.os_path.join('chromium', 'src',
                                            new_owners_file_path), filename)
            return '\n'.join(new_owners_file)

        mock_input_api.ReadFile = FakeReadFile
        mock_input_api.files = [
            MockAffectedFile(new_owners_file_path, new_owners_file),
            MockAffectedFile(
                mock_input_api.os_path.join('services', 'goat', 'public',
                                            'goat.mojom'),
                ['// Scary contents.'])
        ]
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['orange@chromium.org'])
        mock_input_api.is_committing = True
        mock_input_api.dry_run = False
        mock_output_api = MockOutputApi()
        results = PRESUBMIT.CheckSecurityOwners(mock_input_api,
                                                mock_output_api)
        self.assertEqual(0, len(results))

    def testMissingSecurityReviewerAtUpload(self):
        mock_input_api = self._createMockInputApi()
        new_owners_file_path = mock_input_api.os_path.join(
            'services', 'goat', 'public', 'OWNERS')
        new_owners_file = [
            'per-file *.mojom=set noparent',
            'per-file *.mojom=file://ipc/SECURITY_OWNERS'
        ]

        def FakeReadFile(filename):
            self.assertEqual(
                mock_input_api.os_path.join('chromium', 'src',
                                            new_owners_file_path), filename)
            return '\n'.join(new_owners_file)

        mock_input_api.ReadFile = FakeReadFile
        mock_input_api.files = [
            MockAffectedFile(new_owners_file_path, new_owners_file),
            MockAffectedFile(
                mock_input_api.os_path.join('services', 'goat', 'public',
                                            'goat.mojom'),
                ['// Scary contents.'])
        ]
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_input_api.is_committing = False
        mock_input_api.dry_run = False
        mock_output_api = MockOutputApi()
        results = PRESUBMIT.CheckSecurityOwners(mock_input_api,
                                                mock_output_api)
        self.assertEqual(1, len(results))
        self.assertEqual('notify', results[0].type)
        self.assertEqual(
            'Review from an owner in ipc/SECURITY_OWNERS is required for the '
            'following newly-added files:', results[0].message)

    def testMissingSecurityReviewerAtDryRunCommit(self):
        mock_input_api = self._createMockInputApi()
        new_owners_file_path = mock_input_api.os_path.join(
            'services', 'goat', 'public', 'OWNERS')
        new_owners_file = [
            'per-file *.mojom=set noparent',
            'per-file *.mojom=file://ipc/SECURITY_OWNERS'
        ]

        def FakeReadFile(filename):
            self.assertEqual(
                mock_input_api.os_path.join('chromium', 'src',
                                            new_owners_file_path), filename)
            return '\n'.join(new_owners_file)

        mock_input_api.ReadFile = FakeReadFile
        mock_input_api.files = [
            MockAffectedFile(new_owners_file_path, new_owners_file),
            MockAffectedFile(
                mock_input_api.os_path.join('services', 'goat', 'public',
                                            'goat.mojom'),
                ['// Scary contents.'])
        ]
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_input_api.is_committing = True
        mock_input_api.dry_run = True
        mock_output_api = MockOutputApi()
        results = PRESUBMIT.CheckSecurityOwners(mock_input_api,
                                                mock_output_api)
        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertEqual(
            'Review from an owner in ipc/SECURITY_OWNERS is required for the '
            'following newly-added files:', results[0].message)

    def testMissingSecurityApprovalAtRealCommit(self):
        mock_input_api = self._createMockInputApi()
        new_owners_file_path = mock_input_api.os_path.join(
            'services', 'goat', 'public', 'OWNERS')
        new_owners_file = [
            'per-file *.mojom=set noparent',
            'per-file *.mojom=file://ipc/SECURITY_OWNERS'
        ]

        def FakeReadFile(filename):
            self.assertEqual(
                mock_input_api.os_path.join('chromium', 'src',
                                            new_owners_file_path), filename)
            return '\n'.join(new_owners_file)

        mock_input_api.ReadFile = FakeReadFile
        mock_input_api.files = [
            MockAffectedFile(new_owners_file_path, new_owners_file),
            MockAffectedFile(
                mock_input_api.os_path.join('services', 'goat', 'public',
                                            'goat.mojom'),
                ['// Scary contents.'])
        ]
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_input_api.is_committing = True
        mock_input_api.dry_run = False
        mock_output_api = MockOutputApi()
        results = PRESUBMIT.CheckSecurityOwners(mock_input_api,
                                                mock_output_api)
        self.assertEqual('error', results[0].type)
        self.assertEqual(
            'Review from an owner in ipc/SECURITY_OWNERS is required for the '
            'following newly-added files:', results[0].message)

    def testIpcChangeNeedsSecurityOwner(self):
        for is_committing in [True, False]:
            for pattern, filename in self._test_cases:
                with self.subTest(
                        line=
                        f'is_committing={is_committing}, filename={filename}'):
                    mock_input_api = self._createMockInputApi()
                    mock_input_api.files = [
                        MockAffectedFile(
                            mock_input_api.os_path.join(
                                'services', 'goat', 'public', filename),
                            ['// Scary contents.'])
                    ]
                    self._setupFakeChange(mock_input_api)
                    self._injectFakeChangeOwnerAndReviewers(
                        mock_input_api, 'owner@chromium.org',
                        ['banana@chromium.org'])
                    mock_input_api.is_committing = is_committing
                    mock_input_api.dry_run = False
                    mock_output_api = MockOutputApi()
                    results = PRESUBMIT.CheckSecurityOwners(
                        mock_input_api, mock_output_api)
                    self.assertEqual(1, len(results))
                    self.assertEqual('error', results[0].type)
                    self.assertTrue(results[0].message.replace(
                        '\\', '/'
                    ).startswith(
                        'Found missing OWNERS lines for security-sensitive files. '
                        'Please add the following lines to services/goat/public/OWNERS:'
                    ))
                    self.assertEqual(['ipc-security-reviews@chromium.org'],
                                     mock_output_api.more_cc)

    def testServiceManifestChangeNeedsSecurityOwner(self):
        mock_input_api = self._createMockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                mock_input_api.os_path.join('services', 'goat', 'public',
                                            'cpp', 'manifest.cc'),
                [
                    '#include "services/goat/public/cpp/manifest.h"',
                    'const service_manager::Manifest& GetManifest() {}',
                ])
        ]
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSecurityOwners(mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(errors[0].message.replace('\\', '/').startswith(
            'Found missing OWNERS lines for security-sensitive files. '
            'Please add the following lines to services/goat/public/cpp/OWNERS:'
        ))
        self.assertEqual(['ipc-security-reviews@chromium.org'],
                         mock_output_api.more_cc)

    def testNonServiceManifestSourceChangesDoNotRequireSecurityOwner(self):
        mock_input_api = self._createMockInputApi()
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_input_api.files = [
            MockAffectedFile('some/non/service/thing/foo_manifest.cc', [
                'const char kNoEnforcement[] = "not a manifest!";',
            ])
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSecurityOwners(mock_input_api, mock_output_api)
        self.assertEqual([], errors)
        self.assertEqual([], mock_output_api.more_cc)


class FuchsiaSecurityOwnerTest(_SecurityOwnersTestCase):

    def testFidlChangeNeedsSecurityOwner(self):
        mock_input_api = self._createMockInputApi()
        mock_input_api.files = [
            MockAffectedFile('potentially/scary/ipc.fidl',
                             ['library test.fidl'])
        ]
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSecurityOwners(mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(errors[0].message.replace('\\', '/').startswith(
            'Found missing OWNERS lines for security-sensitive files. '
            'Please add the following lines to potentially/scary/OWNERS:'))

    def testComponentManifestV1ChangeNeedsSecurityOwner(self):
        mock_input_api = self._createMockInputApi()
        mock_input_api.files = [
            MockAffectedFile('potentially/scary/v2_manifest.cmx',
                             ['{ "that is no": "manifest!" }'])
        ]
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSecurityOwners(mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(errors[0].message.replace('\\', '/').startswith(
            'Found missing OWNERS lines for security-sensitive files. '
            'Please add the following lines to potentially/scary/OWNERS:'))

    def testComponentManifestV2NeedsSecurityOwner(self):
        mock_input_api = self._createMockInputApi()
        mock_input_api.files = [
            MockAffectedFile('potentially/scary/v2_manifest.cml',
                             ['{ "that is no": "manifest!" }'])
        ]
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSecurityOwners(mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(errors[0].message.replace('\\', '/').startswith(
            'Found missing OWNERS lines for security-sensitive files. '
            'Please add the following lines to potentially/scary/OWNERS:'))

    def testThirdPartyTestsDoNotRequireSecurityOwner(self):
        mock_input_api = MockInputApi()
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_input_api.files = [
            MockAffectedFile('third_party/crashpad/test/tests.cmx', [
                'const char kNoEnforcement[] = "Security?!? Pah!";',
            ])
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSecurityOwners(mock_input_api, mock_output_api)
        self.assertEqual([], errors)

    def testOtherFuchsiaChangesDoNotRequireSecurityOwner(self):
        mock_input_api = MockInputApi()
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_input_api.files = [
            MockAffectedFile(
                'some/non/service/thing/fuchsia_fidl_cml_cmx_magic.cc', [
                    'const char kNoEnforcement[] = "Security?!? Pah!";',
                ])
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSecurityOwners(mock_input_api, mock_output_api)
        self.assertEqual([], errors)


class SecurityChangeTest(_SecurityOwnersTestCase):

    def testDiffGetServiceSandboxType(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('services/goat/teleporter_host.cc', [
                'template <>', 'inline content::SandboxType',
                'content::GetServiceSandboxType<chrome::mojom::GoatTeleporter>() {',
                '#if defined(OS_WIN)', '  return SandboxType::kGoaty;',
                '#else', '  return SandboxType::kNoSandbox;',
                '#endif  // !defined(OS_WIN)', '}'
            ]),
        ]
        files_to_functions = PRESUBMIT._GetFilesUsingSecurityCriticalFunctions(
            mock_input_api)
        self.assertEqual(
            {
                'services/goat/teleporter_host.cc':
                set(['content::GetServiceSandboxType<>()'])
            }, files_to_functions)

    def testDiffRemovingLine(self):
        mock_input_api = MockInputApi()
        mock_file = MockAffectedFile('services/goat/teleporter_host.cc', '')
        mock_file._scm_diff = """--- old 2020-05-04 14:08:25.000000000 -0400
+++ new 2020-05-04 14:08:32.000000000 -0400
@@ -1,5 +1,4 @@
 template <>
 inline content::SandboxType
-content::GetServiceSandboxType<chrome::mojom::GoatTeleporter>() {
 #if defined(OS_WIN)
   return SandboxType::kGoaty;
"""
        mock_input_api.files = [mock_file]
        files_to_functions = PRESUBMIT._GetFilesUsingSecurityCriticalFunctions(
            mock_input_api)
        self.assertEqual(
            {
                'services/goat/teleporter_host.cc':
                set(['content::GetServiceSandboxType<>()'])
            }, files_to_functions)

    def testChangeOwnersMissing(self):
        mock_input_api = self._createMockInputApi()
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_input_api.is_committing = False
        mock_input_api.files = [
            MockAffectedFile('file.cc',
                             ['GetServiceSandboxType<Goat>(Sandbox)'])
        ]
        mock_output_api = MockOutputApi()
        result = PRESUBMIT.CheckSecurityChanges(mock_input_api,
                                                mock_output_api)
        self.assertEqual(1, len(result))
        self.assertEqual(result[0].type, 'notify')
        self.assertEqual(result[0].message,
            'The following files change calls to security-sensitive functions\n' \
            'that need to be reviewed by ipc/SECURITY_OWNERS.\n'
            '  file.cc\n'
            '    content::GetServiceSandboxType<>()\n\n')

    def testChangeOwnersMissingAtCommit(self):
        mock_input_api = self._createMockInputApi()
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'owner@chromium.org',
                                                ['banana@chromium.org'])
        mock_input_api.is_committing = True
        mock_input_api.dry_run = False
        mock_input_api.files = [
            MockAffectedFile('file.cc',
                             ['GetServiceSandboxType<mojom::Goat>()'])
        ]
        mock_output_api = MockOutputApi()
        result = PRESUBMIT.CheckSecurityChanges(mock_input_api,
                                                mock_output_api)
        self.assertEqual(1, len(result))
        self.assertEqual(result[0].type, 'error')
        self.assertEqual(result[0].message,
            'The following files change calls to security-sensitive functions\n' \
            'that need to be reviewed by ipc/SECURITY_OWNERS.\n'
            '  file.cc\n'
            '    content::GetServiceSandboxType<>()\n\n')

    def testChangeOwnersPresent(self):
        mock_input_api = self._createMockInputApi()
        self._injectFakeChangeOwnerAndReviewers(
            mock_input_api, 'owner@chromium.org',
            ['apple@chromium.org', 'banana@chromium.org'])
        mock_input_api.files = [
            MockAffectedFile('file.cc', ['WithSandboxType(Sandbox)'])
        ]
        mock_output_api = MockOutputApi()
        result = PRESUBMIT.CheckSecurityChanges(mock_input_api,
                                                mock_output_api)
        self.assertEqual(0, len(result))

    def testChangeOwnerIsSecurityOwner(self):
        mock_input_api = self._createMockInputApi()
        self._setupFakeChange(mock_input_api)
        self._injectFakeChangeOwnerAndReviewers(mock_input_api,
                                                'orange@chromium.org',
                                                ['pear@chromium.org'])
        mock_input_api.files = [
            MockAffectedFile('file.cc', ['GetServiceSandboxType<T>(Sandbox)'])
        ]
        mock_output_api = MockOutputApi()
        result = PRESUBMIT.CheckSecurityChanges(mock_input_api,
                                                mock_output_api)
        self.assertEqual(1, len(result))


class BannedTypeCheckTest(unittest.TestCase):

    def testBannedJsFunctions(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('ash/webui/file.js', ['chrome.send(something);']),
            MockFile('some/js/ok/file.js', ['chrome.send(something);']),
        ]

        results = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())

        self.assertEqual(1, len(results))
        self.assertTrue('ash/webui/file.js' in results[0].message)
        self.assertFalse('some/js/ok/file.js' in results[0].message)

    def testBannedJavaFunctions(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('some/java/problematic/diskread.java',
                     ['StrictMode.allowThreadDiskReads();']),
            MockFile('some/java/problematic/diskwrite.java',
                     ['StrictMode.allowThreadDiskWrites();']),
            MockFile('some/java/ok/diskwrite.java',
                     ['StrictModeContext.allowDiskWrites();']),
            MockFile('some/java/problematic/waitidleforsync.java',
                     ['instrumentation.waitForIdleSync();']),
            MockFile('some/java/problematic/registerreceiver.java',
                     ['context.registerReceiver();']),
            MockFile('some/java/problematic/property.java',
                     ['new Property<abc, Integer>;']),
            MockFile('some/java/problematic/requestlayout.java',
                     ['requestLayout();']),
            MockFile('some/java/problematic/lastprofile.java',
                     ['ProfileManager.getLastUsedRegularProfile();']),
            MockFile('some/java/problematic/getdrawable1.java',
                     ['ResourcesCompat.getDrawable();']),
            MockFile('some/java/problematic/getdrawable2.java',
                     ['getResources().getDrawable();']),
        ]

        errors = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())
        self.assertEqual(2, len(errors))
        self.assertTrue(
            'some/java/problematic/diskread.java' in errors[0].message)
        self.assertTrue(
            'some/java/problematic/diskwrite.java' in errors[0].message)
        self.assertFalse('some/java/ok/diskwrite.java' in errors[0].message)
        self.assertFalse('some/java/ok/diskwrite.java' in errors[1].message)
        self.assertTrue(
            'some/java/problematic/waitidleforsync.java' in errors[0].message)
        self.assertTrue(
            'some/java/problematic/registerreceiver.java' in errors[1].message)
        self.assertTrue(
            'some/java/problematic/property.java' in errors[0].message)
        self.assertTrue(
            'some/java/problematic/requestlayout.java' in errors[0].message)
        self.assertTrue(
            'some/java/problematic/lastprofile.java' in errors[0].message)
        self.assertTrue(
            'some/java/problematic/getdrawable1.java' in errors[0].message)
        self.assertTrue(
            'some/java/problematic/getdrawable2.java' in errors[0].message)

    def testBannedCppFunctions(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('some/cpp/problematic/file.cc', ['using namespace std;']),
            MockFile('third_party/blink/problematic/file.cc',
                     ['GetInterfaceProvider()']),
            MockFile('some/cpp/ok/file.cc', ['using std::string;']),
            MockFile('some/cpp/problematic/file2.cc',
                     ['set_owned_by_client()']),
            MockFile('some/cpp/nocheck/file.cc',
                     ['using namespace std;  // nocheck']),
            MockFile('some/cpp/comment/file.cc',
                     ['  // A comment about `using namespace std;`']),
            MockFile('some/cpp/problematic/file3.cc', [
                'params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET'
            ]),
            MockFile('some/cpp/problematic/file4.cc', [
                'params.ownership = Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET'
            ]),
            MockFile('some/cpp/problematic/file5.cc', [
                'Browser* browser = chrome::FindBrowserWithTab(web_contents)'
            ]),
            MockFile('allowed_ranges_usage.cc', ['std::ranges::begin(vec)']),
            MockFile('banned_ranges_usage.cc',
                     ['std::ranges::subrange(first, last)']),
            MockFile('views_usage.cc', ['std::views::all(vec)']),
        ]

        results = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())

        # warnings are results[0], errors are results[1]
        self.assertEqual(2, len(results))
        self.assertTrue('some/cpp/problematic/file.cc' in results[1].message)
        self.assertTrue(
            'third_party/blink/problematic/file.cc' in results[0].message)
        self.assertTrue('some/cpp/ok/file.cc' not in results[1].message)
        self.assertTrue('some/cpp/problematic/file2.cc' in results[0].message)
        self.assertTrue('some/cpp/problematic/file3.cc' in results[0].message)
        self.assertTrue('some/cpp/problematic/file4.cc' in results[0].message)
        self.assertTrue('some/cpp/problematic/file5.cc' in results[0].message)
        self.assertFalse('some/cpp/nocheck/file.cc' in results[0].message)
        self.assertFalse('some/cpp/nocheck/file.cc' in results[1].message)
        self.assertFalse('some/cpp/comment/file.cc' in results[0].message)
        self.assertFalse('some/cpp/comment/file.cc' in results[1].message)
        self.assertFalse('allowed_ranges_usage.cc' in results[0].message)
        self.assertFalse('allowed_ranges_usage.cc' in results[1].message)
        self.assertTrue('banned_ranges_usage.cc' in results[1].message)
        self.assertTrue('views_usage.cc' in results[1].message)

    def testBannedCppRandomFunctions(self):
        banned_rngs = [
            'absl::BitGen',
            'absl::InsecureBitGen',
            'std::linear_congruential_engine',
            'std::mersenne_twister_engine',
            'std::subtract_with_carry_engine',
            'std::discard_block_engine',
            'std::independent_bits_engine',
            'std::shuffle_order_engine',
            'std::minstd_rand0',
            'std::minstd_rand',
            'std::mt19937',
            'std::mt19937_64',
            'std::ranlux24_base',
            'std::ranlux48_base',
            'std::ranlux24',
            'std::ranlux48',
            'std::knuth_b',
            'std::default_random_engine',
            'std::random_device',
        ]
        for banned_rng in banned_rngs:
            input_api = MockInputApi()
            input_api.files = [
                MockFile('some/cpp/problematic/file.cc',
                         [f'{banned_rng} engine;']),
                MockFile('third_party/blink/problematic/file.cc',
                         [f'{banned_rng} engine;']),
                MockFile('third_party/ok/file.cc', [f'{banned_rng} engine;']),
            ]
            results = PRESUBMIT.CheckNoBannedFunctions(input_api,
                                                       MockOutputApi())
            self.assertEqual(1, len(results), banned_rng)
            self.assertTrue(
                'some/cpp/problematic/file.cc' in results[0].message,
                banned_rng)
            self.assertTrue(
                'third_party/blink/problematic/file.cc' in results[0].message,
                banned_rng)
            self.assertFalse('third_party/ok/file.cc' in results[0].message,
                             banned_rng)

    def testBannedIosObjcFunctions(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('some/ios/file.mm',
                     ['TEST(SomeClassTest, SomeInteraction) {', '}']),
            MockFile('some/mac/file.mm',
                     ['TEST(SomeClassTest, SomeInteraction) {', '}']),
            MockFile('another/ios_file.mm',
                     ['class SomeTest : public testing::Test {};']),
            MockFile(
                'some/ios/file_egtest.mm',
                ['- (void)testSomething { EXPECT_OCMOCK_VERIFY(aMock); }']),
            MockFile('some/ios/file_unittest.mm', [
                'TEST_F(SomeTest, TestThis) { EXPECT_OCMOCK_VERIFY(aMock); }'
            ]),
        ]

        errors = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())
        self.assertEqual(1, len(errors))
        self.assertTrue('some/ios/file.mm' in errors[0].message)
        self.assertTrue('another/ios_file.mm' in errors[0].message)
        self.assertTrue('some/mac/file.mm' not in errors[0].message)
        self.assertTrue('some/ios/file_egtest.mm' in errors[0].message)
        self.assertTrue('some/ios/file_unittest.mm' not in errors[0].message)

    def testBannedMojoFunctions(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('some/cpp/problematic/file2.cc', ['mojo::ConvertTo<>']),
            MockFile('third_party/blink/ok/file3.cc', ['mojo::ConvertTo<>']),
            MockFile('content/renderer/ok/file3.cc', ['mojo::ConvertTo<>']),
        ]

        results = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())

        # warnings are results[0], errors are results[1]
        self.assertEqual(1, len(results))
        self.assertTrue('some/cpp/problematic/file2.cc' in results[0].message)
        self.assertTrue(
            'third_party/blink/ok/file3.cc' not in results[0].message)
        self.assertTrue(
            'content/renderer/ok/file3.cc' not in results[0].message)

    def testBannedMojomPatterns(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'bad.mojom',
                ['struct Bad {', '  handle<shared_buffer> buffer;', '};']),
            MockFile('good.mojom', [
                'struct  Good {',
                '  mojo_base.mojom.ReadOnlySharedMemoryRegion region1;',
                '  mojo_base.mojom.WritableSharedMemoryRegion region2;',
                '  mojo_base.mojom.UnsafeSharedMemoryRegion region3;', '};'
            ]),
        ]

        results = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())

        # warnings are results[0], errors are results[1]
        self.assertEqual(1, len(results))
        self.assertTrue('bad.mojom' in results[0].message)
        self.assertTrue('good.mojom' not in results[0].message)

class NoProductionCodeUsingTestOnlyFunctionsTest(unittest.TestCase):

    def testTruePositives(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('some/path/foo.cc', ['foo_for_testing();']),
            MockFile('some/path/foo.mm', ['FooForTesting();']),
            MockFile('some/path/foo.cxx', ['FooForTests();']),
            MockFile('some/path/foo.cpp', ['foo_for_test();']),
        ]

        results = PRESUBMIT.CheckNoProductionCodeUsingTestOnlyFunctions(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(4, len(results[0].items))
        self.assertTrue('foo.cc' in results[0].items[0])
        self.assertTrue('foo.mm' in results[0].items[1])
        self.assertTrue('foo.cxx' in results[0].items[2])
        self.assertTrue('foo.cpp' in results[0].items[3])

    def testFalsePositives(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('some/path/foo.h', ['foo_for_testing();']),
            MockFile('some/path/foo.mm', ['FooForTesting() {']),
            MockFile('some/path/foo.cc', ['::FooForTests();']),
            MockFile('some/path/foo.cpp', ['// foo_for_test();']),
            MockFile('some/path/foo.cxx', ['foo_for_test(); // IN-TEST']),
        ]

        results = PRESUBMIT.CheckNoProductionCodeUsingTestOnlyFunctions(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testAllowedFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('path/foo_unittest.cc', ['foo_for_testing();']),
            MockFile('path/bar_unittest_mac.cc', ['foo_for_testing();']),
            MockFile('path/baz_unittests.cc', ['foo_for_testing();']),
        ]

        results = PRESUBMIT.CheckNoProductionCodeUsingTestOnlyFunctions(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))


class NoProductionJavaCodeUsingTestOnlyFunctionsTest(unittest.TestCase):

    def testTruePositives(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/java/src/foo.java', ['FooForTesting();']),
            MockFile('dir/java/src/bar.java', ['FooForTests(x);']),
            MockFile('dir/java/src/baz.java', ['FooForTest(', 'y', ');']),
            MockFile('dir/java/src/mult.java', [
                'int x = SomethingLongHere()',
                '    * SomethingLongHereForTesting();'
            ])
        ]

        results = PRESUBMIT.CheckNoProductionCodeUsingTestOnlyFunctionsJava(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(4, len(results[0].items))
        self.assertTrue('foo.java' in results[0].items[0])
        self.assertTrue('bar.java' in results[0].items[1])
        self.assertTrue('baz.java' in results[0].items[2])
        self.assertTrue('mult.java' in results[0].items[3])

    def testFalsePositives(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/java/src/foo.xml', ['FooForTesting();']),
            MockFile('dir/java/src/foo.java', ['FooForTests() {']),
            MockFile('dir/java/src/bar.java', ['// FooForTest();']),
            MockFile('dir/java/src/bar2.java', ['x = 1; // FooForTest();']),
            MockFile('dir/java/src/bar3.java', ['@VisibleForTesting']),
            MockFile('dir/java/src/bar4.java', ['@VisibleForTesting()']),
            MockFile('dir/java/src/bar5.java', [
                '@VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)'
            ]),
            MockFile('dir/javatests/src/baz.java', ['FooForTest(', 'y', ');']),
            MockFile('dir/junit/src/baz.java', ['FooForTest(', 'y', ');']),
            MockFile('dir/junit/src/javadoc.java',
                     ['/** Use FooForTest(); to obtain foo in tests.'
                      ' */']),
            MockFile(
                'dir/junit/src/javadoc2.java',
                ['/** ', ' * Use FooForTest(); to obtain foo in tests.'
                 ' */']),
            MockFile('dir/java/src/bar6.java',
                     ['FooForTesting(); // IN-TEST']),
        ]

        results = PRESUBMIT.CheckNoProductionCodeUsingTestOnlyFunctionsJava(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))


class NewImagesWarningTest(unittest.TestCase):

    def testTruePositives(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/android/res/drawable/foo.png', []),
            MockFile('dir/android/res/drawable-v21/bar.svg', []),
            MockFile('dir/android/res/mipmap-v21-en/baz.webp', []),
            MockFile('dir/android/res_gshoe/drawable-mdpi/foobar.png', []),
        ]

        results = PRESUBMIT._CheckNewImagesWarning(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(4, len(results[0].items))
        self.assertTrue('foo.png' in results[0].items[0].LocalPath())
        self.assertTrue('bar.svg' in results[0].items[1].LocalPath())
        self.assertTrue('baz.webp' in results[0].items[2].LocalPath())
        self.assertTrue('foobar.png' in results[0].items[3].LocalPath())

    def testFalsePositives(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/pngs/README.md', []),
            MockFile('java/test/res/drawable/foo.png', []),
            MockFile('third_party/blink/foo.png', []),
            MockFile('dir/third_party/libpng/src/foo.cc', ['foobar']),
            MockFile('dir/resources.webp/.gitignore', ['foo.png']),
        ]

        results = PRESUBMIT._CheckNewImagesWarning(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual(0, len(results))

class ProductIconsTest(unittest.TestCase):

    def test(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('components/vector_icons/google_jetpack.icon', []),
            MockFile('components/vector_icons/generic_jetpack.icon', []),
        ]

        results = PRESUBMIT.CheckNoProductIconsAddedToPublicRepo(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(1, len(results[0].items))
        self.assertTrue('google_jetpack.icon' in results[0].items[0])

class CheckUniquePtrTest(unittest.TestCase):

    def testTruePositivesNullptr(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/baz.cc', ['std::unique_ptr<T>()']),
            MockFile('dir/baz-p.cc', ['std::unique_ptr<T<P>>()']),
        ]

        results = PRESUBMIT.CheckUniquePtrOnUpload(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertTrue('nullptr' in results[0].message)
        self.assertEqual(2, len(results[0].items))
        self.assertTrue('baz.cc' in results[0].items[0])
        self.assertTrue('baz-p.cc' in results[0].items[1])

    def testTruePositivesConstructor(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/foo.cc', ['return std::unique_ptr<T>(foo);']),
            MockFile('dir/bar.mm', ['bar = std::unique_ptr<T>(foo)']),
            MockFile('dir/mult.cc', [
                'return',
                '    std::unique_ptr<T>(barVeryVeryLongFooSoThatItWouldNotFitAbove);'
            ]),
            MockFile('dir/mult2.cc', [
                'barVeryVeryLongLongBaaaaaarSoThatTheLineLimitIsAlmostReached =',
                '    std::unique_ptr<T>(foo);'
            ]),
            MockFile('dir/mult3.cc', [
                'bar = std::unique_ptr<T>(',
                '    fooVeryVeryVeryLongStillGoingWellThisWillTakeAWhileFinallyThere);'
            ]),
            MockFile('dir/multi_arg.cc', [
                'auto p = std::unique_ptr<std::pair<T, D>>(new std::pair(T, D));'
            ]),
        ]

        results = PRESUBMIT.CheckUniquePtrOnUpload(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertTrue('std::make_unique' in results[0].message)
        self.assertEqual(6, len(results[0].items))
        self.assertTrue('foo.cc' in results[0].items[0])
        self.assertTrue('bar.mm' in results[0].items[1])
        self.assertTrue('mult.cc' in results[0].items[2])
        self.assertTrue('mult2.cc' in results[0].items[3])
        self.assertTrue('mult3.cc' in results[0].items[4])
        self.assertTrue('multi_arg.cc' in results[0].items[5])

    def testFalsePositives(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/foo.cc', ['return std::unique_ptr<T[]>(foo);']),
            MockFile('dir/bar.mm', ['bar = std::unique_ptr<T[]>(foo)']),
            MockFile('dir/file.cc', ['std::unique_ptr<T> p = Foo();']),
            MockFile('dir/baz.cc',
                     ['std::unique_ptr<T> result = std::make_unique<T>();']),
            MockFile('dir/baz2.cc',
                     ['std::unique_ptr<T> result = std::make_unique<T>(']),
            MockFile('dir/nested.cc', ['set<std::unique_ptr<T>>();']),
            MockFile('dir/nested2.cc', ['map<U, std::unique_ptr<T>>();']),
            # Changed line is inside a multiline template block.
            MockFile('dir/template.cc', [' std::unique_ptr<T>>(']),
            MockFile('dir/template2.cc', [' std::unique_ptr<T>>()']),

            # Two-argument invocation of std::unique_ptr is exempt because there is
            # no equivalent using std::make_unique.
            MockFile('dir/multi_arg.cc',
                     ['auto p = std::unique_ptr<T, D>(new T(), D());']),
        ]

        results = PRESUBMIT.CheckUniquePtrOnUpload(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual([], results)

class CheckNoDirectIncludesHeadersWhichRedefineStrCat(unittest.TestCase):

    def testBlocksDirectIncludes(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/foo_win.cc', ['#include "shlwapi.h"']),
            MockFile('dir/bar.h', ['#include <propvarutil.h>']),
            MockFile('dir/baz.h', ['#include <atlbase.h>']),
            MockFile('dir/jumbo.h', ['#include "sphelper.h"']),
        ]
        results = PRESUBMIT.CheckNoStrCatRedefines(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(4, len(results[0].items))
        self.assertTrue('StrCat' in results[0].message)
        self.assertTrue('foo_win.cc' in results[0].items[0])
        self.assertTrue('bar.h' in results[0].items[1])
        self.assertTrue('baz.h' in results[0].items[2])
        self.assertTrue('jumbo.h' in results[0].items[3])

    def testAllowsToIncludeWrapper(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/baz_win.cc', ['#include "base/win/shlwapi.h"']),
            MockFile('dir/baz-win.h', ['#include "base/win/atl.h"']),
        ]
        results = PRESUBMIT.CheckNoStrCatRedefines(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual(0, len(results))

    def testAllowsToCreateWrapper(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('base/win/shlwapi.h', [
                '#include <shlwapi.h>',
                '#include "base/win/windows_defines.inc"'
            ]),
        ]
        results = PRESUBMIT.CheckNoStrCatRedefines(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual(0, len(results))

    def testIgnoresNonImplAndHeaders(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('dir/foo_win.txt', ['#include "shlwapi.h"']),
            MockFile('dir/bar.asm', ['#include <propvarutil.h>']),
        ]
        results = PRESUBMIT.CheckNoStrCatRedefines(mock_input_api,
                                                   MockOutputApi())
        self.assertEqual(0, len(results))


class StringTest(unittest.TestCase):
    """Tests ICU syntax check and translation screenshots check."""

    # An empty grd file.
    OLD_GRD_CONTENTS = """<?xml version="1.0" encoding="UTF-8"?>
           <grit latest_public_release="1" current_release="1">
             <release seq="1">
               <messages></messages>
             </release>
           </grit>
        """.splitlines()
    # A grd file with a single message.
    NEW_GRD_CONTENTS1 = """<?xml version="1.0" encoding="UTF-8"?>
           <grit latest_public_release="1" current_release="1">
             <release seq="1">
               <messages>
                 <message name="IDS_TEST1">
                   Test string 1
                 </message>
                 <message name="IDS_TEST_STRING_NON_TRANSLATEABLE1"
                     translateable="false">
                   Non translateable message 1, should be ignored
                 </message>
                 <message name="IDS_TEST_STRING_ACCESSIBILITY"
                     is_accessibility_with_no_ui="true">
                   Accessibility label 1, should be ignored
                 </message>
               </messages>
             </release>
           </grit>
        """.splitlines()
    # A grd file with two messages.
    NEW_GRD_CONTENTS2 = """<?xml version="1.0" encoding="UTF-8"?>
           <grit latest_public_release="1" current_release="1">
             <release seq="1">
               <messages>
                 <message name="IDS_TEST1">
                   Test string 1
                 </message>
                 <message name="IDS_TEST2">
                   Test string 2
                 </message>
                 <message name="IDS_TEST_STRING_NON_TRANSLATEABLE2"
                     translateable="false">
                   Non translateable message 2, should be ignored
                 </message>
               </messages>
             </release>
           </grit>
        """.splitlines()
    # A grd file with one ICU syntax message without syntax errors.
    NEW_GRD_CONTENTS_ICU_SYNTAX_OK1 = """<?xml version="1.0" encoding="UTF-8"?>
           <grit latest_public_release="1" current_release="1">
             <release seq="1">
               <messages>
                 <message name="IDS_TEST1">
                   {NUM, plural,
                    =1 {Test text for numeric one}
                    other {Test text for plural with {NUM} as number}}
                 </message>
               </messages>
             </release>
           </grit>
        """.splitlines()
    # A grd file with one ICU syntax message without syntax errors.
    NEW_GRD_CONTENTS_ICU_SYNTAX_OK2 = """<?xml version="1.0" encoding="UTF-8"?>
           <grit latest_public_release="1" current_release="1">
             <release seq="1">
               <messages>
                 <message name="IDS_TEST1">
                   {NUM, plural,
                    =1 {Different test text for numeric one}
                    other {Different test text for plural with {NUM} as number}}
                 </message>
               </messages>
             </release>
           </grit>
        """.splitlines()
    # A grd file with multiple ICU syntax messages without syntax errors.
    NEW_GRD_CONTENTS_ICU_SYNTAX_OK3 = """<?xml version="1.0" encoding="UTF-8"?>
           <grit latest_public_release="1" current_release="1">
             <release seq="1">
               <messages>
                 <message name="IDS_TEST1">
                   {NUM, plural,
                    =0 {New test text for numeric zero}
                    =1 {Different test text for numeric one}
                    =2 {New test text for numeric two}
                    =3 {New test text for numeric three}
                    other {Different test text for plural with {NUM} as number}}
                 </message>
               </messages>
             </release>
           </grit>
        """.splitlines()
    # A grd file with one ICU syntax message with syntax errors (misses a comma).
    NEW_GRD_CONTENTS_ICU_SYNTAX_ERROR = """<?xml version="1.0" encoding="UTF-8"?>
           <grit latest_public_release="1" current_release="1">
             <release seq="1">
               <messages>
                 <message name="IDS_TEST1">
                   {NUM, plural
                    =1 {Test text for numeric one}
                    other {Test text for plural with {NUM} as number}}
                 </message>
               </messages>
             </release>
           </grit>
        """.splitlines()

    OLD_GRDP_CONTENTS = ('<?xml version="1.0" encoding="utf-8"?>',
                         '<grit-part>', '</grit-part>')

    NEW_GRDP_CONTENTS1 = ('<?xml version="1.0" encoding="utf-8"?>',
                          '<grit-part>', '<message name="IDS_PART_TEST1">',
                          'Part string 1', '</message>', '</grit-part>')

    NEW_GRDP_CONTENTS2 = ('<?xml version="1.0" encoding="utf-8"?>',
                          '<grit-part>', '<message name="IDS_PART_TEST1">',
                          'Part string 1', '</message>',
                          '<message name="IDS_PART_TEST2">', 'Part string 2',
                          '</message>', '</grit-part>')

    NEW_GRDP_CONTENTS3 = (
        '<?xml version="1.0" encoding="utf-8"?>', '<grit-part>',
        '<message name="IDS_PART_TEST1" desc="Description with typo.">',
        'Part string 1', '</message>', '</grit-part>')

    NEW_GRDP_CONTENTS4 = (
        '<?xml version="1.0" encoding="utf-8"?>', '<grit-part>',
        '<message name="IDS_PART_TEST1" desc="Description with typo fixed.">',
        'Part string 1', '</message>', '</grit-part>')

    NEW_GRDP_CONTENTS5 = (
        '<?xml version="1.0" encoding="utf-8"?>', '<grit-part>',
        '<message name="IDS_PART_TEST1" meaning="Meaning with typo.">',
        'Part string 1', '</message>', '</grit-part>')

    NEW_GRDP_CONTENTS6 = (
        '<?xml version="1.0" encoding="utf-8"?>', '<grit-part>',
        '<message name="IDS_PART_TEST1" meaning="Meaning with typo fixed.">',
        'Part string 1', '</message>', '</grit-part>')

    # A grdp file with one ICU syntax message without syntax errors.
    NEW_GRDP_CONTENTS_ICU_SYNTAX_OK1 = (
        '<?xml version="1.0" encoding="utf-8"?>', '<grit-part>',
        '<message name="IDS_PART_TEST1">', '{NUM, plural,',
        '=1 {Test text for numeric one}',
        'other {Test text for plural with {NUM} as number}}', '</message>',
        '</grit-part>')
    # A grdp file with one ICU syntax message without syntax errors.
    NEW_GRDP_CONTENTS_ICU_SYNTAX_OK2 = (
        '<?xml version="1.0" encoding="utf-8"?>', '<grit-part>',
        '<message name="IDS_PART_TEST1">', '{NUM, plural,',
        '=1 {Different test text for numeric one}',
        'other {Different test text for plural with {NUM} as number}}',
        '</message>', '</grit-part>')
    # A grdp file with multiple ICU syntax messages without syntax errors.
    NEW_GRDP_CONTENTS_ICU_SYNTAX_OK3 = (
        '<?xml version="1.0" encoding="utf-8"?>', '<grit-part>',
        '<message name="IDS_PART_TEST1">', '{NUM, plural,',
        '=0 {New test text for numeric zero}',
        '=1 {Different test text for numeric one}',
        '=2 {New test text for numeric two}',
        '=3 {New test text for numeric three}',
        'other {Different test text for plural with {NUM} as number}}',
        '</message>', '</grit-part>')

    # A grdp file with one ICU syntax message with syntax errors (superfluous
    # space).
    NEW_GRDP_CONTENTS_ICU_SYNTAX_ERROR = (
        '<?xml version="1.0" encoding="utf-8"?>', '<grit-part>',
        '<message name="IDS_PART_TEST1">', '{NUM, plural,',
        '= 1 {Test text for numeric one}',
        'other {Test text for plural with {NUM} as number}}', '</message>',
        '</grit-part>')

    VALID_SHA1 = ('0000000000000000000000000000000000000000', )
    DO_NOT_UPLOAD_PNG_MESSAGE = ('Do not include actual screenshots in the '
                                 'changelist. Run '
                                 'tools/translate/upload_screenshots.py to '
                                 'upload them instead:')
    ADD_SIGNATURES_MESSAGE = ('You are adding UI strings.\n'
                              'To ensure the best translations, take '
                              'screenshots of the relevant UI '
                              '(https://g.co/chrome/translation) and add '
                              'these files to your changelist:')
    REMOVE_SIGNATURES_MESSAGE = ('You removed strings associated with these '
                                 'files. Remove:')
    ICU_SYNTAX_ERROR_MESSAGE = (
        'ICU syntax errors were found in the following '
        'strings (problems or feedback? Contact '
        'rainhard@chromium.org):')
    SHA1_FORMAT_MESSAGE = (
        'The following files do not seem to contain valid sha1 '
        'hashes. Make sure they contain hashes created by '
        'tools/translate/upload_screenshots.py:')

    def makeInputApi(self, files):
        input_api = MockInputApi()
        input_api.files = files
        # Override os_path.exists because the presubmit uses the actual
        # os.path.exists.
        input_api.CreateMockFileInPath([
            x.LocalPath()
            for x in input_api.AffectedFiles(include_deletes=True)
        ])
        return input_api

    """ CL modified and added messages, but didn't add any screenshots."""

    def testNoScreenshots(self):
        # No new strings (file contents same). Should not warn.
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS1,
                             self.NEW_GRD_CONTENTS1,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS1,
                             self.NEW_GRDP_CONTENTS1,
                             action='M')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

        # Add two new strings. Should have two warnings.
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS2,
                             self.NEW_GRD_CONTENTS1,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS2,
                             self.NEW_GRDP_CONTENTS1,
                             action='M')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual(self.ADD_SIGNATURES_MESSAGE, warnings[0].message)
        self.assertEqual('error', warnings[0].type)
        self.assertEqual([
            os.path.join('part_grdp', 'IDS_PART_TEST2.png.sha1'),
            os.path.join('test_grd', 'IDS_TEST2.png.sha1')
        ], warnings[0].items)

        # Add four new strings. Should have four warnings.
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS2,
                             self.OLD_GRD_CONTENTS,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS2,
                             self.OLD_GRDP_CONTENTS,
                             action='M')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual('error', warnings[0].type)
        self.assertEqual(self.ADD_SIGNATURES_MESSAGE, warnings[0].message)
        self.assertEqual([
            os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
            os.path.join('part_grdp', 'IDS_PART_TEST2.png.sha1'),
            os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
            os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
        ], warnings[0].items)

    def testModifiedMessageDescription(self):
        # CL modified a message description for a message that does not yet have a
        # screenshot. Should not warn.
        input_api = self.makeInputApi([
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS3,
                             self.NEW_GRDP_CONTENTS4,
                             action='M')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

        # CL modified a message description for a message that already has a
        # screenshot. Should not warn.
        input_api = self.makeInputApi([
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS3,
                             self.NEW_GRDP_CONTENTS4,
                             action='M'),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
                     self.VALID_SHA1,
                     action='A')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testModifiedMessageMeaning(self):
        # CL modified a message meaning for a message that does not yet have a
        # screenshot. Should warn.
        input_api = self.makeInputApi([
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS5,
                             self.NEW_GRDP_CONTENTS6,
                             action='M')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

        # CL modified a message meaning for a message that already has a
        # screenshot. Should not warn.
        input_api = self.makeInputApi([
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS5,
                             self.NEW_GRDP_CONTENTS6,
                             action='M'),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
                     self.VALID_SHA1,
                     action='A')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testModifiedIntroducedInvalidSha1(self):
        # CL modified a message and the sha1 file changed to invalid
        input_api = self.makeInputApi([
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS5,
                             self.NEW_GRDP_CONTENTS6,
                             action='M'),
            MockAffectedFile(os.path.join('part_grdp',
                                          'IDS_PART_TEST1.png.sha1'),
                             ('some invalid sha1', ),
                             self.VALID_SHA1,
                             action='M')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testPngAddedSha1NotAdded(self):
        # CL added one new message in a grd file and added the png file associated
        # with it, but did not add the corresponding sha1 file. This should warn
        # twice:
        # - Once for the added png file (because we don't want developers to upload
        #   actual images)
        # - Once for the missing .sha1 file
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS1,
                             self.OLD_GRD_CONTENTS,
                             action='M'),
            MockAffectedFile(os.path.join('test_grd', 'IDS_TEST1.png'),
                             'binary',
                             action='A')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(2, len(warnings))
        self.assertEqual('error', warnings[0].type)
        self.assertEqual(self.DO_NOT_UPLOAD_PNG_MESSAGE, warnings[0].message)
        self.assertEqual([os.path.join('test_grd', 'IDS_TEST1.png')],
                         warnings[0].items)
        self.assertEqual('error', warnings[1].type)
        self.assertEqual(self.ADD_SIGNATURES_MESSAGE, warnings[1].message)
        self.assertEqual([os.path.join('test_grd', 'IDS_TEST1.png.sha1')],
                         warnings[1].items)

        # CL added two messages (one in grd, one in grdp) and added the png files
        # associated with the messages, but did not add the corresponding sha1
        # files. This should warn twice:
        # - Once for the added png files (because we don't want developers to upload
        #   actual images)
        # - Once for the missing .sha1 files
        input_api = self.makeInputApi([
            # Modified files:
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS1,
                             self.OLD_GRD_CONTENTS,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS1,
                             self.OLD_GRDP_CONTENTS,
                             action='M'),
            # Added files:
            MockAffectedFile(os.path.join('test_grd', 'IDS_TEST1.png'),
                             'binary',
                             action='A'),
            MockAffectedFile(os.path.join('part_grdp', 'IDS_PART_TEST1.png'),
                             'binary',
                             action='A')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(2, len(warnings))
        self.assertEqual('error', warnings[0].type)
        self.assertEqual(self.DO_NOT_UPLOAD_PNG_MESSAGE, warnings[0].message)
        self.assertEqual([
            os.path.join('part_grdp', 'IDS_PART_TEST1.png'),
            os.path.join('test_grd', 'IDS_TEST1.png')
        ], warnings[0].items)
        self.assertEqual('error', warnings[0].type)
        self.assertEqual(self.ADD_SIGNATURES_MESSAGE, warnings[1].message)
        self.assertEqual([
            os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
            os.path.join('test_grd', 'IDS_TEST1.png.sha1')
        ], warnings[1].items)

    def testScreenshotsWithSha1(self):
        # CL added four messages (two each in a grd and grdp) and their
        # corresponding .sha1 files. No warnings.
        input_api = self.makeInputApi([
            # Modified files:
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS2,
                             self.OLD_GRD_CONTENTS,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS2,
                             self.OLD_GRDP_CONTENTS,
                             action='M'),
            # Added files:
            MockFile(os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
                     self.VALID_SHA1,
                     action='A'),
            MockFile(os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
                     ('0000000000000000000000000000000000000000', ''),
                     action='A'),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
                     self.VALID_SHA1,
                     action='A'),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST2.png.sha1'),
                     self.VALID_SHA1,
                     action='A'),
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual([], warnings)

    def testScreenshotsWithInvalidSha1(self):
        input_api = self.makeInputApi([
            # Modified files:
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS2,
                             self.OLD_GRD_CONTENTS,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS2,
                             self.OLD_GRDP_CONTENTS,
                             action='M'),
            # Added files:
            MockFile(os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
                     self.VALID_SHA1,
                     action='A'),
            MockFile(os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
                     ('‰PNG', 'test'),
                     action='A'),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
                     self.VALID_SHA1,
                     action='A'),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST2.png.sha1'),
                     self.VALID_SHA1,
                     action='A'),
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual('error', warnings[0].type)
        self.assertEqual(self.SHA1_FORMAT_MESSAGE, warnings[0].message)
        self.assertEqual([os.path.join('test_grd', 'IDS_TEST2.png.sha1')],
                         warnings[0].items)

    def testScreenshotsRemovedWithSha1(self):
        # Replace new contents with old contents in grd and grp files, removing
        # IDS_TEST1, IDS_TEST2, IDS_PART_TEST1 and IDS_PART_TEST2.
        # Should warn to remove the sha1 files associated with these strings.
        input_api = self.makeInputApi([
            # Modified files:
            MockAffectedFile(
                'test.grd',
                self.OLD_GRD_CONTENTS,  # new_contents
                self.NEW_GRD_CONTENTS2,  # old_contents
                action='M'),
            MockAffectedFile(
                'part.grdp',
                self.OLD_GRDP_CONTENTS,  # new_contents
                self.NEW_GRDP_CONTENTS2,  # old_contents
                action='M'),
            # Unmodified files:
            MockFile(os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
                     self.VALID_SHA1, ''),
            MockFile(os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
                     self.VALID_SHA1, ''),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
                     self.VALID_SHA1, ''),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST2.png.sha1'),
                     self.VALID_SHA1, '')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual('error', warnings[0].type)
        self.assertEqual(self.REMOVE_SIGNATURES_MESSAGE, warnings[0].message)
        self.assertEqual([
            os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
            os.path.join('part_grdp', 'IDS_PART_TEST2.png.sha1'),
            os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
            os.path.join('test_grd', 'IDS_TEST2.png.sha1')
        ], warnings[0].items)

        # Same as above, but this time one of the .sha1 files is also removed.
        input_api = self.makeInputApi([
            # Modified files:
            MockAffectedFile(
                'test.grd',
                self.OLD_GRD_CONTENTS,  # new_contents
                self.NEW_GRD_CONTENTS2,  # old_contents
                action='M'),
            MockAffectedFile(
                'part.grdp',
                self.OLD_GRDP_CONTENTS,  # new_contents
                self.NEW_GRDP_CONTENTS2,  # old_contents
                action='M'),
            # Unmodified files:
            MockFile(os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
                     self.VALID_SHA1, ''),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
                     self.VALID_SHA1, ''),
            # Deleted files:
            MockAffectedFile(os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
                             '',
                             'old_contents',
                             action='D'),
            MockAffectedFile(os.path.join('part_grdp',
                                          'IDS_PART_TEST2.png.sha1'),
                             '',
                             'old_contents',
                             action='D')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual('error', warnings[0].type)
        self.assertEqual(self.REMOVE_SIGNATURES_MESSAGE, warnings[0].message)
        self.assertEqual([
            os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
            os.path.join('test_grd', 'IDS_TEST1.png.sha1')
        ], warnings[0].items)

        # Remove all sha1 files. There should be no warnings.
        input_api = self.makeInputApi([
            # Modified files:
            MockAffectedFile('test.grd',
                             self.OLD_GRD_CONTENTS,
                             self.NEW_GRD_CONTENTS2,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.OLD_GRDP_CONTENTS,
                             self.NEW_GRDP_CONTENTS2,
                             action='M'),
            # Deleted files:
            MockFile(os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
                     self.VALID_SHA1,
                     action='D'),
            MockFile(os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
                     self.VALID_SHA1,
                     action='D'),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST1.png.sha1'),
                     self.VALID_SHA1,
                     action='D'),
            MockFile(os.path.join('part_grdp', 'IDS_PART_TEST2.png.sha1'),
                     self.VALID_SHA1,
                     action='D')
        ])
        warnings = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        self.assertEqual([], warnings)

    def testIcuSyntax(self):
        # Add valid ICU syntax string. Should not raise an error.
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS_ICU_SYNTAX_OK2,
                             self.NEW_GRD_CONTENTS1,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS_ICU_SYNTAX_OK2,
                             self.NEW_GRDP_CONTENTS1,
                             action='M')
        ])
        results = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        # We expect no ICU syntax errors.
        icu_errors = [
            e for e in results if e.message == self.ICU_SYNTAX_ERROR_MESSAGE
        ]
        self.assertEqual(0, len(icu_errors))

        # Valid changes in ICU syntax. Should not raise an error.
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS_ICU_SYNTAX_OK2,
                             self.NEW_GRD_CONTENTS_ICU_SYNTAX_OK1,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS_ICU_SYNTAX_OK2,
                             self.NEW_GRDP_CONTENTS_ICU_SYNTAX_OK1,
                             action='M')
        ])
        results = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        # We expect no ICU syntax errors.
        icu_errors = [
            e for e in results if e.message == self.ICU_SYNTAX_ERROR_MESSAGE
        ]
        self.assertEqual(0, len(icu_errors))

        # Valid changes in ICU syntax. Should not raise an error.
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS_ICU_SYNTAX_OK3,
                             self.NEW_GRD_CONTENTS_ICU_SYNTAX_OK1,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS_ICU_SYNTAX_OK3,
                             self.NEW_GRDP_CONTENTS_ICU_SYNTAX_OK1,
                             action='M')
        ])
        results = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        # We expect no ICU syntax errors.
        icu_errors = [
            e for e in results if e.message == self.ICU_SYNTAX_ERROR_MESSAGE
        ]
        self.assertEqual(0, len(icu_errors))

        # Add invalid ICU syntax strings. Should raise two errors.
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS_ICU_SYNTAX_ERROR,
                             self.NEW_GRD_CONTENTS1,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS_ICU_SYNTAX_ERROR,
                             self.NEW_GRD_CONTENTS1,
                             action='M')
        ])
        results = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        # We expect 2 ICU syntax errors.
        icu_errors = [
            e for e in results if e.message == self.ICU_SYNTAX_ERROR_MESSAGE
        ]
        self.assertEqual(1, len(icu_errors))
        self.assertEqual([
            'IDS_TEST1: This message looks like an ICU plural, but does not follow '
            'ICU syntax.',
            'IDS_PART_TEST1: Variant "= 1" is not valid for plural message'
        ], icu_errors[0].items)

        # Change two strings to have ICU syntax errors. Should raise two errors.
        input_api = self.makeInputApi([
            MockAffectedFile('test.grd',
                             self.NEW_GRD_CONTENTS_ICU_SYNTAX_ERROR,
                             self.NEW_GRD_CONTENTS_ICU_SYNTAX_OK1,
                             action='M'),
            MockAffectedFile('part.grdp',
                             self.NEW_GRDP_CONTENTS_ICU_SYNTAX_ERROR,
                             self.NEW_GRDP_CONTENTS_ICU_SYNTAX_OK1,
                             action='M')
        ])
        results = PRESUBMIT.CheckStrings(input_api, MockOutputApi())
        # We expect 2 ICU syntax errors.
        icu_errors = [
            e for e in results if e.message == self.ICU_SYNTAX_ERROR_MESSAGE
        ]
        self.assertEqual(1, len(icu_errors))
        self.assertEqual([
            'IDS_TEST1: This message looks like an ICU plural, but does not follow '
            'ICU syntax.',
            'IDS_PART_TEST1: Variant "= 1" is not valid for plural message'
        ], icu_errors[0].items)


class TranslationExpectationsTest(unittest.TestCase):
    ERROR_MESSAGE_FORMAT = (
        "Failed to get a list of translatable grd files. "
        "This happens when:\n"
        " - One of the modified grd or grdp files cannot be parsed or\n"
        " - %s is not updated.\n"
        "Stack:\n")
    REPO_ROOT = os.path.join('tools', 'translation', 'testdata')
    # This lists all .grd files under REPO_ROOT.
    EXPECTATIONS = os.path.join(REPO_ROOT, "translation_expectations.pyl")
    # This lists all .grd files under REPO_ROOT except unlisted.grd.
    EXPECTATIONS_WITHOUT_UNLISTED_FILE = os.path.join(
        REPO_ROOT, "translation_expectations_without_unlisted_file.pyl")

    # Tests that the presubmit doesn't return when no grd or grdp files are
    # modified.
    def testExpectationsNoModifiedGrd(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('not_used.txt',
                             'not used',
                             'not used',
                             action='M')
        ]
        # Fake list of all grd files in the repo. This list is missing all grd/grdps
        # under tools/translation/testdata. This is OK because the presubmit won't
        # run in the first place since there are no modified grd/grps in input_api.
        grd_files = ['doesnt_exist_doesnt_matter.grd']
        warnings = PRESUBMIT.CheckTranslationExpectations(
            input_api, MockOutputApi(), self.REPO_ROOT, self.EXPECTATIONS,
            grd_files)
        self.assertEqual(0, len(warnings))

    # Tests that the list of files passed to the presubmit matches the list of
    # files in the expectations.
    def testExpectationsSuccess(self):
        # Mock input file list needs a grd or grdp file in order to run the
        # presubmit. The file itself doesn't matter.
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('dummy.grd', 'not used', 'not used', action='M')
        ]
        # List of all grd files in the repo.
        grd_files = [
            'test.grd', 'unlisted.grd', 'not_translated.grd', 'internal.grd'
        ]
        warnings = PRESUBMIT.CheckTranslationExpectations(
            input_api, MockOutputApi(), self.REPO_ROOT, self.EXPECTATIONS,
            grd_files)
        self.assertEqual(0, len(warnings))

    # Tests that the presubmit warns when a file is listed in expectations, but
    # does not actually exist.
    def testExpectationsMissingFile(self):
        # Mock input file list needs a grd or grdp file in order to run the
        # presubmit.
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('dummy.grd', 'not used', 'not used', action='M')
        ]
        # unlisted.grd is listed under tools/translation/testdata but is not
        # included in translation expectations.
        grd_files = ['unlisted.grd', 'not_translated.grd', 'internal.grd']
        warnings = PRESUBMIT.CheckTranslationExpectations(
            input_api, MockOutputApi(), self.REPO_ROOT, self.EXPECTATIONS,
            grd_files)
        self.assertEqual(1, len(warnings))
        self.assertTrue(warnings[0].message.startswith(
            self.ERROR_MESSAGE_FORMAT % self.EXPECTATIONS))
        self.assertTrue(
            ("test.grd is listed in the translation expectations, "
             "but this grd file does not exist") in warnings[0].message)

    # Tests that the presubmit warns when a file is not listed in expectations but
    # does actually exist.
    def testExpectationsUnlistedFile(self):
        # Mock input file list needs a grd or grdp file in order to run the
        # presubmit.
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('dummy.grd', 'not used', 'not used', action='M')
        ]
        # unlisted.grd is listed under tools/translation/testdata but is not
        # included in translation expectations.
        grd_files = [
            'test.grd', 'unlisted.grd', 'not_translated.grd', 'internal.grd'
        ]
        warnings = PRESUBMIT.CheckTranslationExpectations(
            input_api, MockOutputApi(), self.REPO_ROOT,
            self.EXPECTATIONS_WITHOUT_UNLISTED_FILE, grd_files)
        self.assertEqual(1, len(warnings))
        self.assertTrue(warnings[0].message.startswith(
            self.ERROR_MESSAGE_FORMAT %
            self.EXPECTATIONS_WITHOUT_UNLISTED_FILE))
        self.assertTrue(("unlisted.grd appears to be translatable "
                         "(because it contains <file> or <message> elements), "
                         "but is not listed in the translation expectations."
                         ) in warnings[0].message)

    # Tests that the presubmit warns twice:
    # - for a non-existing file listed in expectations
    # - for an existing file not listed in expectations
    def testMultipleWarnings(self):
        # Mock input file list needs a grd or grdp file in order to run the
        # presubmit.
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('dummy.grd', 'not used', 'not used', action='M')
        ]
        # unlisted.grd is listed under tools/translation/testdata but is not
        # included in translation expectations.
        # test.grd is not listed under tools/translation/testdata but is included
        # in translation expectations.
        grd_files = ['unlisted.grd', 'not_translated.grd', 'internal.grd']
        warnings = PRESUBMIT.CheckTranslationExpectations(
            input_api, MockOutputApi(), self.REPO_ROOT,
            self.EXPECTATIONS_WITHOUT_UNLISTED_FILE, grd_files)
        self.assertEqual(1, len(warnings))
        self.assertTrue(warnings[0].message.startswith(
            self.ERROR_MESSAGE_FORMAT %
            self.EXPECTATIONS_WITHOUT_UNLISTED_FILE))
        self.assertTrue(("unlisted.grd appears to be translatable "
                         "(because it contains <file> or <message> elements), "
                         "but is not listed in the translation expectations."
                         ) in warnings[0].message)
        self.assertTrue(
            ("test.grd is listed in the translation expectations, "
             "but this grd file does not exist") in warnings[0].message)


class DISABLETypoInTest(unittest.TestCase):

    def testPositive(self):
        # Verify the typo "DISABLE_" instead of "DISABLED_" in various contexts
        # where the desire is to disable a test.
        tests = [
            # Disabled on one platform:
            '#if defined(OS_WIN)\n'
            '#define MAYBE_FoobarTest DISABLE_FoobarTest\n'
            '#else\n'
            '#define MAYBE_FoobarTest FoobarTest\n'
            '#endif\n',
            # Disabled on one platform spread cross lines:
            '#if defined(OS_WIN)\n'
            '#define MAYBE_FoobarTest \\\n'
            '    DISABLE_FoobarTest\n'
            '#else\n'
            '#define MAYBE_FoobarTest FoobarTest\n'
            '#endif\n',
            # Disabled on all platforms:
            '  TEST_F(FoobarTest, DISABLE_Foo)\n{\n}',
            # Disabled on all platforms but multiple lines
            '  TEST_F(FoobarTest,\n   DISABLE_foo){\n}\n',
        ]

        for test in tests:
            mock_input_api = MockInputApi()
            mock_input_api.files = [
                MockFile('some/path/foo_unittest.cc', test.splitlines()),
            ]

            results = PRESUBMIT.CheckNoDISABLETypoInTests(
                mock_input_api, MockOutputApi())
            self.assertEqual(
                1,
                len(results),
                msg=('expected len(results) == 1 but got %d in test: %s' %
                     (len(results), test)))
            self.assertTrue(
                'foo_unittest.cc' in results[0].message,
                msg=(
                    'expected foo_unittest.cc in message but got %s in test %s'
                    % (results[0].message, test)))

    def testIgnoreNotTestFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('some/path/foo.cc', 'TEST_F(FoobarTest, DISABLE_Foo)'),
        ]

        results = PRESUBMIT.CheckNoDISABLETypoInTests(mock_input_api,
                                                      MockOutputApi())
        self.assertEqual(0, len(results))

    def testIgnoreDeletedFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('some/path/foo.cc', 'TEST_F(FoobarTest, Foo)',
                     action='D'),
        ]

        results = PRESUBMIT.CheckNoDISABLETypoInTests(mock_input_api,
                                                      MockOutputApi())
        self.assertEqual(0, len(results))

class ForgettingMAYBEInTests(unittest.TestCase):

    def testPositive(self):
        test = ('#if defined(HAS_ENERGY)\n'
                '#define MAYBE_CastExplosion DISABLED_CastExplosion\n'
                '#else\n'
                '#define MAYBE_CastExplosion CastExplosion\n'
                '#endif\n'
                'TEST_F(ArchWizard, CastExplosion) {\n'
                '#if defined(ARCH_PRIEST_IN_PARTY)\n'
                '#define MAYBE_ArchPriest ArchPriest\n'
                '#else\n'
                '#define MAYBE_ArchPriest DISABLED_ArchPriest\n'
                '#endif\n'
                'TEST_F(ArchPriest, CastNaturesBounty) {\n'
                '#if !defined(CRUSADER_IN_PARTY)\n'
                '#define MAYBE_Crusader \\\n'
                '    DISABLED_Crusader \n'
                '#else\n'
                '#define MAYBE_Crusader \\\n'
                '    Crusader\n'
                '#endif\n'
                '  TEST_F(\n'
                '    Crusader,\n'
                '    CastTaunt) { }\n'
                '#if defined(LEARNED_BASIC_SKILLS)\n'
                '#define MAYBE_CastSteal \\\n'
                '    DISABLED_CastSteal \n'
                '#else\n'
                '#define MAYBE_CastSteal \\\n'
                '    CastSteal\n'
                '#endif\n'
                '  TEST_F(\n'
                '    ThiefClass,\n'
                '    CastSteal) { }\n')
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('fantasyworld/classes_unittest.cc', test.splitlines()),
        ]
        results = PRESUBMIT.CheckForgettingMAYBEInTests(
            mock_input_api, MockOutputApi())
        self.assertEqual(4, len(results))
        self.assertTrue('CastExplosion' in results[0].message)
        self.assertTrue(
            'fantasyworld/classes_unittest.cc:2' in results[0].message)
        self.assertTrue('ArchPriest' in results[1].message)
        self.assertTrue(
            'fantasyworld/classes_unittest.cc:8' in results[1].message)
        self.assertTrue('Crusader' in results[2].message)
        self.assertTrue(
            'fantasyworld/classes_unittest.cc:14' in results[2].message)
        self.assertTrue('CastSteal' in results[3].message)
        self.assertTrue(
            'fantasyworld/classes_unittest.cc:24' in results[3].message)

    def testNegative(self):
        test = ('#if defined(HAS_ENERGY)\n'
                '#define MAYBE_CastExplosion DISABLED_CastExplosion\n'
                '#else\n'
                '#define MAYBE_CastExplosion CastExplosion\n'
                '#endif\n'
                'TEST_F(ArchWizard, MAYBE_CastExplosion) {\n'
                '#if defined(ARCH_PRIEST_IN_PARTY)\n'
                '#define MAYBE_ArchPriest ArchPriest\n'
                '#else\n'
                '#define MAYBE_ArchPriest DISABLED_ArchPriest\n'
                '#endif\n'
                'TEST_F(MAYBE_ArchPriest, CastNaturesBounty) {\n'
                '#if !defined(CRUSADER_IN_PARTY)\n'
                '#define MAYBE_Crusader \\\n'
                '    DISABLED_Crusader \n'
                '#else\n'
                '#define MAYBE_Crusader \\\n'
                '    Crusader\n'
                '#endif\n'
                '  TEST_F(\n'
                '    MAYBE_Crusader,\n'
                '    CastTaunt) { }\n'
                '#if defined(LEARNED_BASIC_SKILLS)\n'
                '#define MAYBE_CastSteal \\\n'
                '    DISABLED_CastSteal \n'
                '#else\n'
                '#define MAYBE_CastSteal \\\n'
                '    CastSteal\n'
                '#endif\n'
                '  TEST_F(\n'
                '    ThiefClass,\n'
                '    MAYBE_CastSteal) { }\n')

        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile('fantasyworld/classes_unittest.cc', test.splitlines()),
        ]
        results = PRESUBMIT.CheckForgettingMAYBEInTests(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

class CheckFuzzTargetsTest(unittest.TestCase):

    def _check(self, files):
        mock_input_api = MockInputApi()
        mock_input_api.files = []
        for fname, contents in files.items():
            mock_input_api.files.append(MockFile(fname, contents.splitlines()))
        return PRESUBMIT.CheckFuzzTargetsOnUpload(mock_input_api,
                                                  MockOutputApi())

    def testLibFuzzerSourcesIgnored(self):
        results = self._check({
            "third_party/lib/Fuzzer/FuzzerDriver.cpp":
            "LLVMFuzzerInitialize",
        })
        self.assertEqual(results, [])

    def testNonCodeFilesIgnored(self):
        results = self._check({
            "README.md": "LLVMFuzzerInitialize",
        })
        self.assertEqual(results, [])

    def testNoErrorHeaderPresent(self):
        results = self._check({
            "fuzzer.cc":
            ("#include \"testing/libfuzzer/libfuzzer_exports.h\"\n" +
             "LLVMFuzzerInitialize")
        })
        self.assertEqual(results, [])

    def testErrorMissingHeader(self):
        results = self._check({"fuzzer.cc": "LLVMFuzzerInitialize"})
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].items, ['fuzzer.cc'])


class SetNoParentTest(unittest.TestCase):

    def testSetNoParentTopLevelAllowed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('goat/OWNERS', [
                'set noparent',
                'jochen@chromium.org',
            ])
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSetNoParent(mock_input_api, mock_output_api)
        self.assertEqual([], errors)

    def testSetNoParentMissing(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('services/goat/OWNERS', [
                'set noparent',
                'jochen@chromium.org',
                'per-file *.json=set noparent',
                'per-file *.json=jochen@chromium.org',
            ])
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSetNoParent(mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue('goat/OWNERS:1' in errors[0].long_text)
        self.assertTrue('goat/OWNERS:3' in errors[0].long_text)

    def testSetNoParentWithCorrectRule(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('services/goat/OWNERS', [
                'set noparent',
                'file://ipc/SECURITY_OWNERS',
                'per-file *.json=set noparent',
                'per-file *.json=file://ipc/SECURITY_OWNERS',
            ])
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckSetNoParent(mock_input_api, mock_output_api)
        self.assertEqual([], errors)


class MojomStabilityCheckTest(unittest.TestCase):

    def runTestWithAffectedFiles(self, affected_files):
        mock_input_api = MockInputApi()
        mock_input_api.files = affected_files
        mock_output_api = MockOutputApi()
        return PRESUBMIT.CheckStableMojomChanges(mock_input_api,
                                                 mock_output_api)

    def testSafeChangePasses(self):
        errors = self.runTestWithAffectedFiles([
            MockAffectedFile(
                'foo/foo.mojom',
                ['[Stable] struct S { [MinVersion=1] int32 x; };'],
                old_contents=['[Stable] struct S {};'])
        ])
        self.assertEqual([], errors)

    def testBadChangeFails(self):
        errors = self.runTestWithAffectedFiles([
            MockAffectedFile('foo/foo.mojom',
                             ['[Stable] struct S { int32 x; };'],
                             old_contents=['[Stable] struct S {};'])
        ])
        self.assertEqual(1, len(errors))
        self.assertTrue('not backward-compatible' in errors[0].message)

    def testDeletedFile(self):
        """Regression test for https://crbug.com/1091407."""
        errors = self.runTestWithAffectedFiles([
            MockAffectedFile('a.mojom', [],
                             old_contents=['struct S {};'],
                             action='D'),
            MockAffectedFile(
                'b.mojom', ['struct S {}; struct T { S s; };'],
                old_contents=['import "a.mojom"; struct T { S s; };'])
        ])
        self.assertEqual([], errors)


class CheckForUseOfChromeAppsDeprecationsTest(unittest.TestCase):

    ERROR_MSG_PIECE = 'technologies which will soon be deprecated'

    # Each positive test is also a naive negative test for the other cases.

    def testWarningNMF(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                'foo.NMF', ['"program"', '"Z":"content"', 'B'],
                ['"program"', 'B'],
                scm_diff='\n'.join([
                    '--- foo.NMF.old  2020-12-02 20:40:54.430676385 +0100',
                    '+++ foo.NMF.new  2020-12-02 20:41:02.086700197 +0100',
                    '@@ -1,2 +1,3 @@', ' "program"', '+"Z":"content"', ' B'
                ]),
                action='M')
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckForUseOfChromeAppsDeprecations(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(self.ERROR_MSG_PIECE in errors[0].message)
        self.assertTrue('foo.NMF' in errors[0].message)

    def testWarningManifest(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                'manifest.json', ['"app":', '"Z":"content"', 'B'],
                ['"app":"', 'B'],
                scm_diff='\n'.join([
                    '--- manifest.json.old  2020-12-02 20:40:54.430676385 +0100',
                    '+++ manifest.json.new  2020-12-02 20:41:02.086700197 +0100',
                    '@@ -1,2 +1,3 @@', ' "app"', '+"Z":"content"', ' B'
                ]),
                action='M')
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckForUseOfChromeAppsDeprecations(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(self.ERROR_MSG_PIECE in errors[0].message)
        self.assertTrue('manifest.json' in errors[0].message)

    def testOKWarningManifestWithoutApp(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                'manifest.json', ['"name":', '"Z":"content"', 'B'],
                ['"name":"', 'B'],
                scm_diff='\n'.join([
                    '--- manifest.json.old  2020-12-02 20:40:54.430676385 +0100',
                    '+++ manifest.json.new  2020-12-02 20:41:02.086700197 +0100',
                    '@@ -1,2 +1,3 @@', ' "app"', '+"Z":"content"', ' B'
                ]),
                action='M')
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckForUseOfChromeAppsDeprecations(
            mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))

    def testWarningPPAPI(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                'foo.hpp', ['A', '#include <ppapi.h>', 'B'], ['A', 'B'],
                scm_diff='\n'.join([
                    '--- foo.hpp.old  2020-12-02 20:40:54.430676385 +0100',
                    '+++ foo.hpp.new  2020-12-02 20:41:02.086700197 +0100',
                    '@@ -1,2 +1,3 @@', ' A', '+#include <ppapi.h>', ' B'
                ]),
                action='M')
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckForUseOfChromeAppsDeprecations(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(self.ERROR_MSG_PIECE in errors[0].message)
        self.assertTrue('foo.hpp' in errors[0].message)

    def testNoWarningPPAPI(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                'foo.txt', ['A', 'Peppapig', 'B'], ['A', 'B'],
                scm_diff='\n'.join([
                    '--- foo.txt.old  2020-12-02 20:40:54.430676385 +0100',
                    '+++ foo.txt.new  2020-12-02 20:41:02.086700197 +0100',
                    '@@ -1,2 +1,3 @@', ' A', '+Peppapig', ' B'
                ]),
                action='M')
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckForUseOfChromeAppsDeprecations(
            mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))


class CheckDeprecationOfPreferencesTest(unittest.TestCase):
    # Test that a warning is generated if a preference registration is removed
    # from a random file.
    def testWarning(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                'foo.cc', ['A', 'B'],
                ['A', 'prefs->RegisterStringPref("foo", "default");', 'B'],
                scm_diff='\n'.join([
                    '--- foo.cc.old  2020-12-02 20:40:54.430676385 +0100',
                    '+++ foo.cc.new  2020-12-02 20:41:02.086700197 +0100',
                    '@@ -1,3 +1,2 @@', ' A',
                    '-prefs->RegisterStringPref("foo", "default");', ' B'
                ]),
                action='M')
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckDeprecationOfPreferences(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(
            'Discovered possible removal of preference registrations' in
            errors[0].message)

    # Test that a warning is inhibited if the preference registration was moved
    # to the deprecation functions in browser prefs.
    def testNoWarningForMigration(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            # RegisterStringPref was removed from foo.cc.
            MockAffectedFile(
                'foo.cc', ['A', 'B'],
                ['A', 'prefs->RegisterStringPref("foo", "default");', 'B'],
                scm_diff='\n'.join([
                    '--- foo.cc.old  2020-12-02 20:40:54.430676385 +0100',
                    '+++ foo.cc.new  2020-12-02 20:41:02.086700197 +0100',
                    '@@ -1,3 +1,2 @@', ' A',
                    '-prefs->RegisterStringPref("foo", "default");', ' B'
                ]),
                action='M'),
            # But the preference was properly migrated.
            MockAffectedFile(
                'chrome/browser/prefs/browser_prefs.cc', [
                    '// BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS',
                    'prefs->RegisterStringPref("foo", "default");',
                    '// END_MIGRATE_OBSOLETE_PROFILE_PREFS',
                ], [
                    '// BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS',
                    '// END_MIGRATE_OBSOLETE_PROFILE_PREFS',
                ],
                scm_diff='\n'.join([
                    '--- browser_prefs.cc.old 2020-12-02 20:51:40.812686731 +0100',
                    '+++ browser_prefs.cc.new 2020-12-02 20:52:02.936755539 +0100',
                    '@@ -2,3 +2,4 @@',
                    ' // END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    ' // BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS',
                    '+prefs->RegisterStringPref("foo", "default");',
                    ' // END_MIGRATE_OBSOLETE_PROFILE_PREFS'
                ]),
                action='M'),
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckDeprecationOfPreferences(
            mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))

    # Test that a warning is NOT inhibited if the preference registration was
    # moved to a place outside of the migration functions in browser_prefs.cc
    def testWarningForImproperMigration(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            # RegisterStringPref was removed from foo.cc.
            MockAffectedFile(
                'foo.cc', ['A', 'B'],
                ['A', 'prefs->RegisterStringPref("foo", "default");', 'B'],
                scm_diff='\n'.join([
                    '--- foo.cc.old  2020-12-02 20:40:54.430676385 +0100',
                    '+++ foo.cc.new  2020-12-02 20:41:02.086700197 +0100',
                    '@@ -1,3 +1,2 @@', ' A',
                    '-prefs->RegisterStringPref("foo", "default");', ' B'
                ]),
                action='M'),
            # The registration call was moved to a place in browser_prefs.cc that
            # is outside the migration functions.
            MockAffectedFile(
                'chrome/browser/prefs/browser_prefs.cc', [
                    'prefs->RegisterStringPref("foo", "default");',
                    '// BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS',
                    '// END_MIGRATE_OBSOLETE_PROFILE_PREFS',
                ], [
                    '// BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS',
                    '// END_MIGRATE_OBSOLETE_PROFILE_PREFS',
                ],
                scm_diff='\n'.join([
                    '--- browser_prefs.cc.old 2020-12-02 20:51:40.812686731 +0100',
                    '+++ browser_prefs.cc.new 2020-12-02 20:52:02.936755539 +0100',
                    '@@ -1,2 +1,3 @@',
                    '+prefs->RegisterStringPref("foo", "default");',
                    ' // BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    ' // END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS'
                ]),
                action='M'),
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckDeprecationOfPreferences(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue(
            'Discovered possible removal of preference registrations' in
            errors[0].message)

    # Check that the presubmit fails if a marker line in browser_prefs.cc is
    # deleted.
    def testDeletedMarkerRaisesError(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                'chrome/browser/prefs/browser_prefs.cc',
                [
                    '// BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS',
                    '// BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS',
                    # The following line is deleted for this test
                    # '// END_MIGRATE_OBSOLETE_PROFILE_PREFS',
                ])
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckDeprecationOfPreferences(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(
            'Broken .*MIGRATE_OBSOLETE_.*_PREFS markers in browser_prefs.cc.',
            errors[0].message)

class CheckCrosApiNeedBrowserTestTest(unittest.TestCase):
    def testWarning(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.files = [
            MockAffectedFile('chromeos/crosapi/mojom/example.mojom', [], action='A'),
        ]
        result = PRESUBMIT.CheckCrosApiNeedBrowserTest(mock_input_api, mock_output_api)
        self.assertEqual(1, len(result))
        self.assertEqual(result[0].type, 'warning')

    def testNoWarningWithBrowserTest(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.files = [
            MockAffectedFile('chromeos/crosapi/mojom/example.mojom', [], action='A'),
            MockAffectedFile('chrome/example_browsertest.cc', [], action='A'),
        ]
        result = PRESUBMIT.CheckCrosApiNeedBrowserTest(mock_input_api, mock_output_api)
        self.assertEqual(0, len(result))

    def testNoWarningModifyCrosapi(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.files = [
            MockAffectedFile('chromeos/crosapi/mojom/example.mojom', [], action='M'),
        ]
        result = PRESUBMIT.CheckCrosApiNeedBrowserTest(mock_input_api, mock_output_api)
        self.assertEqual(0, len(result))

    def testNoWarningAddNonMojomFile(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.files = [
            MockAffectedFile('chromeos/crosapi/mojom/example.cc', [], action='A'),
        ]
        result = PRESUBMIT.CheckCrosApiNeedBrowserTest(mock_input_api, mock_output_api)
        self.assertEqual(0, len(result))

    def testNoWarningNoneRelatedMojom(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.files = [
            MockAffectedFile('random/folder/example.mojom', [], action='A'),
        ]
        result = PRESUBMIT.CheckCrosApiNeedBrowserTest(mock_input_api, mock_output_api)
        self.assertEqual(0, len(result))


class AssertAshOnlyCodeTest(unittest.TestCase):
    def testErrorsOnlyOnAshDirectories(self):
        files_in_ash = [
            MockFile('ash/BUILD.gn', []),
            MockFile('chrome/browser/ash/BUILD.gn', []),
        ]
        other_files = [
            MockFile('chrome/browser/BUILD.gn', []),
            MockFile('chrome/browser/BUILD.gn', ['assert(is_chromeos_ash)']),
        ]
        input_api = MockInputApi()
        input_api.files = files_in_ash
        errors = PRESUBMIT.CheckAssertAshOnlyCode(input_api, MockOutputApi())
        self.assertEqual(2, len(errors))

        input_api.files = other_files
        errors = PRESUBMIT.CheckAssertAshOnlyCode(input_api, MockOutputApi())
        self.assertEqual(0, len(errors))

    def testDoesNotErrorOnNonGNFiles(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('ash/test.h', ['assert(is_chromeos_ash)']),
            MockFile('chrome/browser/ash/test.cc',
                     ['assert(is_chromeos_ash)']),
        ]
        errors = PRESUBMIT.CheckAssertAshOnlyCode(input_api, MockOutputApi())
        self.assertEqual(0, len(errors))

    def testDeletedFile(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('ash/BUILD.gn', []),
            MockFile('ash/foo/BUILD.gn', [], action='D'),
        ]
        errors = PRESUBMIT.CheckAssertAshOnlyCode(input_api, MockOutputApi())
        self.assertEqual(1, len(errors))

    def testDoesNotErrorWithAssertion(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('ash/BUILD.gn', ['assert(is_chromeos_ash)']),
            MockFile('chrome/browser/ash/BUILD.gn',
                     ['assert(is_chromeos_ash)']),
            MockFile('chrome/browser/ash/BUILD.gn',
                     ['assert(is_chromeos_ash, "test")']),
        ]
        errors = PRESUBMIT.CheckAssertAshOnlyCode(input_api, MockOutputApi())
        self.assertEqual(0, len(errors))


class CheckRawPtrUsageTest(unittest.TestCase):

    def testAllowedCases(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            # Browser-side files are allowed.
            MockAffectedFile('test10/browser/foo.h', ['raw_ptr<int>']),
            MockAffectedFile('test11/browser/foo.cc', ['raw_ptr<int>']),
            MockAffectedFile('test12/blink/common/foo.cc', ['raw_ptr<int>']),
            MockAffectedFile('test13/blink/public/common/foo.cc',
                             ['raw_ptr<int>']),
            MockAffectedFile('test14/blink/public/platform/foo.cc',
                             ['raw_ptr<int>']),

            # Non-C++ files are allowed.
            MockAffectedFile('test20/renderer/foo.md', ['raw_ptr<int>']),

            # Renderer code is generally allowed (except specifically
            # disallowed directories).
            MockAffectedFile('test30/renderer/foo.cc', ['raw_ptr<int>']),
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckRawPtrUsage(mock_input_api, mock_output_api)
        self.assertFalse(errors)

    def testDisallowedCases(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test1/third_party/blink/renderer/core/foo.h',
                             ['raw_ptr<int>']),
            MockAffectedFile(
                'test2/third_party/blink/renderer/platform/heap/foo.cc',
                ['raw_ptr<int>']),
            MockAffectedFile(
                'test3/third_party/blink/renderer/platform/wtf/foo.cc',
                ['raw_ptr<int>']),
            MockAffectedFile(
                'test4/third_party/blink/renderer/platform/fonts/foo.h',
                ['raw_ptr<int>']),
            MockAffectedFile(
                'test5/third_party/blink/renderer/core/paint/foo.cc',
                ['raw_ptr<int>']),
            MockAffectedFile(
                'test6/third_party/blink/renderer/platform/graphics/compositing/foo.h',
                ['raw_ptr<int>']),
            MockAffectedFile(
                'test7/third_party/blink/renderer/platform/graphics/paint/foo.cc',
                ['raw_ptr<int>']),
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckRawPtrUsage(mock_input_api, mock_output_api)
        self.assertEqual(len(mock_input_api.files), len(errors))
        for error in errors:
            self.assertTrue(
                'raw_ptr<T> should not be used in this renderer code' in
                error.message)

class CheckAdvancedMemorySafetyChecksUsageTest(unittest.TestCase):

    def testAllowedCases(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            # Non-C++ files are allowed.
            MockAffectedFile('test20/renderer/foo.md',
                             ['ADVANCED_MEMORY_SAFETY_CHECKS()']),

            # Mentions in a comment are allowed.
            MockAffectedFile('test30/renderer/foo.cc',
                             ['//ADVANCED_MEMORY_SAFETY_CHECKS()']),
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckAdvancedMemorySafetyChecksUsage(
            mock_input_api, mock_output_api)
        self.assertFalse(errors)

    def testDisallowedCases(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('test1/foo.h',
                             ['ADVANCED_MEMORY_SAFETY_CHECKS()']),
            MockAffectedFile('test2/foo.cc',
                             ['ADVANCED_MEMORY_SAFETY_CHECKS()']),
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT.CheckAdvancedMemorySafetyChecksUsage(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue('ADVANCED_MEMORY_SAFETY_CHECKS() macro is managed by'
                        in errors[0].message)

class AssertPythonShebangTest(unittest.TestCase):
    def testError(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('ash/test.py', ['#!/usr/bin/python']),
            MockFile('chrome/test.py', ['#!/usr/bin/python2']),
            MockFile('third_party/blink/test.py', ['#!/usr/bin/python3']),
            MockFile('empty.py', []),
        ]
        errors = PRESUBMIT.CheckPythonShebang(input_api, MockOutputApi())
        self.assertEqual(3, len(errors))

    def testNonError(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('chrome/browser/BUILD.gn', ['#!/usr/bin/python']),
            MockFile('third_party/blink/web_tests/external/test.py',
                     ['#!/usr/bin/python2']),
            MockFile('third_party/test/test.py', ['#!/usr/bin/python3']),
        ]
        errors = PRESUBMIT.CheckPythonShebang(input_api, MockOutputApi())
        self.assertEqual(0, len(errors))

class VerifyDcheckParentheses(unittest.TestCase):

    def testPermissibleUsage(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('okay1.cc', ['DCHECK_IS_ON()']),
            MockFile('okay2.cc', ['#if DCHECK_IS_ON()']),

            # Other constructs that aren't exactly `DCHECK_IS_ON()` do their
            # own thing at their own risk.
            MockFile('okay3.cc', ['PA_DCHECK_IS_ON']),
            MockFile('okay4.cc', ['#if PA_DCHECK_IS_ON']),
            MockFile('okay6.cc', ['PA_BUILDFLAG(PA_DCHECK_IS_ON)']),
        ]
        errors = PRESUBMIT.CheckDCHECK_IS_ONHasBraces(input_api,
                                                      MockOutputApi())
        self.assertEqual(0, len(errors))

    def testMissingParentheses(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('bad1.cc', ['DCHECK_IS_ON']),
            MockFile('bad2.cc', ['#if DCHECK_IS_ON']),
            MockFile('bad3.cc', ['DCHECK_IS_ON && foo']),
        ]
        errors = PRESUBMIT.CheckDCHECK_IS_ONHasBraces(input_api,
                                                      MockOutputApi())
        self.assertEqual(3, len(errors))
        for error in errors:
            self.assertRegex(error.message, r'DCHECK_IS_ON().+parentheses')


class CheckBatchAnnotation(unittest.TestCase):
    """Test the CheckBatchAnnotation presubmit check."""

    def testTruePositives(self):
        """Examples of when there is no @Batch or @DoNotBatch is correctly flagged.
"""
        mock_input = MockInputApi()
        mock_input.files = [
            MockFile('path/OneTest.java', ['public class OneTest']),
            MockFile('path/TwoTest.java', ['public class TwoTest']),
            MockFile('path/ThreeTest.java', [
                '@Batch(Batch.PER_CLASS)',
                'import org.chromium.base.test.BaseRobolectricTestRunner;',
                'public class Three {'
            ]),
            MockFile('path/FourTest.java', [
                '@DoNotBatch(reason = "placeholder reason 1")',
                'import org.chromium.base.test.BaseRobolectricTestRunner;',
                'public class Four {'
            ]),
        ]
        errors = PRESUBMIT.CheckBatchAnnotation(mock_input, MockOutputApi())
        self.assertEqual(2, len(errors))
        self.assertEqual(2, len(errors[0].items))
        self.assertIn('OneTest.java', errors[0].items[0])
        self.assertIn('TwoTest.java', errors[0].items[1])
        self.assertEqual(2, len(errors[1].items))
        self.assertIn('ThreeTest.java', errors[1].items[0])
        self.assertIn('FourTest.java', errors[1].items[1])

    def testAnnotationsPresent(self):
        """Examples of when there is @Batch or @DoNotBatch is correctly flagged."""
        mock_input = MockInputApi()
        mock_input.files = [
            MockFile('path/OneTest.java',
                     ['@Batch(Batch.PER_CLASS)', 'public class One {']),
            MockFile('path/TwoTest.java', [
                '@DoNotBatch(reason = "placeholder reasons.")',
                'public class Two {'
            ]),
            MockFile('path/ThreeTest.java', [
                '@Batch(Batch.PER_CLASS)',
                'public class Three extends BaseTestA {'
            ], [
                '@Batch(Batch.PER_CLASS)',
                'public class Three extends BaseTestB {'
            ]),
            MockFile('path/FourTest.java', [
                '@DoNotBatch(reason = "placeholder reason 1")',
                'public class Four extends BaseTestA {'
            ], [
                '@DoNotBatch(reason = "placeholder reason 2")',
                'public class Four extends BaseTestB {'
            ]),
            MockFile('path/FiveTest.java', [
                'import androidx.test.uiautomator.UiDevice;',
                'public class Five extends BaseTestA {'
            ], [
                'import androidx.test.uiautomator.UiDevice;',
                'public class Five extends BaseTestB {'
            ]),
            MockFile('path/SixTest.java', [
                'import org.chromium.base.test.BaseRobolectricTestRunner;',
                'public class Six extends BaseTestA {'
            ], [
                'import org.chromium.base.test.BaseRobolectricTestRunner;',
                'public class Six extends BaseTestB {'
            ]),
            MockFile('path/SevenTest.java', [
                'import org.robolectric.annotation.Config;',
                'public class Seven extends BaseTestA {'
            ], [
                'import org.robolectric.annotation.Config;',
                'public class Seven extends BaseTestB {'
            ]),
            MockFile(
                'path/OtherClass.java',
                ['public class OtherClass {'],
            ),
            MockFile('path/PRESUBMIT.py', [
                '@Batch(Batch.PER_CLASS)',
                '@DoNotBatch(reason = "placeholder reason)'
            ]),
            MockFile(
                'path/AnnotationTest.java',
                ['public @interface SomeAnnotation {'],
            ),
        ]
        errors = PRESUBMIT.CheckBatchAnnotation(mock_input, MockOutputApi())
        self.assertEqual(0, len(errors))


class CheckMockAnnotation(unittest.TestCase):
    """Test the CheckMockAnnotation presubmit check."""

    def testTruePositives(self):
        """Examples of @Mock or @Spy being used and nothing should be flagged."""
        mock_input = MockInputApi()
        mock_input.files = [
            MockFile('path/OneTest.java', [
                'import a.b.c.Bar;',
                'import a.b.c.Foo;',
                '@Mock public static Foo f = new Foo();',
                'Mockito.mock(new Bar(a, b, c))'
            ]),
            MockFile('path/TwoTest.java', [
                'package x.y.z;',
                'import static org.mockito.Mockito.spy;',
                '@Spy',
                'public static FooBar<Baz> f;',
                'a = spy(Baz.class)'
            ]),
        ]
        errors = PRESUBMIT.CheckMockAnnotation(mock_input, MockOutputApi())
        self.assertEqual(1, len(errors))
        self.assertEqual(2, len(errors[0].items))
        self.assertIn('a.b.c.Bar in path/OneTest.java', errors[0].items)
        self.assertIn('x.y.z.Baz in path/TwoTest.java', errors[0].items)

    def testTrueNegatives(self):
        """Examples of when we should not be flagging mock() or spy() calls."""
        mock_input = MockInputApi()
        mock_input.files = [
            MockFile('path/OneTest.java', [
                'package a.b.c;',
                'import org.chromium.base.test.BaseRobolectricTestRunner;',
                'Mockito.mock(Abc.class)'
            ]),
            MockFile('path/TwoTest.java', [
                'package a.b.c;',
                'import androidx.test.uiautomator.UiDevice;',
                'Mockito.spy(new Def())'
            ]),
            MockFile('path/ThreeTest.java', [
                'package a.b.c;',
                'import static org.mockito.Mockito.spy;',
                '@Spy',
                'public static Foo f = new Abc();',
                'a = spy(Foo.class)'
            ]),
            MockFile('path/FourTest.java', [
                'package a.b.c;',
                'import static org.mockito.Mockito.mock;',
                '@Spy',
                'public static Bar b = new Abc(a, b, c, d);',
                ' mock(new Bar(a,b,c))'
            ]),
            MockFile('path/FiveTest.java', [
                'package a.b.c;',
                '@Mock',
                'public static Baz<abc> b;',
                'Mockito.mock(Baz.class)'
            ]),
            MockFile('path/SixTest.java', [
                'package a.b.c;',
                'import android.view.View;',
                'import java.ArrayList;',
                'Mockito.spy(new View())',
                'Mockito.mock(ArrayList.class)'
            ]),
            MockFile('path/SevenTest.java', [
                'package a.b.c;',
                '@Mock private static Seven s;',
                'Mockito.mock(Seven.class)'
            ]),
            MockFile('path/EightTest.java', [
                'package a.b.c;',
                '@Spy Eight e = new Eight2();',
                'Mockito.py(new Eight())'
            ]),
        ]
        errors = PRESUBMIT.CheckMockAnnotation(mock_input, MockOutputApi())
        self.assertEqual(0, len(errors))


class AssertNoJsInIosTest(unittest.TestCase):
    def testErrorJs(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('components/feature/ios/resources/script.js', []),
            MockFile('ios/chrome/feature/resources/script.js', []),
        ]
        results = PRESUBMIT.CheckNoJsInIos(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertEqual(2, len(results[0].items))

    def testNonError(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('chrome/resources/script.js', []),
            MockFile('components/feature/ios/resources/script.ts', []),
            MockFile('ios/chrome/feature/resources/script.ts', []),
            MockFile('ios/web/feature/resources/script.ts', []),
            MockFile('ios/third_party/script.js', []),
            MockFile('third_party/ios/script.js', []),
        ]
        results = PRESUBMIT.CheckNoJsInIos(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testExistingFilesWarningOnly(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('ios/chrome/feature/resources/script.js', [], action='M'),
            MockFile('ios/chrome/feature/resources/script2.js', [], action='D'),
        ]
        results = PRESUBMIT.CheckNoJsInIos(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('warning', results[0].type)
        self.assertEqual(1, len(results[0].items))

    def testMovedScriptWarningOnly(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('ios/chrome/feature/resources/script.js', [], action='D'),
            MockFile('ios/chrome/renamed_feature/resources/script.js', [], action='A'),
        ]
        results = PRESUBMIT.CheckNoJsInIos(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('warning', results[0].type)
        self.assertEqual(1, len(results[0].items))

class CheckNoAbbreviationInPngFileNameTest(unittest.TestCase):

    def testHasAbbreviation(self):
        """test png file names with abbreviation that fails the check"""
        input_api = MockInputApi()
        input_api.files = [
            MockFile('image_a.png', [], action='A'),
            MockFile('image_a_.png', [], action='A'),
            MockFile('image_a_name.png', [], action='A'),
            MockFile('chrome/ui/feature_name/resources/image_a.png', [],
                     action='A'),
            MockFile('chrome/ui/feature_name/resources/image_a_.png', [],
                     action='A'),
            MockFile('chrome/ui/feature_name/resources/image_a_name.png', [],
                     action='A'),
        ]
        results = PRESUBMIT.CheckNoAbbreviationInPngFileName(
            input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertEqual(len(input_api.files), len(results[0].items))

    def testNoAbbreviation(self):
        """test png file names without abbreviation that passes the check"""
        input_api = MockInputApi()
        input_api.files = [
            MockFile('a.png', [], action='A'),
            MockFile('_a.png', [], action='A'),
            MockFile('image.png', [], action='A'),
            MockFile('image_ab_.png', [], action='A'),
            MockFile('image_ab_name.png', [], action='A'),
            # These paths used to fail because `feature_a_name` matched the regex by mistake.
            # They should pass now because the path components ahead of the file name are ignored in the check.
            MockFile('chrome/ui/feature_a_name/resources/a.png', [],
                     action='A'),
            MockFile('chrome/ui/feature_a_name/resources/_a.png', [],
                     action='A'),
            MockFile('chrome/ui/feature_a_name/resources/image.png', [],
                     action='A'),
            MockFile('chrome/ui/feature_a_name/resources/image_ab_.png', [],
                     action='A'),
            MockFile('chrome/ui/feature_a_name/resources/image_ab_name.png',
                     [],
                     action='A'),
        ]
        results = PRESUBMIT.CheckNoAbbreviationInPngFileName(
            input_api, MockOutputApi())
        self.assertEqual(0, len(results))

class CheckDanglingUntriagedTest(unittest.TestCase):

    def testError(self):
        """Test patch adding dangling pointers are reported"""
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.change.DescriptionText = lambda: "description"
        mock_input_api.files = [
            MockAffectedFile(
                local_path="foo/foo.cc",
                old_contents=["raw_ptr<T>"],
                new_contents=["raw_ptr<T, DanglingUntriaged>"],
            )
        ]
        msgs = PRESUBMIT.CheckDanglingUntriaged(mock_input_api,
                                                mock_output_api)
        self.assertEqual(len(msgs), 1)
        self.assertEqual(len(msgs[0].message), 10)
        self.assertEqual(
            msgs[0].message[0],
            "Unexpected new occurrences of `DanglingUntriaged` detected. Please",
        )

class CheckDanglingUntriagedTest(unittest.TestCase):

    def testError(self):
        """Test patch adding dangling pointers are reported"""
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.change.DescriptionText = lambda: "description"
        mock_input_api.files = [
            MockAffectedFile(
                local_path="foo/foo.cc",
                old_contents=["raw_ptr<T>"],
                new_contents=["raw_ptr<T, DanglingUntriaged>"],
            )
        ]
        msgs = PRESUBMIT.CheckDanglingUntriaged(mock_input_api,
                                                mock_output_api)
        self.assertEqual(len(msgs), 1)
        self.assertTrue(
            ("Unexpected new occurrences of `DanglingUntriaged` detected"
             in msgs[0].message))

    def testNonCppFile(self):
        """Test patch adding dangling pointers are not reported in non C++ files"""
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.change.DescriptionText = lambda: "description"
        mock_input_api.files = [
            MockAffectedFile(
                local_path="foo/README.md",
                old_contents=[""],
                new_contents=["The DanglingUntriaged annotation means"],
            )
        ]
        msgs = PRESUBMIT.CheckDanglingUntriaged(mock_input_api,
                                                mock_output_api)
        self.assertEqual(len(msgs), 0)

    def testDeveloperAcknowledgeInCommitDescription(self):
        """Test patch adding dangling pointers, but acknowledged by the developers
    aren't reported"""
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile(
                local_path="foo/foo.cc",
                old_contents=["raw_ptr<T>"],
                new_contents=["raw_ptr<T, DanglingUntriaged>"],
            )
        ]
        mock_input_api.change.DescriptionText = lambda: (
            "DanglingUntriaged-notes: Sorry about this!")
        msgs = PRESUBMIT.CheckDanglingUntriaged(mock_input_api,
                                                mock_output_api)
        self.assertEqual(len(msgs), 0)

    def testDeveloperAcknowledgeInCommitFooter(self):
        """Test patch adding dangling pointers, but acknowledged by the developers
    aren't reported"""
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile(
                local_path="foo/foo.cc",
                old_contents=["raw_ptr<T>"],
                new_contents=["raw_ptr<T, DanglingUntriaged>"],
            )
        ]
        mock_input_api.change.DescriptionText = lambda: "description"
        mock_input_api.change.footers["DanglingUntriaged-notes"] = ["Sorry!"]
        msgs = PRESUBMIT.CheckDanglingUntriaged(mock_input_api,
                                                mock_output_api)
        self.assertEqual(len(msgs), 0)

    def testCongrats(self):
        """Test the presubmit congrats users removing dangling pointers"""
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile(
                local_path="foo/foo.cc",
                old_contents=["raw_ptr<T, DanglingUntriaged>"],
                new_contents=["raw_ptr<T>"],
            )
        ]
        mock_input_api.change.DescriptionText = lambda: (
            "This patch fixes some DanglingUntriaged pointers!")
        msgs = PRESUBMIT.CheckDanglingUntriaged(mock_input_api,
                                                mock_output_api)
        self.assertEqual(len(msgs), 1)
        self.assertTrue(
            "DanglingUntriaged pointers removed: 1" in msgs[0].message)
        self.assertTrue("Thank you!" in msgs[0].message)

    def testRenameFile(self):
        """Patch that we do not warn about DanglingUntriaged when moving files"""
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()

        mock_input_api.files = [
            MockAffectedFile(
                local_path="foo/foo.cc",
                old_contents=["raw_ptr<T, DanglingUntriaged>"],
                new_contents=[""],
                action="D",
            ),
            MockAffectedFile(
                local_path="foo/foo.cc",
                old_contents=[""],
                new_contents=["raw_ptr<T, DanglingUntriaged>"],
                action="A",
            ),
        ]
        mock_input_api.change.DescriptionText = lambda: (
            "This patch moves files")
        msgs = PRESUBMIT.CheckDanglingUntriaged(mock_input_api,
                                                mock_output_api)
        self.assertEqual(len(msgs), 0)

class CheckInlineConstexprDefinitionsInHeadersTest(unittest.TestCase):

    def testNoInlineConstexprInHeaderFile(self):
        """Tests that non-inlined constexpr variables in headers fail the test."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/constants.h',
                             ['constexpr int kVersion = 5;'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testNoInlineConstexprInHeaderFileInitializedFromFunction(self):
        """Tests that non-inlined constexpr header variables that are initialized from a function fail."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/constants.h',
                             ['constexpr int kVersion = GetVersion();'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testNoInlineConstexprInHeaderFileInitializedWithExpression(self):
        """Tests that non-inlined constexpr header variables initialized with an expression fail."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/constants.h',
                             ['constexpr int kVersion = (4 + 5)*3;'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testNoInlineConstexprInHeaderFileBraceInitialized(self):
        """Tests that non-inlined constexpr header variables that are brace-initialized fail."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/constants.h', ['constexpr int kVersion{5};'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testNoInlineConstexprInHeaderWithAttribute(self):
        """Tests that non-inlined constexpr header variables that have compiler attributes fail."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/constants.h',
                             ['constexpr [[maybe_unused]] int kVersion{5};'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testInlineConstexprInHeaderWithAttribute(self):
        """Tests that inlined constexpr header variables that have compiler attributes pass."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile(
                'src/constants.h',
                ['inline constexpr [[maybe_unused]] int kVersion{5};']),
            MockAffectedFile(
                'src/constants.h',
                ['constexpr inline [[maybe_unused]] int kVersion{5};']),
            MockAffectedFile(
                'src/constants.h',
                ['inline constexpr [[maybe_unused]] inline int kVersion{5};'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testNoInlineConstexprInHeaderFileMultipleLines(self):
        """Tests that non-inlined constexpr header variable definitions spanning multiple lines fail."""
        input_api = MockInputApi()
        lines = [
            'constexpr char kLongName =',
            '    "This is a very long name of something.";'
        ]
        input_api.files = [MockAffectedFile('src/constants.h', lines)]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testNoInlineConstexprInCCFile(self):
        """Tests that non-inlined constexpr variables in .cc files pass the test."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/implementation.cc',
                             ['constexpr int kVersion = 5;'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testInlineConstexprInHeaderFile(self):
        """Tests that inlined constexpr variables in header files pass the test."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/constants.h',
                             ['constexpr inline int kX = 5;']),
            MockAffectedFile('src/version.h',
                             ['inline constexpr float kY = 5.0f;'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testConstexprStandaloneFunctionInHeaderFile(self):
        """Tests that non-inlined constexpr functions in headers pass the test."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/helpers.h', ['constexpr int GetVersion();'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testConstexprWithAbseilAttributeInHeader(self):
        """Tests that non-inlined constexpr variables with Abseil-type prefixes in headers fail."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/helpers.h',
                             ['ABSL_FOOFOO constexpr int i = 5;'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testInlineConstexprWithAbseilAttributeInHeader(self):
        """Tests that inlined constexpr variables with Abseil-type prefixes in headers pass."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/helpers.h',
                             ['constexpr ABSL_FOO inline int i = 5;'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testConstexprWithClangAttributeInHeader(self):
        """Tests that non-inlined constexpr variables with attributes with colons in headers fail."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/helpers.h',
                             ['[[clang::someattribute]] constexpr int i = 5;'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testInlineConstexprWithClangAttributeInHeader(self):
        """Tests that inlined constexpr variables with attributes with colons in headers pass."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile(
                'src/helpers.h',
                ['constexpr [[clang::someattribute]] inline int i = 5;'])
        ]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testNoExplicitInlineConstexprInsideClassInHeaderFile(self):
        """Tests that non-inlined constexpr class members pass the test."""
        input_api = MockInputApi()
        lines = [
            'class SomeClass {', ' public:',
            '  static constexpr kVersion = 5;', '};'
        ]
        input_api.files = [MockAffectedFile('src/class.h', lines)]
        warnings = PRESUBMIT.CheckInlineConstexprDefinitionsInHeaders(
            input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

    def testTodoBugReferencesWithOldBugId(self):
        """Tests that an old monorail bug ID in a TODO fails."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/helpers.h', ['// TODO(crbug.com/12345)'])
        ]
        warnings = PRESUBMIT.CheckTodoBugReferences(input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))

    def testTodoBugReferencesWithUpdatedBugId(self):
        """Tests that a new issue tracker bug ID in a TODO passes."""
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('src/helpers.h', ['// TODO(crbug.com/40781525)'])
        ]
        warnings = PRESUBMIT.CheckTodoBugReferences(input_api, MockOutputApi())
        self.assertEqual(0, len(warnings))

class CheckDeprecatedSyncConsentFunctionsTest(unittest.TestCase):
    """Test the presubmit for deprecated ConsentLevel::kSync functions."""

    def testCppMobilePlatformPath(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('chrome/browser/android/file.cc', ['OtherFunction']),
            MockFile('chrome/android/file.cc', ['HasSyncConsent']),
            MockFile('ios/file.mm', ['CanSyncFeatureStart']),
            MockFile('components/foo/ios/file.cc', ['IsSyncFeatureEnabled']),
            MockFile('components/foo/delegate_android.cc',
                     ['IsSyncFeatureActive']),
            MockFile('components/foo/delegate_ios.cc',
                     ['IsSyncFeatureActive']),
            MockFile('components/foo/android_delegate.cc',
                     ['IsSyncFeatureActive']),
            MockFile('components/foo/ios_delegate.cc',
                     ['IsSyncFeatureActive']),
        ]

        results = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())

        self.assertEqual(1, len(results))
        self.assertFalse(
            'chrome/browser/android/file.cc' in results[0].message),
        self.assertTrue('chrome/android/file.cc' in results[0].message),
        self.assertTrue('ios/file.mm' in results[0].message),
        self.assertTrue('components/foo/ios/file.cc' in results[0].message),
        self.assertTrue(
            'components/foo/delegate_android.cc' in results[0].message),
        self.assertTrue(
            'components/foo/delegate_ios.cc' in results[0].message),
        self.assertTrue(
            'components/foo/android_delegate.cc' in results[0].message),
        self.assertTrue(
            'components/foo/ios_delegate.cc' in results[0].message),

    def testCppNonMobilePlatformPath(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('chrome/browser/file.cc', ['HasSyncConsent']),
            MockFile('bios/file.cc', ['HasSyncConsent']),
            MockFile('components/kiosk/file.cc', ['HasSyncConsent']),
        ]

        results = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())

        self.assertEqual(0, len(results))

    def testJavaPath(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile('components/foo/file1.java', ['otherFunction']),
            MockFile('components/foo/file2.java', ['hasSyncConsent']),
            MockFile('chrome/foo/file3.java', ['canSyncFeatureStart']),
            MockFile('chrome/foo/file4.java', ['isSyncFeatureEnabled']),
            MockFile('chrome/foo/file5.java', ['isSyncFeatureActive']),
        ]

        results = PRESUBMIT.CheckNoBannedFunctions(input_api, MockOutputApi())

        self.assertEqual(1, len(results))
        self.assertFalse('components/foo/file1.java' in results[0].message),
        self.assertTrue('components/foo/file2.java' in results[0].message),
        self.assertTrue('chrome/foo/file3.java' in results[0].message),
        self.assertTrue('chrome/foo/file4.java' in results[0].message),
        self.assertTrue('chrome/foo/file5.java' in results[0].message),


if __name__ == '__main__':
    unittest.main()
