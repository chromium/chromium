#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Reads glic.mojom and other referenced mojom files, and outputs generated code to
glic_api.ts.

Translates enums, structs, and unions with a "// @generate glic_api" comment
above them and copies them into glic_api.ts. Comments from mojom files are
copied over, but are ignored if they have a '///' prefix, allowing for internal
documentation to be filtered out.

Supports the following annotations on mojom fields:
- "@glic_type <type>": Overrides the generated TypeScript type.
- "@glic_optional": Marks a property as optional (appending '?').
- "@glic_ignore": Excludes a property from the generated interface.

Ideally, we would output generated code to a new file, but this way is
less likely to break downstream users of glic_api.

Also generates chrome/browser/resources/glic/glic_api_impl/enum_conversions.ts.
'''

import argparse
import codecs
import io
import os
import re
import sys

GENERATE_GLIC_API_RE = re.compile(r'.*@generate glic_api')
TYPE_OVERRIDE_RE = re.compile(r'.*@glic_type\s+(\S+)')
GLIC_OPTIONAL_RE = re.compile(r'.*@glic_optional')
GLIC_IGNORE_RE = re.compile(r'.*@glic_ignore')
STRINGLIKE_ID_RE = re.compile(r'(tab|window)_id$')


def _SnakeToCamelCase(name: str) -> str:
    components = name.split('_')
    return components[0] + ''.join(x.title() for x in components[1:])


IGNORE_LINE_RES = [
    re.compile(r'.*([L]INT\.IfChange|[L]INT\.ThenChange)'),
    re.compile(r'\s*//\s*//(components|chrome|tools|ui)/.*'),
    re.compile(r'\s*// Next version:'),
    re.compile(r'\s*///'),
    GENERATE_GLIC_API_RE,
    TYPE_OVERRIDE_RE,
    GLIC_OPTIONAL_RE,
    GLIC_IGNORE_RE,
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
        tree = parser.Parse(f.read(), mojom_abspath, with_comments=True)
        tree.filename = mojom_abspath
        return tree


def _FunctionSignatureString(function_name, param_type, return_type):
    return (f'export function {function_name}(\n' +
            f'  val: {param_type}):\n' + f'    {return_type};')

class Converter:

    def __init__(self):
        self.out = io.StringIO()
        mojom_files = [
            'chrome/browser/glic/host/glic.mojom',
            'chrome/common/actor_webui.mojom',
            'chrome/common/glic_enums.mojom',
            'third_party/blink/public/mojom/content_extraction' +
            '/ai_page_content_metadata.mojom',
        ]
        self.mojom_trees = [
            _ParseAst(os.path.join(SOURCE_DIR, f)) for f in mojom_files
        ]
        self.generated_names = set()
        self.generated_interfaces = set()
        self.generated_enums = set()
        self.referenced_types = set()

    def PrintComments(self, node, indent=0):
        if not node.comments_before:
            return
        comment_lines = []
        for c in node.comments_before:
            for line in c.value.splitlines():
                line = line.strip()
                if GENERATE_GLIC_API_RE.match(line):
                    # Trim comments above @generate glic_api line.
                    comment_lines = []
                    continue
                if any([r.match(line) for r in IGNORE_LINE_RES]):
                    continue
                comment_lines.append(line)
        for line in comment_lines:
            self.Print(' ' * indent + line)

    def LookupName(self, name, remap):
        if name not in remap:
            return generator.ToUpperSnakeCase(name)
        new_name = remap[name]
        del remap[name]
        return new_name

    def ConvertEnums(self, remappings):
        self.converted_enums = []
        for tree in self.mojom_trees:
            if 'actor_webui.mojom' in tree.filename:
                source = 'actor'
            elif 'glic_enums.mojom' in tree.filename:
                source = 'glic_enums'
            else:
                source = 'glic'
            for v in tree.definition_list:
                if not isinstance(v, ast.Enum):
                    continue
                if v.comments_before and any(
                    (GENERATE_GLIC_API_RE.search(comment.value)
                     for comment in v.comments_before)):
                    self.ConvertEnum(v, remappings.get(v.mojom_name.name, {}))
                    self.converted_enums.append((v.mojom_name.name, source))

    def ConvertEnum(self, enum, remap={}):
        enum_name = enum.mojom_name.name
        self.generated_names.add(enum_name)
        self.generated_enums.add(enum_name)
        remap = dict(remap)
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

    def ConvertStructs(self, type_mappings):
        for tree in self.mojom_trees:
            for v in tree.definition_list:
                if not isinstance(v, ast.Struct):
                    continue
                if v.comments_before and any(
                    (GENERATE_GLIC_API_RE.search(comment.value)
                     for comment in v.comments_before)):
                    self.ConvertStruct(v, type_mappings)

    def MapMojoTypeToTs(self, typename, type_mappings):
        ident = typename.identifier
        if isinstance(ident, ast.Array):
            item_type = self.MapMojoTypeToTs(ident.value_type, type_mappings)
            if item_type:
                return f'{item_type}[]'
            return None
        if not isinstance(ident, ast.Identifier):
            return None
        type_str = ident.id
        mapped_type = type_str
        if type_str in type_mappings:
            mapped_type = type_mappings[type_str]
        else:
            # Strip namespaces (e.g. glic.mojom.FeatureMode -> FeatureMode)
            mapped_type = type_str.split('.')[-1]

        if mapped_type.isalnum():
            # If not a primitive type, record it as referenced.
            if mapped_type not in ('string', 'boolean', 'number', 'any',
                                   'void'):
                self.referenced_types.add(mapped_type)
            return mapped_type
        return None

    def MapMojoFieldTypeToTs(self, field, type_mappings):
        typename = field.typename
        is_int32 = (isinstance(typename.identifier, ast.Identifier)
                    and typename.identifier.id == 'int32')

        if is_int32 and STRINGLIKE_ID_RE.search(field.mojom_name.name):
            return 'string'
        return self.MapMojoTypeToTs(typename, type_mappings)

    def ConvertStruct(self, struct, type_mappings):
        struct_name = struct.mojom_name.name
        self.generated_names.add(struct_name)
        self.generated_interfaces.add(struct_name)
        self.PrintComments(struct)
        self.Print(f'export declare interface {struct_name} {{')
        self._ConvertFields(struct_name,
                            'struct',
                            struct.body,
                            type_mappings,
                            force_optional=False)
        self.Print('}')
        self.Print('')

    def ConvertUnions(self, type_mappings):
        for tree in self.mojom_trees:
            for v in tree.definition_list:
                if not isinstance(v, ast.Union):
                    continue
                if v.comments_before and any(
                    (GENERATE_GLIC_API_RE.search(comment.value)
                     for comment in v.comments_before)):
                    self.ConvertUnion(v, type_mappings)

    def ConvertUnion(self, union, type_mappings):
        union_name = union.mojom_name.name
        self.generated_names.add(union_name)
        self.generated_interfaces.add(union_name)
        self.PrintComments(union)
        self.Print(f'export declare interface {union_name} {{')
        self._ConvertFields(union_name,
                            'union',
                            union.body,
                            type_mappings,
                            force_optional=True)
        self.Print('}')
        self.Print('')

    def _ConvertFields(self,
                       container_name,
                       container_type,
                       fields,
                       type_mappings,
                       force_optional=False):
        for field in fields:
            if not isinstance(field, (ast.StructField, ast.UnionField)):
                continue
            ts_type = None
            is_optional = False
            is_ignored = False
            if field.comments_before:
                for comment in field.comments_before:
                    for line in comment.value.splitlines():
                        line_stripped = line.strip()
                        m = TYPE_OVERRIDE_RE.match(line_stripped)
                        if m:
                            ts_type = m.group(1)
                            if ts_type.endswith('?'):
                                ts_type = ts_type[:-1]
                                is_optional = True
                        if GLIC_OPTIONAL_RE.match(line_stripped):
                            is_optional = True
                        if GLIC_IGNORE_RE.match(line_stripped):
                            is_ignored = True
            if is_ignored:
                continue
            self.PrintComments(field, 2)
            typename = field.typename
            if not ts_type:
                ts_type = self.MapMojoFieldTypeToTs(field, type_mappings)
            if not ts_type:
                raise Exception(
                    f"Unsupported Mojo type '{typename}' for field" +
                    f" '{field.mojom_name.name}' in {container_type}" +
                    f" '{container_name}'")
            field_name = _SnakeToCamelCase(field.mojom_name.name)
            optional_suffix = '?' if (force_optional or typename.nullable
                                      or is_optional) else ''
            self.Print(f'  {field_name}{optional_suffix}: {ts_type};')

    def Print(self, *args, **kwargs):
        print(*args, file=self.out, **kwargs)

    def GenerateConversions(self):
        out = io.StringIO()
        out.write("""// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS IS A GENERATED FILE. DO NOT MODIFY BELOW
// This file is generated by
// chrome/browser/resources/glic/glic_api_impl/generate.py
// clang-format off

import type * as mojomGlic from '../glic.mojom-webui.js';
import type * as mojomActor from '../actor_webui.mojom-webui.js';
import type * as mojomGlicEnums from '../glic_enums.mojom-webui.js';
import type * as glicApi from '../glic_api/glic_api.js';

""")

        sorted_enums = sorted(self.converted_enums)
        for enum_name, source in sorted_enums:
            if source == 'glic':
                mojom_ns = 'mojomGlic'
            elif source == 'actor':
                mojom_ns = 'mojomActor'
            else:
                mojom_ns = 'mojomGlicEnums'
            print(_FunctionSignatureString('enumToClient',
                                           f'{mojom_ns}.{enum_name}',
                                           f'glicApi.{enum_name}'),
                  file=out)
            print(_FunctionSignatureString('enumToClient',
                                           f'{mojom_ns}.{enum_name} | null',
                                           f'glicApi.{enum_name} | undefined'),
                  file=out)
        print('export function enumToClient(val: unknown): unknown {',
              file=out)
        print('  return val ?? undefined;', file=out)
        print('}', file=out)
        print('', file=out)

        for enum_name, source in sorted_enums:
            if source == 'glic':
                mojom_ns = 'mojomGlic'
            elif source == 'actor':
                mojom_ns = 'mojomActor'
            else:
                mojom_ns = 'mojomGlicEnums'
            print(_FunctionSignatureString('enumFromClient',
                                           f'glicApi.{enum_name}',
                                           f'{mojom_ns}.{enum_name}'),
                  file=out)
            print(_FunctionSignatureString('enumFromClient',
                                           f'glicApi.{enum_name} | undefined',
                                           f'{mojom_ns}.{enum_name} | null'),
                  file=out)
        print('export function enumFromClient(val: unknown): unknown {',
              file=out)
        print('  return val ?? null;', file=out)
        print('}', file=out)
        return out.getvalue()


def _WriteFile(target_path, text, check_only):
    if os.path.exists(target_path):
        with open(target_path, 'r') as f:
            original_text = f.read()
        if original_text == text:
            return

    if check_only:
        print(f'{os.path.abspath(target_path)} is out of date,',
              ' run chrome/browser/resources/glic/glic_api_impl/generate.py',
              file=sys.stderr)
        sys.exit(1)

    with open(target_path, 'w', newline='') as f:
        f.write(text)


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


def _UpdateGlicApiExports(glic_api_path, generated_interfaces, generated_enums,
                          check_only):
    # Format the new exports block
    new_exports = "import * as generated from './glic_api_generated.js';\n"

    for name in sorted(generated_interfaces):
        line = f'export import {name} = generated.{name};'
        if len(line) > 80:
            line = '=\n   '.join(line.split('='))
        new_exports += line + '\n'

    for name in sorted(generated_enums):
        line = f'export import {name} = generated.{name};'
        if len(line) > 80:
            line = '=\n   '.join(line.split('='))
        new_exports += line + '\n'

    _ApplyChange(glic_api_path, new_exports, check_only)


def _Main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check-only',
                        action='store_true',
                        help='Check if the output file is up to date.')
    args = parser.parse_args()

    c = Converter()

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

    type_mappings = {
        'string': 'string',
        'bool': 'boolean',
        'int8': 'number',
        'int16': 'number',
        'int32': 'number',
        'int64': 'number',
        'uint8': 'number',
        'uint16': 'number',
        'uint32': 'number',
        'uint64': 'number',
        'double': 'number',
        'float': 'number',
        'url.mojom.Url': 'string',
        'url.mojom.Origin': 'string',
        'mojo_base.mojom.UnguessableToken': 'string',
        'SafeBrowsingVerdictResult': 'SafeBrowsingVerdict',
        'mojo_base.mojom.ByteString': 'string',
        'gfx.mojom.Rect': 'Rect',
        'gfx.mojom.Point': 'Point',
    }

    c.ConvertStructs(type_mappings)

    c.ConvertUnions(type_mappings)

    # Calculate the unresolved imports from ./glic_api.js
    unresolved_types = c.referenced_types - c.generated_names
    imports_str = ""
    if unresolved_types:
        imports_list = ", ".join(sorted(unresolved_types))
        imports_str = f"import type {{{imports_list}}} from './glic_api.js';\n\n"

    header = f'''// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS IS A GENERATED FILE. DO NOT MODIFY.
// This file is generated by
// chrome/browser/resources/glic/glic_api_impl/generate.py

{imports_str}'''

    target_path = os.path.join(
        SOURCE_DIR,
        'chrome/browser/resources/glic/glic_api/glic_api_generated.ts')
    generated_text = header + c.out.getvalue().rstrip() + '\n'
    _WriteFile(target_path, generated_text, args.check_only)

    conversions_path = os.path.join(
        SOURCE_DIR,
        'chrome/browser/resources/glic/glic_api_impl/enum_conversions.ts')
    conversions_text = c.GenerateConversions()
    _WriteFile(conversions_path, conversions_text, args.check_only)

    glic_api_path = os.path.join(
        SOURCE_DIR, 'chrome/browser/resources/glic/glic_api/glic_api.ts')
    _UpdateGlicApiExports(glic_api_path, c.generated_interfaces,
                          c.generated_enums, args.check_only)


if __name__ == '__main__':
    _Main()
