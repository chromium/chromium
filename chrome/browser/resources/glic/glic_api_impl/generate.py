#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Reads glic.mojom and outputs generated code to glic_api.ts.

Translates enums with a "// @generate glic_api" comment above them
and copies them into glic_api.ts. Comments from mojom files are
ignored if they have a '///' prefix, allowing for internal
documentation to be filtered out.

Ideally, we would output generated code to a new file, but this way is
less likely to break downstream users of glic_api.
'''

import argparse
import codecs
import io
import os
import re
import sys

GENERATE_GLIC_API_RE = re.compile(r'.*@generate glic_api')

IGNORE_LINE_RES = [
    re.compile(r'.*([L]INT\.IfChange|[L]INT\.ThenChange)'),
    re.compile(r'\s*// Next version:'),
    re.compile(r'\s*///'),
    GENERATE_GLIC_API_RE,
]


def _GetDirAbove(dirname: str):
    """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
    path = os.path.abspath(__file__)
    while True:
        path, tail = os.path.split(path)
        if not tail:
            return None
        if tail == dirname:
            return path


SOURCE_DIR = _GetDirAbove('chrome')

# WARNING!
# Using mojo internal parser here, which is subject to change.
# mojo owners are NOT responsible for ensuring this script keeps working.

sys.path.append(
    os.path.abspath(os.path.join(SOURCE_DIR, 'mojo/public/tools/mojom')), )

from mojom.parse import parser
from mojom.parse import ast
from mojom.generate import generator


def _ParseAst(mojom_abspath):
    with codecs.open(mojom_abspath, encoding='utf-8') as f:
        return parser.Parse(f.read(), mojom_abspath, with_comments=True)


class Converter:

    def __init__(self):
        self.out = io.StringIO()
        mojom_files = [
            'chrome/browser/glic/host/glic.mojom',
            'chrome/common/actor_webui.mojom',
        ]
        self.mojom_trees = [
            _ParseAst(os.path.join(SOURCE_DIR, f)) for f in mojom_files
        ]

    def PrintComments(self, node, indent=0):
        if not node.comments_before:
            return
        for c in node.comments_before:
            for line in c.value.splitlines():
                line = line.strip()
                if any([r.match(line) for r in IGNORE_LINE_RES]):
                    continue
                self.Print(' ' * indent + line)

    def LookupName(self, name, remap):
        if name not in remap:
            return generator.ToUpperSnakeCase(name)
        new_name = remap[name]
        del remap[name]
        return new_name

    def ConvertEnums(self, remappings):
        for tree in self.mojom_trees:
            for v in tree.definition_list:
                if not isinstance(v, ast.Enum):
                    continue
                if v.comments_before and any(
                    (GENERATE_GLIC_API_RE.match(comment.value)
                     for comment in v.comments_before)):
                    self.ConvertEnum(v, remappings.get(v.mojom_name.name, {}))

    def ConvertEnum(self, enum, remap={}):
        enum_name = enum.mojom_name.name
        remap = dict(remap)
        self.Print('///////////////////////////////////////////////')
        self.Print('// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.')
        self.PrintComments(enum)
        self.Print(f'export enum {enum_name} {{')
        value = 0
        for v in enum.enum_value_list:
            value_name = self.LookupName(v.mojom_name.name, remap)
            if value_name:
                self.PrintComments(v, 2)
                if v.value is not None:
                    value = int(v.value.value)
                self.Print(f'  {value_name} = {value},')
            value += 1
        self.Print(f'}}')
        self.Print('')
        if remap:
            raise AssertionError('Unused remap for {enum_name}: {remap}')

    def Print(self, *args, **kwargs):
        print(*args, file=self.out, **kwargs)


def _ApplyChange(target_path, text, check_only):
    with open(target_path, 'r') as f:
        original_text = f.read()
    START = '\n/// BEGIN_GENERATED - DO NOT MODIFY BELOW\n'
    END = '\n/// END_GENERATED - DO NOT MODIFY ABOVE\n'
    start_pos = original_text.find(START)
    end_pos = original_text.find(END)
    if start_pos < 0:
        raise AssertionError(f'No BEGIN_GENERATED block in {target_path}')
    if end_pos < 0:
        raise AssertionError(f'No END_GENERATED block in {target_path}')
    full_text = original_text[:start_pos] + START + text + original_text[
        end_pos:]
    if full_text == original_text:
        return
    if check_only:
        print(f'{os.path.abspath(target_path)} is out of date,',
              ' run chrome/browser/resources/glic/glic_api_impl/generate.py',
              file=sys.stderr)
        sys.exit(1)
    with open(target_path, 'w', newline='') as f:
        f.write(full_text)


def _Main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check-only',
                        action='store_true',
                        help='Check if the output file is up to date.')
    args = parser.parse_args()

    c = Converter()
    c.Print('''
// This block is generated by
// chrome/browser/resources/glic/glic_api_impl/generate.py

''')
    c.ConvertEnums({
        'WebClientMode': {
            'kUnknown': None
        },
        'SettingsPageField': {
            'kNone': None
        },
        'PerformActionsErrorReason': {
            'kMissingTaskId': None,
            'kInvalidProto': 'INVALID_ACTION_PROTO'
        },
    })

    target_path = os.path.join(
        SOURCE_DIR, 'chrome/browser/resources/glic/glic_api/glic_api.ts')
    generated_text = c.out.getvalue()

    _ApplyChange(target_path, generated_text, args.check_only)


if __name__ == '__main__':
    _Main()
