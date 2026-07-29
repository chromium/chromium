#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import os
import sys
import tempfile
import unittest

import sort_headers


class SortHeadersTest(unittest.TestCase):

    def test_primary_header(self):
        content = ('// Copyright 2026\n'
                   '\n'
                   '#include <vector>\n'
                   '#include "foo/bar.h"\n'
                   '#include "base/logging.h"\n')
        expected = ('// Copyright 2026\n'
                    '\n'
                    '#include "foo/bar.h"\n'
                    '\n'
                    '#include <vector>\n'
                    '\n'
                    '#include "base/logging.h"\n')
        result = sort_headers.sort_file_content(content, "foo/bar.cc")
        self.assertEqual(result, expected)

    def test_primary_header_suffix(self):
        content = ('#include <vector>\n'
                   '#include "foo/bar.h"\n'
                   '#include "base/logging.h"\n')
        expected = ('#include "foo/bar.h"\n'
                    '\n'
                    '#include <vector>\n'
                    '\n'
                    '#include "base/logging.h"\n')
        result = sort_headers.sort_file_content(content, "foo/bar_win.cc")
        self.assertEqual(result, expected)

    def test_primary_header_compound_suffix(self):
        content = ('#include <vector>\n'
                   '#include "foo/bar.h"\n'
                   '#include "base/logging.h"\n')
        expected = ('#include "foo/bar.h"\n'
                    '\n'
                    '#include <vector>\n'
                    '\n'
                    '#include "base/logging.h"\n')
        result = sort_headers.sort_file_content(content,
                                                "foo/bar_win_unittest.cc")
        self.assertEqual(result, expected)

    def test_primary_header_closest_match(self):
        content = (
            '#include "components/soda/soda_installer.h"\n'
            '#include "chrome/browser/accessibility/soda_installer_impl.h"\n'
            '#include "base/logging.h"\n')
        expected = (
            '#include "chrome/browser/accessibility/soda_installer_impl.h"\n'
            '\n'
            '#include "base/logging.h"\n'
            '#include "components/soda/soda_installer.h"\n')
        result = sort_headers.sort_file_content(
            content,
            "chrome/browser/accessibility/soda_installer_impl_unittest.cc")
        self.assertEqual(result, expected)

    def test_c_and_cxx_system_headers_split(self):
        content = ('#include <vector>\n'
                   '#include <jni.h>\n'
                   '#include <string>\n'
                   '#include <stdlib.h>\n')
        expected = ('#include <jni.h>\n'
                    '#include <stdlib.h>\n'
                    '\n'
                    '#include <string>\n'
                    '#include <vector>\n')
        result = sort_headers.sort_file_content(content, "foo.cc")
        self.assertEqual(result, expected)

    def test_windows_special_headers_order(self):
        content = ('#include <windows.h>\n'
                   '#include <objbase.h>\n'
                   '#include <winsock2.h>\n'
                   '#include <initguid.h>\n')
        expected = ('#include <objbase.h>\n'
                    '\n'
                    '#include <initguid.h>\n'
                    '#include <windows.h>\n'
                    '#include <winsock2.h>\n')
        result = sort_headers.sort_file_content(content, "foo_win.cc")
        self.assertEqual(result, expected)

    def test_preserve_else_chain_when_no_collision(self):
        content = ('// Some comment\n'
                   '#if BUILDFLAG(IS_CHROMEOS)\n'
                   '#include "ash.h"\n'
                   '#else\n'
                   '#include "base.h"\n'
                   '#endif  // BUILDFLAG(IS_CHROMEOS)\n')
        expected = ('// Some comment\n'
                    '#if BUILDFLAG(IS_CHROMEOS)\n'
                    '#include "ash.h"\n'
                    '#else\n'
                    '#include "base.h"\n'
                    '#endif  // BUILDFLAG(IS_CHROMEOS)\n')
        result = sort_headers.sort_file_content(content, "foo.cc")
        self.assertEqual(result, expected)

    def test_guarded_system_includes_above_user_includes(self):
        content = ('#if !BUILDFLAG(IS_ANDROID)\n'
                   '#include "base/containers/flat_map.h"\n'
                   '#include <list>\n'
                   '#endif\n')
        expected = ('#if !BUILDFLAG(IS_ANDROID)\n'
                    '#include <list>\n'
                    '\n'
                    '#include "base/containers/flat_map.h"\n'
                    '#endif\n')
        result = sort_headers.sort_file_content(content, "foo.h")
        self.assertEqual(result, expected)

    def test_guarded_headers_combine(self):
        content = ('#include "base/check.h"\n'
                   '#if BUILDFLAG(IS_WIN)\n'
                   '#include "base/logging.h"\n'
                   '#endif\n'
                   '#if BUILDFLAG(IS_WIN)\n'
                   '#include "base/macros.h"\n'
                   '#endif\n')
        expected = ('#include "base/check.h"\n'
                    '\n'
                    '#if BUILDFLAG(IS_WIN)\n'
                    '#include "base/logging.h"\n'
                    '#include "base/macros.h"\n'
                    '#endif\n')
        result = sort_headers.sort_file_content(content, "foo.cc")
        self.assertEqual(result, expected)

    def test_special_win_headers_block_separated_by_blank_line(self):
        content = ('// Must be before <uiautomation.h>\n'
                   '#include <objbase.h>\n'
                   '\n'
                   '#include <uiautomation.h>\n'
                   '\n'
                   '#include <cstdint>\n'
                   '#include <string>\n'
                   '#include <vector>\n')
        expected = ('// Must be before <uiautomation.h>\n'
                    '#include <objbase.h>\n'
                    '\n'
                    '#include <cstdint>\n'
                    '#include <string>\n'
                    '#include <vector>\n'
                    '\n'
                    '#include <uiautomation.h>\n')
        result = sort_headers.sort_file_content(content, "foo.h")
        self.assertEqual(result, expected)

    def test_comment_barrier(self):
        content = ('#include "b.h"\n'
                   '#include "a.h"\n'
                   '\n'
                   '// Section comment\n'
                   '#include "d.h"\n'
                   '#include "c.h"\n')
        expected = ('#include "a.h"\n'
                    '#include "b.h"\n'
                    '\n'
                    '// Section comment\n'
                    '#include "c.h"\n'
                    '#include "d.h"\n')
        result = sort_headers.sort_file_content(content, "foo.cc")
        self.assertEqual(result, expected)

    def test_preamble_comments(self):
        content = ('// Preamble for b.h\n'
                   '#include "b.h"\n'
                   '#include "a.h"\n')
        expected = ('// Preamble for b.h\n'
                    '#include "a.h"\n'
                    '#include "b.h"\n')
        result = sort_headers.sort_file_content(content, "foo.cc")
        self.assertEqual(result, expected)

    def test_comment_inside_if_block_preserved(self):
        content = (
            '#include "ui/accessibility/ax_tree_update_forward.h"\n'
            '\n'
            '#if BUILDFLAG(ENABLE_PRINT_PREVIEW)\n'
            '// Causes circular dependencies with //chrome/browser/ui.\n'
            '#include "chrome/browser/ui/webui/print_preview/'
            'printer_handler.h"\n'
            '#endif\n')
        expected = (
            '#include "ui/accessibility/ax_tree_update_forward.h"\n'
            '\n'
            '#if BUILDFLAG(ENABLE_PRINT_PREVIEW)\n'
            '// Causes circular dependencies with //chrome/browser/ui.\n'
            '#include "chrome/browser/ui/webui/print_preview/'
            'printer_handler.h"\n'
            '#endif\n')
        result = sort_headers.sort_file_content(
            content, "chrome/browser/printing/print_view_manager_base.h")
        self.assertEqual(result, expected)

    def test_else_inline_comment_preserved(self):
        content = (
            '#if BUILDFLAG(IS_ANDROID)\n'
            '#include "ui/android/ui_android_features.h"\n'
            '#else  // BUILDFLAG(IS_ANDROID)\n'
            '#include "chrome/browser/media/router/media_router_feature.h"\n'
            '#endif\n')
        expected = (
            '#if BUILDFLAG(IS_ANDROID)\n'
            '#include "ui/android/ui_android_features.h"\n'
            '#else  // BUILDFLAG(IS_ANDROID)\n'
            '#include "chrome/browser/media/router/media_router_feature.h"\n'
            '#endif\n')
        result = sort_headers.sort_file_content(
            content, "chrome/browser/about_flags.cc")
        self.assertEqual(result, expected)

    def test_order_constraint_comment_barrier(self):
        content = ('#include "base/files/file_path.h"\n'
                   '#include "content/public/browser/web_contents.h"\n'
                   '// Must come after all headers that specialize '
                   'FromJniType() / ToJniType().\n'
                   '#include "chrome/android/chrome_jni_headers/'
                   'WebAppLaunchHandler_jni.h"\n')
        expected = ('#include "base/files/file_path.h"\n'
                    '#include "content/public/browser/web_contents.h"\n'
                    '\n'
                    '// Must come after all headers that specialize '
                    'FromJniType() / ToJniType().\n'
                    '#include "chrome/android/chrome_jni_headers/'
                    'WebAppLaunchHandler_jni.h"\n')
        result = sort_headers.sort_file_content(
            content,
            "chrome/browser/android/webapps/web_app_launch_handler.cc")
        self.assertEqual(result, expected)

    def test_header_guard_preservation(self):
        content = ('// Copyright 2026\n'
                   '\n'
                   '#ifndef FOO_BAR_H_\n'
                   '#define FOO_BAR_H_\n'
                   '\n'
                   '#include "base/types/expected.h"\n'
                   '#include "base/test/gmock_expected_support.h"\n'
                   '\n'
                   'template <typename T>\n'
                   'class Foo {};\n'
                   '\n'
                   '#endif  // FOO_BAR_H_\n')
        expected = ('// Copyright 2026\n'
                    '\n'
                    '#ifndef FOO_BAR_H_\n'
                    '#define FOO_BAR_H_\n'
                    '\n'
                    '#include "base/test/gmock_expected_support.h"\n'
                    '#include "base/types/expected.h"\n'
                    '\n'
                    'template <typename T>\n'
                    'class Foo {};\n'
                    '\n'
                    '#endif  // FOO_BAR_H_\n')
        result = sort_headers.sort_file_content(content, "foo/bar.h")
        self.assertEqual(result, expected)

    def test_endif_comment_preservation(self):
        content = ('#if BUILDFLAG(IS_CHROMEOS)\n'
                   '#include "components/sync/base/features.h"\n'
                   '#include "chromeos/constants/chromeos_features.h"\n'
                   '#endif  // BUILDFLAG(IS_CHROMEOS)\n')
        expected = ('#if BUILDFLAG(IS_CHROMEOS)\n'
                    '#include "chromeos/constants/chromeos_features.h"\n'
                    '#include "components/sync/base/features.h"\n'
                    '#endif  // BUILDFLAG(IS_CHROMEOS)\n')
        result = sort_headers.sort_file_content(content, "foo.h")
        self.assertEqual(result, expected)

    def test_ignore_if_block_with_code(self):
        content = ('#include <memory>\n'
                   '#include "base/logging.h"\n'
                   '\n'
                   '#if !BUILDFLAG(IS_ANDROID)\n'
                   'namespace views {\n'
                   'class View;\n'
                   '}\n'
                   '#endif\n')
        expected = ('#include <memory>\n'
                    '\n'
                    '#include "base/logging.h"\n'
                    '\n'
                    '#if !BUILDFLAG(IS_ANDROID)\n'
                    'namespace views {\n'
                    'class View;\n'
                    '}\n'
                    '#endif\n')
        result = sort_headers.sort_file_content(content, "foo.h")
        self.assertEqual(result, expected)

    def test_code_if_block_after_includes(self):
        content = ('#include "ui/display/screen.h"\n'
                   '\n'
                   '// Comment before code #if\n'
                   '#if defined(ADDRESS_SANITIZER)\n'
                   '#define SLOW_BINARY\n'
                   '#endif\n')
        expected = ('#include "ui/display/screen.h"\n'
                    '\n'
                    '// Comment before code #if\n'
                    '#if defined(ADDRESS_SANITIZER)\n'
                    '#define SLOW_BINARY\n'
                    '#endif\n')
        result = sort_headers.sort_file_content(content, "foo.cc")
        self.assertEqual(result, expected)

    def test_if_block_with_unparsed_code_preserved(self):
        content = ('#include "b.h"\n'
                   '#include "a.h"\n'
                   '\n'
                   '#if BUILDFLAG(IS_ANDROID)\n'
                   '#include "c.h"\n'
                   '#pragma clang diagnostic push\n'
                   '#include "d.h"\n'
                   '#pragma clang diagnostic pop\n'
                   '#endif\n')
        expected = ('#include "a.h"\n'
                    '#include "b.h"\n'
                    '\n'
                    '#if BUILDFLAG(IS_ANDROID)\n'
                    '#include "c.h"\n'
                    '#pragma clang diagnostic push\n'
                    '#include "d.h"\n'
                    '#pragma clang diagnostic pop\n'
                    '#endif\n')
        result = sort_headers.sort_file_content(content, "foo.cc")
        self.assertEqual(result, expected)


if __name__ == "__main__":
    unittest.main()
