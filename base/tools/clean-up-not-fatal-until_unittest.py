#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib.util
from pathlib import Path
import textwrap
import unittest

_SCRIPT_PATH = Path(__file__).parent / 'clean-up-not-fatal-until.py'
_SPEC = importlib.util.spec_from_file_location('clean_up_script', _SCRIPT_PATH)
clean_up_script = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(clean_up_script)


class CleanUpNotFatalUntilTest(unittest.TestCase):

    def assertCleanUp(self, before, after):
        self.assertEqual(
            clean_up_script.clean_up(textwrap.dedent(before), ['140']),
            textwrap.dedent(after))

    def test_remove_header_entries(self):
        header = 'enum {\n  M139 = 139,\n  M140 = 140,\n  M141 = 141,\n};\n'
        self.assertEqual(clean_up_script.remove_header_entries(header, 140),
                         ('enum {\n  M141 = 141,\n};\n', ['139', '140']))

    def test_last_argument(self):
        self.assertCleanUp('CHECK(a, base::NotFatalUntil::M140);', 'CHECK(a);')
        self.assertCleanUp('CHECK_EQ(a, b, NotFatalUntil::M140) << "Message";',
                           'CHECK_EQ(a, b) << "Message";')
        self.assertCleanUp('CHECK(a,\n      base::NotFatalUntil::M140);',
                           'CHECK(a);')

    def test_only_argument(self):
        self.assertCleanUp('CHECK_IS_TEST(base::NotFatalUntil::M140);',
                           'CHECK_IS_TEST();')

    def test_retained_milestones(self):
        for code in ('CHECK(a, base::NotFatalUntil::M141);',
                     'CHECK(a, base::NotFatalUntil::M1400);'):
            self.assertCleanUp(code, code)

    def test_include_cleanup(self):
        self.assertCleanUp(
            '''
            #include "base/not_fatal_until.h"
            CHECK(a, base::NotFatalUntil::M140);
            ''', '''
            CHECK(a);
            ''')
        self.assertCleanUp(
            '''
            #include "base/not_fatal_until.h"
            CHECK(a, base::NotFatalUntil::M140);
            CHECK(b, base::NotFatalUntil::M141);
            ''', '''
            #include "base/not_fatal_until.h"
            CHECK(a);
            CHECK(b, base::NotFatalUntil::M141);
            ''')

    def test_unreachable_code_after_notreached(self):
        self.assertCleanUp(
            '''
            if (!ptr) {
              NOTREACHED(base::NotFatalUntil::M140);
              return nullptr;
            }
            ''', '''
            if (!ptr) {
              NOTREACHED();
            }
            ''')

    def test_unreachable_code_after_check_false(self):
        self.assertCleanUp(
            '''
            if (!ptr) {
              CHECK(false, base::NotFatalUntil::M140);
              return nullptr;
            }
            ''', '''
            if (!ptr) {
              CHECK(false);
            }
            ''')

    def test_unreachable_code_in_switch(self):
        self.assertCleanUp(
            '''
            switch (value) {
              case 1:
                NOTREACHED(base::NotFatalUntil::M140);
                break;

                // Comment about case 2.
              case 2:
                return 2;
            }
            ''', '''
            switch (value) {
              case 1:
                NOTREACHED();

                // Comment about case 2.
              case 2:
                return 2;
            }
            ''')

    def test_unreachable_code_after_multiline_statement(self):
        self.assertCleanUp(
            '''
            void F() {
              NOTREACHED(base::NotFatalUntil::M140)
                  << "Message";

              // Comment.
              if (a) {
                B();
              }
            }
            ''', '''
            void F() {
              NOTREACHED()
                  << "Message";
            }
            ''')

    def test_reachable_code(self):
        for code in (
                '''
                if (!ptr)
                  NOTREACHED(base::NotFatalUntil::M140);
                return ptr;
                ''',
                '''
                CHECK(ptr, base::NotFatalUntil::M140);
                return ptr;
                ''',
                '''
                void F() {
                  NOTREACHED(base::NotFatalUntil::M140);
                #if BUILDFLAG(IS_WIN)
                  return;
                #endif
                }
                ''',
        ):
            self.assertCleanUp(code,
                               code.replace(', base::NotFatalUntil::M140', '')
                               .replace('base::NotFatalUntil::M140', ''))


if __name__ == '__main__':
    unittest.main()
