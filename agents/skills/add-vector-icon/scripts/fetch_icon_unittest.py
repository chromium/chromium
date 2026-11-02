#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for fetch_icon.py."""

import io
import pathlib
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import fetch_icon  # noqa: E402  # pytype: disable=import-error

# Real Google Symbols SVG for 'add' (wght500rond100, 20px optical size).
ADD_500_ROND100_20PX_SVG = (
    '<svg xmlns="http://www.w3.org/2000/svg" height="20" '
    'viewBox="0 -960 960 960" width="20">'
    '<path d="M438.5-438.5H273.37q-17.45 0-29.48-11.96-12.02-11.97-'
    '12.02-29.33t12.02-29.54q12.03-12.17 29.48-12.17H438.5v-165.13q0-'
    '17.45 11.96-29.48 11.97-12.02 29.33-12.02t29.54 12.02q12.17 12.03 '
    '12.17 29.48v165.13h165.13q17.45 0 29.48 11.96 12.02 11.97 12.02 '
    '29.33t-12.02 29.54q-12.03 12.17-29.48 12.17H521.5v165.13q0 '
    '17.45-11.96 29.48-11.97 12.02-29.33 12.02t-29.54-12.02q-12.17-'
    '12.03-12.17-29.48V-438.5Z"/></svg>'
)

EXPECTED_ADD_16PX_BLOCK = """\
CANVAS_DIMENSIONS, 16,
FILL_RULE_NONZERO,
MOVE_TO, 7.31f, 8.69f,
H_LINE_TO, 4.56f,
R_ARC_TO, 0.67f, 0.67f, 0, 0, 1, -0.49f, -0.2f,
R_ARC_TO, 0.66f, 0.66f, 0, 0, 1, -0.2f, -0.49f,
R_ARC_TO, 0.67f, 0.67f, 0, 0, 1, 0.2f, -0.49f,
R_ARC_TO, 0.66f, 0.66f, 0, 0, 1, 0.49f, -0.2f,
R_H_LINE_TO, 2.75f,
V_LINE_TO, 4.56f,
R_CUBIC_TO, 0, -0.19f, 0.07f, -0.36f, 0.2f, -0.49f,
R_ARC_TO, 0.66f, 0.66f, 0, 0, 1, 0.49f, -0.2f,
R_ARC_TO, 0.67f, 0.67f, 0, 0, 1, 0.49f, 0.2f,
R_ARC_TO, 0.66f, 0.66f, 0, 0, 1, 0.2f, 0.49f,
R_V_LINE_TO, 2.75f,
R_H_LINE_TO, 2.75f,
R_CUBIC_TO, 0.19f, 0, 0.36f, 0.07f, 0.49f, 0.2f,
R_ARC_TO, 0.66f, 0.66f, 0, 0, 1, 0.2f, 0.49f,
R_ARC_TO, 0.67f, 0.67f, 0, 0, 1, -0.2f, 0.49f,
R_ARC_TO, 0.66f, 0.66f, 0, 0, 1, -0.49f, 0.2f,
H_LINE_TO, 8.69f,
R_V_LINE_TO, 2.75f,
R_ARC_TO, 0.67f, 0.67f, 0, 0, 1, -0.2f, 0.49f,
R_ARC_TO, 0.66f, 0.66f, 0, 0, 1, -0.49f, 0.2f,
R_ARC_TO, 0.67f, 0.67f, 0, 0, 1, -0.49f, -0.2f,
R_ARC_TO, 0.66f, 0.66f, 0, 0, 1, -0.2f, -0.49f,
CLOSE"""


class SymbolUrlTest(unittest.TestCase):
    """Tests for Google Symbols URL construction."""

    def test_symbol_urls(self) -> None:
        self.assertEqual(
            fetch_icon.build_symbol_url('account_circle', 24),
            'https://fonts.gstatic.com/s/i/short-term/release/googlesymbols/'
            'account_circle/rond100/24px.svg',
        )
        self.assertEqual(
            fetch_icon.build_symbol_url('account_circle', 20),
            'https://fonts.gstatic.com/s/i/short-term/release/googlesymbols/'
            'account_circle/rond100/20px.svg',
        )
        self.assertEqual(
            fetch_icon.build_symbol_url('account_circle', 16),
            'https://fonts.gstatic.com/s/i/short-term/release/googlesymbols/'
            'account_circle/wght500rond100/20px.svg',
        )
        self.assertEqual(
            fetch_icon.build_symbol_url('account_circle.icon', 24, fill=True),
            'https://fonts.gstatic.com/s/i/short-term/release/googlesymbols/'
            'account_circle/fill1rond100/24px.svg',
        )
        self.assertEqual(
            fetch_icon.build_symbol_url('account_circle', 20, fill=True),
            'https://fonts.gstatic.com/s/i/short-term/release/googlesymbols/'
            'account_circle/fill1rond100/20px.svg',
        )
        self.assertEqual(
            fetch_icon.build_symbol_url('account_circle', 16, fill=True),
            'https://fonts.gstatic.com/s/i/short-term/release/googlesymbols/'
            'account_circle/wght500fill1rond100/20px.svg',
        )
        with self.assertRaises(ValueError):
            fetch_icon.build_symbol_url('add', 18)
        with self.assertRaises(ValueError):
            fetch_icon.build_symbol_url('   ', 20)


class SvgToIconConversionTest(unittest.TestCase):
    """Tests for the SVG -> PathData -> SVGO -> Skiafy .icon pipeline."""

    def test_add_16px_exact_conversion(self) -> None:
        block = fetch_icon.svg_to_icon_block(ADD_500_ROND100_20PX_SVG, 16)
        self.assertEqual(block, EXPECTED_ADD_16PX_BLOCK)

    def test_cairo_collinear_line_and_close_simplification(self) -> None:
        # Collinear L segments in 24.8 fixed-point space should merge, and a
        # closing L back to the subpath start should be folded into Z.
        svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20">'
            '<path d="M2 2L4 4L6 6L2 6L2 4L2 2Z"/></svg>'
        )
        elevated = fetch_icon.resize_and_elevate_to_path_data(svg, 20)
        self.assertEqual(
            [(item.command, item.args) for item in elevated],
            [
                ('M', [2.0, 2.0]),
                ('L', [6.0, 6.0]),
                ('L', [2.0, 6.0]),
                ('Z', []),
            ],
        )

    def test_preserves_close_when_curve_returns_to_subpath_start(self) -> None:
        # When a subpath's final curve/arc returns to the subpath start,
        # Z must still be preserved so the resulting .icon block ends with
        # CLOSE (and multi-subpath icons emit CLOSE between subpaths).
        svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 -960 960 960">'
            '<path d="M480-96q-160 0-272-112T96-480q0-160 112-272t272-112'
            'q160 0 272 112t112 272q0 160-112 272T480-96Z"/></svg>'
        )
        block = fetch_icon.svg_to_icon_block(svg, 20)
        self.assertTrue(block.endswith('\nCLOSE'))

    def test_strips_trailing_zero_move_before_close(self) -> None:
        svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 -960 960 960">'
            '<path d="M120-120v-720h720v720H120Zm-240-240Zm240 240Z"/></svg>'
        )
        block = fetch_icon.svg_to_icon_block(svg, 20)
        self.assertNotIn('R_MOVE_TO, 0, 0,', block)
        self.assertTrue(block.endswith('\nCLOSE'))

    @mock.patch('fetch_icon.fetch_svg')
    def test_main_fetches_descending_and_writes_stdout(
        self, mock_fetch: mock.MagicMock
    ) -> None:
        mock_fetch.return_value = ADD_500_ROND100_20PX_SVG
        stdout = io.StringIO()
        with mock.patch('sys.stdout', stdout):
            rc = fetch_icon.main(
                ['--icon', 'add', '--sizes', '16', '24', '20', '16']
            )
        self.assertEqual(rc, 0)
        self.assertEqual(
            [call.args[0] for call in mock_fetch.call_args_list],
            [
                'https://fonts.gstatic.com/s/i/short-term/release/'
                'googlesymbols/add/rond100/24px.svg',
                'https://fonts.gstatic.com/s/i/short-term/release/'
                'googlesymbols/add/rond100/20px.svg',
                'https://fonts.gstatic.com/s/i/short-term/release/'
                'googlesymbols/add/wght500rond100/20px.svg',
            ],
        )
        combined = stdout.getvalue()
        idx_24 = combined.index('CANVAS_DIMENSIONS, 24,')
        idx_20 = combined.index('CANVAS_DIMENSIONS, 20,')
        idx_16 = combined.index('CANVAS_DIMENSIONS, 16,')
        self.assertLess(idx_24, idx_20)
        self.assertLess(idx_20, idx_16)
        self.assertIn(EXPECTED_ADD_16PX_BLOCK, combined)
        self.assertTrue(combined.endswith('CLOSE\n'))


if __name__ == '__main__':
    unittest.main()
