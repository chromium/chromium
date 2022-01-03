# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 6))
sys.path.append(os.path.join(_CHROME_SOURCE, 'build/android/gyp'))

import argparse
import json
import xml.etree.ElementTree
from itertools import chain
from util import build_utils
from emoji_data import parse_emoji_annotations, parse_emoji_metadata, _chr


class EmojiConverter:
    def __init__(self, names, keywords):
        """
        Args:
            names (dict(str, str)): dictionary that maps an emoji string
                representation to its names.
            keywords (dict(str, list(str))): dictionary that maps an emoji
                string representation to the emoji's keywords.
        """
        self.emoji_names = names
        self.emoji_keywords = keywords

    # TODO(b/211822956): Refactor to a common function with emoji_data.
    def get_emoji_string(self, codepoints):
        """Convert emoji codepoints into its unicode representation.

        Args:
            codepoints (list(int)): list of integer codepoints.

        Returns:
            str: unicode string representation of the emoji.
        """
        # Transform array of codepoint values into unicode string.
        string = u''.join(_chr(x) for x in codepoints)
        if string in self.emoji_names:
            return string
        else:
            return string.replace(u'\ufe0f', u'')

    def pull_emoticons_from_emoji(self, emoji):
        """Convert a single emoji object into a list of emoticon objects.
        Each emoji object contains zero, one or many emoticon representations.

        Args:
            emoji (dict): the emoji object from the emoji ordering.

        Returns:
            list(dict): array of emoticon objects.
        """
        if 'emoticons' not in emoji:
            return []

        emoticon_list = []
        for emoticon_string in emoji['emoticons']:
            codepoints = emoji['base']
            emoji_repr = self.get_emoji_string(codepoints)
            new_emoticon = {
                'base': {
                    'string': emoticon_string,
                    'name': self.emoji_names.get(emoji_repr, ''),
                    'keywords': self.emoji_keywords.get(emoji_repr, [])
                },
                # TODO(b/211696216): Handle the case when emoticons have
                # alternates.
                'alternates': []
            }
            emoticon_list.append(new_emoticon)

        return emoticon_list

    def get_emoticon_data(self, metadata):
        """Get the emoticon data from the emoji ordering data.
        Args:
            metadata (list(dict)): list of emoji objects.

        Returns:
            list(dict): list of emoticon objects.
        """

        return [{
            "group":
            group["group"],
            "items":
            list(
                chain.from_iterable(
                    self.pull_emoticons_from_emoji(emoji)
                    for emoji in group["emoji"]))
        } for group in metadata]


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--metadata',
                        required=True,
                        help='emoji metadata ordering file as JSON')
    parser.add_argument('--output',
                        required=True,
                        help='output JSON file path')
    parser.add_argument('--keywords',
                        required=True,
                        nargs='+',
                        help='emoji keyword files as list of XML files')

    options = parser.parse_args(args)

    metadata_file = options.metadata
    keyword_files = options.keywords
    output_file = options.output

    # TODO(b/211822956): Refactor to a common function with emoji_data.
    # Iterate through keyword files and combine them.
    names = {}
    keywords = {}
    for file in keyword_files:
        _names, _keywords = parse_emoji_annotations(file)
        names.update(_names)
        keywords.update(_keywords)

    # Parse emoji ordering data.
    metadata = parse_emoji_metadata(metadata_file)
    emoji_converter = EmojiConverter(names, keywords)
    emoticon_data = emoji_converter.get_emoticon_data(metadata)

    # Write output file atomically in utf-8 format.
    with build_utils.AtomicOutput(output_file) as tmp_file:
        tmp_file.write(
            json.dumps(emoticon_data,
                       separators=(',', ':'),
                       ensure_ascii=False).encode('utf-8'))


if __name__ == '__main__':
    main(sys.argv[1:])
