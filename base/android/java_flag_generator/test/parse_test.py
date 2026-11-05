#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for cpp_lexer.py and cpp_parser.py."""

import os
import sys
import unittest

sys.path.insert(
    0, os.path.normpath(os.path.join(os.path.dirname(__file__), os.pardir))
)

import cpp_lexer
import cpp_parser
import models

_PATH = 'feature_list.cc'


def _parse_source(text):
    """Calls cpp_parser.parse_source() with the views that it expects."""
    _, structure = cpp_lexer.blank_preprocessor_lines(*cpp_lexer.lex(text))
    return cpp_parser.parse_source(_PATH, text, structure)


def _parse_feature_definitions(text):
    """Calls cpp_parser.parse_feature_definitions() with the views that it
    expects.
    """
    return cpp_parser.parse_feature_definitions(
        _PATH, *cpp_lexer.blank_preprocessor_lines(*cpp_lexer.lex(text))
    )


def _parse_param_definitions(text):
    """Calls cpp_parser.parse_param_definitions() with the views that it
    expects.
    """
    return cpp_parser.parse_param_definitions(
        _PATH, *cpp_lexer.blank_preprocessor_lines(*cpp_lexer.lex(text))
    )


class CppLexerTest(unittest.TestCase):
    def _blanked(self, text):
        """Returns the expected lexer output, where '~' marks a blanked char."""
        return text.replace('~', ' ')

    def _assert_lexed(self, text, code, structure=None):
        """Checks both views returned by lex(). `structure` defaults to `code`.
        In `code` and `structure`, '~' marks a character replaced by a space.
        """
        actual_code, actual_structure = cpp_lexer.lex(text)
        if structure is None:
            structure = code
        self.assertEqual(self._blanked(code), actual_code)
        self.assertEqual(self._blanked(structure), actual_structure)

    def testLineComment(self):
        self._assert_lexed('a; // b, (c\nd;', 'a; ~~~~~~~~\nd;')

    def testBlockComment(self):
        self._assert_lexed('a /* b,\n(c */ d', 'a ~~~~~\n~~~~~ d')

    def testUnterminatedBlockComment(self):
        self._assert_lexed('a /* b\nc', 'a ~~~~\n~')

    def testQuotesInComments(self):
        self._assert_lexed("// it's\n/* \" */ f();", '~~~~~~~\n~~~~~~~ f();')

    def testStringLiterals(self):
        self._assert_lexed(
            'f("a(b,c)", "/* d */", "// e");',
            # Code:
            'f("a(b,c)", "/* d */", "// e");',
            # Structure:
            'f("~~~~~~", "~~~~~~~", "~~~~");',
        )

    def testCharLiterals(self):
        self._assert_lexed(
            "f('(', '\"', ',');",
            # Code:
            "f('(', '\"', ',');",
            # Structure:
            "f('~', '~', '~');",
        )

    def testEscapedQuotes(self):
        self._assert_lexed(
            r'f("a\")", b);',
            # Code:
            r'f("a\")", b);',
            # Structure:
            r'f("~~~~", b);',
        )
        self._assert_lexed(
            r"g('\'', c);",
            # Code:
            r"g('\'', c);",
            # Structure:
            r"g('~~', c);",
        )

    def testUnterminatedStringEndsAtNewline(self):
        self._assert_lexed(
            '"a(b\nf(c)',
            # Code:
            '"a(b\nf(c)',
            # Structure:
            '"~~~\nf(c)',
        )

    def testDigitSeparators(self):
        self._assert_lexed(
            "x = 1'000'000 + 0x1'FF + 'a';",
            # Code:
            "x = 1'000'000 + 0x1'FF + 'a';",
            # Structure:
            "x = 1'000'000 + 0x1'FF + '~';",
        )

    def testRawStrings(self):
        self._assert_lexed(
            'R"(a "b" (c)";',
            # Code:
            'R"(a "b" (c)";',
            # Structure:
            'R"(~~~~~~~~)";',
        )
        # The delimiter allows )" within the string.
        self._assert_lexed(
            'R"x()")x";',
            # Code:
            'R"x()")x";',
            # Structure:
            'R"x(~~)x";',
        )
        self._assert_lexed(
            'u8R"(a)" LR"(b)" uR"(c)" UR"(d)"',
            # Code:
            'u8R"(a)" LR"(b)" uR"(c)" UR"(d)"',
            # Structure:
            'u8R"(~)" LR"(~)" uR"(~)" UR"(~)"',
        )

    def testIdentifierEndingInRIsNotRawStringPrefix(self):
        self._assert_lexed(
            'xR"(a)"',
            # Code:
            'xR"(a)"',
            # Structure:
            'xR"~~~"',
        )

    def testFindMatching(self):
        structure = 'f(a[b{c}], (d))'
        self.assertEqual(14, cpp_lexer.find_matching(structure, 1))
        self.assertEqual(8, cpp_lexer.find_matching(structure, 3))
        self.assertEqual(7, cpp_lexer.find_matching(structure, 5))
        self.assertEqual(13, cpp_lexer.find_matching(structure, 11))

    def testFindMatchingErrors(self):
        with self.assertRaisesRegex(ValueError, 'Unbalanced "]"'):
            cpp_lexer.find_matching('(a]', 0)
        with self.assertRaisesRegex(ValueError, 'Unterminated bracket'):
            cpp_lexer.find_matching('(a', 0)

    def testSplitTopLevel(self):
        structure = 'a, f(b, c), {d, e}, [f, g],'
        ranges = cpp_lexer.split_top_level(structure, 0, len(structure))
        self.assertEqual(
            ['a', ' f(b, c)', ' {d, e}', ' [f, g]', ''],
            [structure[start:end] for start, end in ranges],
        )

    def testSplitTopLevelWithinRange(self):
        structure = 'f(x, g(y, z))'
        ranges = cpp_lexer.split_top_level(structure, 2, 12)
        self.assertEqual(
            ['x', ' g(y, z)'], [structure[start:end] for start, end in ranges]
        )

    def testBlankPreprocessorLines(self):
        code, structure = cpp_lexer.blank_preprocessor_lines(
            *cpp_lexer.lex(
                '#include "a.h"\n'
                'a;\n'
                '  # define M(x) \\\n'
                '    (x)\n'
                'b;\n'
                'R"(\n'
                '#not_a_directive\n'
                ')";\n'
            )
        )
        self.assertEqual(
            self._blanked(
                '~~~~~~~~~~~~~~\n'
                'a;\n'
                '~~~~~~~~~~~~~~~~~\n'
                '~~~~~~~\n'
                'b;\n'
                'R"(\n'
                '#not_a_directive\n'
                ')";\n'
            ),
            code,
        )
        self.assertEqual(
            self._blanked(
                '~~~~~~~~~~~~~~\n'
                'a;\n'
                '~~~~~~~~~~~~~~~~~\n'
                '~~~~~~~\n'
                'b;\n'
                'R"(\n'
                '~~~~~~~~~~~~~~~~\n'
                ')";\n'
            ),
            structure,
        )


class ParseSourceTest(unittest.TestCase):
    def _entries(self, source):
        return [
            (entry.line, entry.written)
            for entry in source.feature_array.entries
        ]

    def _assert_error(self, expected_messages, text):
        with self.assertRaises(models.GeneratorError) as cm:
            _parse_source(text)
        self.assertEqual(expected_messages, cm.exception.messages)

    def testParseSource(self):
        text = """\
#include "chrome/flags/feature_list.h"

#include <string>

#include "base/feature_list.h"
// #include "commented/out.h"

// FEATURE_DEFINITION_FILE: //third_party/foo/features.cc
//FEATURE_DEFINITION_FILE://third_party/bar/features.cc
Foo();  // FEATURE_DEFINITION_FILE: //not/on/its/own/line.cc

namespace chrome {
namespace android {
namespace {

const base::Feature* const kFeaturesExposedToJava[] = {
    &kLocal,
    &features::kQualified,
    &::kAbsolute,
};

}  // namespace

static int64_t JNI_ChromeFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

}  // namespace android
}  // namespace chrome
"""
        self.assertEqual(
            models.ParsedSource(
                path=_PATH,
                includes=('chrome/flags/feature_list.h', 'base/feature_list.h'),
                directives=(
                    models.DefinitionDirective(
                        _PATH, 8, '//third_party/foo/features.cc'
                    ),
                    models.DefinitionDirective(
                        _PATH, 9, '//third_party/bar/features.cc'
                    ),
                ),
                feature_map_names=('ChromeFeatureMap',),
                feature_array=models.Array(
                    param_type=None,
                    namespace=('chrome', 'android'),
                    entries=(
                        models.ArrayEntry(_PATH, 17, '', 'kLocal'),
                        models.ArrayEntry(
                            _PATH, 18, 'features::', 'kQualified'
                        ),
                        models.ArrayEntry(_PATH, 19, '::', 'kAbsolute'),
                    ),
                ),
            ),
            _parse_source(text),
        )

    def testArrayDeclarations(self):
        for declaration in (
            'const base::Feature* const kFeaturesExposedToJava[] = {',
            'const base::Feature* const kFeaturesExposedToJava[2] = {',
            'const base::Feature* const kFeaturesExposedToJava [ ] = {',
            'const std::array<const base::Feature*, 2> '
            'kFeaturesExposedToJava = {',
        ):
            with self.subTest(declaration=declaration):
                source = _parse_source(
                    declaration + '\n    &kFoo,\n    &kBar,\n};\n'
                )
                self.assertEqual(
                    [(2, 'kFoo'), (3, 'kBar')], self._entries(source)
                )

    def testIgnoresArrayNameInCommentsAndStrings(self):
        text = """\
// const base::Feature* const kFeaturesExposedToJava[] = {&kInComment};
/* kFeaturesExposedToJava[] = {&kInBlockComment}; */
const char kText[] = "kFeaturesExposedToJava[] = {&kInString}";
const base::Feature* const kFeaturesExposedToJava[] = {&kReal};
"""
        self.assertEqual([(4, 'kReal')], self._entries(_parse_source(text)))

    def testEntryFormatting(self):
        text = """\
const base::Feature* const kFeaturesExposedToJava[] = {
    // Comments, even with commas or &kInComment, are ignored.
    &kFirst,  // Trailing comment.
    & ns :: kSpaced,
    &ns::
        kWrapped,
    /* Block comment */ &kAfterBlockComment,
    &kLast,
};
"""
        self.assertEqual(
            [
                (3, 'kFirst'),
                (4, 'ns::kSpaced'),
                (5, 'ns::kWrapped'),
                (7, 'kAfterBlockComment'),
                (8, 'kLast'),
            ],
            self._entries(_parse_source(text)),
        )

    def testEmptyArray(self):
        text = """\
const std::array<const base::Feature*, 0> kFeaturesExposedToJava = {};
"""
        self.assertEqual((), _parse_source(text).feature_array.entries)

    def testIncludes(self):
        text = """\
#include "a/b.h"
  #  include "c.h"
#include <vector>
// #include "commented_out.h"
/* #include "block_commented_out.h" */
#include "d.inc"
const char kText[] = "#include \\"in_string.h\\"";
const base::Feature* const kFeaturesExposedToJava[] = {&kFoo};
"""
        self.assertEqual(
            ('a/b.h', 'c.h', 'd.inc'), _parse_source(text).includes
        )

    def testArrayNamespace(self):
        array = 'const base::Feature* const kFeaturesExposedToJava[] = {};'
        for prefix, expected in (
            ('', ()),
            ('namespace a {\nnamespace b {\n', ('a', 'b')),
            ('namespace a::b {\n', ('a', 'b')),
            ('namespace a {\nnamespace {\n', ('a',)),
            ('namespace a {\ninline namespace v1 {\n', ('a',)),
            ('namespace a {}\nnamespace b {\n', ('b',)),
            ('namespace a {\nstruct S {\n  void F() {}\n};\n', ('a',)),
            ('namespace a {\nnamespace b = c;\n', ('a',)),
            ('namespace a {\n// }\nconst char kText[] = "}";\n', ('a',)),
            ('namespace a {\n#define CLOSE_NAMESPACE }\n', ('a',)),
        ):
            with self.subTest(prefix=prefix):
                source = _parse_source(prefix + array)
                self.assertEqual(expected, source.feature_array.namespace)

    def testFeatureMapNames(self):
        text = """\
// JNI_InComment_GetNativeMap(
#define JNI_InMacro_GetNativeMap(env) 0
const char kText[] = "JNI_InString_GetNativeMap(";
static int64_t JNI_SecondFeatureMap_GetNativeMap(JNIEnv* env);
static int64_t JNI_FirstFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFirstFeatureMap());
}
static int64_t JNI_SecondFeatureMap_GetNativeMap (JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetSecondFeatureMap());
}
const base::Feature* const kFeaturesExposedToJava[] = {&kFoo};
"""
        self.assertEqual(
            ('FirstFeatureMap', 'SecondFeatureMap'),
            _parse_source(text).feature_map_names,
        )

    def testParamArrays(self):
        text = """\
namespace chrome {

const base::Feature* const kFeaturesExposedToJava[] = {&kFoo};

namespace android {
const base::FeatureParam<std::string>* const kStringParamsExposedToJava[] = {
    &kStringParam,
};
}  // namespace android

const std::array<const base::FeatureParam<double>*, 1>
    kDoubleParamsExposedToJava = {&kDoubleParam};
const base::FeatureParam<int>* const kIntParamsExposedToJava[] = {
    &::kAbsoluteParam,
};
const base::FeatureParam<bool>* const kBooleanParamsExposedToJava[] = {
    &kBoolParam,
    &params::kQualifiedParam,
};

// Uses of the arrays are not definitions.
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava, kBooleanParamsExposedToJava,
      kIntParamsExposedToJava, kDoubleParamsExposedToJava,
      kStringParamsExposedToJava);
  return kFeatureMap.get();
}

}  // namespace chrome
"""
        boolean_type, int_type, double_type, string_type = models.PARAM_TYPES
        # The arrays are in PARAM_TYPES order, regardless of their order in the
        # file.
        self.assertEqual(
            (
                models.Array(
                    param_type=boolean_type,
                    namespace=('chrome',),
                    entries=(
                        models.ArrayEntry(_PATH, 17, '', 'kBoolParam'),
                        models.ArrayEntry(
                            _PATH, 18, 'params::', 'kQualifiedParam'
                        ),
                    ),
                ),
                models.Array(
                    param_type=int_type,
                    namespace=('chrome',),
                    entries=(
                        models.ArrayEntry(_PATH, 14, '::', 'kAbsoluteParam'),
                    ),
                ),
                models.Array(
                    param_type=double_type,
                    namespace=('chrome',),
                    entries=(models.ArrayEntry(_PATH, 12, '', 'kDoubleParam'),),
                ),
                models.Array(
                    param_type=string_type,
                    namespace=('chrome', 'android'),
                    entries=(models.ArrayEntry(_PATH, 7, '', 'kStringParam'),),
                ),
            ),
            _parse_source(text).param_arrays,
        )

    def testParamArraysAreOptional(self):
        text = 'const base::Feature* const kFeaturesExposedToJava[] = {&kFoo};'
        self.assertEqual((), _parse_source(text).param_arrays)

    def testErrorNoArray(self):
        text = """\
BASE_FEATURE(kFoo, base::FEATURE_DISABLED_BY_DEFAULT);
// const base::Feature* const kFeaturesExposedToJava[] = {&kFoo};
"""
        self._assert_error(
            [
                f'{_PATH}: error: Cannot find the definition of '
                'kFeaturesExposedToJava.'
            ],
            text,
        )

    def testErrorMultipleArrays(self):
        text = """\
#if BUILDFLAG(IS_DESKTOP_ANDROID)
const base::Feature* const kFeaturesExposedToJava[] = {&kFoo};
#else
const base::Feature* const kFeaturesExposedToJava[] = {&kBar};
#endif
"""
        self._assert_error(
            [
                f'{_PATH}: error: Found multiple definitions of '
                'kFeaturesExposedToJava (lines 2, 4).'
            ],
            text,
        )

    def testErrorUnbalancedArray(self):
        for body, reason in (
            ('{&kFoo,\n', 'Unterminated bracket'),
            ('{&kFoo(];\n', 'Unbalanced "]"'),
        ):
            with self.subTest(reason=reason):
                self._assert_error(
                    [
                        f'{_PATH}:1: error: Cannot parse kFeaturesExposedToJava: '
                        f'{reason}.'
                    ],
                    'const base::Feature* const kFeaturesExposedToJava[] = '
                    + body,
                )

    def testErrorUnsupportedEntries(self):
        text = """\
const base::Feature* const kFeaturesExposedToJava[] = {
    kNotAPointer,
    &kfoo,
    &GetFeature(),
    &features::kFoo.member,
    &kValid,
};
"""

        def message(line, entry):
            return (
                f'{_PATH}:{line}: error: Unsupported entry "{entry}" in '
                'kFeaturesExposedToJava. Expected the form '
                '"&namespace::kSomeName".'
            )

        self._assert_error(
            [
                message(2, 'kNotAPointer'),
                message(3, '&kfoo'),
                message(4, '&GetFeature()'),
                message(5, '&features::kFoo.member'),
            ],
            text,
        )

    def testErrorEmptyEntries(self):
        text = """\
const base::Feature* const kFeaturesExposedToJava[] = {
    ,
    &kFoo,,
    &kBar,
};
"""
        message = 'error: Empty entry in kFeaturesExposedToJava.'
        self._assert_error(
            [f'{_PATH}:2: {message}', f'{_PATH}:3: {message}'], text
        )

    def testErrorDuplicateEntries(self):
        text = """\
const base::Feature* const kFeaturesExposedToJava[] = {
    &ns::kFoo,
    &kBar,
    & ns::kFoo,
    &kBar,
};
"""
        self._assert_error(
            [
                f'{_PATH}:4: error: Duplicate entry "ns::kFoo" (first listed on '
                'line 2).',
                f'{_PATH}:5: error: Duplicate entry "kBar" (first listed on line '
                '3).',
            ],
            text,
        )

    def testErrorPreprocessorDirectivesInArray(self):
        text = """\
const base::Feature* const kFeaturesExposedToJava[] = {
    &kFoo,
#if BUILDFLAG(IS_ANDROID)
    &kBar,
#endif  // BUILDFLAG(IS_ANDROID)
};
"""
        message = (
            'error: Preprocessor directives are not supported inside '
            'kFeaturesExposedToJava, because the generated Java class '
            'is not conditional.'
        )
        self._assert_error(
            [f'{_PATH}:3: {message}', f'{_PATH}:5: {message}'], text
        )

    def testErrorInvalidDirectives(self):
        text = """\
// FEATURE_DEFINITION_FILE: third_party/foo/features.cc
// FEATURE_DEFINITION_FILE:
// FEATURE_DEFINITION_FILE: //third_party/foo/features.cc, //other.cc
// FEATURE_DEFINITION_FILE: //third_party/foo/valid.cc
const base::Feature* const kFeaturesExposedToJava[] = {&kFoo};
"""

        def message(line, found):
            return (
                f'{_PATH}:{line}: error: FEATURE_DEFINITION_FILE must be '
                'followed by a source-absolute path such as '
                f'"//path/to/file.cc", found "{found}".'
            )

        self._assert_error(
            [
                message(1, 'third_party/foo/features.cc'),
                message(2, ''),
                message(3, '//third_party/foo/features.cc, //other.cc'),
            ],
            text,
        )

    def testErrorsInParamArrays(self):
        # The errors in all arrays are reported at once.
        text = """\
const base::Feature* const kFeaturesExposedToJava[] = {kNotAPointer};
const base::FeatureParam<bool>* const kBooleanParamsExposedToJava[] = {
    &kFoo,
    &kFoo,
};
const base::FeatureParam<int>* const kIntParamsExposedToJava[] = {
    kNotAPointer,
};
const base::FeatureParam<double>* const kDoubleParamsExposedToJava[] = {
#if BUILDFLAG(IS_ANDROID)
    &kBar,
#endif
};
const base::FeatureParam<std::string>* const kStringParamsExposedToJava[] = {};
const base::FeatureParam<std::string>* const kStringParamsExposedToJava[] = {};
"""

        def unsupported_entry_message(line, array_name):
            return (
                f'{_PATH}:{line}: error: Unsupported entry "kNotAPointer" '
                f'in {array_name}. Expected the form '
                '"&namespace::kSomeName".'
            )

        def preprocessor_message(line):
            return (
                f'{_PATH}:{line}: error: Preprocessor directives are not '
                'supported inside kDoubleParamsExposedToJava, because the '
                'generated Java class is not conditional.'
            )

        self._assert_error(
            [
                unsupported_entry_message(1, 'kFeaturesExposedToJava'),
                f'{_PATH}:4: error: Duplicate entry "kFoo" (first listed on '
                'line 3).',
                unsupported_entry_message(7, 'kIntParamsExposedToJava'),
                preprocessor_message(10),
                preprocessor_message(12),
                f'{_PATH}: error: Found multiple definitions of '
                'kStringParamsExposedToJava (lines 14, 15).',
            ],
            text,
        )


class ParseFeatureDefinitionsTest(unittest.TestCase):
    def _definition(self, line, namespace, identifier, macro, name):
        return models.FeatureDefinition(
            _PATH, line, namespace, identifier, macro, name
        )

    def testParseDefinitions(self):
        text = """\
namespace foo {

BASE_FEATURE(kDerivedName, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCustomName, "CustomFeatureName",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kRuntimeMutable,
                             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kRuntimeMutableCustomName,
                             "RuntimeMutableName",
                             base::FEATURE_DISABLED_BY_DEFAULT);

namespace bar {
BASE_FEATURE(kNested, base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace bar

}  // namespace foo

BASE_FEATURE(kGlobal, base::FEATURE_DISABLED_BY_DEFAULT);
"""
        self.assertEqual(
            [
                self._definition(
                    3, ('foo',), 'kDerivedName', 'BASE_FEATURE', 'DerivedName'
                ),
                self._definition(
                    4,
                    ('foo',),
                    'kCustomName',
                    'BASE_FEATURE',
                    'CustomFeatureName',
                ),
                self._definition(
                    6,
                    ('foo',),
                    'kRuntimeMutable',
                    'BASE_RUNTIME_MUTABLE_FEATURE',
                    'RuntimeMutable',
                ),
                self._definition(
                    8,
                    ('foo',),
                    'kRuntimeMutableCustomName',
                    'BASE_RUNTIME_MUTABLE_FEATURE',
                    'RuntimeMutableName',
                ),
                self._definition(
                    13, ('foo', 'bar'), 'kNested', 'BASE_FEATURE', 'Nested'
                ),
                self._definition(18, (), 'kGlobal', 'BASE_FEATURE', 'Global'),
            ],
            _parse_feature_definitions(text),
        )

    def testOtherFeatureMacros(self):
        text = """\
#define DEFINE_SYNC_AUTOFILL_AI_FEATURE(feature_name) \\
  BASE_FEATURE(feature_name, base::FEATURE_ENABLED_BY_DEFAULT)

BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kCountryRestricted,
                                       base::FEATURE_DISABLED_FOR_COUNTRIES,
                                       "us", "ca");
DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncAutofillAi);
"""
        self.assertEqual(
            [
                self._definition(
                    4,
                    (),
                    'kCountryRestricted',
                    'BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS',
                    'CountryRestricted',
                ),
                self._definition(
                    7,
                    (),
                    'kSyncAutofillAi',
                    'DEFINE_SYNC_AUTOFILL_AI_FEATURE',
                    'SyncAutofillAi',
                ),
            ],
            _parse_feature_definitions(text),
        )

    def testIgnoredMacroCalls(self):
        text = """\
#define BASE_FEATURE(feature, ...) Define(feature)
BASE_DECLARE_FEATURE(kDeclared);
BASE_FEATURE_PARAM(int, kParam, &kFeature, "param", 1);
BASE_FEATURE_ENUM_PARAM(Mode, kEnumParam, &kFeature, "mode", Mode::kA,
                        &kModeOptions);
EXPECT_SINGLE_FEATURE(features::kFeature);
// BASE_FEATURE(kInLineComment, base::FEATURE_DISABLED_BY_DEFAULT);
/* BASE_FEATURE(kInBlockComment, base::FEATURE_DISABLED_BY_DEFAULT); */
const char kText[] = "BASE_FEATURE(kInString, FEATURE_DISABLED_BY_DEFAULT)";
bool enabled = base::FeatureList::IsEnabled(kFeature);
"""
        self.assertEqual([], _parse_feature_definitions(text))

    def testPreprocessorLinesBetweenArguments(self):
        text = """\
BASE_FEATURE(kPlatformDependent,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
"""
        self.assertEqual(
            [
                self._definition(
                    1,
                    (),
                    'kPlatformDependent',
                    'BASE_FEATURE',
                    'PlatformDependent',
                ),
            ],
            _parse_feature_definitions(text),
        )

    def testSpecialCharactersInFeatureName(self):
        text = """\
BASE_FEATURE(kComma, "Comma,Name", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kParen, "Paren)Name", base::FEATURE_DISABLED_BY_DEFAULT);
"""
        self.assertEqual(
            [('kComma', 'Comma,Name'), ('kParen', 'Paren)Name')],
            [(d.identifier, d.name) for d in _parse_feature_definitions(text)],
        )

    def testInvalidFeatureNames(self):
        text = """\
#define DEFINE_CUSTOM_FEATURE(feature, name) Define(feature, name)
BASE_FEATURE(kNotALiteral, kName, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kConcatenated, "Foo" "Bar", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTooManyArgs, "Name", base::FEATURE_DISABLED_BY_DEFAULT, 1);
DEFINE_CUSTOM_FEATURE(kCustomMacro, "CustomName");
"""
        definitions = _parse_feature_definitions(text)
        self.assertEqual(
            [None, None, None, None], [d.name for d in definitions]
        )
        self.assertEqual(
            [
                f'{_PATH}:2: The feature name of kNotALiteral must be a plain '
                'string literal, found "kName".',
                f'{_PATH}:3: The feature name of kConcatenated must be a plain '
                'string literal, found ""Foo" "Bar"".',
                f'{_PATH}:4: BASE_FEATURE(kTooManyArgs, ...) has 4 arguments; '
                'expected 2 or 3.',
                f'{_PATH}:5: Cannot determine the feature name of kCustomMacro '
                'from macro DEFINE_CUSTOM_FEATURE, because it is passed a string '
                'literal.',
            ],
            [d.error for d in definitions],
        )

    def testSkipsUnbalancedMacroCalls(self):
        text = """\
BASE_FEATURE(kUnterminated,
BASE_FEATURE(kMismatched, ];
BASE_FEATURE(kValid, base::FEATURE_DISABLED_BY_DEFAULT);
"""
        self.assertEqual(
            [
                self._definition(3, (), 'kValid', 'BASE_FEATURE', 'Valid'),
            ],
            _parse_feature_definitions(text),
        )


class ParseParamDefinitionsTest(unittest.TestCase):
    def _definition(
        self,
        line,
        namespace,
        identifier,
        name,
        feature_qualifier,
        feature_identifier,
    ):
        return models.ParamDefinition(
            _PATH,
            line,
            namespace,
            identifier,
            name,
            models.ArrayEntry(
                _PATH, line, feature_qualifier, feature_identifier
            ),
        )

    def testParseParamDefinitions(self):
        text = """\
namespace foo {

BASE_FEATURE_PARAM(bool, kDerivedName, &kFeature, false);
BASE_FEATURE_PARAM(int,
                   kCustomName,
                   &kFeature,
                   "custom_name",
                   0);
BASE_FEATURE_PARAM(double, kQualifiedFeature, &features::kFeature, 0.5);
BASE_FEATURE_PARAM(std::string, kGlobalFeature, & :: kGlobal, "global", "");

namespace bar {
BASE_FEATURE_PARAM(base::TimeDelta, kNested, &kFeature, base::Seconds(1));
}  // namespace bar

}  // namespace foo
"""
        self.assertEqual(
            [
                self._definition(
                    3, ('foo',), 'kDerivedName', 'DerivedName', '', 'kFeature'
                ),
                self._definition(
                    4, ('foo',), 'kCustomName', 'custom_name', '', 'kFeature'
                ),
                self._definition(
                    9,
                    ('foo',),
                    'kQualifiedFeature',
                    'QualifiedFeature',
                    'features::',
                    'kFeature',
                ),
                self._definition(
                    10, ('foo',), 'kGlobalFeature', 'global', '::', 'kGlobal'
                ),
                self._definition(
                    13, ('foo', 'bar'), 'kNested', 'Nested', '', 'kFeature'
                ),
            ],
            _parse_param_definitions(text),
        )

    def testIgnoredMacroCalls(self):
        text = """\
#define BASE_FEATURE_PARAM(T, name, ...) Define(name)
BASE_DECLARE_FEATURE_PARAM(int, kDeclared);
BASE_FEATURE(kFeature, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_ENUM_PARAM(Mode, kEnumParam, &kFeature, "mode", Mode::kA,
                        &kModeOptions);
constexpr base::FeatureParam<int> kNotAMacro{&kFeature, "not_a_macro", 0};
// BASE_FEATURE_PARAM(int, kInLineComment, &kFeature, 0);
/* BASE_FEATURE_PARAM(int, kInBlockComment, &kFeature, 0); */
const char kText[] = "BASE_FEATURE_PARAM(int, kInString, &kFeature, 0)";
"""
        self.assertEqual([], _parse_param_definitions(text))

    def testPreprocessorLinesBetweenArguments(self):
        text = """\
BASE_FEATURE_PARAM(int, kPlatformDependent, &kFeature, "size",
#if BUILDFLAG(IS_DESKTOP_ANDROID)
                   2);
#else
                   1);
#endif
"""
        self.assertEqual(
            [
                self._definition(
                    1, (), 'kPlatformDependent', 'size', '', 'kFeature'
                ),
            ],
            _parse_param_definitions(text),
        )

    def testSpecialCharactersInParamName(self):
        text = """\
BASE_FEATURE_PARAM(int, kComma, &kFeature, "comma,name", 0);
BASE_FEATURE_PARAM(int, kParen, &kFeature, "paren)name", 0);
"""
        self.assertEqual(
            [('kComma', 'comma,name'), ('kParen', 'paren)name')],
            [(d.identifier, d.name) for d in _parse_param_definitions(text)],
        )

    def testInvalidParamDefinitions(self):
        text = """\
BASE_FEATURE_PARAM(int, kNotALiteral, &kFeature, kName, 0);
BASE_FEATURE_PARAM(int, kConcatenated, &kFeature, "foo" "bar", 0);
BASE_FEATURE_PARAM(int, kTooFewArgs, &kFeature);
BASE_FEATURE_PARAM(int, kTooManyArgs, &kFeature, "name", 0, 1);
BASE_FEATURE_PARAM(int, kNotAPointer, kFeature, 0);
BASE_FEATURE_PARAM(int, kFunctionCall, &GetFeature(), 0);
"""
        definitions = _parse_param_definitions(text)
        self.assertEqual(
            [(None, None)] * 6, [(d.name, d.feature) for d in definitions]
        )
        self.assertEqual(
            [
                f'{_PATH}:1: The param name of kNotALiteral must be a plain '
                'string literal, found "kName".',
                f'{_PATH}:2: The param name of kConcatenated must be a plain '
                'string literal, found ""foo" "bar"".',
                f'{_PATH}:3: BASE_FEATURE_PARAM(int, kTooFewArgs, ...) has 3 '
                'arguments; expected 4 or 5.',
                f'{_PATH}:4: BASE_FEATURE_PARAM(int, kTooManyArgs, ...) has 6 '
                'arguments; expected 4 or 5.',
                f'{_PATH}:5: The feature of kNotAPointer must have the form '
                '"&namespace::kFeatureName", found "kFeature".',
                f'{_PATH}:6: The feature of kFunctionCall must have the form '
                '"&namespace::kFeatureName", found "&GetFeature()".',
            ],
            [d.error for d in definitions],
        )

    def testSkipsMacroCallsThatAreNotDefinitions(self):
        text = """\
BASE_FEATURE_PARAM(int, notAKIdentifier, &kFeature, 0);
BASE_FEATURE_PARAM(kOnlyOneArgument);
BASE_FEATURE_PARAM(int, kUnterminated, &kFeature,
BASE_FEATURE_PARAM(int, kMismatched, &kFeature, ];
BASE_FEATURE_PARAM(int, kValid, &kFeature, 0);
"""
        self.assertEqual(
            [
                self._definition(5, (), 'kValid', 'Valid', '', 'kFeature'),
            ],
            _parse_param_definitions(text),
        )


if __name__ == '__main__':
    unittest.main()
