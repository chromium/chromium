#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for fetch_icon.py and its pipeline modules."""

import io
import pathlib
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import fetch_icon  # noqa: E402  # pytype: disable=import-error
import skiafy  # noqa: E402  # pytype: disable=import-error
import svg_optimizer  # noqa: E402  # pytype: disable=import-error
import svg_resizer  # noqa: E402  # pytype: disable=import-error

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


class SvgResizerTest(unittest.TestCase):
    """Tests for svg_resizer.py (validation, scaling, degree elevation)."""

    def test_cairo_collinear_line_and_close_simplification(self) -> None:
        svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20">'
            '<path d="M2 2L4 4L6 6L2 6L2 4L2 2Z"/></svg>'
        )
        elevated = svg_resizer.resize_and_elevate_to_path_data(svg, 20)
        self.assertEqual(
            [(item.command, item.args) for item in elevated],
            [
                ('M', [2.0, 2.0]),
                ('L', [6.0, 6.0]),
                ('L', [2.0, 6.0]),
                ('Z', []),
            ],
        )

    def test_quadratic_to_cubic_elevation(self) -> None:
        svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 30 30">'
            '<path d="M0 0Q15 30 30 0Z"/></svg>'
        )
        elevated = svg_resizer.resize_and_elevate_to_path_data(svg, 30)
        self.assertEqual(
            [(item.command, item.args) for item in elevated],
            [
                ('M', [0.0, 0.0]),
                ('C', [10.0, 20.0, 20.0, 20.0, 30.0, 0.0]),
                ('Z', []),
            ],
        )

    def test_rejects_non_path_or_multiple_paths(self) -> None:
        for primitive in (
            '<circle cx="10" cy="10" r="5"/>',
            '<rect x="2" y="2" width="16" height="16"/>',
            '<ellipse cx="10" cy="10" rx="5" ry="3"/>',
            '<g><path d="M0 0h20v20H0Z"/></g>',
        ):
            svg = (
                '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20">'
                f'{primitive}<path d="M0 0h20v20H0Z"/></svg>'
            )
            with self.assertRaises(ValueError):
                svg_resizer.resize_and_elevate_to_path_data(svg, 20)

        multi_path_svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20">'
            '<path d="M0 0h10v10H0Z"/><path d="M10 10h10v10H10Z"/></svg>'
        )
        with self.assertRaises(ValueError):
            svg_resizer.resize_and_elevate_to_path_data(multi_path_svg, 20)

    def test_infer_svg_size_rejects_oversized_viewbox(self) -> None:
        unscaled_svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" '
            'viewBox="0 -960 960 960"><path d="M120-120v-720h720v720H120Z"/>'
            '</svg>'
        )
        with self.assertRaises(ValueError):
            svg_resizer.infer_svg_size(unscaled_svg)

    def test_reads_exact_attributes_and_ignores_comments(self) -> None:
        svg = (
            '<!-- <svg viewBox="0 0 99 99"> -->'
            '<svg xmlns="http://www.w3.org/2000/svg" '
            'xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" '
            'stroke-width="2" width="24" height="24" viewBox="0 0 24 24">'
            '<path inkscape:original-d="M9 9h1" d="M2 2h4"/></svg>'
        )
        self.assertEqual(svg_resizer.infer_svg_size(svg), 24)
        elevated = svg_resizer.resize_and_elevate_to_path_data(svg, 24)
        self.assertEqual(elevated[0].args, [2.0, 2.0])

    def test_rejects_malformed_or_non_square_bounds(self) -> None:
        for svg_attrs in (
            'width="20"',
            'viewBox="0 0 20"',
            'viewBox="0 0 16 24"',
            'width="16" height="24" viewBox="0 0 24 24"',
            'width="128" height="128" viewBox="0 0 24 24"',
        ):
            svg = (
                f'<svg xmlns="http://www.w3.org/2000/svg" {svg_attrs}>'
                '<path d="M2 2h4v4H2Z"/></svg>'
            )
            with self.assertRaises(ValueError, msg=svg_attrs):
                svg_resizer.infer_svg_size(svg)

    def test_rejects_unsupported_attributes_and_path_data(self) -> None:
        for path_attrs in (
            'd="M2 2h4v4H2Z" transform="rotate(45)"',
            'd="M2 2h4v4H2Z" fill="none" stroke="currentColor"',
            'd="M2 2h4v4H2Z" style="stroke:#000"',
            'd="M2 2A8 8 0 1 0 18 2Z"',
            'd="M2 2L18"',
        ):
            svg = (
                '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20">'
                f'<path {path_attrs}/></svg>'
            )
            with self.assertRaises(ValueError, msg=path_attrs):
                svg_resizer.resize_and_elevate_to_path_data(svg, 20)


class SvgOptimizerTest(unittest.TestCase):
    """Tests for svg_optimizer.py (SVGO convertPathData optimization)."""

    def test_optimizes_axis_aligned_lines_and_closing_segment(self) -> None:
        path_data = [
            svg_resizer.PathItem('M', [3.0, 3.0]),
            svg_resizer.PathItem('L', [15.0, 3.0]),
            svg_resizer.PathItem('L', [15.0, 15.0]),
            svg_resizer.PathItem('L', [3.0, 15.0]),
            svg_resizer.PathItem('Z', []),
        ]
        optimized = svg_optimizer.optimize_path_data(path_data)
        self.assertEqual(
            [(item.command, item.args) for item in optimized],
            [
                ('M', [3.0, 3.0]),
                ('h', [12.0]),
                ('v', [12.0]),
                ('H', [3.0]),
                ('Z', []),
            ],
        )


class SkiafyTest(unittest.TestCase):
    """Tests for skiafy.py (.icon directive serialization)."""

    def test_add_16px_exact_conversion(self) -> None:
        block = fetch_icon.svg_to_icon_block(ADD_500_ROND100_20PX_SVG, 16)
        self.assertEqual(block, EXPECTED_ADD_16PX_BLOCK)

    def test_preserves_close_when_curve_returns_to_subpath_start(self) -> None:
        svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 -960 960 960">'
            '<path d="M480-96q-160 0-272-112T96-480q0-160 112-272t272-112'
            'q160 0 272 112t112 272q0 160-112 272T480-96Z"/></svg>'
        )
        block = fetch_icon.svg_to_icon_block(svg, 20)
        self.assertTrue(block.endswith('\nCLOSE'))

    def test_strips_trailing_zero_move_before_close(self) -> None:
        path_data = [
            svg_resizer.PathItem('M', [2.0, 2.0]),
            svg_resizer.PathItem('h', [16.0]),
            svg_resizer.PathItem('v', [16.0]),
            svg_resizer.PathItem('H', [2.0]),
            svg_resizer.PathItem('Z', []),
            svg_resizer.PathItem('m', [0.0, 0.0]),
        ]
        block = skiafy.path_data_to_icon_block(path_data, 20)
        self.assertNotIn('R_MOVE_TO, 0, 0,', block)
        self.assertTrue(block.endswith('\nCLOSE'))

    def test_evenodd_fill_rule_omits_fill_rule_nonzero(self) -> None:
        svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20">'
            '<path fill-rule="evenodd" d="M2 2h16v16H2ZM6 6h8v8H6Z"/></svg>'
        )
        block = fetch_icon.svg_to_icon_block(svg, 20)
        self.assertTrue(block.startswith('CANVAS_DIMENSIONS, 20,\nMOVE_TO,'))


class CliIntegrationTest(unittest.TestCase):
    """Tests for fetch_icon.py CLI orchestration."""

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

    def test_custom_svg_single_size_and_inferred_size(self) -> None:
        custom_svg = (
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 18 18">'
            '<path d="M3 3h12v12H3Z"/></svg>'
        )
        with mock.patch.object(
            pathlib.Path, 'read_text', return_value=custom_svg
        ):
            stdout_inferred = io.StringIO()
            with mock.patch('sys.stdout', stdout_inferred):
                rc = fetch_icon.main(['--svg', 'custom.svg'])
            self.assertEqual(rc, 0)
            self.assertIn(
                'CANVAS_DIMENSIONS, 18,\nFILL_RULE_NONZERO,\nMOVE_TO, 3, 3,',
                stdout_inferred.getvalue(),
            )

            stdout_custom = io.StringIO()
            with mock.patch('sys.stdout', stdout_custom):
                rc = fetch_icon.main(['--svg', 'custom.svg', '--sizes', '14'])
            self.assertEqual(rc, 0)
            self.assertIn('CANVAS_DIMENSIONS, 14,', stdout_custom.getvalue())

            for sizes in (['14', '32'], ['960']):
                with mock.patch('sys.stderr', io.StringIO()):
                    rc = fetch_icon.main(
                        ['--svg', 'custom.svg', '--sizes', *sizes]
                    )
                self.assertEqual(rc, 1, msg=sizes)


if __name__ == '__main__':
    unittest.main()
