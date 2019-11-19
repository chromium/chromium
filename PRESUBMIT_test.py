#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import subprocess
import unittest

import PRESUBMIT
from PRESUBMIT_test_mocks import MockFile, MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi


_TEST_DATA_DIR = 'base/test/data/presubmit'


class VersionControlConflictsTest(unittest.TestCase):
  def testTypicalConflict(self):
    lines = ['<<<<<<< HEAD',
             '  base::ScopedTempDir temp_dir_;',
             '=======',
             '  ScopedTempDir temp_dir_;',
             '>>>>>>> master']
    errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(3, len(errors))
    self.assertTrue('1' in errors[0])
    self.assertTrue('3' in errors[1])
    self.assertTrue('5' in errors[2])

  def testIgnoresReadmes(self):
    lines = ['A First Level Header',
             '====================',
             '',
             'A Second Level Header',
             '---------------------']
    errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
        MockInputApi(), MockFile('some/polymer/README.md', lines))
    self.assertEqual(0, len(errors))


class UmaHistogramChangeMatchedOrNotTest(unittest.TestCase):
  def testTypicalCorrectlyMatchedChange(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_java = [
      'RecordHistogram.recordBooleanHistogram("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram name="Bla.Foo.Dummy"> </histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo.java', diff_java),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testTypicalNotMatchedChange(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_java = [
      'RecordHistogram.recordBooleanHistogram("Bla.Foo.Dummy", true)']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo.java', diff_java),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual('warning', warnings[0].type)
    self.assertTrue('foo.cc' in warnings[0].items[0])
    self.assertTrue('foo.java' in warnings[0].items[1])

  def testTypicalNotMatchedChangeViaSuffixes(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_java = [
      'RecordHistogram.recordBooleanHistogram("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Snafu.Dummy"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo.java', diff_java),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual('warning', warnings[0].type)
    self.assertTrue('foo.cc' in warnings[0].items[0])
    self.assertTrue('foo.java' in warnings[0].items[1])

  def testTypicalCorrectlyMatchedChangeViaSuffixes(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_java = [
      'RecordHistogram.recordBooleanHistogram("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Bla.Foo"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo.java', diff_java),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testTypicalCorrectlyMatchedChangeViaSuffixesWithSeparator(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Snafu_Dummy", true)']
    diff_java = ['RecordHistogram.recordBooleanHistogram("Snafu_Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram" separator="_">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Snafu"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo.java', diff_java),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testCorrectlyMatchedChangeViaSuffixesWithLineWrapping(self):
    diff_cc = [
        'UMA_HISTOGRAM_BOOL("LongHistogramNameNeedsLineWrapping.Dummy", true)']
    diff_java = ['RecordHistogram.recordBooleanHistogram(' +
                 '"LongHistogramNameNeedsLineWrapping.Dummy", true)']
    diff_xml = ['<histogram_suffixes',
                '    name="LongHistogramNameNeedsLineWrapping"',
                '    separator=".">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram',
                '      name="LongHistogramNameNeedsLineWrapping"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo.java', diff_java),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testNameMatch(self):
    # Check that the detected histogram name is "Dummy" and not, e.g.,
    # "Dummy\", true);  // The \"correct"
    diff_cc = ['UMA_HISTOGRAM_BOOL("Dummy", true);  // The "correct" histogram']
    diff_java = [
      'RecordHistogram.recordBooleanHistogram("Dummy", true);' +
      '  // The "correct" histogram']
    diff_xml = ['<histogram name="Dummy"> </histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo.java', diff_java),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testSimilarMacroNames(self):
    diff_cc = ['PUMA_HISTOGRAM_COOL("Mountain Lion", 42)']
    diff_java = [
      'FakeRecordHistogram.recordFakeHistogram("Mountain Lion", 42)']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo.java', diff_java),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testMultiLine(self):
    diff_cc = ['UMA_HISTOGRAM_BOOLEAN(', '    "Multi.Line", true)']
    diff_cc2 = ['UMA_HISTOGRAM_BOOLEAN(', '    "Multi.Line"', '    , true)']
    diff_java = [
      'RecordHistogram.recordBooleanHistogram(',
      '    "Multi.Line", true);',
    ]
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('some/path/foo2.cc', diff_cc2),
      MockFile('some/path/foo.java', diff_java),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual('warning', warnings[0].type)
    self.assertTrue('foo.cc' in warnings[0].items[0])
    self.assertTrue('foo2.cc' in warnings[0].items[1])


class BadExtensionsTest(unittest.TestCase):
  def testBadRejFile(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', ''),
      MockFile('some/path/foo.cc.rej', ''),
      MockFile('some/path2/bar.h.rej', ''),
    ]

    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
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

    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual(1, len(results[0].items))
    self.assertTrue('qux.h.orig' in results[0].items[0])

  def testGoodFiles(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('other/path/qux.h', ''),
      MockFile('other/path/qux.cc', ''),
    ]
    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(0, len(results))


class CheckSingletonInHeadersTest(unittest.TestCase):
  def testSingletonInArbitraryHeader(self):
    diff_singleton_h = ['base::subtle::AtomicWord '
                        'base::Singleton<Type, Traits, DifferentiatingType>::']
    diff_foo_h = ['// base::Singleton<Foo> in comment.',
                  'friend class base::Singleton<Foo>']
    diff_foo2_h = ['  //Foo* bar = base::Singleton<Foo>::get();']
    diff_bad_h = ['Foo* foo = base::Singleton<Foo>::get();']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockAffectedFile('base/memory/singleton.h',
                                             diff_singleton_h),
                            MockAffectedFile('foo.h', diff_foo_h),
                            MockAffectedFile('foo2.h', diff_foo2_h),
                            MockAffectedFile('bad.h', diff_bad_h)]
    warnings = PRESUBMIT._CheckSingletonInHeaders(mock_input_api,
                                                  MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(1, len(warnings[0].items))
    self.assertEqual('error', warnings[0].type)
    self.assertTrue('Found base::Singleton<T>' in warnings[0].message)

  def testSingletonInCC(self):
    diff_cc = ['Foo* foo = base::Singleton<Foo>::get();']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockAffectedFile('some/path/foo.cc', diff_cc)]
    warnings = PRESUBMIT._CheckSingletonInHeaders(mock_input_api,
                                                  MockOutputApi())
    self.assertEqual(0, len(warnings))


class InvalidOSMacroNamesTest(unittest.TestCase):
  def testInvalidOSMacroNames(self):
    lines = ['#if defined(OS_WINDOWS)',
             ' #elif defined(OS_WINDOW)',
             ' # if defined(OS_MACOSX) || defined(OS_CHROME)',
             '# else  // defined(OS_MAC)',
             '#endif  // defined(OS_MACOS)']
    errors = PRESUBMIT._CheckForInvalidOSMacrosInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(len(lines), len(errors))
    self.assertTrue(':1 OS_WINDOWS' in errors[0])
    self.assertTrue('(did you mean OS_WIN?)' in errors[0])

  def testValidOSMacroNames(self):
    lines = ['#if defined(%s)' % m for m in PRESUBMIT._VALID_OS_MACROS]
    errors = PRESUBMIT._CheckForInvalidOSMacrosInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(0, len(errors))


class InvalidIfDefinedMacroNamesTest(unittest.TestCase):
  def testInvalidIfDefinedMacroNames(self):
    lines = ['#if defined(TARGET_IPHONE_SIMULATOR)',
             '#if !defined(TARGET_IPHONE_SIMULATOR)',
             '#elif defined(TARGET_IPHONE_SIMULATOR)',
             '#ifdef TARGET_IPHONE_SIMULATOR',
             ' # ifdef TARGET_IPHONE_SIMULATOR',
             '# if defined(VALID) || defined(TARGET_IPHONE_SIMULATOR)',
             '# else  // defined(TARGET_IPHONE_SIMULATOR)',
             '#endif  // defined(TARGET_IPHONE_SIMULATOR)']
    errors = PRESUBMIT._CheckForInvalidIfDefinedMacrosInFile(
        MockInputApi(), MockFile('some/path/source.mm', lines))
    self.assertEqual(len(lines), len(errors))

  def testValidIfDefinedMacroNames(self):
    lines = ['#if defined(FOO)',
             '#ifdef BAR']
    errors = PRESUBMIT._CheckForInvalidIfDefinedMacrosInFile(
        MockInputApi(), MockFile('some/path/source.cc', lines))
    self.assertEqual(0, len(errors))


class CheckAddedDepsHaveTetsApprovalsTest(unittest.TestCase):

  def calculate(self, old_include_rules, old_specific_include_rules,
                new_include_rules, new_specific_include_rules):
    return PRESUBMIT._CalculateAddedDeps(
        os.path, 'include_rules = %r\nspecific_include_rules = %r' % (
            old_include_rules, old_specific_include_rules),
        'include_rules = %r\nspecific_include_rules = %r' % (
            new_include_rules, new_specific_include_rules))

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
    self.assertEqual(set(),
                     self.calculate(old_include_rules, {}, new_include_rules,
                                    {}))


class JSONParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    filename = 'valid_json.json'
    contents = ['// This is a comment.',
                '{',
                '  "key1": ["value1", "value2"],',
                '  "key2": 3  // This is an inline comment.',
                '}'
                ]
    input_api.files = [MockFile(filename, contents)]
    self.assertEqual(None,
                     PRESUBMIT._GetJSONParseError(input_api, filename))

  def testFailure(self):
    input_api = MockInputApi()
    test_data = [
      ('invalid_json_1.json',
       ['{ x }'],
       'Expecting property name:'),
      ('invalid_json_2.json',
       ['// Hello world!',
        '{ "hello": "world }'],
       'Unterminated string starting at:'),
      ('invalid_json_3.json',
       ['{ "a": "b", "c": "d", }'],
       'Expecting property name:'),
      ('invalid_json_4.json',
       ['{ "a": "b" "c": "d" }'],
       'Expecting , delimiter:'),
    ]

    input_api.files = [MockFile(filename, contents)
                       for (filename, contents, _) in test_data]

    for (filename, _, expected_error) in test_data:
      actual_error = PRESUBMIT._GetJSONParseError(input_api, filename)
      self.assertTrue(expected_error in str(actual_error),
                      "'%s' not found in '%s'" % (expected_error, actual_error))

  def testNoEatComments(self):
    input_api = MockInputApi()
    file_with_comments = 'file_with_comments.json'
    contents_with_comments = ['// This is a comment.',
                              '{',
                              '  "key1": ["value1", "value2"],',
                              '  "key2": 3  // This is an inline comment.',
                              '}'
                              ]
    file_without_comments = 'file_without_comments.json'
    contents_without_comments = ['{',
                                 '  "key1": ["value1", "value2"],',
                                 '  "key2": 3',
                                 '}'
                                 ]
    input_api.files = [MockFile(file_with_comments, contents_with_comments),
                       MockFile(file_without_comments,
                                contents_without_comments)]

    self.assertEqual('No JSON object could be decoded',
                     str(PRESUBMIT._GetJSONParseError(input_api,
                                                      file_with_comments,
                                                      eat_comments=False)))
    self.assertEqual(None,
                     PRESUBMIT._GetJSONParseError(input_api,
                                                  file_without_comments,
                                                  eat_comments=False))


class IDLParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    filename = 'valid_idl_basics.idl'
    contents = ['// Tests a valid IDL file.',
                'namespace idl_basics {',
                '  enum EnumType {',
                '    name1,',
                '    name2',
                '  };',
                '',
                '  dictionary MyType1 {',
                '    DOMString a;',
                '  };',
                '',
                '  callback Callback1 = void();',
                '  callback Callback2 = void(long x);',
                '  callback Callback3 = void(MyType1 arg);',
                '  callback Callback4 = void(EnumType type);',
                '',
                '  interface Functions {',
                '    static void function1();',
                '    static void function2(long x);',
                '    static void function3(MyType1 arg);',
                '    static void function4(Callback1 cb);',
                '    static void function5(Callback2 cb);',
                '    static void function6(Callback3 cb);',
                '    static void function7(Callback4 cb);',
                '  };',
                '',
                '  interface Events {',
                '    static void onFoo1();',
                '    static void onFoo2(long x);',
                '    static void onFoo2(MyType1 arg);',
                '    static void onFoo3(EnumType type);',
                '  };',
                '};'
                ]
    input_api.files = [MockFile(filename, contents)]
    self.assertEqual(None,
                     PRESUBMIT._GetIDLParseError(input_api, filename))

  def testFailure(self):
    input_api = MockInputApi()
    test_data = [
      ('invalid_idl_1.idl',
       ['//',
        'namespace test {',
        '  dictionary {',
        '    DOMString s;',
        '  };',
        '};'],
       'Unexpected "{" after keyword "dictionary".\n'),
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
      ('invalid_idl_3.idl',
       ['//',
        'namespace test {',
        '  enum MissingComma {',
        '    name1',
        '    name2',
        '  };',
        '};'],
       'Unexpected symbol name2 after symbol name1.'),
      ('invalid_idl_4.idl',
       ['//',
        'namespace test {',
        '  enum TrailingComma {',
        '    name1,',
        '    name2,',
        '  };',
        '};'],
       'Trailing comma in block.'),
      ('invalid_idl_5.idl',
       ['//',
        'namespace test {',
        '  callback Callback1 = void(;',
        '};'],
       'Unexpected ";" after "(".'),
      ('invalid_idl_6.idl',
       ['//',
        'namespace test {',
        '  callback Callback1 = void(long );',
        '};'],
       'Unexpected ")" after symbol long.'),
      ('invalid_idl_7.idl',
       ['//',
        'namespace test {',
        '  interace Events {',
        '    static void onFoo1();',
        '  };',
        '};'],
       'Unexpected symbol Events after symbol interace.'),
      ('invalid_idl_8.idl',
       ['//',
        'namespace test {',
        '  interface NotEvent {',
        '    static void onFoo1();',
        '  };',
        '};'],
       'Did not process Interface Interface(NotEvent)'),
      ('invalid_idl_9.idl',
       ['//',
        'namespace test {',
        '  interface {',
        '    static void function1();',
        '  };',
        '};'],
       'Interface missing name.'),
    ]

    input_api.files = [MockFile(filename, contents)
                       for (filename, contents, _) in test_data]

    for (filename, _, expected_error) in test_data:
      actual_error = PRESUBMIT._GetIDLParseError(input_api, filename)
      self.assertTrue(expected_error in str(actual_error),
                      "'%s' not found in '%s'" % (expected_error, actual_error))


class TryServerMasterTest(unittest.TestCase):
  def testTryServerMasters(self):
    bots = {
        'master.tryserver.chromium.android': [
            'android_archive_rel_ng',
            'android_arm64_dbg_recipe',
            'android_blink_rel',
            'android_clang_dbg_recipe',
            'android_compile_dbg',
            'android_compile_x64_dbg',
            'android_compile_x86_dbg',
            'android_coverage',
            'android_cronet_tester'
            'android_swarming_rel',
            'cast_shell_android',
            'linux_android_dbg_ng',
            'linux_android_rel_ng',
        ],
        'master.tryserver.chromium.mac': [
            'ios_dbg_simulator',
            'ios_rel_device',
            'ios_rel_device_ninja',
            'mac_asan',
            'mac_asan_64',
            'mac_chromium_compile_dbg',
            'mac_chromium_compile_rel',
            'mac_chromium_dbg',
            'mac_chromium_rel',
            'mac_nacl_sdk',
            'mac_nacl_sdk_build',
            'mac_rel_naclmore',
            'mac_x64_rel',
            'mac_xcodebuild',
        ],
        'master.tryserver.chromium.linux': [
            'chromium_presubmit',
            'linux_arm_cross_compile',
            'linux_arm_tester',
            'linux_chromeos_asan',
            'linux_chromeos_browser_asan',
            'linux_chromeos_valgrind',
            'linux_chromium_chromeos_dbg',
            'linux_chromium_chromeos_rel',
            'linux_chromium_compile_dbg',
            'linux_chromium_compile_rel',
            'linux_chromium_dbg',
            'linux_chromium_gn_dbg',
            'linux_chromium_gn_rel',
            'linux_chromium_rel',
            'linux_chromium_trusty32_dbg',
            'linux_chromium_trusty32_rel',
            'linux_chromium_trusty_dbg',
            'linux_chromium_trusty_rel',
            'linux_clang_tsan',
            'linux_ecs_ozone',
            'linux_layout',
            'linux_layout_asan',
            'linux_layout_rel',
            'linux_layout_rel_32',
            'linux_nacl_sdk',
            'linux_nacl_sdk_bionic',
            'linux_nacl_sdk_bionic_build',
            'linux_nacl_sdk_build',
            'linux_redux',
            'linux_rel_naclmore',
            'linux_rel_precise32',
            'linux_valgrind',
            'tools_build_presubmit',
        ],
        'master.tryserver.chromium.win': [
            'win8_aura',
            'win8_chromium_dbg',
            'win8_chromium_rel',
            'win_chromium_compile_dbg',
            'win_chromium_compile_rel',
            'win_chromium_dbg',
            'win_chromium_rel',
            'win_chromium_rel',
            'win_chromium_x64_dbg',
            'win_chromium_x64_rel',
            'win_nacl_sdk',
            'win_nacl_sdk_build',
            'win_rel_naclmore',
         ],
    }
    for master, bots in bots.iteritems():
      for bot in bots:
        self.assertEqual(master, PRESUBMIT.GetTryServerMasterForBot(bot),
                         'bot=%s: expected %s, computed %s' % (
            bot, master, PRESUBMIT.GetTryServerMasterForBot(bot)))


class UserMetricsActionTest(unittest.TestCase):
  def testUserMetricsActionInActions(self):
    input_api = MockInputApi()
    file_with_user_action = 'file_with_user_action.cc'
    contents_with_user_action = [
      'base::UserMetricsAction("AboutChrome")'
    ]

    input_api.files = [MockFile(file_with_user_action,
                                contents_with_user_action)]

    self.assertEqual(
      [], PRESUBMIT._CheckUserActionUpdate(input_api, MockOutputApi()))

  def testUserMetricsActionNotAddedToActions(self):
    input_api = MockInputApi()
    file_with_user_action = 'file_with_user_action.cc'
    contents_with_user_action = [
      'base::UserMetricsAction("NotInActionsXml")'
    ]

    input_api.files = [MockFile(file_with_user_action,
                                contents_with_user_action)]

    output = PRESUBMIT._CheckUserActionUpdate(input_api, MockOutputApi())
    self.assertEqual(
      ('File %s line %d: %s is missing in '
       'tools/metrics/actions/actions.xml. Please run '
       'tools/metrics/actions/extract_actions.py to update.'
       % (file_with_user_action, 1, 'NotInActionsXml')),
      output[0].message)


class PydepsNeedsUpdatingTest(unittest.TestCase):

  class MockSubprocess(object):
    CalledProcessError = subprocess.CalledProcessError

  def setUp(self):
    mock_all_pydeps = ['A.pydeps', 'B.pydeps']
    self.old_ALL_PYDEPS_FILES = PRESUBMIT._ALL_PYDEPS_FILES
    PRESUBMIT._ALL_PYDEPS_FILES = mock_all_pydeps
    self.mock_input_api = MockInputApi()
    self.mock_output_api = MockOutputApi()
    self.mock_input_api.subprocess = PydepsNeedsUpdatingTest.MockSubprocess()
    self.checker = PRESUBMIT.PydepsChecker(self.mock_input_api, mock_all_pydeps)
    self.checker._file_cache = {
        'A.pydeps': '# Generated by:\n# CMD A\nA.py\nC.py\n',
        'B.pydeps': '# Generated by:\n# CMD B\nB.py\nC.py\n',
    }

  def tearDown(self):
    PRESUBMIT._ALL_PYDEPS_FILES = self.old_ALL_PYDEPS_FILES

  def _RunCheck(self):
    return PRESUBMIT._CheckPydepsNeedsUpdating(self.mock_input_api,
                                               self.mock_output_api,
                                               checker_for_tests=self.checker)

  def testAddedPydep(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('new.pydeps', [], action='A'),
    ]

    self.mock_input_api.CreateMockFileInPath(
        [x.LocalPath() for x in self.mock_input_api.AffectedFiles(
            include_deletes=True)])
    results = self._RunCheck()
    self.assertEqual(1, len(results))
    self.assertTrue('PYDEPS_FILES' in str(results[0]))

  def testPydepNotInSrc(self):
    self.mock_input_api.files = [
      MockAffectedFile('new.pydeps', [], action='A'),
    ]
    self.mock_input_api.CreateMockFileInPath([])
    results = self._RunCheck()
    self.assertEqual(0, len(results))

  def testRemovedPydep(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile(PRESUBMIT._ALL_PYDEPS_FILES[0], [], action='D'),
    ]
    self.mock_input_api.CreateMockFileInPath(
        [x.LocalPath() for x in self.mock_input_api.AffectedFiles(
            include_deletes=True)])
    results = self._RunCheck()
    self.assertEqual(1, len(results))
    self.assertTrue('PYDEPS_FILES' in str(results[0]))

  def testRandomPyIgnored(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('random.py', []),
    ]

    results = self._RunCheck()
    self.assertEqual(0, len(results), 'Unexpected results: %r' % results)

  def testRelevantPyNoChange(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('A.py', []),
    ]

    def mock_check_output(cmd, shell=False, env=None):
      self.assertEqual('CMD A --output ""', cmd)
      return self.checker._file_cache['A.pydeps']

    self.mock_input_api.subprocess.check_output = mock_check_output

    results = self._RunCheck()
    self.assertEqual(0, len(results), 'Unexpected results: %r' % results)

  def testRelevantPyOneChange(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('A.py', []),
    ]

    def mock_check_output(cmd, shell=False, env=None):
      self.assertEqual('CMD A --output ""', cmd)
      return 'changed data'

    self.mock_input_api.subprocess.check_output = mock_check_output

    results = self._RunCheck()
    self.assertEqual(1, len(results))
    self.assertTrue('File is stale' in str(results[0]))

  def testRelevantPyTwoChanges(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('C.py', []),
    ]

    def mock_check_output(cmd, shell=False, env=None):
      return 'changed data'

    self.mock_input_api.subprocess.check_output = mock_check_output

    results = self._RunCheck()
    self.assertEqual(2, len(results))
    self.assertTrue('File is stale' in str(results[0]))
    self.assertTrue('File is stale' in str(results[1]))


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
        ]),
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
        # Using a bad include guard in Blink is not accepted even if
        # it's an old file.
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
        ]),
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
      ]
    msgs = PRESUBMIT._CheckForIncludeGuards(
        mock_input_api, mock_output_api)
    expected_fail_count = 8
    self.assertEqual(expected_fail_count, len(msgs),
                     'Expected %d items, found %d: %s'
                     % (expected_fail_count, len(msgs), msgs))
    self.assertEqual(msgs[0].items, ['content/browser/thing/bar.h'])
    self.assertEqual(msgs[0].message,
                     'Include guard CONTENT_BROWSER_THING_BAR_H_ '
                     'not covering the whole file')

    self.assertEqual(msgs[1].items, ['content/browser/test1.h'])
    self.assertEqual(msgs[1].message,
                     'Missing include guard CONTENT_BROWSER_TEST1_H_')

    self.assertEqual(msgs[2].items, ['content/browser/test2.h:3'])
    self.assertEqual(msgs[2].message,
                     'Missing "#define CONTENT_BROWSER_TEST2_H_" for '
                     'include guard')

    self.assertEqual(msgs[3].items, ['content/browser/spleling.h:1'])
    self.assertEqual(msgs[3].message,
                     'Header using the wrong include guard name '
                     'CONTENT_BROWSER_SPLLEING_H_')

    self.assertEqual(msgs[4].items, ['content/browser/foo+bar.h'])
    self.assertEqual(msgs[4].message,
                     'Missing include guard CONTENT_BROWSER_FOO_BAR_H_')

    self.assertEqual(msgs[5].items, ['content/NotInBlink.h:1'])
    self.assertEqual(msgs[5].message,
                     'Header using the wrong include guard name '
                     'NotInBlink_h')

    self.assertEqual(msgs[6].items, ['third_party/blink/InBlink.h:1'])
    self.assertEqual(msgs[6].message,
                     'Header using the wrong include guard name '
                     'InBlink_h')

    self.assertEqual(msgs[7].items, ['third_party/blink/AlsoInBlink.h:1'])
    self.assertEqual(msgs[7].message,
                     'Header using the wrong include guard name '
                     'WrongInBlink_h')


class AndroidDeprecatedTestAnnotationTest(unittest.TestCase):
  def testCheckAndroidTestAnnotationUsage(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
        MockAffectedFile('LalaLand.java', [
          'random stuff'
        ]),
        MockAffectedFile('CorrectUsage.java', [
          'import android.support.test.filters.LargeTest;',
          'import android.support.test.filters.MediumTest;',
          'import android.support.test.filters.SmallTest;',
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
    self.assertEqual(1, len(msgs),
                     'Expected %d items, found %d: %s'
                     % (1, len(msgs), msgs))
    self.assertEqual(4, len(msgs[0].items),
                     'Expected %d items, found %d: %s'
                     % (4, len(msgs[0].items), msgs[0].items))
    self.assertTrue('UsedDeprecatedLargeTestAnnotation.java:1' in msgs[0].items,
                    'UsedDeprecatedLargeTestAnnotation not found in errors')
    self.assertTrue('UsedDeprecatedMediumTestAnnotation.java:1'
                    in msgs[0].items,
                    'UsedDeprecatedMediumTestAnnotation not found in errors')
    self.assertTrue('UsedDeprecatedSmallTestAnnotation.java:1' in msgs[0].items,
                    'UsedDeprecatedSmallTestAnnotation not found in errors')
    self.assertTrue('UsedDeprecatedSmokeAnnotation.java:1' in msgs[0].items,
                    'UsedDeprecatedSmokeAnnotation not found in errors')


class AndroidDeprecatedJUnitFrameworkTest(unittest.TestCase):
  def testCheckAndroidTestJUnitFramework(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
        MockAffectedFile('LalaLand.java', [
          'random stuff'
        ]),
        MockAffectedFile('CorrectUsage.java', [
          'import org.junit.ABC',
          'import org.junit.XYZ;',
        ]),
        MockAffectedFile('UsedDeprecatedJUnit.java', [
          'import junit.framework.*;',
        ]),
        MockAffectedFile('UsedDeprecatedJUnitAssert.java', [
          'import junit.framework.Assert;',
        ]),
    ]
    msgs = PRESUBMIT._CheckAndroidTestJUnitFrameworkImport(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(msgs),
                     'Expected %d items, found %d: %s'
                     % (1, len(msgs), msgs))
    self.assertEqual(2, len(msgs[0].items),
                     'Expected %d items, found %d: %s'
                     % (2, len(msgs[0].items), msgs[0].items))
    self.assertTrue('UsedDeprecatedJUnit.java:1' in msgs[0].items,
                    'UsedDeprecatedJUnit.java not found in errors')
    self.assertTrue('UsedDeprecatedJUnitAssert.java:1'
                    in msgs[0].items,
                    'UsedDeprecatedJUnitAssert not found in errors')


class AndroidJUnitBaseClassTest(unittest.TestCase):
  def testCheckAndroidTestJUnitBaseClass(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
        MockAffectedFile('LalaLand.java', [
          'random stuff'
        ]),
        MockAffectedFile('CorrectTest.java', [
          '@RunWith(ABC.class);'
          'public class CorrectTest {',
          '}',
        ]),
        MockAffectedFile('HistoricallyIncorrectTest.java', [
          'public class Test extends BaseCaseA {',
          '}',
        ], old_contents=[
          'public class Test extends BaseCaseB {',
          '}',
        ]),
        MockAffectedFile('CorrectTestWithInterface.java', [
          '@RunWith(ABC.class);'
          'public class CorrectTest implement Interface {',
          '}',
        ]),
        MockAffectedFile('IncorrectTest.java', [
          'public class IncorrectTest extends TestCase {',
          '}',
        ]),
        MockAffectedFile('IncorrectWithInterfaceTest.java', [
          'public class Test implements X extends BaseClass {',
          '}',
        ]),
        MockAffectedFile('IncorrectMultiLineTest.java', [
          'public class Test implements X, Y, Z',
          '        extends TestBase {',
          '}',
        ]),
    ]
    msgs = PRESUBMIT._CheckAndroidTestJUnitInheritance(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(msgs),
                     'Expected %d items, found %d: %s'
                     % (1, len(msgs), msgs))
    self.assertEqual(3, len(msgs[0].items),
                     'Expected %d items, found %d: %s'
                     % (3, len(msgs[0].items), msgs[0].items))
    self.assertTrue('IncorrectTest.java:1' in msgs[0].items,
                    'IncorrectTest not found in errors')
    self.assertTrue('IncorrectWithInterfaceTest.java:1'
                    in msgs[0].items,
                    'IncorrectWithInterfaceTest not found in errors')
    self.assertTrue('IncorrectMultiLineTest.java:2' in msgs[0].items,
                    'IncorrectMultiLineTest not found in errors')

class AndroidDebuggableBuildTest(unittest.TestCase):

  def testCheckAndroidDebuggableBuild(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
      MockAffectedFile('RandomStuff.java', [
        'random stuff'
      ]),
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

    msgs = PRESUBMIT._CheckAndroidDebuggableBuild(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(msgs),
                     'Expected %d items, found %d: %s'
                     % (1, len(msgs), msgs))
    self.assertEqual(4, len(msgs[0].items),
                     'Expected %d items, found %d: %s'
                     % (4, len(msgs[0].items), msgs[0].items))
    self.assertTrue('JustCheckUserdebugBuild.java:3' in msgs[0].items)
    self.assertTrue('JustCheckEngineeringBuild.java:3' in msgs[0].items)
    self.assertTrue('UsedBuildType.java:3' in msgs[0].items)
    self.assertTrue('UsedExplicitBuildType.java:2' in msgs[0].items)


class LogUsageTest(unittest.TestCase):

  def testCheckAndroidCrLogUsage(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
      MockAffectedFile('RandomStuff.java', [
        'random stuff'
      ]),
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
      MockAffectedFile('HasUnprefixedTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "rubbish";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasTooLongTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "21_charachers_long___";',
        'Log.d(TAG, "foo");',
      ]),
    ]

    msgs = PRESUBMIT._CheckAndroidCrLogUsage(
        mock_input_api, mock_output_api)

    self.assertEqual(5, len(msgs),
                     'Expected %d items, found %d: %s' % (5, len(msgs), msgs))

    # Declaration format
    nb = len(msgs[0].items)
    self.assertEqual(2, nb,
                     'Expected %d items, found %d: %s' % (2, nb, msgs[0].items))
    self.assertTrue('HasNoTagDecl.java' in msgs[0].items)
    self.assertTrue('HasIncorrectTagDecl.java' in msgs[0].items)

    # Tag length
    nb = len(msgs[1].items)
    self.assertEqual(1, nb,
                     'Expected %d items, found %d: %s' % (1, nb, msgs[1].items))
    self.assertTrue('HasTooLongTag.java' in msgs[1].items)

    # Tag must be a variable named TAG
    nb = len(msgs[2].items)
    self.assertEqual(1, nb,
                     'Expected %d items, found %d: %s' % (1, nb, msgs[2].items))
    self.assertTrue('HasInlineTag.java:4' in msgs[2].items)

    # Util Log usage
    nb = len(msgs[3].items)
    self.assertEqual(2, nb,
                     'Expected %d items, found %d: %s' % (2, nb, msgs[3].items))
    self.assertTrue('HasAndroidLog.java:3' in msgs[3].items)
    self.assertTrue('IsInBasePackageButImportsLog.java:4' in msgs[3].items)

    # Tag must not contain
    nb = len(msgs[4].items)
    self.assertEqual(3, nb,
                     'Expected %d items, found %d: %s' % (2, nb, msgs[4].items))
    self.assertTrue('HasDottedTag.java' in msgs[4].items)
    self.assertTrue('HasDottedTagPublic.java' in msgs[4].items)
    self.assertTrue('HasOldTag.java' in msgs[4].items)


class GoogleAnswerUrlFormatTest(unittest.TestCase):

  def testCatchAnswerUrlId(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('somewhere/file.cc',
               ['char* host = '
                '  "https://support.google.com/chrome/answer/123456";']),
      MockFile('somewhere_else/file.cc',
               ['char* host = '
                '  "https://support.google.com/chrome/a/answer/123456";']),
    ]

    warnings = PRESUBMIT._CheckGoogleSupportAnswerUrl(
      input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(2, len(warnings[0].items))

  def testAllowAnswerUrlParam(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('somewhere/file.cc',
               ['char* host = '
                '  "https://support.google.com/chrome/?p=cpn_crash_reports";']),
    ]

    warnings = PRESUBMIT._CheckGoogleSupportAnswerUrl(
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

    warnings = PRESUBMIT._CheckHardcodedGoogleHostsInLowerLayers(
      input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(3, len(warnings[0].items))

  def testAllowInComment(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('content/file.cc',
               ['char* host = "https://www.aol.com"; // google.com'])
    ]

    warnings = PRESUBMIT._CheckHardcodedGoogleHostsInLowerLayers(
      input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))


class ChromeOsSyncedPrefRegistrationTest(unittest.TestCase):

  def testWarnsOnChromeOsDirectories(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('ash/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF']),
      MockFile('chrome/browser/chromeos/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF']),
      MockFile('chromeos/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF']),
      MockFile('components/arc/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF']),
      MockFile('components/exo/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF']),
    ]
    warnings = PRESUBMIT._CheckChromeOsSyncedPrefRegistration(
      input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))

  def testDoesNotWarnOnSyncOsPref(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('chromeos/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_OS_PREF']),
    ]
    warnings = PRESUBMIT._CheckChromeOsSyncedPrefRegistration(
      input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testDoesNotWarnOnCrossPlatformDirectories(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('chrome/browser/ui/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF']),
      MockFile('components/sync/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF']),
      MockFile('content/browser/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF']),
    ]
    warnings = PRESUBMIT._CheckChromeOsSyncedPrefRegistration(
      input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testSeparateWarningForPriorityPrefs(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('chromeos/file.cc',
               ['PrefRegistrySyncable::SYNCABLE_PREF',
                'PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF']),
    ]
    warnings = PRESUBMIT._CheckChromeOsSyncedPrefRegistration(
      input_api, MockOutputApi())
    self.assertEqual(2, len(warnings))


class ForwardDeclarationTest(unittest.TestCase):
  def testCheckHeadersOnlyOutsideThirdParty(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('somewhere/file.cc', [
        'class DummyClass;'
      ]),
      MockAffectedFile('third_party/header.h', [
        'class DummyClass;'
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
                                                          MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testNoNestedDeclaration(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('somewhere/header.h', [
        'class SomeClass {',
        ' protected:',
        '  class NotAMatch;',
        '};'
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
                                                          MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testSubStrings(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('somewhere/header.h', [
        'class NotUsefulClass;',
        'struct SomeStruct;',
        'UsefulClass *p1;',
        'SomeStructPtr *p2;'
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
                                                          MockOutputApi())
    self.assertEqual(2, len(warnings))

  def testUselessForwardDeclaration(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('somewhere/header.h', [
        'class DummyClass;',
        'struct DummyStruct;',
        'class UsefulClass;',
        'std::unique_ptr<UsefulClass> p;'
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
                                                          MockOutputApi())
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
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
                                                          MockOutputApi())
    self.assertEqual(4, len(warnings))


class RelativeIncludesTest(unittest.TestCase):
  def testThirdPartyNotWebKitIgnored(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('third_party/test.cpp', '#include "../header.h"'),
      MockAffectedFile('third_party/test/test.cpp', '#include "../header.h"'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testNonCppFileIgnored(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.py', '#include "../header.h"'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testInnocuousChangesAllowed(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.cpp', '#include "header.h"'),
      MockAffectedFile('test2.cpp', '../'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testRelativeIncludeNonWebKitProducesError(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.cpp', ['#include "../header.h"']),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))

  def testRelativeIncludeWebKitProducesError(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('third_party/blink/test.cpp',
                       ['#include "../header.h']),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))


class CCIncludeTest(unittest.TestCase):
  def testThirdPartyNotBlinkIgnored(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('third_party/test.cpp', '#include "file.cc"'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForCcIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testPythonFileIgnored(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.py', '#include "file.cc"'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForCcIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testIncFilesAccepted(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.py', '#include "file.inc"'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForCcIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testInnocuousChangesAllowed(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.cpp', '#include "header.h"'),
      MockAffectedFile('test2.cpp', 'Something "file.cc"'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForCcIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testCcIncludeNonBlinkProducesError(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.cpp', ['#include "file.cc"']),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForCcIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))

  def testCppIncludeBlinkProducesError(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('third_party/blink/test.cpp',
                       ['#include "foo/file.cpp"']),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForCcIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))


class NewHeaderWithoutGnChangeTest(unittest.TestCase):
  def testAddHeaderWithoutGn(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', ''),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertTrue('base/stuff.h' in warnings[0].items)

  def testModifyHeader(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', '', action='M'),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testDeleteHeader(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', '', action='D'),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testAddHeaderWithGn(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', ''),
      MockAffectedFile('base/BUILD.gn', 'stuff.h'),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testAddHeaderWithGni(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', ''),
      MockAffectedFile('base/files.gni', 'stuff.h'),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testAddHeaderWithOther(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', ''),
      MockAffectedFile('base/stuff.cc', 'stuff.h'),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))

  def testAddHeaderWithWrongGn(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', ''),
      MockAffectedFile('base/BUILD.gn', 'stuff_h'),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))

  def testAddHeadersWithGn(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', ''),
      MockAffectedFile('base/another.h', ''),
      MockAffectedFile('base/BUILD.gn', 'another.h\nstuff.h'),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testAddHeadersWithWrongGn(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('base/stuff.h', ''),
      MockAffectedFile('base/another.h', ''),
      MockAffectedFile('base/BUILD.gn', 'another_h\nstuff.h'),
    ]
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
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
    warnings = PRESUBMIT._CheckNewHeaderWithoutGnChange(
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
    warnings = PRESUBMIT._CheckCorrectProductNameInMessages(
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
    warnings = PRESUBMIT._CheckCorrectProductNameInMessages(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertTrue('chrome/app/chromium_strings.grd' in warnings[0].items[0])

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
    warnings = PRESUBMIT._CheckCorrectProductNameInMessages(
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
    warnings = PRESUBMIT._CheckCorrectProductNameInMessages(
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
      MockAffectedFile('components/components_google_chrome_strings.grd', [
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
    warnings = PRESUBMIT._CheckCorrectProductNameInMessages(
        mock_input_api, MockOutputApi())
    self.assertEqual(2, len(warnings))
    self.assertTrue(
        'components/components_google_chrome_strings.grd:5'
             in warnings[0].items[0])
    self.assertTrue(
        'chrome/app/chromium_strings.grd:2' in warnings[1].items[0])
    self.assertTrue(
        'chrome/app/chromium_strings.grd:8' in warnings[1].items[1])


class ServiceManifestOwnerTest(unittest.TestCase):
  def testServiceManifestJsonChangeNeedsSecurityOwner(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('services/goat/manifest.json',
                       [
                         '{',
                         '  "name": "teleporter",',
                         '  "display_name": "Goat Teleporter",'
                         '  "interface_provider_specs": {',
                         '  }',
                         '}',
                       ])
    ]
    mock_output_api = MockOutputApi()
    errors = PRESUBMIT._CheckIpcOwners(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))
    self.assertEqual(
        'Found OWNERS files that need to be updated for IPC security review ' +
        'coverage.\nPlease update the OWNERS files below:', errors[0].message)

    # No warning if already covered by an OWNERS rule.

  def testNonManifestJsonChangesDoNotRequireSecurityOwner(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('services/goat/species.json',
                       [
                         '[',
                         '  "anglo-nubian",',
                         '  "angora"',
                         ']',
                       ])
    ]
    mock_output_api = MockOutputApi()
    errors = PRESUBMIT._CheckIpcOwners(
        mock_input_api, mock_output_api)
    self.assertEqual([], errors)

  def testServiceManifestChangeNeedsSecurityOwner(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('services/goat/public/cpp/manifest.cc',
                       [
                         '#include "services/goat/public/cpp/manifest.h"',
                         'const service_manager::Manifest& GetManifest() {}',
                       ])]
    mock_output_api = MockOutputApi()
    errors = PRESUBMIT._CheckIpcOwners(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))
    self.assertEqual(
        'Found OWNERS files that need to be updated for IPC security review ' +
        'coverage.\nPlease update the OWNERS files below:', errors[0].message)

  def testNonServiceManifestSourceChangesDoNotRequireSecurityOwner(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('some/non/service/thing/foo_manifest.cc',
                       [
                         'const char kNoEnforcement[] = "not a manifest!";',
                       ])]
    mock_output_api = MockOutputApi()
    errors = PRESUBMIT._CheckIpcOwners(
        mock_input_api, mock_output_api)
    self.assertEqual([], errors)


class BannedTypeCheckTest(unittest.TestCase):

  def testBannedCppFunctions(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('some/cpp/problematic/file.cc',
               ['using namespace std;']),
      MockFile('some/cpp/ok/file.cc',
               ['using std::string;']),
    ]

    errors = PRESUBMIT._CheckNoBannedFunctions(input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertTrue('some/cpp/problematic/file.c' in errors[0].message)
    self.assertTrue('some/cpp/ok/file.cc' not in errors[0].message)

  def testBannedBlinkDowncastHelpers(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('some/cpp/problematic/file1.cc',
               ['DEFINE_TYPE_CASTS(ToType, FromType, from_argument,'
                'PointerPredicate(), ReferencePredicate());']),
      MockFile('some/cpp/problematic/file2.cc',
               ['bool is_test_ele = IsHTMLTestElement(n);']),
      MockFile('some/cpp/problematic/file3.cc',
               ['auto* html_test_ele = ToHTMLTestElement(n);']),
      MockFile('some/cpp/problematic/file4.cc',
               ['auto* html_test_ele_or_null = ToHTMLTestElementOrNull(n);']),
      MockFile('some/cpp/ok/file1.cc',
               ['bool is_test_ele = IsA<HTMLTestElement>(n);']),
      MockFile('some/cpp/ok/file2.cc',
               ['auto* html_test_ele = To<HTMLTestElement>(n);']),
      MockFile('some/cpp/ok/file3.cc',
               ['auto* html_test_ele_or_null = ',
                'DynamicTo<HTMLTestElement>(n);']),
    ]

    # warnings are errors[0], errors are errors[1]
    errors = PRESUBMIT._CheckNoBannedFunctions(input_api, MockOutputApi())
    self.assertEqual(2, len(errors))
    self.assertTrue('some/cpp/problematic/file1.cc' in errors[1].message)
    self.assertTrue('some/cpp/problematic/file2.cc' in errors[0].message)
    self.assertTrue('some/cpp/problematic/file3.cc' in errors[0].message)
    self.assertTrue('some/cpp/problematic/file4.cc' in errors[0].message)
    self.assertTrue('some/cpp/ok/file1.cc' not in errors[0].message)
    self.assertTrue('some/cpp/ok/file2.cc' not in errors[0].message)
    self.assertTrue('some/cpp/ok/file3.cc' not in errors[0].message)

  def testBannedIosObjcFunctions(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('some/ios/file.mm',
               ['TEST(SomeClassTest, SomeInteraction) {',
                '}']),
      MockFile('some/mac/file.mm',
               ['TEST(SomeClassTest, SomeInteraction) {',
                '}']),
      MockFile('another/ios_file.mm',
               ['class SomeTest : public testing::Test {};']),
      MockFile('some/ios/file_egtest.mm',
               ['- (void)testSomething { EXPECT_OCMOCK_VERIFY(aMock); }']),
      MockFile('some/ios/file_unittest.mm',
               ['TEST_F(SomeTest, TestThis) { EXPECT_OCMOCK_VERIFY(aMock); }']),
    ]

    errors = PRESUBMIT._CheckNoBannedFunctions(input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertTrue('some/ios/file.mm' in errors[0].message)
    self.assertTrue('another/ios_file.mm' in errors[0].message)
    self.assertTrue('some/mac/file.mm' not in errors[0].message)
    self.assertTrue('some/ios/file_egtest.mm' in errors[0].message)
    self.assertTrue('some/ios/file_unittest.mm' not in errors[0].message)

  def testBannedMojoFunctions(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('some/cpp/problematic/file.cc',
               ['mojo::DataPipe();']),
      MockFile('some/cpp/problematic/file2.cc',
               ['mojo::ConvertTo<>']),
      MockFile('some/cpp/ok/file.cc',
               ['CreateDataPipe();']),
      MockFile('some/cpp/ok/file2.cc',
               ['mojo::DataPipeDrainer();']),
      MockFile('third_party/blink/ok/file3.cc',
               ['mojo::ConvertTo<>']),
      MockFile('content/renderer/ok/file3.cc',
               ['mojo::ConvertTo<>']),
    ]

    results = PRESUBMIT._CheckNoBannedFunctions(input_api, MockOutputApi())

    # warnings are results[0], errors are results[1]
    self.assertEqual(2, len(results))
    self.assertTrue('some/cpp/problematic/file.cc' in results[1].message)
    self.assertTrue('some/cpp/problematic/file2.cc' in results[0].message)
    self.assertTrue('some/cpp/ok/file.cc' not in results[1].message)
    self.assertTrue('some/cpp/ok/file2.cc' not in results[1].message)
    self.assertTrue('third_party/blink/ok/file3.cc' not in results[0].message)
    self.assertTrue('content/renderer/ok/file3.cc' not in results[0].message)

  def testDeprecatedMojoTypes(self):
    ok_paths = ['some/cpp']
    warning_paths = ['third_party/blink']
    test_cases = [
      {
        'type': 'mojo::AssociatedBinding<>;',
        'file': 'file1.c'
      },
      {
        'type': 'mojo::AssociatedBindingSet<>;',
        'file': 'file2.c'
      },
      {
        'type': 'mojo::AssociatedInterfacePtr<>',
        'file': 'file3.cc'
      },
      {
        'type': 'mojo::AssociatedInterfacePtrInfo<>',
        'file': 'file4.cc'
      },
      {
        'type': 'mojo::AssociatedInterfaceRequest<>',
        'file': 'file5.cc'
      },
      {
        'type': 'mojo::Binding<>',
        'file': 'file6.cc'
      },
      {
        'type': 'mojo::BindingSet<>',
        'file': 'file7.cc'
      },
      {
        'type': 'mojo::InterfacePtr<>',
        'file': 'file8.cc'
      },
      {
        'type': 'mojo::InterfacePtrInfo<>',
        'file': 'file9.cc'
      },
      {
        'type': 'mojo::InterfaceRequest<>',
        'file': 'file10.cc'
      },
      {
        'type': 'mojo::MakeRequest()',
        'file': 'file11.cc'
      },
      {
        'type': 'mojo::MakeRequestAssociatedWithDedicatedPipe()',
        'file': 'file12.cc'
      },
      {
        'type': 'mojo::MakeStrongBinding()<>',
        'file': 'file13.cc'
      },
      {
        'type': 'mojo::MakeStrongAssociatedBinding()<>',
        'file': 'file14.cc'
      },
      {
        'type': 'mojo::StrongAssociatedBindingSet<>',
        'file': 'file15.cc'
      },
      {
        'type': 'mojo::StrongBindingSet<>',
        'file': 'file16.cc'
      },
    ]

    # Build the list of MockFiles considering paths that should trigger warnings
    # as well as paths that should not trigger anything.
    input_api = MockInputApi()
    input_api.files = []
    for test_case in test_cases:
      for path in ok_paths:
        input_api.files.append(MockFile(os.path.join(path, test_case['file']),
                                        [test_case['type']]))
      for path in warning_paths:
        input_api.files.append(MockFile(os.path.join(path, test_case['file']),
                                        [test_case['type']]))

    results = PRESUBMIT._CheckNoDeprecatedMojoTypes(input_api, MockOutputApi())

    # Only warnings for now for all deprecated Mojo types.
    self.assertEqual(1, len(results))

    for test_case in test_cases:
      # Check that no warnings or errors have been triggered for these paths.
      for path in ok_paths:
        self.assertFalse(path in results[0].message)

      # Check warnings have been triggered for these paths.
      for path in warning_paths:
        self.assertTrue(path in results[0].message)


class NoProductionCodeUsingTestOnlyFunctionsTest(unittest.TestCase):
  def testTruePositives(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', ['foo_for_testing();']),
      MockFile('some/path/foo.mm', ['FooForTesting();']),
      MockFile('some/path/foo.cxx', ['FooForTests();']),
      MockFile('some/path/foo.cpp', ['foo_for_test();']),
    ]

    results = PRESUBMIT._CheckNoProductionCodeUsingTestOnlyFunctions(
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
    ]

    results = PRESUBMIT._CheckNoProductionCodeUsingTestOnlyFunctions(
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

    results = PRESUBMIT._CheckNoProductionCodeUsingTestOnlyFunctionsJava(
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
      MockFile('dir/javatests/src/baz.java', ['FooForTest(', 'y', ');']),
      MockFile('dir/junit/src/baz.java', ['FooForTest(', 'y', ');']),
      MockFile('dir/junit/src/javadoc.java', [
        '/** Use FooForTest(); to obtain foo in tests.'
        ' */'
      ]),
      MockFile('dir/junit/src/javadoc2.java', [
        '/** ',
        ' * Use FooForTest(); to obtain foo in tests.'
        ' */'
      ]),
    ]

    results = PRESUBMIT._CheckNoProductionCodeUsingTestOnlyFunctionsJava(
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

    results = PRESUBMIT._CheckNewImagesWarning(mock_input_api, MockOutputApi())
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

    results = PRESUBMIT._CheckNewImagesWarning(mock_input_api, MockOutputApi())
    self.assertEqual(0, len(results))


class CheckUniquePtrTest(unittest.TestCase):
  def testTruePositivesNullptr(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('dir/baz.cc', ['std::unique_ptr<T>()']),
      MockFile('dir/baz-p.cc', ['std::unique_ptr<T<P>>()']),
    ]

    results = PRESUBMIT._CheckUniquePtr(mock_input_api, MockOutputApi())
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
          'auto p = std::unique_ptr<std::pair<T, D>>(new std::pair(T, D));']),
    ]

    results = PRESUBMIT._CheckUniquePtr(mock_input_api, MockOutputApi())
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
      MockFile('dir/baz.cc', [
        'std::unique_ptr<T> result = std::make_unique<T>();'
      ]),
      MockFile('dir/baz2.cc', [
        'std::unique_ptr<T> result = std::make_unique<T>('
      ]),
      MockFile('dir/nested.cc', ['set<std::unique_ptr<T>>();']),
      MockFile('dir/nested2.cc', ['map<U, std::unique_ptr<T>>();']),

      # Two-argument invocation of std::unique_ptr is exempt because there is
      # no equivalent using std::make_unique.
      MockFile('dir/multi_arg.cc', [
        'auto p = std::unique_ptr<T, D>(new T(), D());']),
    ]

    results = PRESUBMIT._CheckUniquePtr(mock_input_api, MockOutputApi())
    self.assertEqual(0, len(results))

class CheckNoDirectIncludesHeadersWhichRedefineStrCat(unittest.TestCase):
  def testBlocksDirectIncludes(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('dir/foo_win.cc', ['#include "shlwapi.h"']),
      MockFile('dir/bar.h', ['#include <propvarutil.h>']),
      MockFile('dir/baz.h', ['#include <atlbase.h>']),
      MockFile('dir/jumbo.h', ['#include "sphelper.h"']),
    ]
    results = PRESUBMIT._CheckNoStrCatRedefines(mock_input_api, MockOutputApi())
    self.assertEquals(1, len(results))
    self.assertEquals(4, len(results[0].items))
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
    results = PRESUBMIT._CheckNoStrCatRedefines(mock_input_api, MockOutputApi())
    self.assertEquals(0, len(results))

  def testAllowsToCreateWrapper(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('base/win/shlwapi.h', [
        '#include <shlwapi.h>',
        '#include "base/win/windows_defines.inc"']),
    ]
    results = PRESUBMIT._CheckNoStrCatRedefines(mock_input_api, MockOutputApi())
    self.assertEquals(0, len(results))

class TranslationScreenshotsTest(unittest.TestCase):
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
               </messages>
             </release>
           </grit>
        """.splitlines()

  DO_NOT_UPLOAD_PNG_MESSAGE = ('Do not include actual screenshots in the '
                               'changelist. Run '
                               'tools/translate/upload_screenshots.py to '
                               'upload them instead:')
  GENERATE_SIGNATURES_MESSAGE = ('You are adding or modifying UI strings.\n'
                                 'To ensure the best translations, take '
                                 'screenshots of the relevant UI '
                                 '(https://g.co/chrome/translation) and add '
                                 'these files to your changelist:')
  REMOVE_SIGNATURES_MESSAGE = ('You removed strings associated with these '
                               'files. Remove:')

  def makeInputApi(self, files):
    input_api = MockInputApi()
    input_api.files = files
    # Override os_path.exists because the presubmit uses the actual
    # os.path.exists.
    input_api.CreateMockFileInPath(
        [x.LocalPath() for x in input_api.AffectedFiles(include_deletes=True)])
    return input_api

  def testNoScreenshots(self):
    # CL modified and added messages, but didn't add any screenshots.
    input_api = self.makeInputApi([
      MockAffectedFile('test.grd', self.NEW_GRD_CONTENTS2,
                       self.OLD_GRD_CONTENTS, action='M')])
    warnings = PRESUBMIT._CheckTranslationScreenshots(input_api,
                                                      MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.GENERATE_SIGNATURES_MESSAGE, warnings[0].message)
    self.assertEqual([
        os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
        os.path.join('test_grd', 'IDS_TEST2.png.sha1')
    ], warnings[0].items)

    input_api = self.makeInputApi([
      MockAffectedFile('test.grd', self.NEW_GRD_CONTENTS2,
                       self.NEW_GRD_CONTENTS1, action='M')])
    warnings = PRESUBMIT._CheckTranslationScreenshots(input_api,
                                                      MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.GENERATE_SIGNATURES_MESSAGE, warnings[0].message)
    self.assertEqual([os.path.join('test_grd', 'IDS_TEST2.png.sha1')],
                     warnings[0].items)


  def testUnnecessaryScreenshots(self):
    # CL added a single message and added the png file, but not the sha1 file.
    input_api = self.makeInputApi([
        MockAffectedFile(
            'test.grd',
            self.NEW_GRD_CONTENTS1,
            self.OLD_GRD_CONTENTS,
            action='M'),
        MockAffectedFile(
            os.path.join('test_grd', 'IDS_TEST1.png'), 'binary', action='A')
    ])
    warnings = PRESUBMIT._CheckTranslationScreenshots(input_api,
                                                      MockOutputApi())
    self.assertEqual(2, len(warnings))
    self.assertEqual(self.DO_NOT_UPLOAD_PNG_MESSAGE, warnings[0].message)
    self.assertEqual([os.path.join('test_grd', 'IDS_TEST1.png')],
                     warnings[0].items)
    self.assertEqual(self.GENERATE_SIGNATURES_MESSAGE, warnings[1].message)
    self.assertEqual([os.path.join('test_grd', 'IDS_TEST1.png.sha1')],
                     warnings[1].items)

    # CL added two messages, one has a png. Expect two messages:
    # - One for the unnecessary png.
    # - Another one for missing .sha1 files.
    input_api = self.makeInputApi([
        MockAffectedFile(
            'test.grd',
            self.NEW_GRD_CONTENTS2,
            self.OLD_GRD_CONTENTS,
            action='M'),
        MockAffectedFile(
            os.path.join('test_grd', 'IDS_TEST1.png'), 'binary', action='A')
    ])
    warnings = PRESUBMIT._CheckTranslationScreenshots(input_api,
                                                      MockOutputApi())
    self.assertEqual(2, len(warnings))
    self.assertEqual(self.DO_NOT_UPLOAD_PNG_MESSAGE, warnings[0].message)
    self.assertEqual([os.path.join('test_grd', 'IDS_TEST1.png')],
                     warnings[0].items)
    self.assertEqual(self.GENERATE_SIGNATURES_MESSAGE, warnings[1].message)
    self.assertEqual([
        os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
        os.path.join('test_grd', 'IDS_TEST2.png.sha1')
    ], warnings[1].items)

  def testScreenshotsWithSha1(self):
    # CL added two messages and their corresponding .sha1 files. No warnings.
    input_api = self.makeInputApi([
        MockAffectedFile(
            'test.grd',
            self.NEW_GRD_CONTENTS2,
            self.OLD_GRD_CONTENTS,
            action='M'),
        MockFile(
            os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
            'binary',
            action='A'),
        MockFile(
            os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
            'binary',
            action='A')
    ])
    warnings = PRESUBMIT._CheckTranslationScreenshots(input_api,
                                                      MockOutputApi())
    self.assertEqual([], warnings)

  def testScreenshotsRemovedWithSha1(self):
    # Swap old contents with new contents, remove IDS_TEST1 and IDS_TEST2. The
    # sha1 files associated with the messages should also be removed by the CL.
    input_api = self.makeInputApi([
        MockAffectedFile(
            'test.grd',
            self.OLD_GRD_CONTENTS,
            self.NEW_GRD_CONTENTS2,
            action='M'),
        MockFile(os.path.join('test_grd', 'IDS_TEST1.png.sha1'), 'binary', ""),
        MockFile(os.path.join('test_grd', 'IDS_TEST2.png.sha1'), 'binary', "")
    ])
    warnings = PRESUBMIT._CheckTranslationScreenshots(input_api,
                                                      MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.REMOVE_SIGNATURES_MESSAGE, warnings[0].message)
    self.assertEqual([
        os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
        os.path.join('test_grd', 'IDS_TEST2.png.sha1')
    ], warnings[0].items)

    # Same as above, but this time one of the .sha1 files is removed.
    input_api = self.makeInputApi([
        MockAffectedFile(
            'test.grd',
            self.OLD_GRD_CONTENTS,
            self.NEW_GRD_CONTENTS2,
            action='M'),
        MockFile(os.path.join('test_grd', 'IDS_TEST1.png.sha1'), 'binary', ''),
        MockAffectedFile(
            os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
            '',
            'old_contents',
            action='D')
    ])
    warnings = PRESUBMIT._CheckTranslationScreenshots(input_api,
                                                      MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.REMOVE_SIGNATURES_MESSAGE, warnings[0].message)
    self.assertEqual([os.path.join('test_grd', 'IDS_TEST1.png.sha1')],
                     warnings[0].items)

    # Remove both sha1 files. No presubmit warnings.
    input_api = self.makeInputApi([
        MockAffectedFile(
            'test.grd',
            self.OLD_GRD_CONTENTS,
            self.NEW_GRD_CONTENTS2,
            action='M'),
        MockFile(
            os.path.join('test_grd', 'IDS_TEST1.png.sha1'),
            'binary',
            action='D'),
        MockFile(
            os.path.join('test_grd', 'IDS_TEST2.png.sha1'),
            'binary',
            action='D')
    ])
    warnings = PRESUBMIT._CheckTranslationScreenshots(input_api,
                                                      MockOutputApi())
    self.assertEqual([], warnings)


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

      results = PRESUBMIT._CheckNoDISABLETypoInTests(mock_input_api,
                                                     MockOutputApi())
      self.assertEqual(
          1,
          len(results),
          msg=('expected len(results) == 1 but got %d in test: %s' %
               (len(results), test)))
      self.assertTrue(
          'foo_unittest.cc' in results[0].message,
          msg=('expected foo_unittest.cc in message but got %s in test %s' %
               (results[0].message, test)))

  def testIngoreNotTestFiles(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('some/path/foo.cc', 'TEST_F(FoobarTest, DISABLE_Foo)'),
    ]

    results = PRESUBMIT._CheckNoDISABLETypoInTests(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(results))

  def testIngoreDeletedFiles(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('some/path/foo.cc', 'TEST_F(FoobarTest, Foo)', action='D'),
    ]

    results = PRESUBMIT._CheckNoDISABLETypoInTests(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(results))


class BuildtoolsRevisionsAreInSyncTest(unittest.TestCase):
  # TODO(crbug.com/941824): We need to make sure the entries in
  # //buildtools/DEPS are kept in sync with the entries in //DEPS
  # so that users of //buildtools in other projects get the same tooling
  # Chromium gets. If we ever fix the referenced bug and add 'includedeps'
  # support to gclient, we can eliminate the duplication and delete
  # these tests for the corresponding presubmit check.

  def _check(self, files):
    mock_input_api = MockInputApi()
    mock_input_api.files = []
    for fname, contents in files.items():
      mock_input_api.files.append(MockFile(fname, contents.splitlines()))
    return PRESUBMIT._CheckBuildtoolsRevisionsAreInSync(mock_input_api,
                                                        MockOutputApi())

  def testOneFileChangedButNotTheOther(self):
    results = self._check({
        "DEPS": "'libunwind_revision': 'onerev'",
    })
    self.assertNotEqual(results, [])

  def testNeitherFileChanged(self):
    results = self._check({
        "OWNERS": "foobar@example.com",
    })
    self.assertEqual(results, [])

  def testBothFilesChangedAndMatch(self):
    results = self._check({
        "DEPS": "'libunwind_revision': 'onerev'",
        "buildtools/DEPS": "'libunwind_revision': 'onerev'",
    })
    self.assertEqual(results, [])

  def testBothFilesWereChangedAndDontMatch(self):
    results = self._check({
        "DEPS": "'libunwind_revision': 'onerev'",
        "buildtools/DEPS": "'libunwind_revision': 'anotherrev'",
    })
    self.assertNotEqual(results, [])


class CheckFuzzTargetsTest(unittest.TestCase):

  def _check(self, files):
    mock_input_api = MockInputApi()
    mock_input_api.files = []
    for fname, contents in files.items():
      mock_input_api.files.append(MockFile(fname, contents.splitlines()))
    return PRESUBMIT._CheckFuzzTargets(mock_input_api, MockOutputApi())

  def testLibFuzzerSourcesIgnored(self):
    results = self._check({
        "third_party/lib/Fuzzer/FuzzerDriver.cpp": "LLVMFuzzerInitialize",
    })
    self.assertEqual(results, [])

  def testNonCodeFilesIgnored(self):
    results = self._check({
        "README.md": "LLVMFuzzerInitialize",
    })
    self.assertEqual(results, [])

  def testNoErrorHeaderPresent(self):
    results = self._check({
        "fuzzer.cc": (
            "#include \"testing/libfuzzer/libfuzzer_exports.h\"\n" +
            "LLVMFuzzerInitialize"
        )
    })
    self.assertEqual(results, [])

  def testErrorMissingHeader(self):
    results = self._check({
        "fuzzer.cc": "LLVMFuzzerInitialize"
    })
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].items, ['fuzzer.cc'])


class SetNoParentTest(unittest.TestCase):
  def testSetNoParentMissing(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('goat/OWNERS',
                       [
                         'set noparent',
                         'jochen@chromium.org',
                         'per-file *.json=set noparent',
                         'per-file *.json=jochen@chromium.org',
                       ])
    ]
    mock_output_api = MockOutputApi()
    errors = PRESUBMIT._CheckSetNoParent(mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))
    self.assertTrue('goat/OWNERS:1' in errors[0].long_text)
    self.assertTrue('goat/OWNERS:3' in errors[0].long_text)


  def testSetNoParentWithCorrectRule(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('goat/OWNERS',
                       [
                         'set noparent',
                         'file://ipc/SECURITY_OWNERS',
                         'per-file *.json=set noparent',
                         'per-file *.json=file://ipc/SECURITY_OWNERS',
                       ])
    ]
    mock_output_api = MockOutputApi()
    errors = PRESUBMIT._CheckSetNoParent(mock_input_api, mock_output_api)
    self.assertEqual([], errors)


if __name__ == '__main__':
  unittest.main()
