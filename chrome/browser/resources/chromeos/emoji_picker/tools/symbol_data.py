#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import dataclasses
import json
import logging
import os
import sys
import unicodedata
from typing import Generator, List, Tuple

# Add extra dependencies to the python path.
_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 6))
sys.path.append(os.path.join(_CHROME_SOURCE, 'build/android/gyp'))

from util import build_utils

# Initialize logger.
logging.basicConfig(stream=sys.stdout, level=logging.ERROR)
LOGGER = logging.getLogger(__name__)

# List of unicode ranges for each symbol group (ranges are inclusive).
SYMBOLS_GROUPS = {
    'Arrows': [
        # Arrows Unicode Block.
        (0x2190, 0x21ff),
        # Supplemental Arrows-C Unicode Block.
        # Note: There are unassigned code points in the block which are
        # automatically skipped by the script.
        (0x1f800, 0x1f8b2),
    ],
    'Bullet/Stars': [
        # Some rows from Miscellaneous Symbols and Arrows Unicode block.
        (0x2b20, 0x2b2f),
        (0x2b50, 0x2b5f),
        (0x2b90, 0x2b9f),
        (0x2bb0, 0x2bbf),
        (0x2bc0, 0x2bcf),
    ],
    'Currency': [
        # Currency Unicode Block.
        (0x20a0, 0x20bf),
    ],
    'Letterlike': [
        # Letterlike Symbols Unicode Block.
        (0x2100, 0x210f),
    ],
    'Math': [
        # Greek Letters and Symbols from Mathematical and Alphanumeric
        # Symbols Unicode Block.
        # Normal Capital Letters.
        (0x0391, 0x0391 + 25),
        # Normal Small Letters.
        (0x03b1, 0x03b1 + 25),
    ],
    'Miscellaneous': [
        # Miscellaneous Symbols Unicode Block.
        (0x2600, 0x26ff)
    ]
}


@dataclasses.dataclass
class EmojiPickerChar:
    """A type representing a single character in EmojiPicker."""
    # Unicode character.
    string: str
    # Name of the unicode character.
    name: str
    # Search keywords related to the unicode character.
    keywords: List[str] = dataclasses.field(
        default_factory=list)


@dataclasses.dataclass
class EmojiPickerEmoji:
    """A type representing an emoji/emoticon/symbol in EmojiPicker."""
    # Base Emoji.
    base: EmojiPickerChar
    # Base Emoji's variants and alternative emojis.
    alternates: List[EmojiPickerChar] = dataclasses.field(
        default_factory=list)


@dataclasses.dataclass
class EmojiPickerGroup:
    """A type representing a group of emoji/emoticon/symbols."""
    # Name of the group.
    group: str
    # List of the emojis in the group.
    emoji: List[EmojiPickerEmoji]


def _convert_unicode_ranges_to_emoji_chars(
        unicode_ranges: List[Tuple[int, int]],
        ignore_errors: bool = True) -> Generator[EmojiPickerChar, None, None]:
    """Converts unicode ranges to `EmojiPickerChar` instances.

    Given a list of unicode ranges, it iterates over all characters in all the
    ranges and creates and yields an instance of `EmojiPickerChar` for each
    one.

    Args:
        unicode_ranges: A list of unicode ranges.
        ignore_errors: If True, any exceptions raised during processing
            unicode characters is silently ignored.

    Raises:
        ValueError: If a unicode character does not exist in the data source
            and `ignore_errors` is true, an exception is raised.

    Yields:
        The converted version of each unicode character in the input ranges.
    """

    LOGGER.info(
        'generating EmojiPickerChar instances for ranges: [%s].',
        ', '.join(
            '(U+{:02x}, U+{:02x})'.format(*rng)
            for rng in unicode_ranges))

    num_chars = 0
    num_ignored = 0

    # Iterate over the input unicode ranges.
    for (start_code_point, end_code_point) in unicode_ranges:
        LOGGER.debug(
            'generating EmojiPickerChar instances '
            'for range (U+%02x to U+%02x).',
            start_code_point,
            end_code_point)

        num_chars += end_code_point + 1 - start_code_point
        # Iterate over all code points in the range.
        for code_point in range(start_code_point, end_code_point + 1):
            try:
                # For the current code point, create the corresponding
                # character and lookup its name in the unicodedata. Then,
                # create an instance of  `EmojiPickerChar` from the data.
                unicode_character = chr(code_point)
                yield EmojiPickerChar(
                    string=unicode_character,
                    name=unicodedata.name(unicode_character).lower())
            except ValueError:
                # If ignore_errors is False, raise the exception.
                if not ignore_errors:
                    raise
                else:
                    num_ignored += 1
                    LOGGER.warning(
                        'invalid code point U+%02x.', code_point)

    LOGGER.info(
        'stats: #returned instances: %d, #ignored code points: %d',
        num_chars,
        num_ignored)


def get_symbols_groups(ignore_errors: bool = True) -> List[EmojiPickerGroup]:
    """Creates symbols data from predefined groups and their unicode ranges.

    Args:
        ignore_errors: If True, any exceptions raised during processing
            unicode characters is silently ignored.

    Raises:
        ValueError: If a unicode character does not exist in the data source
            and `ignore_errors` is true, the exception is raised.
    """
    # TODO(b/232160008): Exclude symbols that are in emoji/emoticon lists.

    emoji_groups = list()
    for (group_name, unicode_ranges) in SYMBOLS_GROUPS.items():
        LOGGER.info('generating symbols for group %s.', group_name)
        emoji_chars = _convert_unicode_ranges_to_emoji_chars(
            unicode_ranges, ignore_errors=ignore_errors)
        emoji = [
            EmojiPickerEmoji(base=emoji_char)
            for emoji_char in emoji_chars]

        emoji_groups.append(EmojiPickerGroup(group=group_name, emoji=emoji))
    return emoji_groups


def main(argv: List[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--output', required=True, type=str,
        help='Path to write the output JSON file.')
    parser.add_argument(
        '--verbose', required=False, default=False,
        action='store_true',
        help="Set the logging level to Debug.")
    args = parser.parse_args(argv)

    if args.verbose:
        LOGGER.setLevel(level=logging.DEBUG)

    symbols_groups = get_symbols_groups()

    # Create the data and convert them to dict.
    symbols_groups_dicts = [
        dataclasses.asdict(symbol_group)
        for symbol_group in symbols_groups]

    # Write the result to output path as json file.
    with build_utils.AtomicOutput(args.output) as tmp_file:
        tmp_file.write(
            json.dumps(
                symbols_groups_dicts,
                separators=(',', ':'),
                ensure_ascii=False).encode('utf-8'))


if __name__ == "__main__":
    main(sys.argv[1:])
