#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import sys
import argparse
import json
import xml.etree.ElementTree

_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 6))

sys.path.append(os.path.join(_CHROME_SOURCE, 'build/android/gyp'))

from util import build_utils

_chr = unichr if sys.version_info.major == 2 else chr


def parse_emoji_annotations(keyword_file):
    names = {}
    keywords = {}

    tree = xml.etree.ElementTree.parse(keyword_file)
    root = tree.getroot()

    for tag in root.iterfind('./annotations/annotation'):
        cp = tag.attrib['cp']
        if tag.attrib.get('type') == 'tts':
            if tag.text.startswith("flag"):
              names[cp] = tag.text.replace("flag:","flag of")
            else:
              names[cp] = tag.text
        else:
            keywords[cp] = tag.text.split(' | ')

    return names, keywords


def parse_emoji_metadata(metadata_file):
    with open(metadata_file, 'r') as file:
        return json.load(file)


def transform_emoji_data(metadata, names, keywords, first_only):
    def transform(codepoints, is_variant, emoticons = None, shortcodes = None):
        if emoticons is None:
          emoticons = []
        if shortcodes is None:
          shortcodes = []
        # transform array of codepoint values into unicode string.
        string = u''.join(_chr(x) for x in codepoints)

        # keyword data has U+FE0F emoji presentation characters removed.
        if string not in names:
            string = string.replace(u'\ufe0f', u'')
        # TODO(b/183440310): Better handle search for non-standard emoji.
        if string in names:
          name = names[string]
          keyword_list = keywords[string] + emoticons + shortcodes
        else:
          name = ''
          keyword_list = emoticons

        if is_variant:
          return {'string': string, 'name': name}
        else:
          return {'string': string, 'name': name, 'keywords': keyword_list}
    if first_only:
      metadata = [metadata[0]]
    else:
      metadata = metadata[1:]

    # Create a new object for output since they keep adding extra properties to
    # the JSON (rather than just editing the input object).
    out = []
    for group in metadata:
        newGroup = []
        for emoji in group['emoji']:
            newobj = {
                'base': transform(
                    emoji['base'],
                    False,
                    emoji['emoticons'],
                    emoji.get('shortcodes', []),
                ),
            }
            if emoji['alternates']:
              newobj['alternates'] = [
                  transform(e, True) for e in emoji['alternates']
              ]
            newGroup.append(newobj)
        out.append({'emoji': newGroup})
    return out

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
    parser.add_argument('--firstgroup',
                        required = True,
                        help='Only output the first group, otherwise output all groups'
    )

    options = parser.parse_args(args)

    metadata_file = options.metadata
    keyword_files = options.keywords
    output_file = options.output
    first_group = options.firstgroup == "True"

    # iterate through keyword files and combine them
    names = {}
    keywords = {}
    for file in keyword_files:
        _names, _keywords = parse_emoji_annotations(file)
        names.update(_names)
        keywords.update(_keywords)

    # parse emoji ordering data
    metadata = parse_emoji_metadata(metadata_file)
    metadata = transform_emoji_data(metadata, names, keywords, first_group)

    # write output file atomically in utf-8 format.
    with build_utils.AtomicOutput(output_file) as tmp_file:
        tmp_file.write(
            json.dumps(metadata, separators=(',', ':'),
                       ensure_ascii=False).encode('utf-8'))


if __name__ == '__main__':
    main(sys.argv[1:])
