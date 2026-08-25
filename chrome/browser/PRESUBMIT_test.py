#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import unittest

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(file_dir_path, '..', '..'))
from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

_VALID_DEP = "+third_party/blink/public/platform/web_something.h,"
_INVALID_DEP = "+third_party/blink/public/web/web_something.h,"
_INVALID_DEP2 = "+third_party/blink/public/web/web_nothing.h,"


class CheckNoNewProfileIDPrefixesTest(unittest.TestCase):
    def testAdditionOfNewPrefix(self):
        lines = [
            'constexpr char kMyNewOTRProfileIDPrefix[] = "MyNew::OTRPrefix";',
            'constexpr char kAnotherProfileIDPrefix[] = "Another::Prefix";'
        ]
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/profiles/profile.cc', lines)
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT._CheckNoNewProfileIDPrefixes(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(2, len(errors[0].items))

    def testNoNewPrefix(self):
        lines = ['const char kSomethingElse[] = "SomethingElse";']
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/profiles/profile.cc', lines)
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT._CheckNoNewProfileIDPrefixes(
            mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))

    def testAdditionInOtherFile(self):
        lines = ['const char kMyNewProfileIDPrefix[] = "MyNew::Prefix";']
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/profiles/other_file.cc', lines)
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT._CheckNoNewProfileIDPrefixes(
            mock_input_api, mock_output_api)
        self.assertEqual(0, len(errors))


class BlinkPublicWebUnwantedDependenciesTest(unittest.TestCase):

    def makeInputApi(self, files):
        input_api = MockInputApi()
        input_api.InitFiles(files)
        return input_api

    INVALID_DEPS_MESSAGE = ('chrome/browser cannot depend on '
                            'blink/public/web interfaces. Use'
                            ' blink/public/common instead.')

    def testAdditionOfUnwantedDependency(self):
        input_api = self.makeInputApi(
            [MockAffectedFile('DEPS', [_INVALID_DEP], [], action='M')])
        warnings = PRESUBMIT._CheckUnwantedDependencies(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
        self.assertEqual(1, len(warnings[0].items))

    def testAdditionOfUnwantedDependencyInComment(self):
        input_api = self.makeInputApi(
            [MockAffectedFile('DEPS', ["#" + _INVALID_DEP], [], action='M')])
        warnings = PRESUBMIT._CheckUnwantedDependencies(
            input_api, MockOutputApi())
        self.assertEqual([], warnings)

    def testAdditionOfValidDependency(self):
        input_api = self.makeInputApi(
            [MockAffectedFile('DEPS', [_VALID_DEP], [], action='M')])
        warnings = PRESUBMIT._CheckUnwantedDependencies(
            input_api, MockOutputApi())
        self.assertEqual([], warnings)

    def testAdditionOfMultipleUnwantedDependency(self):
        input_api = self.makeInputApi([
            MockAffectedFile('DEPS', [_INVALID_DEP, _INVALID_DEP2], action='M')
        ])
        warnings = PRESUBMIT._CheckUnwantedDependencies(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
        self.assertEqual(2, len(warnings[0].items))

        input_api = self.makeInputApi([
            MockAffectedFile('DEPS', [_INVALID_DEP, _VALID_DEP], [],
                             action='M')
        ])
        warnings = PRESUBMIT._CheckUnwantedDependencies(
            input_api, MockOutputApi())
        self.assertEqual(1, len(warnings))
        self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
        self.assertEqual(1, len(warnings[0].items))

    def testRemovalOfUnwantedDependency(self):
        input_api = self.makeInputApi(
            [MockAffectedFile('DEPS', [], [_INVALID_DEP], action='M')])
        warnings = PRESUBMIT._CheckUnwantedDependencies(
            input_api, MockOutputApi())
        self.assertEqual([], warnings)

    def testRemovalOfValidDependency(self):
        input_api = self.makeInputApi(
            [MockAffectedFile('DEPS', [], [_VALID_DEP], action='M')])
        warnings = PRESUBMIT._CheckUnwantedDependencies(
            input_api, MockOutputApi())
        self.assertEqual([], warnings)


class InteractiveUiTestLibIncludeTest(unittest.TestCase):

    def testAdditionOfUnwantedDependency(self):
        lines = [
            '#include "ui/base/test/ui_controls.h"',
            '#include "ui/base/test/foo.h"',
            '#include "chrome/test/base/interactive_test_utils.h"'
        ]
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('foo_interactive_uitest.cc', lines),
            MockAffectedFile('foo_browsertest.cc', lines),
            MockAffectedFile('foo_interactive_browsertest.cc', lines),
            MockAffectedFile('foo_unittest.cc', lines)
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT._CheckNoInteractiveUiTestLibInNonInteractiveUiTest(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(errors))
        # 2 lines from 2 files.
        self.assertEqual(4, len(errors[0].items))


class CheckBuildFilesForIndirectAshSourcesTest(unittest.TestCase):
    MESSAGE = "Indirect sources detected."

    def testScope(self):
        """We only complain for changes to BUILD.gn under certain
        directories."""

        new_contents = [
            'source_set("foo") {',
            '  sources = [ "a/b.cc" ]',
            '}',
        ]

        mock_output_api = MockOutputApi()
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('BUILD.gn', new_contents),
            MockAffectedFile('chrome/browser/BUILD.gn', new_contents),
            MockAffectedFile('chrome/browser/ash/build.cc', new_contents),
            MockAffectedFile('chrome/browser/ash/BUILD.gn', new_contents),
            MockAffectedFile('chrome/browser/ashley/BUILD.gn', new_contents),
            MockAffectedFile('chrome/browser/chromeos/a/b/BUILD.gn',
                             new_contents),
            MockAffectedFile('chrome/browser/resources/ash/BUILD.gn',
                             new_contents),
            MockAffectedFile('chrome/browser/ui/BUILD.gn', new_contents),
            MockAffectedFile('chrome/browser/ui/ash/foo/BUILD.gn',
                             new_contents),
            MockAffectedFile('chrome/browser/ui/chromeos/BUILD.gn',
                             new_contents),
            MockAffectedFile('chrome/browser/ui/webui/ash/BUILD.gn',
                             new_contents),
        ]

        results = PRESUBMIT._CheckBuildFilesForIndirectAshSources(
            mock_input_api, mock_output_api)

        for result in results:
            self.assertEqual(result.message, self.MESSAGE)

        self.assertCountEqual([r.items for r in results],
                              [["chrome/browser/ash/BUILD.gn"],
                               ["chrome/browser/chromeos/a/b/BUILD.gn"],
                               ["chrome/browser/ui/ash/foo/BUILD.gn"],
                               ["chrome/browser/ui/chromeos/BUILD.gn"],
                               ["chrome/browser/ui/webui/ash/BUILD.gn"]])

    def testComplexFormatting(self):
        new_contents = [
            'source_set("foo") {',
            '  sources = [ "../0", "a/1",]',
            '\tsources += ["a/2" ]',
            'sources += [ # bla',
            '   "a/3",',
            '  ]',
            '   # sources = ["a/b"]',
            'sources += # bla',
            '    ["a/4"]#bla',
            '}',
            'static_library("bar"){',
            ' deps = []',
            ' sources = []',
            ' if (something) {',
            '   sources += [',
            '',
            '     "a/5", "ab", "a/6","a/7",# "a/b"',
            '     "a/8"]',
            '   sources',
            '     += [ "a/9" ]}',
            '}',
        ]

        mock_output_api = MockOutputApi()
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/ash/BUILD.gn', new_contents),
        ]

        results = PRESUBMIT._CheckBuildFilesForIndirectAshSources(
            mock_input_api, mock_output_api)

        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].message, self.MESSAGE)
        self.assertEqual(results[0].items, ["chrome/browser/ash/BUILD.gn"])
        self.assertEqual(
            [s.lstrip() for s in results[0].long_text.splitlines()[1:]], [
                '../0', 'a/1', 'a/2', 'a/3', 'a/4', 'a/5', 'a/6', 'a/7', 'a/8',
                'a/9'
            ])

    def testModifications(self):
        old_contents = [
            'source_set("foo") {',
            '  sources = ["x/y", "a/b"]',
            '}',
        ]
        new_contents_good = [
            'source_set("foo") {',
            '  sources = ["x/y", "ab"]',
            '}',
        ]
        new_contents_bad = [
            'source_set("foo") {',
            '  sources = ["x/y", "a/b", "a/c"]',
            '}',
        ]

        mock_output_api = MockOutputApi()
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/ash/BUILD.gn', new_contents_bad,
                             old_contents),
            MockAffectedFile('chrome/browser/chromeos/BUILD.gn',
                             new_contents_good, old_contents),
        ]

        results = PRESUBMIT._CheckBuildFilesForIndirectAshSources(
            mock_input_api, mock_output_api)

        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].message, self.MESSAGE)
        self.assertEqual(results[0].items, ["chrome/browser/ash/BUILD.gn"])
        self.assertEqual(
            [s.lstrip() for s in results[0].long_text.splitlines()[1:]],
            ['a/c'])


class CheckAshSourcesForBadIncludes(unittest.TestCase):
    MESSAGE = "Bad includes detected in the following files."

    def testScope(self):
        """We only complain for changes under certain directories."""

        new_contents = ['#include "chrome/browser/ui/browser.h"']

        mock_output_api = MockOutputApi()
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('foo.cc', new_contents),
            MockAffectedFile('chrome/browser/foo.cc', new_contents),
            MockAffectedFile('chrome/browser/ash/foo.cc', new_contents),
            MockAffectedFile('chrome/browser/ashley/foo.cc', new_contents),
            MockAffectedFile('chrome/browser/chromeos/a/b/foo.cc',
                             new_contents),
            MockAffectedFile('chrome/browser/resources/ash/foo.cc',
                             new_contents),
            MockAffectedFile('chrome/browser/ui/foo.cc', new_contents),
            MockAffectedFile('chrome/browser/ui/ash/foo/foo.cc', new_contents),
            MockAffectedFile('chrome/browser/ui/chromeos/foo.cc',
                             new_contents),
            MockAffectedFile('chrome/browser/ui/webui/ash/foo.cc',
                             new_contents),
            MockAffectedFile('chrome/foo/ash/foo.cc', new_contents),
        ]

        results = PRESUBMIT._CheckAshSourcesForBadIncludes(
            mock_input_api, mock_output_api)

        for result in results:
            self.assertEqual(result.message, self.MESSAGE)

        self.assertCountEqual([r.items for r in results],
                              [["chrome/browser/ash/foo.cc"],
                               ["chrome/browser/chromeos/a/b/foo.cc"],
                               ["chrome/browser/resources/ash/foo.cc"],
                               ["chrome/browser/ui/ash/foo/foo.cc"],
                               ["chrome/browser/ui/chromeos/foo.cc"],
                               ["chrome/browser/ui/webui/ash/foo.cc"]])

    def testComments(self):
        """We don't complain about bad includes in single-line comments."""

        new_contents = ['// No #include "chrome/browser/ui/browser.h"']

        mock_output_api = MockOutputApi()
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/ash/foo.cc', new_contents),
        ]

        results = PRESUBMIT._CheckAshSourcesForBadIncludes(
            mock_input_api, mock_output_api)

        self.assertEqual(results, [])

    def testModifications(self):
        """We don't complain about bad includes that were already there."""

        old_contents = [
            '#include "chrome/browser/foo/bar.h"',
            '#include "chrome/browser/ui/browser.h"',
        ]
        new_contents = [
            '#include "chrome/browser/foo/bar.h"',
            '#include "chrome/browser/ui/browser.h"',
            '#include "chrome/browser/ui/browser.h"',
        ]

        mock_output_api = MockOutputApi()
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/ash/foo.cc', new_contents,
                             old_contents),
        ]

        results = PRESUBMIT._CheckAshSourcesForBadIncludes(
            mock_input_api, mock_output_api)

        self.assertEqual(results, [])


class CheckNewDirectoryHasBuildGnTest(unittest.TestCase):
    def testNewDirectoryWithBuildGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/new_dir/foo.cc', [''],
                             action='A'),
            MockAffectedFile('chrome/browser/new_dir/BUILD.gn', [''],
                             action='A'),
        ]
        mock_input_api.InitFiles(mock_input_api.files)
        mock_output_api = MockOutputApi()
        warnings = PRESUBMIT._CheckNewDirectoryHasBuildGn(
            mock_input_api, mock_output_api)
        self.assertEqual([], warnings)

    def testNewDirectoryMissingBuildGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/new_dir/foo.cc', [''],
                             action='A'),
        ]
        mock_input_api.InitFiles(mock_input_api.files)
        mock_output_api = MockOutputApi()
        warnings = PRESUBMIT._CheckNewDirectoryHasBuildGn(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(warnings))
        self.assertEqual(
            'New direct subdirectories of chrome/browser or '
            'chrome/browser/ui must have a BUILD.gn file.',
            warnings[0].message)
        warning_items_norm = [s.replace('\\', '/') for s in warnings[0].items]
        self.assertEqual(['chrome/browser/new_dir'], warning_items_norm)

    def testNewUiDirectoryMissingBuildGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/ui/new_ui/foo.cc', [''],
                             action='A'),
        ]
        mock_input_api.InitFiles(mock_input_api.files)
        mock_output_api = MockOutputApi()
        warnings = PRESUBMIT._CheckNewDirectoryHasBuildGn(
            mock_input_api, mock_output_api)
        self.assertEqual(1, len(warnings))
        self.assertEqual(
            'New direct subdirectories of chrome/browser or '
            'chrome/browser/ui must have a BUILD.gn file.',
            warnings[0].message)
        warning_items_norm = [s.replace('\\', '/') for s in warnings[0].items]
        self.assertEqual(['chrome/browser/ui/new_ui'], warning_items_norm)

    def testNewNestedDirectoryMissingBuildGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/new_dir/nested_dir/foo.cc', [''],
                             action='A'),
        ]
        mock_input_api.InitFiles(mock_input_api.files)
        mock_output_api = MockOutputApi()
        warnings = PRESUBMIT._CheckNewDirectoryHasBuildGn(
            mock_input_api, mock_output_api)
        self.assertEqual([], warnings)

    def testExistingDirectoryWithMissingBuildGn(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/existing_dir/foo.cc', [''],
                             action='A'),
            MockAffectedFile('chrome/browser/existing_dir/bar.cc', [''],
                             action='M'),
        ]
        mock_input_api.InitFiles(mock_input_api.files)
        mock_output_api = MockOutputApi()
        warnings = PRESUBMIT._CheckNewDirectoryHasBuildGn(
            mock_input_api, mock_output_api)
        self.assertEqual([], warnings)

    def testExistingDirectoryWithBuildGnOnDisk(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/existing_dir/foo.cc', [''],
                             action='A'),
        ]
        mock_input_api.InitFiles(mock_input_api.files)
        # Simulate BUILD.gn existing on disk.
        original_exists = mock_input_api.os_path.exists

        def side_effect(path):
            if path.replace(
                    '\\',
                    '/').endswith('chrome/browser/existing_dir/BUILD.gn'):
                return True
            return original_exists(path)

        mock_input_api.os_path.exists = side_effect

        mock_output_api = MockOutputApi()
        warnings = PRESUBMIT._CheckNewDirectoryHasBuildGn(
            mock_input_api, mock_output_api)
        self.assertEqual([], warnings)

    def testChromeBrowserRoot(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/browser/foo.cc', [''], action='A'),
        ]
        mock_input_api.InitFiles(mock_input_api.files)
        mock_output_api = MockOutputApi()
        warnings = PRESUBMIT._CheckNewDirectoryHasBuildGn(
            mock_input_api, mock_output_api)
        self.assertEqual([], warnings)


class CheckNoNewBrowserWindowGetterTest(unittest.TestCase):
    def testWarnsOnNewCallSites(self):
        # New code under chrome/browser/ should not call
        # `<expr>->window()->X(` or `<expr>.window()->X(` where X is declared
        # on ui::BaseWindow, including when the call line-wraps across
        # `window()` and `->X`.
        input_api = MockInputApi()
        input_api.files = [
            # Should warn: single-line Browser->window()->BaseWindowMethod().
            MockAffectedFile('chrome/browser/ui/single.cc',
                             ['browser->window()->GetNativeWindow();']),
            MockAffectedFile('chrome/browser/ui/single2.cc',
                             ['browser_->window()->Show();']),
            MockAffectedFile('chrome/browser/ui/dot.cc',
                             ['settings.window()->IsVisible();']),
            MockAffectedFile('chrome/browser/ui/method_call.cc',
                             ['browser()->window()->GetBounds();']),
            # Should warn: line-wrap before `->X(`.
            MockAffectedFile('chrome/browser/ui/wrap_before_arrow.cc', [
                'some_long_browser_expression->window()',
                '    ->GetNativeWindow();',
            ]),
            # Should warn: line-wrap after `->`, before the method name.
            MockAffectedFile('chrome/browser/ui/wrap_after_arrow.cc', [
                'browser->window()->',
                '    GetBounds();',
            ]),
            # Should warn: line-wrap with dot-form receiver.
            MockAffectedFile('chrome/browser/ui/wrap_dot.cc', [
                'new_browser.window()',
                '    ->Activate();',
            ]),
            # Should NOT warn: uses Browser::GetWindow() (the recommended
            # API).
            MockAffectedFile('chrome/browser/ui/ok_getwindow.cc',
                             ['browser->GetWindow()->GetNativeWindow();']),
            # Should NOT warn: bare `window()` (Browser-internal or unrelated
            # classes like extensions::WindowController).
            MockAffectedFile('chrome/browser/ui/ok_bare.cc',
                             ['  if (window()->IsFullscreen()) {}']),
            # Should NOT warn: identifier ends with `_window`, not `window`.
            MockAffectedFile('chrome/browser/ui/ok_other_window.cc', [
                'dialog_window()->Show();',
                'root_window()->SetBounds(b);',
                'app_window()->IsFullscreen();',
            ]),
            # Should NOT warn: file is in the excluded_paths list because its
            # window() method is extensions::WindowController::window().
            MockAffectedFile(
                'chrome/browser/extensions/api/tabs/tabs_api.cc',
                ['window_controller->window()->Close();']),
            # Should NOT warn: `// nocheck` escape hatch on the method line.
            MockAffectedFile('chrome/browser/ui/nocheck.cc', [
                'browser->window()->Show();  // nocheck',
            ]),
            # Should NOT warn: `// nocheck` on a different line of the
            # multi-line match.
            MockAffectedFile('chrome/browser/ui/nocheck_multiline.cc', [
                'browser->window()  // nocheck',
                '    ->Show();',
            ]),
            # Should NOT warn: comment line is ignored.
            MockAffectedFile('chrome/browser/ui/comment.cc', [
                '// Replaced browser->window()->Show() with GetWindow().',
            ]),
        ]

        results = PRESUBMIT._CheckNoNewBrowserWindowGetter(
            input_api, MockOutputApi())

        self.assertEqual(1, len(results))
        message = results[0].message
        self.assertIn('chrome/browser/ui/single.cc', message)
        self.assertIn('chrome/browser/ui/single2.cc', message)
        self.assertIn('chrome/browser/ui/dot.cc', message)
        self.assertIn('chrome/browser/ui/method_call.cc', message)
        self.assertIn('chrome/browser/ui/wrap_before_arrow.cc', message)
        self.assertIn('chrome/browser/ui/wrap_after_arrow.cc', message)
        self.assertIn('chrome/browser/ui/wrap_dot.cc', message)
        self.assertNotIn('chrome/browser/ui/ok_getwindow.cc', message)
        self.assertNotIn('chrome/browser/ui/ok_bare.cc', message)
        self.assertNotIn('chrome/browser/ui/ok_other_window.cc', message)
        self.assertNotIn('chrome/browser/extensions/api/tabs/tabs_api.cc',
                         message)
        self.assertNotIn('chrome/browser/ui/nocheck.cc', message)
        self.assertNotIn('chrome/browser/ui/nocheck_multiline.cc', message)
        self.assertNotIn('chrome/browser/ui/comment.cc', message)


class CheckNoNewBrowserWindowMemberCallTest(unittest.TestCase):
    # See https://crbug.com/496674143. Companion to
    # `CheckNoNewBrowserWindowGetterTest`: this check warns on any new
    # `browser->window()` / `browser_->window()` call site that isn't
    # already covered by the BaseWindow-method-specific check.
    def testWarnsAndSkips(self):
        input_api = MockInputApi()
        input_api.files = [
            # Should warn: browser->window() chained to a non-BaseWindow
            # method.
            MockAffectedFile('chrome/browser/ui/non_base.cc',
                             ['browser->window()->GetLocationBar();']),
            MockAffectedFile('chrome/browser/ui/non_base_member.cc',
                             ['browser_->window()->UpdateToolbar(nullptr);']),
            # Should warn: `browser()->window()` (member-function accessor,
            # common in test fixtures like BrowserWithTestWindowTest).
            MockAffectedFile('chrome/browser/ui/accessor.cc',
                             ['browser()->window()->GetLocationBar();']),
            MockAffectedFile('chrome/browser/ui/accessor_passed.cc',
                             ['DoSomething(browser()->window());']),
            # Should warn: browser->window() passed as a parameter (no
            # chained call at all).
            MockAffectedFile('chrome/browser/ui/passed.cc',
                             ['DoSomething(browser->window());']),
            MockAffectedFile('chrome/browser/ui/passed_member.cc',
                             ['  view_ = browser_->window();']),
            # Should NOT warn: BaseWindow method chain is handled by
            # _CheckNoNewBrowserWindowGetter; avoid duplicate warnings.
            MockAffectedFile('chrome/browser/ui/base_chain.cc',
                             ['browser->window()->GetNativeWindow();']),
            MockAffectedFile('chrome/browser/ui/base_chain_member.cc',
                             ['browser_->window()->Show();']),
            MockAffectedFile('chrome/browser/ui/base_chain_accessor.cc',
                             ['browser()->window()->Show();']),
            # Should NOT warn: already migrated to BrowserWindow::FromBrowser.
            MockAffectedFile('chrome/browser/ui/migrated.cc', [
                'BrowserWindow::FromBrowser(browser)->GetLocationBar();',
            ]),
            # Should NOT warn: already migrated to GetWindow().
            MockAffectedFile('chrome/browser/ui/get_window.cc',
                             ['browser->GetWindow()->GetNativeWindow();']),
            # Should NOT warn: other identifier ending in `browser` is not
            # the receiver we're targeting.
            MockAffectedFile('chrome/browser/ui/other_var.cc', [
                'my_browser->window()->GetLocationBar();',
                'new_browser_->window()->UpdateToolbar(nullptr);',
                'GetBrowser()->window()->GetLocationBar();',
            ]),
            # Should NOT warn: file is in the allowlist (declaration /
            # fallback implementation).
            MockAffectedFile('chrome/browser/ui/browser.h',
                             ['BrowserWindow* window() const;']),
            MockAffectedFile(
                'chrome/browser/ui/views/frame/browser_window_factory.cc',
                ['return concrete ? concrete->window() : nullptr;']),
            # Should NOT warn: `// nocheck` escape hatch.
            MockAffectedFile('chrome/browser/ui/nocheck.cc', [
                'browser->window()->GetLocationBar();  // nocheck',
            ]),
            # Should NOT warn: comment lines are ignored.
            MockAffectedFile('chrome/browser/ui/comment.cc', [
                '// browser->window()->GetLocationBar() is being removed.',
            ]),
        ]

        results = PRESUBMIT._CheckNoNewBrowserWindowMemberCall(
            input_api, MockOutputApi())

        self.assertEqual(1, len(results))
        message = results[0].message
        self.assertIn('chrome/browser/ui/non_base.cc', message)
        self.assertIn('chrome/browser/ui/non_base_member.cc', message)
        self.assertIn('chrome/browser/ui/accessor.cc', message)
        self.assertIn('chrome/browser/ui/accessor_passed.cc', message)
        self.assertIn('chrome/browser/ui/passed.cc', message)
        self.assertIn('chrome/browser/ui/passed_member.cc', message)
        self.assertNotIn('chrome/browser/ui/base_chain.cc', message)
        self.assertNotIn('chrome/browser/ui/base_chain_member.cc', message)
        self.assertNotIn('chrome/browser/ui/base_chain_accessor.cc', message)
        self.assertNotIn('chrome/browser/ui/migrated.cc', message)
        self.assertNotIn('chrome/browser/ui/get_window.cc', message)
        self.assertNotIn('chrome/browser/ui/other_var.cc', message)
        self.assertNotIn('chrome/browser/ui/browser.h', message)
        self.assertNotIn(
            'chrome/browser/ui/views/frame/browser_window_factory.cc',
            message)
        self.assertNotIn('chrome/browser/ui/nocheck.cc', message)
        self.assertNotIn('chrome/browser/ui/comment.cc', message)

    def testNoChangesProducesNoWarnings(self):
        # The check must be diff-driven: a file with no changed contents
        # must not emit warnings even if old code already uses
        # browser->window().
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/ui/old_file.cc', []),
        ]
        results = PRESUBMIT._CheckNoNewBrowserWindowMemberCall(
            input_api, MockOutputApi())
        self.assertEqual(0, len(results))


class CheckNoNewBrowserUsageTest(unittest.TestCase):

    def testWarnsOnBrowserHeaderInclude(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/ui/include_quotes.cc',
                             ['#include "chrome/browser/ui/browser.h"']),
            MockAffectedFile('chrome/browser/ui/include_brackets.h',
                             ['#include <chrome/browser/ui/browser.h>']),
            MockAffectedFile('chrome/browser/ui/include_spaces.mm',
                             ['#include   "chrome/browser/ui/browser.h"']),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        message = results[0].message
        self.assertIn('chrome/browser/ui/include_quotes.cc', message)
        self.assertIn('chrome/browser/ui/include_brackets.h', message)
        self.assertIn('chrome/browser/ui/include_spaces.mm', message)

    def testWarnsOnBrowserClassUsage(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/ui/ptr.cc',
                             ['Browser* browser = nullptr;']),
            MockAffectedFile('chrome/browser/ui/const_ptr.cc',
                             ['const Browser* browser = nullptr;']),
            MockAffectedFile('chrome/browser/ui/ref.cc',
                             ['Browser& browser = *b;']),
            MockAffectedFile('chrome/browser/ui/const_ref.cc',
                             ['const Browser& browser = *b;']),
            MockAffectedFile('chrome/browser/ui/fwd_decl.h',
                             ['class Browser;']),
            MockAffectedFile('chrome/browser/ui/struct_decl.h',
                             ['struct Browser;']),
            MockAffectedFile('chrome/browser/ui/static_method.cc',
                             ['Browser::Create(params);']),
            MockAffectedFile('chrome/browser/ui/unique_ptr.cc',
                             ['std::unique_ptr<Browser> browser;']),
            MockAffectedFile('chrome/browser/ui/raw_ptr.h',
                             ['raw_ptr<Browser> browser_;']),
            MockAffectedFile('chrome/browser/ui/raw_ref.h',
                             ['raw_ref<Browser> browser_;']),
            MockAffectedFile('chrome/browser/ui/scoped_refptr.h',
                             ['scoped_refptr<Browser> browser_;']),
            MockAffectedFile('chrome/browser/ui/cast.cc',
                             ['auto* b = static_cast<Browser*>(window);']),
            MockAffectedFile('chrome/browser/ui/new_expr.cc',
                             ['auto* b = new Browser(profile);']),
            MockAffectedFile('chrome/browser/ui/func_sig.h',
                             ['Browser* GetBrowser();',
                              'void SetBrowser(Browser* b);']),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        message = results[0].message
        self.assertIn('chrome/browser/ui/ptr.cc', message)
        self.assertIn('chrome/browser/ui/const_ptr.cc', message)
        self.assertIn('chrome/browser/ui/ref.cc', message)
        self.assertIn('chrome/browser/ui/const_ref.cc', message)
        self.assertIn('chrome/browser/ui/fwd_decl.h', message)
        self.assertIn('chrome/browser/ui/struct_decl.h', message)
        self.assertIn('chrome/browser/ui/static_method.cc', message)
        self.assertIn('chrome/browser/ui/unique_ptr.cc', message)
        self.assertIn('chrome/browser/ui/raw_ptr.h', message)
        self.assertIn('chrome/browser/ui/raw_ref.h', message)
        self.assertIn('chrome/browser/ui/scoped_refptr.h', message)
        self.assertIn('chrome/browser/ui/cast.cc', message)
        self.assertIn('chrome/browser/ui/new_expr.cc', message)
        self.assertIn('chrome/browser/ui/func_sig.h', message)

    def testWarnsOnBannedHeadersInDesktopUnitTests(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile(
                'chrome/browser/ui/foo_unittest.cc',
                ['#include "chrome/test/base/browser_with_test_window_test.h"'
                 ]),
            MockAffectedFile(
                'chrome/browser/ui/views/bar_unittest.cc',
                ['#include "chrome/browser/ui/views/frame/test_with_browser_view.h"'
                 ]),
            MockAffectedFile(
                'chrome/browser/baz_unittest.cc',
                ['#include "chrome/test/base/test_browser_window.h"']),
            MockAffectedFile(
                'chrome/browser/qux_unittest.cc',
                ['#include "chrome/browser/ui/browser.h"']),
            MockAffectedFile(
                'chrome/browser/ui/header_unittest.h',
                ['#include <chrome/browser/ui/browser.h>']),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('warning', results[0].type)
        message = results[0].message
        self.assertIn('chrome/browser/ui/foo_unittest.cc', message)
        self.assertIn('chrome/browser/ui/views/bar_unittest.cc', message)
        self.assertIn('chrome/browser/baz_unittest.cc', message)
        self.assertIn('chrome/browser/qux_unittest.cc', message)
        self.assertIn('chrome/browser/ui/header_unittest.h', message)

    def testWarnsOnBannedFixturesInDesktopUnitTests(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile(
                'chrome/browser/ui/foo_unittest.cc',
                ['class FooTest : public BrowserWithTestWindowTest {};']),
            MockAffectedFile(
                'chrome/browser/ui/bar_unittest.cc',
                ['class BarTest : public TestWithBrowserView {};']),
            MockAffectedFile(
                'chrome/browser/ui/baz_unittest.cc',
                ['class BazTest : public ::BrowserWithTestWindowTest {};']),
            MockAffectedFile(
                'chrome/browser/ui/qux_unittest.cc',
                ['class QuxTest : public ::TestWithBrowserView {};']),
            MockAffectedFile(
                'chrome/browser/ui/multiple_inheritance_unittest.cc',
                ['class MultiTest : public testing::Test, public BrowserWithTestWindowTest {};'
                 ]),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('warning', results[0].type)
        message = results[0].message
        self.assertIn('chrome/browser/ui/foo_unittest.cc', message)
        self.assertIn('chrome/browser/ui/bar_unittest.cc', message)
        self.assertIn('chrome/browser/ui/baz_unittest.cc', message)
        self.assertIn('chrome/browser/ui/qux_unittest.cc', message)
        self.assertIn(
            'chrome/browser/ui/multiple_inheritance_unittest.cc',
            message)

    def testAshAndChromeOsExcluded(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile(
                'chrome/browser/ash/foo_unittest.cc',
                [
                    '#include "chrome/test/base/browser_with_test_window_test.h"',
                    'class FooTest : public BrowserWithTestWindowTest {};',
                ]),
            MockAffectedFile(
                'chrome/browser/ui/ash/bar_unittest.cc',
                [
                    '#include "chrome/browser/ui/views/frame/test_with_browser_view.h"',
                    'class BarTest : public TestWithBrowserView {};',
                ]),
            MockAffectedFile(
                'chrome/browser/chromeos/baz_unittest.cc',
                [
                    '#include "chrome/test/base/test_browser_window.h"',
                ]),
            MockAffectedFile(
                'chrome/browser/ui/chromeos/qux_unittest.cc',
                [
                    'class QuxTest : public BrowserWithTestWindowTest {};',
                ]),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNonUnittestFilesPassBannedTestFixturesAndHeaders(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile(
                'chrome/browser/ui/foo_browsertest.cc',
                [
                    '#include "chrome/test/base/browser_with_test_window_test.h"',
                    'class FooTest : public BrowserWithTestWindowTest {};',
                ]),
            MockAffectedFile(
                'chrome/browser/ui/foo_interactive_uitest.cc',
                [
                    '#include "chrome/test/base/browser_with_test_window_test.h"',
                ]),
            MockAffectedFile(
                'chrome/browser/ui/foo.cc',
                [
                    '#include "chrome/test/base/test_browser_window.h"',
                    '#include "chrome/browser/ui/views/frame/test_with_browser_view.h"',
                ]),
            MockAffectedFile(
                'chrome/browser/ui/foo.h',
                [
                    'class Foo : public BrowserWithTestWindowTest {};',
                ]),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testDoesNotWarnOnBrowserWindowInterface(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile(
                'chrome/browser/ui/bwi_include.cc',
                ['#include "chrome/browser/ui/browser_window/public/'
                 'browser_window_interface.h"']
            ),
            MockAffectedFile(
                'chrome/browser/ui/bwi_ptr.cc',
                ['BrowserWindowInterface* browser_window = nullptr;']
            ),
            MockAffectedFile('chrome/browser/ui/bwi_ref.cc',
                             ['const BrowserWindowInterface& bwi = *window;']),
            MockAffectedFile('chrome/browser/ui/bwi_fwd.h',
                             ['class BrowserWindowInterface;']),
            MockAffectedFile('chrome/browser/ui/bwi_uptr.h',
                             ['std::unique_ptr<BrowserWindowInterface> bwi;']),
            MockAffectedFile('chrome/browser/ui/bwi_rawptr.h',
                             ['raw_ptr<BrowserWindowInterface> bwi_;']),
            MockAffectedFile('chrome/browser/ui/bwi_enum.cc',
                             ['BrowserWindowInterface::Type type;']),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testDoesNotWarnOnOtherBrowserPrefixedClasses(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/ui/other_classes.cc', [
                'BrowserWindow* window = nullptr;',
                'BrowserView* view = nullptr;',
                'BrowserList* list = nullptr;',
                'BrowserContext* context = nullptr;',
                'BrowserThread::UI;',
                'BrowserWindowFeatures* features = nullptr;',
                'InProcessBrowserTest* test = nullptr;',
                'browser_->DoSomething();',
                'browser()->DoSomething();',
                'my_browser->DoSomething();',
            ]),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testDoesNotWarnOnModernHeadersAndFixtures(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile(
                'chrome/browser/ui/modern_unittest.cc',
                [
                    '#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"',
                    '#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"',
                    '#include "chrome/browser/ui/tabs/public/tab_interface.h"',
                    '#include "content/public/test/test_renderer_host.h"',
                    'class ModernTest : public ChromeRenderViewHostTestHarness {};',
                    'class StandardTest : public testing::Test {};',
                ]),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testDoesNotWarnOnCommentsAndStrings(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/ui/comments_and_strings.cc', [
                '// #include "chrome/browser/ui/browser.h"',
                '// Browser* browser = nullptr;',
                '/* Browser* browser = nullptr; */',
                ' * Use Browser here',
                'void Foo();  // TODO: Browser refactoring',
                'LOG(INFO) << "Browser started";',
                'base::UmaHistogramBoolean("Browser.State", true);',
                'std::string name = "Browser";',
                '// #include "chrome/test/base/browser_with_test_window_test.h"',
                '// class FooTest : public BrowserWithTestWindowTest {};',
                '/* #include "chrome/browser/ui/views/frame/test_with_browser_view.h" */',
                ' * class BarTest : public TestWithBrowserView {};',
                'int x = 0; // #include "chrome/test/base/test_browser_window.h"',
                'int y = 0; // class BazTest : public BrowserWithTestWindowTest {};',
            ]),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testDoesNotWarnOnNocheck(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/ui/nocheck_include.cc', [
                '#include "chrome/browser/ui/browser.h"  // nocheck',
            ]),
            MockAffectedFile('chrome/browser/ui/nocheck_class.cc', [
                'Browser* browser = nullptr;  // nocheck',
            ]),
            MockAffectedFile('chrome/browser/ui/nocheck_unittest.cc', [
                '#include "chrome/test/base/browser_with_test_window_test.h"  // nocheck',
                '#include "chrome/browser/ui/views/frame/test_with_browser_view.h" // nocheck',
                '#include "chrome/test/base/test_browser_window.h" //nocheck',
                'class FooTest : public BrowserWithTestWindowTest {};  // nocheck',
                'class BarTest : public TestWithBrowserView {}; // nocheck',
            ]),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testDoesNotWarnOnExcludedFiles(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/ui/browser.h',
                             ['class Browser;']),
            MockAffectedFile('chrome/browser/ui/browser.cc',
                             ['Browser::Browser() {}']),
            MockAffectedFile(
                ('chrome/browser/ui/browser_window/public/'
                 'browser_window_interface.h'),
                ['class Browser;']
            ),
            MockAffectedFile(
                ('chrome/browser/ui/browser_window/public/'
                 'create_browser_window.h'),
                ['class Browser;']
            ),
            MockAffectedFile(
                ('chrome/browser/ui/browser_window/internal/'
                 'create_browser_window_non_android.cc'),
                ['#include "chrome/browser/ui/browser.h"',
                 'return Browser::Create(std::move(create_params));']
            ),
            MockAffectedFile(
                'chrome/browser/ui/views/frame/browser_window_factory.cc',
                ['#include "chrome/browser/ui/browser.h"']
            ),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testDoesNotWarnOnNonCppFiles(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/BUILD.gn',
                             ['sources = [ "browser.h" ]']),
            MockAffectedFile('chrome/browser/README.md',
                             ['Use Browser class']),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoChangesProducesNoWarnings(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile('chrome/browser/ui/old_file.cc', []),
            MockAffectedFile('chrome/browser/ui/old_unittest.cc', []),
        ]
        results = PRESUBMIT._CheckNoNewBrowserUsage(input_api, MockOutputApi())
        self.assertEqual(0, len(results))


if __name__ == '__main__':
    unittest.main()
