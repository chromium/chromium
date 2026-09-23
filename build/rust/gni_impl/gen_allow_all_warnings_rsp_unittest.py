#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.append(os.path.dirname(__file__))
from gen_allow_all_warnings_rsp import generate_rsp_contents

# A shortened sample of the output of `rustc -W help`.
RUSTC_W_HELP_SAMPLE = """\
Available lint options:
    -W <foo>           Warn about <foo>
    -A <foo>           Allow <foo>

Lint checks provided by rustc:

                              name  default level  meaning
                              ----  -------------  -------
                       dead-code  warn   detect unused, unexported items
                     missing-docs  allow  missing documentation
                       unsafe-code  allow  usage of `unsafe` code
              arithmetic-overflow  deny   arithmetic operation overflows

Lint groups provided by rustc:

                              name  sub-lints
                              ----  ---------
                          warnings  all lints that are set to issue warnings
                            unused  unused-imports, dead-code

Lint tools like Clippy can load additional lints and lint groups.
"""


class GenAllowAllWarningsRspTest(unittest.TestCase):
    def test_sample_output(self):
        # Note that:
        # * The lint names use `_` rather than `-`,
        # * `warn`-level lints are covered (`-Awarnings` is not sufficient -
        #   see the comments in `gen_allow_all_warnings_rsp.py`),
        # * Table headers, lint groups, and `unsafe_code` are skipped.
        self.assertEqual(
            generate_rsp_contents(RUSTC_W_HELP_SAMPLE),
            '-Aarithmetic_overflow\n-Adead_code\n-Amissing_docs\n',
        )

    def test_empty_input_is_an_error(self):
        with self.assertRaises(ValueError):
            generate_rsp_contents('')


if __name__ == '__main__':
    unittest.main()
