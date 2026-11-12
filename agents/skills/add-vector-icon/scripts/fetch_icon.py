#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fetches Google Symbols SVGs or converts custom single-path SVGs to .icon."""

import argparse
from collections.abc import Sequence
import pathlib
import sys
from typing import Optional
import urllib.error
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import skiafy  # noqa: E402  # pytype: disable=import-error
import svg_optimizer  # noqa: E402  # pytype: disable=import-error
import svg_resizer  # noqa: E402  # pytype: disable=import-error

GOOGLE_SYMBOLS_BASE_URL = (
    'https://fonts.gstatic.com/s/i/short-term/release/googlesymbols'
)
VALID_SYMBOL_SIZES = (16, 20, 24)


def svg_to_icon_block(svg_content: str, target_size: int) -> str:
    """Converts an SVG string into a single-size .icon representation block."""
    elevated = svg_resizer.resize_and_elevate_to_path_data(
        svg_content, target_size
    )
    optimized = svg_optimizer.optimize_path_data(elevated)
    return skiafy.path_data_to_icon_block(
        optimized,
        target_size,
        nonzero_fill=not svg_resizer.has_evenodd_fill_rule(svg_content),
    )


def build_symbol_url(icon_name: str, size: int, fill: bool = False) -> str:
    """Builds the Google Symbols URL for 100 ROND and 400 weight.

    For 16px icons, fetches the 20px optical size at 500 weight so that
    scaling from 20px to 16px (16/20 == 400/500) preserves 400 stroke weight.
    """
    if size not in VALID_SYMBOL_SIZES:
        raise ValueError(
            f'Unsupported Google Symbols size {size}px. '
            f'Supported sizes: {VALID_SYMBOL_SIZES}'
        )
    clean_name = (
        icon_name.strip().lower().removesuffix('.icon').removesuffix('_filled')
    )
    if not clean_name:
        raise ValueError('Icon name must not be empty.')

    opsz = 24 if size == 24 else 20
    parts: list[str] = []
    if size == 16:
        parts.append('wght500')
    if fill:
        parts.append('fill1')
    parts.append('rond100')
    suffix = ''.join(parts)
    return f'{GOOGLE_SYMBOLS_BASE_URL}/{clean_name}/{suffix}/{opsz}px.svg'


def fetch_svg(url: str) -> str:
    """Downloads an SVG from the given URL."""
    try:
        with urllib.request.urlopen(url, timeout=15) as response:
            return response.read().decode('utf-8')
    except urllib.error.HTTPError as exc:
        raise RuntimeError(
            f'HTTP {exc.code} fetching {url}. Verify the Google Symbols icon '
            'name is valid.'
        ) from exc
    except (urllib.error.URLError, OSError) as exc:
        raise RuntimeError(f'Failed to fetch {url}: {exc}') from exc


def build_combined_icon_from_symbols(
    icon_name: str,
    sizes: Sequence[int],
    fill: bool = False,
) -> str:
    """Fetches and converts Google Symbols sizes into .icon blocks."""
    ordered_sizes = sorted(set(sizes), reverse=True)
    blocks: list[str] = []
    for size in ordered_sizes:
        url = build_symbol_url(icon_name, size, fill=fill)
        svg_content = fetch_svg(url)
        blocks.append(svg_to_icon_block(svg_content, size))
    return '\n\n'.join(b.strip() for b in blocks if b.strip()) + '\n'


def build_icon_from_svg(svg_content: str, size: int) -> str:
    """Converts a custom single-<path> SVG into a single-size .icon block."""
    if not (0 < size <= svg_resizer.MAX_CUSTOM_ICON_SIZE):
        raise ValueError(
            f'Custom SVG size must be between 1 and '
            f'{svg_resizer.MAX_CUSTOM_ICON_SIZE} DIP (got {size}). Values '
            'like 960 are unscaled SVG viewBox coordinates, not Chromium '
            'icon sizes.'
        )
    return svg_to_icon_block(svg_content, size).strip() + '\n'


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            'Fetch Google Symbols SVGs (100 ROND, 400 weight) or convert '
            'custom single-<path> SVGs to Chromium .icon representation '
            'blocks.'
        )
    )
    source_group = parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument(
        '--icon',
        help='Google Symbols icon name to fetch (e.g. "account_circle").',
    )
    source_group.add_argument(
        '--svg',
        help=(
            'Path to a local custom SVG file (must contain a single <path>). '
            'Name the resulting file <name>_custom.icon.'
        ),
    )
    parser.add_argument(
        '--sizes',
        type=int,
        nargs='+',
        default=None,
        help=(
            'Target canvas sizes in DIP (e.g. --sizes 20 16). '
            'For --icon, must be in {16, 20, 24} (default: 20). '
            'For --svg, a single size (defaults to the SVG width/viewBox).'
        ),
    )
    parser.add_argument(
        '--fill',
        action='store_true',
        help='Use the filled variant (fill1) for --icon.',
    )

    args = parser.parse_args(argv)

    try:
        if args.svg is not None:
            if args.fill:
                raise ValueError('--fill cannot be used with --svg.')
            if args.sizes and len(args.sizes) > 1:
                raise ValueError(
                    '--svg accepts a single --sizes value. PaintVectorIcon '
                    'scales one vector representation at runtime, so scaled '
                    'copies of the same SVG are redundant (see '
                    '//components/vector_icons/README.md).'
                )
            svg_content = pathlib.Path(args.svg).read_text(encoding='utf-8')
            size = (
                args.sizes[0]
                if args.sizes
                else svg_resizer.infer_svg_size(svg_content)
            )
            combined = build_icon_from_svg(svg_content, size)
        else:
            sizes = args.sizes or [20]
            combined = build_combined_icon_from_symbols(
                args.icon, sizes, fill=args.fill
            )
    except (ValueError, RuntimeError, OSError) as exc:
        print(f'Error: {exc}', file=sys.stderr)
        return 1

    sys.stdout.write(combined)
    return 0


if __name__ == '__main__':
    sys.exit(main())
