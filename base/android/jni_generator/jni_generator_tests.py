#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for jni_generator.py.

This test suite contains various tests for the JNI generator.
It exercises the low-level parser all the way up to the
code generator and ensures the output matches a golden
file.
"""

import collections
import copy
import difflib
import inspect
import optparse
import os
import sys
import tempfile
import unittest
import jni_generator
import jni_registration_generator
import zipfile
from util import build_utils

_INCLUDES = ('base/android/jni_generator/jni_generator_helper.h')
_JAVA_SRC_DIR = os.path.join('java', 'src', 'org', 'chromium', 'example',
                             'jni_generator')

# Set this environment variable in order to regenerate the golden text
# files.
_REBASELINE_ENV = 'REBASELINE'


class JniGeneratorOptions(object):
  """The mock options object which is passed to the jni_generator.py script."""

  def __init__(self):
    self.namespace = None
    self.includes = _INCLUDES
    self.ptr_type = 'long'
    self.cpp = 'cpp'
    self.javap = build_utils.JAVAP_PATH
    self.enable_profiling = False
    self.use_proxy_hash = False
    self.enable_jni_multiplexing = False
    self.unchecked_exceptions = False
    self.split_name = None
    self.include_test_only = True
    self.package_prefix = None


class JniRegistrationGeneratorOptions(object):
  """The mock options object which is passed to the jni_generator.py script."""

  def __init__(self):
    self.sources_exclusions = []
    self.namespace = None
    self.enable_proxy_mocks = False
    self.require_mocks = False
    self.use_proxy_hash = False
    self.enable_jni_multiplexing = False
    self.manual_jni_registration = False
    self.include_test_only = False
    self.header_path = None
    self.module_name = ''
    self.package_prefix = None
    self.remove_uncalled_methods = False
    self.add_stubs_for_missing_native = False


class BaseTest(unittest.TestCase):

  def _TestEndToEndGeneration(self, input_java, options, golden):
    input_java_path = self._JoinScriptDir(
        os.path.join(_JAVA_SRC_DIR, input_java))
    with tempfile.TemporaryDirectory() as tdir:
      output_path = os.path.join(tdir, 'output.h')
      jni_generator.GenerateJNIHeader(input_java_path, output_path, options)
      with open(output_path, 'r') as f:
        contents = f.read()
      self.AssertGoldenTextEquals(contents, golden)

  def _TestEndToEndRegistration(self,
                                input_src_files,
                                options,
                                name_to_goldens,
                                src_files_for_asserts_and_stubs=None,
                                header_golden=None):
    with tempfile.TemporaryDirectory() as tdir:
      options.srcjar_path = os.path.join(tdir, 'srcjar.jar')
      if header_golden:
        options.header_path = os.path.join(tdir, 'header.h')

      input_java_paths = {
          self._JoinScriptDir(os.path.join(_JAVA_SRC_DIR, f))
          for f in input_src_files
      }

      if src_files_for_asserts_and_stubs:
        asserts_and_stubs_java_paths = {
            self._JoinScriptDir(os.path.join(_JAVA_SRC_DIR, f))
            for f in src_files_for_asserts_and_stubs
        }
      else:
        asserts_and_stubs_java_paths = input_java_paths

      jni_registration_generator._Generate(options, input_java_paths,
                                           asserts_and_stubs_java_paths)
      with zipfile.ZipFile(options.srcjar_path, 'r') as srcjar:
        for name in srcjar.namelist():
          self.assertTrue(
              name in name_to_goldens,
              f'Found {name} output, but not present in name_to_goldens map.')
          contents = srcjar.read(name).decode('utf-8')
          self.AssertGoldenTextEquals(contents, name_to_goldens[name])
      if header_golden:
        with open(options.header_path, 'r') as f:
          # Temp directory will cause some diffs each time we run if we don't
          # normalize.
          contents = f.read().replace(
              tdir.replace('/', '_').upper(), 'TEMP_DIR')
          self.AssertGoldenTextEquals(contents, header_golden)

  def _JoinScriptDir(self, path):
    script_dir = os.path.dirname(sys.argv[0])
    return os.path.join(script_dir, path)

  def _JoinGoldenPath(self, golden_file_name):
    return self._JoinScriptDir(os.path.join('golden', golden_file_name))

  def _ReadGoldenFile(self, golden_file_name):
    golden_file_name = self._JoinGoldenPath(golden_file_name)
    if not os.path.exists(golden_file_name):
      return None
    with open(golden_file_name, 'r') as f:
      return f.read()

  def AssertTextEquals(self, golden_text, generated_text):
    if not self.CompareText(golden_text, generated_text):
      self.fail('Golden text mismatch.')

  def CompareText(self, golden_text, generated_text):

    def FilterText(text):
      return [
          l.strip() for l in text.split('\n')
          if not l.startswith('// Copyright')
      ]

    stripped_golden = FilterText(golden_text)
    stripped_generated = FilterText(generated_text)
    if stripped_golden == stripped_generated:
      return True
    print(self.id())
    for line in difflib.context_diff(stripped_golden, stripped_generated):
      print(line)
    print('\n\nGenerated')
    print('=' * 80)
    print(generated_text)
    print('=' * 80)
    print('Run with:')
    print('REBASELINE=1', sys.argv[0])
    print('to regenerate the data files.')

  def AssertGoldenTextEquals(self, generated_text, golden_file):
    """Compares generated text with the corresponding golden_file

    It will instead compare the generated text with
    script_dir/golden/golden_file."""
    # This is the caller test method.
    caller = inspect.stack()[1][3]

    golden_text = self._ReadGoldenFile(golden_file)
    if os.environ.get(_REBASELINE_ENV):
      if golden_text != generated_text:
        with open(self._JoinGoldenPath(golden_file), 'w') as f:
          f.write(generated_text)
      return
    # golden_text is None if no file is found. Better to fail than in
    # AssertTextEquals so we can give a clearer message.
    if golden_text is None:
      self.fail(
          'Golden file %s does not exist.' % self._JoinGoldenPath(golden_file))
    self.AssertTextEquals(golden_text, generated_text)


@unittest.skipIf(os.name == 'nt', 'Not intended to work on Windows')
class Tests(BaseTest):
  def testNonProxy(self):
    self._TestEndToEndGeneration('SampleNonProxy.java', JniGeneratorOptions(),
                                 'SampleNonProxy_jni.h.golden')

  def testBirectionalNonProxy(self):
    self._TestEndToEndGeneration('SampleBidirectionalNonProxy.java',
                                 JniGeneratorOptions(),
                                 'SampleBidirectionalNonProxy_jni.h.golden')

  def testBidirectionalClass(self):
    self._TestEndToEndGeneration('SampleForTests.java', JniGeneratorOptions(),
                                 'SampleForTests_jni.h.golden')
    self._TestEndToEndRegistration(
        ['SampleForTests.java'], JniRegistrationGeneratorOptions(), {
            'org/chromium/base/natives/GEN_JNI.java':
            'SampleForTestsGenJni.java.golden'
        })

  def testFromClassFile(self):
    self._TestEndToEndGeneration('SampleNonProxy.class', JniGeneratorOptions(),
                                 'SampleNonProxy_class_file_jni.h.golden')

  def testUniqueAnnotations(self):
    self._TestEndToEndGeneration('SampleUniqueAnnotations.java',
                                 JniGeneratorOptions(),
                                 'SampleUniqueAnnotations_jni.h.golden')

  def testSplitNameExample(self):
    self._TestEndToEndGeneration('SampleForTests.java', JniGeneratorOptions(),
                                 'SampleForTestsWithSplit_jni.h.golden')

  def testEndToEndProxyHashed(self):
    input_java_files = ['SampleForAnnotationProcessor.java']
    options = JniRegistrationGeneratorOptions()
    options.use_proxy_hash = True
    name_to_goldens = {
        'org/chromium/base/natives/GEN_JNI.java':
        'SampleForAnnotationProcessor_proxy_GenJni.java.golden',
        'J/N.java': 'SampleForAnnotationProcessor_proxy_JN.java.golden'
    }
    self._TestEndToEndRegistration(input_java_files, options, name_to_goldens)

  def testEndToEndManualRegistration(self):
    input_java_files = ['SampleForAnnotationProcessor.java']
    options = JniRegistrationGeneratorOptions()
    options.manual_jni_registration = True
    name_to_goldens = {
        'org/chromium/base/natives/GEN_JNI.java':
        'SampleForAnnotationProcessorGenJni.golden'
    }
    self._TestEndToEndRegistration(
        input_java_files,
        options,
        name_to_goldens,
        header_golden='SampleForAnnotationProcessor_manual.h.golden')

  def testEndToEndProxyJniWithModules(self):
    input_java_files = [
        'SampleForAnnotationProcessor.java', 'SampleModule.java'
    ]
    options = JniRegistrationGeneratorOptions()
    options.use_proxy_hash = True
    options.module_name = 'module'
    name_to_goldens = {
        'org/chromium/base/natives/module_GEN_JNI.java':
        'SampleModuleGenJni.golden',
        'J/module_N.java': 'SampleModuleJN.golden'
    }
    self._TestEndToEndRegistration(input_java_files, options, name_to_goldens)

  def testStubRegistration(self):
    input_java_files = ['SampleForAnnotationProcessor.java']
    stubs_java_files = input_java_files + [
        'TinySample.java', 'SampleProxyEdgeCases.java'
    ]
    extra_input_java_files = ['TinySample2.java']
    options = JniRegistrationGeneratorOptions()
    options.add_stubs_for_missing_native = True
    options.remove_uncalled_methods = True
    name_to_goldens = {
        'org/chromium/base/natives/GEN_JNI.java': 'StubGenJni.golden',
        'J/N.java': 'HashedSampleForAnnotationProcessorGenJni.golden'
    }
    self._TestEndToEndRegistration(
        input_java_files + extra_input_java_files,
        options,
        name_to_goldens,
        src_files_for_asserts_and_stubs=stubs_java_files)

  def testForTestingKept(self):
    input_java_file = 'SampleProxyEdgeCases.java'
    gen_options = JniGeneratorOptions()
    gen_options.include_test_only = True
    self._TestEndToEndGeneration(
        input_java_file, gen_options,
        'SampleForProxyEdgeCases_test_kept_jni.h.golden')
    options = JniRegistrationGeneratorOptions()
    options.use_proxy_hash = True
    options.include_test_only = True
    name_to_goldens = {
        'org/chromium/base/natives/GEN_JNI.java':
        'SampleForProxyEdgeCases_test_kept_GenJni.java.golden',
        'J/N.java': 'SampleForProxyEdgeCases_test_kept_JN.java.golden'
    }
    self._TestEndToEndRegistration([input_java_file], options, name_to_goldens)

  def testForTestingRemoved(self):
    input_java_file = 'SampleProxyEdgeCases.java'
    gen_options = JniGeneratorOptions()
    gen_options.include_test_only = False
    self._TestEndToEndGeneration(
        input_java_file, gen_options,
        'SampleForProxyEdgeCases_test_removed_jni.h.golden')
    options = JniRegistrationGeneratorOptions()
    options.use_proxy_hash = True
    options.include_test_only = False
    name_to_goldens = {
        'org/chromium/base/natives/GEN_JNI.java':
        'SampleForProxyEdgeCases_test_removed_GenJni.java.golden',
        'J/N.java': 'SampleForProxyEdgeCases_test_removed_JN.java.golden'
    }
    self._TestEndToEndRegistration([input_java_file], options, name_to_goldens)

  def testProxyMocks(self):
    input_java_files = ['TinySample.java']
    options = JniRegistrationGeneratorOptions()
    options.enable_proxy_mocks = True
    name_to_goldens = {
        'org/chromium/base/natives/GEN_JNI.java':
        'TinySample_enable_mocks_GenJni.java.golden',
    }
    self._TestEndToEndRegistration(input_java_files, options, name_to_goldens)

  def testRequireProxyMocks(self):
    input_java_files = ['TinySample.java']
    options = JniRegistrationGeneratorOptions()
    options.enable_proxy_mocks = True
    options.require_mocks = True
    name_to_goldens = {
        'org/chromium/base/natives/GEN_JNI.java':
        'TinySample_require_mocks_GenJni.java.golden',
    }
    self._TestEndToEndRegistration(input_java_files, options, name_to_goldens)

  def testPackagePrefixGenerator(self):
    options = JniGeneratorOptions()
    options.package_prefix = 'this.is.a.package.prefix'
    self._TestEndToEndGeneration('SampleForTests.java', options,
                                 'SampleForTests_package_prefix_jni.h.golden')

  def testPackagePrefixWithManualRegistration(self):
    input_java_files = ['SampleForAnnotationProcessor.java']
    options = JniRegistrationGeneratorOptions()
    options.package_prefix = 'this.is.a.package.prefix'
    options.manual_jni_registration = True
    name_to_goldens = {
        'this/is/a/package/prefix/org/chromium/base/natives/GEN_JNI.java':
        'SampleForAnnotationProcessor_package_prefix_manual_GenJni.java.golden',
    }
    self._TestEndToEndRegistration(
        input_java_files,
        options,
        name_to_goldens,
        header_golden=
        'SampleForAnnotationProcessor_package_prefix_manual.h.golden')

  def testPackagePrefixWithProxyHash(self):
    input_java_files = ['SampleForAnnotationProcessor.java']
    options = JniRegistrationGeneratorOptions()
    options.package_prefix = 'this.is.a.package.prefix'
    options.use_proxy_hash = True
    name_to_goldens = {
        'this/is/a/package/prefix/org/chromium/base/natives/GEN_JNI.java':
        'SampleForAnnotationProcessor_package_prefix_proxy_GenJni.java.golden',
        'this/is/a/package/prefix/J/N.java':
        'SampleForAnnotationProcessor_package_prefix_proxy_JN.java.golden',
    }
    self._TestEndToEndRegistration(input_java_files, options, name_to_goldens)

  def testPackagePrefixWithManualRegistrationWithProxyHash(self):
    input_java_files = ['SampleForAnnotationProcessor.java']
    options = JniRegistrationGeneratorOptions()
    options.package_prefix = 'this.is.a.package.prefix'
    options.use_proxy_hash = True
    options.manual_jni_registration = True
    name_to_goldens = {
        'this/is/a/package/prefix/org/chromium/base/natives/GEN_JNI.java':
        'SampleForAnnotationProcessor_package_prefix_proxy_manual_GenJni.java.golden',
        'this/is/a/package/prefix/J/N.java':
        'SampleForAnnotationProcessor_package_prefix_proxy_manual_JN.java.golden',
    }
    self._TestEndToEndRegistration(
        input_java_files,
        options,
        name_to_goldens,
        header_golden=
        'SampleForAnnotationProcessor_package_prefix_proxy_manual.h.golden')

  def testMultiplexing(self):
    input_java_files = ['SampleForAnnotationProcessor.java']
    options = JniRegistrationGeneratorOptions()
    options.enable_jni_multiplexing = True
    options.use_proxy_hash = True
    name_to_goldens = {
        'org/chromium/base/natives/GEN_JNI.java':
        'SampleForAnnotationProcessor_multiplexing_GenJni.java.golden',
        'J/N.java': 'SampleForAnnotationProcessor_multiplexing_JN.java.golden',
    }
    self._TestEndToEndRegistration(
        input_java_files,
        options,
        name_to_goldens,
        header_golden='SampleForAnnotationProcessor_multiplexing.h.golden')


def TouchStamp(stamp_path):
  dir_name = os.path.dirname(stamp_path)
  if not os.path.isdir(dir_name):
    os.makedirs(dir_name)

  with open(stamp_path, 'a'):
    os.utime(stamp_path, None)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--stamp', help='Path to touch on success.')
  parser.add_option(
      '-v', '--verbose', action='store_true', help='Whether to output details.')
  options, _ = parser.parse_args(argv[1:])

  test_result = unittest.main(
      argv=argv[0:1], exit=False, verbosity=(2 if options.verbose else 1))

  if test_result.result.wasSuccessful() and options.stamp:
    TouchStamp(options.stamp)

  return not test_result.result.wasSuccessful()


if __name__ == '__main__':
  sys.exit(main(sys.argv))
