#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for java_flag_generator.py.

Each test copies its input files from test/data/<test name>/ to a temporary
directory, which becomes the source root (//), and runs the generator on them
the same way the java_flag_generator() GN template does. The generated Java
class is compared with a golden file, or the reported errors are checked.

Input files are checked in with an extra ".in" suffix (e.g. "foo.cc.in") so
that presubmit checks and formatters for C++ ignore them. The suffix is removed
when the files are copied.
"""

import difflib
import glob
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_DATA_DIR = os.path.join(_SCRIPT_DIR, 'data')
_DATA_SUFFIX = '.in'
_GOLDENS_DIR = os.path.join(_SCRIPT_DIR, 'golden')
_GENERATOR_PATH = os.path.normpath(
    os.path.join(_SCRIPT_DIR, os.pardir, 'java_flag_generator.py'))

# Set this environment variable in order to regenerate the golden text files.
_REBASELINE = os.environ.get('REBASELINE', '0') != '0'

# Like for GN actions, the generator runs in the build directory and gets paths
# relative to it. Files generated in $root_gen_dir are therefore under
# out/Test/gen/ in the data directory of a test.
_BUILD_DIR = 'out/Test'
_SOURCE_ROOT = '../..'
_ROOT_GEN_DIR = 'gen'
_OUTPUT_SRCJAR = 'gen/java_flags.srcjar'
_DEPFILE = 'gen/java_flags.d'

_CLASS_NAME = 'org.chromium.foo.SampleFeatures'
# Sources relative to the source root, and relative to the build directory as
# they appear in error messages.
_SOURCE = 'chrome/flags/test_feature_list.cc'
_SOURCE_PATH = '../../chrome/flags/test_feature_list.cc'
_OTHER_SOURCE = 'components/other/other_feature_list.cc'
_OTHER_SOURCE_PATH = '../../components/other/other_feature_list.cc'

_accessed_goldens = set()


class BaseTest(unittest.TestCase):

    def _copy_test_data(self, dest_dir):
        """Copies the files in test/data/<test name>/ to `dest_dir`.
        Removes _DATA_SUFFIX from the file names.
        """
        data_dir = os.path.join(_DATA_DIR, self._testMethodName)
        if not os.path.isdir(data_dir):
            self.fail(f'Test data directory does not exist: {data_dir}')
        for dirpath, _, filenames in os.walk(data_dir):
            for filename in filenames:
                src_path = os.path.join(dirpath, filename)
                if not filename.endswith(_DATA_SUFFIX):
                    self.fail(f'Test data file name must end with '
                              f'"{_DATA_SUFFIX}": {src_path}')
                rel_path = os.path.relpath(src_path, data_dir)
                dest_path = os.path.join(dest_dir,
                                         rel_path[:-len(_DATA_SUFFIX)])
                os.makedirs(os.path.dirname(dest_path), exist_ok=True)
                shutil.copyfile(src_path, dest_path)

    def _run_generator(self, sources, class_name, feature_map_class_name):
        """Runs the generator on a copy of the test's data directory.
        Returns the temporary build directory (which is deleted at the end of
        the test) and the completed process.
        """
        tdir = tempfile.TemporaryDirectory()
        self.addCleanup(tdir.cleanup)
        self._copy_test_data(tdir.name)
        build_dir = os.path.join(tdir.name, _BUILD_DIR)
        os.makedirs(build_dir, exist_ok=True)

        cmd = [
            sys.executable,
            _GENERATOR_PATH,
            '--class-name',
            class_name,
            '--source-root',
            _SOURCE_ROOT,
            '--root-gen-dir',
            _ROOT_GEN_DIR,
            '--output-srcjar',
            _OUTPUT_SRCJAR,
            '--depfile',
            _DEPFILE,
        ]
        if feature_map_class_name:
            cmd += ['--feature-map-class-name', feature_map_class_name]
        cmd += [f'{_SOURCE_ROOT}/{source}' for source in sources]
        result = subprocess.run(cmd,
                                cwd=build_dir,
                                capture_output=True,
                                check=False,
                                text=True)
        if 'Traceback' in result.stderr:
            self.fail(f'Generator crashed. Command: {shlex.join(cmd)}\n'
                      f'{result.stderr}')
        return build_dir, result

    def assert_golden_text_equals(self, generated_text, golden_file):
        """Compares generated text with golden/`golden_file`."""
        golden_path = os.path.join(_GOLDENS_DIR, golden_file)
        _accessed_goldens.add(golden_path)
        golden_text = None
        if os.path.exists(golden_path):
            with open(golden_path, encoding='utf-8') as f:
                golden_text = f.read()

        if _REBASELINE:
            if golden_text != generated_text:
                print('Updated', golden_path)
                os.makedirs(_GOLDENS_DIR, exist_ok=True)
                with open(golden_path, 'w', encoding='utf-8', newline='') as f:
                    f.write(generated_text)
            return

        if golden_text is None:
            self.fail(f'Golden file does not exist: {golden_path}\n'
                      f'Run with:\n    REBASELINE=1 {sys.argv[0]}\n'
                      'to generate it.')
        if golden_text != generated_text:
            diff = ''.join(
                difflib.unified_diff(golden_text.splitlines(keepends=True),
                                     generated_text.splitlines(keepends=True),
                                     fromfile=golden_path,
                                     tofile='generated'))
            self.fail(f'Golden text mismatch:\n{diff}\n'
                      f'Run with:\n    REBASELINE=1 {sys.argv[0]}\n'
                      'to regenerate the golden files.')

    def _parse_depfile(self, path):
        """Returns the output and the inputs listed in a depfile."""
        with open(path, encoding='utf-8') as f:
            output, inputs = f.read().split(':', 1)
        return output, inputs.replace('\\\n', ' ').split()

    def _test_generate(self,
                       *,
                       sources=(_SOURCE, ),
                       class_name=_CLASS_NAME,
                       feature_map_class_name=None):
        """Checks the generated class against the test's golden file.
        Returns the output and the inputs listed in the depfile.
        """
        build_dir, result = self._run_generator(sources, class_name,
                                                feature_map_class_name)
        self.assertEqual(0, result.returncode,
                         f'Generator failed:\n{result.stderr}')
        self.assertEqual('', result.stderr)

        java_path = class_name.replace('.', '/') + '.java'
        with zipfile.ZipFile(os.path.join(build_dir, _OUTPUT_SRCJAR)) as z:
            self.assertEqual([java_path], z.namelist())
            java_source = z.read(java_path).decode('utf-8')
        simple_name = class_name.rsplit('.', 1)[-1]
        self.assert_golden_text_equals(
            java_source, f'{self._testMethodName}-{simple_name}.java.golden')
        return self._parse_depfile(os.path.join(build_dir, _DEPFILE))

    def _test_error(self,
                    error_snippets=(),
                    *,
                    sources=(_SOURCE, ),
                    class_name=_CLASS_NAME,
                    feature_map_class_name=None):
        """Checks that the generator fails and reports `error_snippets`.
        Returns stderr.
        """
        build_dir, result = self._run_generator(sources, class_name,
                                                feature_map_class_name)
        self.assertEqual(
            1, result.returncode,
            f'Expected the generator to fail. stderr:\n{result.stderr}')
        for snippet in error_snippets:
            self.assertIn(snippet, result.stderr)
        self.assertFalse(
            os.path.exists(os.path.join(build_dir, _OUTPUT_SRCJAR)))
        self.assertFalse(os.path.exists(os.path.join(build_dir, _DEPFILE)))
        return result.stderr


@unittest.skipIf(os.name == 'nt', 'Not intended to work on Windows')
class Tests(BaseTest):

    def testDefinitionMacros(self):
        self._test_generate()

    def testDefinitionFileDiscovery(self):
        depfile = self._test_generate()
        # The depfile lists every file that was read.
        self.assertEqual((_OUTPUT_SRCJAR, [
            '../../chrome/flags/local_features.cc',
            '../../chrome/flags/test_feature_list.cc',
            '../../components/both/both_features.cc',
            '../../components/foo/foo_features.cc',
            '../../components/unused/unused_features.cc',
            '../../third_party/bar/bar_features.cc',
            '../../third_party/both/both_features.cc',
            'gen/third_party/generated/generated_features.cc',
        ]), depfile)

    def testNamespaceLookup(self):
        self._test_generate()

    def testMultipleSourcesWithSameFeatureMap(self):
        self._test_generate(sources=[_SOURCE, _OTHER_SOURCE])

    def testMultipleFeatureMaps(self):
        self._test_generate(sources=[
            _SOURCE, _OTHER_SOURCE, 'chrome/more/more_feature_list.cc'
        ])

    def testFeatureMapClassName(self):
        self._test_generate(
            feature_map_class_name='org.chromium.base.BaseFeatureMap')

    def testEmptyFeatureList(self):
        self._test_generate()

    def testError_invalidClassName(self):
        self._test_error(
            [
                'error: --class-name "SampleFeatures" is not a '
                'fully-qualified Java class name such as '
                '"org.chromium.foo.FooFeatures".'
            ],
            class_name='SampleFeatures')

    def testError_invalidFeatureMapClassName(self):
        # Nested classes are not supported.
        self._test_error(
            [
                'error: --feature-map-class-name "org.chromium.foo.Outer.Map" '
                'is not a fully-qualified Java class name such as '
                '"org.chromium.foo.FooFeatures".'
            ],
            feature_map_class_name='org.chromium.foo.Outer.Map')

    def testError_noFeatureMap(self):
        self._test_error([
            f'{_SOURCE_PATH}: error: Cannot determine the FeatureMap '
            'class: no JNI_<Name>_GetNativeMap function found. Please set '
            'feature_map_class_name in the java_flag_generator() target, '
            'e.g. feature_map_class_name = '
            '"org.chromium.foo.FooFeatureMap".'
        ])

    def testError_multipleFeatureMaps(self):
        self._test_error([
            f'{_SOURCE_PATH}: error: Cannot determine the FeatureMap '
            'class: found several JNI_<Name>_GetNativeMap functions '
            '(FirstFeatureMap, SecondFeatureMap). Set '
            'feature_map_class_name in the java_flag_generator() target, '
            'e.g. feature_map_class_name = '
            '"org.chromium.foo.FooFeatureMap".'
        ])

    def testError_reportsErrorsOfAllSources(self):
        self._test_error(
            [
                f'{_SOURCE_PATH}:2: error: Cannot find the definition of '
                '"kMissing".',
                f'{_OTHER_SOURCE_PATH}: error: Cannot find the definition of '
                'kFeaturesExposedToJava.',
            ],
            sources=[_SOURCE, _OTHER_SOURCE])

    def testError_missingDefinitions(self):
        stderr = self._test_error()
        self.assertEqual(
            f"""\
{_SOURCE_PATH}:10: error: Cannot find the definition of "kMissingFeature".
{_SOURCE_PATH}:11: error: Cannot find the definition of \
"bar::kOtherMissingFeature".
Searched these files:
    //chrome/flags/test_feature_list.cc
    //components/foo/foo_features.cc
    $root_gen_dir/third_party/generated/generated_features.cc
To fix, add a comment of the form
    // FEATURE_DEFINITION_FILE: //path/to/file.cc
to //chrome/flags/test_feature_list.cc, where file.cc contains the feature's \
BASE_FEATURE() definition.
If file.cc is a generated file, also add the GN target that generates it to
the `deps` of the java_flag_generator() target.
""", stderr)

    def testError_ambiguousDefinition(self):
        stderr = self._test_error()
        self.assertEqual(
            f"""\
{_SOURCE_PATH}:8: error: Cannot tell which definition "kFoo" refers to. \
Candidates:
    {_SOURCE_PATH}:2: a::kFoo -> FooInA
    {_SOURCE_PATH}:5: b::kFoo -> FooInB
""", stderr)

    def testError_conflictingDefinitions(self):
        stderr = self._test_error()
        self.assertEqual(
            f"""\
{_SOURCE_PATH}:7: error: "kFoo" has conflicting definitions:
    {_SOURCE_PATH}:3: foo::kFoo -> DesktopFoo
    {_SOURCE_PATH}:5: foo::kFoo -> MobileFoo
""", stderr)

    def testError_invalidFeatureNames(self):
        stderr = self._test_error()
        self.assertEqual(
            f"""\
{_SOURCE_PATH}:9: error: {_SOURCE_PATH}:3: The feature name of kNotALiteral \
must be a plain string literal, found "kName".
{_SOURCE_PATH}:10: error: {_SOURCE_PATH}:4: The feature name of kConcatenated \
must be a plain string literal, found ""Foo" "Bar"".
{_SOURCE_PATH}:11: error: {_SOURCE_PATH}:5: BASE_FEATURE(kTooManyArgs, ...) \
has 4 arguments; expected 2 or 3.
{_SOURCE_PATH}:12: error: {_SOURCE_PATH}:6: Cannot determine the feature name \
of kCustomMacro from macro DEFINE_CUSTOM_FEATURE, because it is passed a \
string literal.
""", stderr)

    def testError_definitionFileDoesNotExist(self):
        stderr = self._test_error()
        self.assertEqual(
            f"""\
{_SOURCE_PATH}:1: error: FEATURE_DEFINITION_FILE \
//third_party/missing/features.cc does not exist. Checked:
    ../../third_party/missing/features.cc
    gen/third_party/missing/features.cc
If the file is generated, add the GN target that generates it to the `deps` \
of the java_flag_generator() target.
""", stderr)

    def testError_duplicateDefinitionFile(self):
        self._test_error([
            f'{_SOURCE_PATH}:2: error: Duplicate FEATURE_DEFINITION_FILE '
            '//third_party/bar/bar_features.cc (first on line 1).'
        ])

    def testError_redundantDefinitionFiles(self):
        self._test_error([
            f'{_SOURCE_PATH}:2: error: FEATURE_DEFINITION_FILE '
            '//components/foo/foo_features.cc is redundant: it is already '
            'searched automatically. Remove this comment.',
            f'{_SOURCE_PATH}:3: error: FEATURE_DEFINITION_FILE '
            '//chrome/flags/test_feature_list.cc is redundant: it is '
            'already searched automatically. Remove this comment.',
        ])

    def testError_unusedDefinitionFile(self):
        self._test_error([
            f'{_SOURCE_PATH}:1: error: FEATURE_DEFINITION_FILE '
            '//third_party/bar/bar_features.cc is unused: no entry in '
            f'{_SOURCE_PATH} is defined there. Please remove this comment.'
        ])

    def testError_sameNestedClassName(self):
        self._test_error(
            [
                f'{_OTHER_SOURCE_PATH}:4: error: "b::kFoo" and "a::kFoo" '
                f'({_SOURCE_PATH}:4) both map to the nested class "Foo".'
            ],
            sources=[_SOURCE, _OTHER_SOURCE])

    def testError_sameFeatureName(self):
        self._test_error([
            f'{_SOURCE_PATH}:6: error: "kBar" and "kFoo" '
            f'({_SOURCE_PATH}:5) both have the feature name "Bar".'
        ])

    def testError_reservedNestedClassNames(self):
        stderr = self._test_error()
        self.assertEqual(
            f"""\
{_SOURCE_PATH}:7: error: Nested class "SampleFeatures" would have the same \
name as the generated class.
{_SOURCE_PATH}:8: error: Nested class "String" would shadow a type used by \
the generated class.
{_SOURCE_PATH}:9: error: Nested class "NullMarked" would shadow a type used \
by the generated class.
{_SOURCE_PATH}:10: error: Nested class "SampleFeatureMap" would shadow a type \
used by the generated class.
""", stderr)

    def testError_featureMapNamedLikeGeneratedClass(self):
        self._test_error(
            [
                'error: The generated class SampleFeatureMap has the same '
                'name as its FeatureMap class '
                'org.chromium.foo.SampleFeatureMap.'
            ],
            class_name='org.chromium.foo.SampleFeatureMap')

    def testEveryDataDirectoryHasATest(self):
        test_names = unittest.TestLoader().getTestCaseNames(type(self))
        self.assertEqual([],
                         sorted(set(os.listdir(_DATA_DIR)) - set(test_names)),
                         'Found test data directories without a test.')


def main():
    program = unittest.main(exit=False)
    result = program.result
    # Only remove obsolete goldens when every test ran and passed, since other
    # tests' goldens are not accessed otherwise.
    ran_all_tests = not any(not arg.startswith('-') for arg in sys.argv[1:])
    if (_REBASELINE and ran_all_tests and result.wasSuccessful()
            and not result.skipped):
        for path in glob.glob(os.path.join(_GOLDENS_DIR, '*.golden')):
            if path not in _accessed_goldens:
                print('Removing obsolete golden:', path)
                os.remove(path)
    sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
