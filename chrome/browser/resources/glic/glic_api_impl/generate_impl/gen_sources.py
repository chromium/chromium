# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import io
import os
import re
import sys

from . import parse


def _SnakeToCamelCase(name: str) -> str:
    components = name.split('_')
    return components[0] + ''.join(x.title() for x in components[1:])


class Converter(parse.MojomParser):

    def __init__(self):
        super().__init__()
        self.out = io.StringIO()
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
                if parse.GENERATE_GLIC_API_RE.match(line):
                    # Trim comments above @generate glic_api line.
                    comment_lines = []
                    continue
                if any([r.match(line) for r in parse.IGNORE_LINE_RES]):
                    continue
                comment_lines.append(line)
        for line in comment_lines:
            self.Print(' ' * indent + line)

    def LookupName(self, name, remap):
        if name not in remap:
            return parse.generator.ToUpperSnakeCase(name)
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
                if not isinstance(v, parse.ast.Enum):
                    continue
                if v.comments_before and any(
                    (parse.GENERATE_GLIC_API_RE.search(comment.value)
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
                if not isinstance(v, parse.ast.Struct):
                    continue
                if v.comments_before and any(
                    (parse.GENERATE_GLIC_API_RE.search(comment.value)
                     for comment in v.comments_before)):
                    self.ConvertStruct(v, type_mappings)

    def MapMojoTypeToTs(self, typename, type_mappings):
        ident = typename.identifier
        if isinstance(ident, parse.ast.Array):
            item_type = self.MapMojoTypeToTs(ident.value_type, type_mappings)
            if item_type:
                return f'{item_type}[]'
            return None
        if not isinstance(ident, parse.ast.Identifier):
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
        is_int32 = (isinstance(typename.identifier, parse.ast.Identifier)
                    and typename.identifier.id == 'int32')

        if is_int32 and parse.STRINGLIKE_ID_RE.search(field.mojom_name.name):
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
                if not isinstance(v, parse.ast.Union):
                    continue
                if v.comments_before and any(
                    (parse.GENERATE_GLIC_API_RE.search(comment.value)
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
            if not isinstance(field,
                              (parse.ast.StructField, parse.ast.UnionField)):
                continue
            ts_type = None
            is_optional = False
            is_ignored = False
            if field.comments_before:
                for comment in field.comments_before:
                    for line in comment.value.splitlines():
                        line_stripped = line.strip()
                        m = parse.TYPE_OVERRIDE_RE.match(line_stripped)
                        if m:
                            ts_type = m.group(1)
                            if ts_type.endswith('?'):
                                ts_type = ts_type[:-1]
                                is_optional = True
                        if parse.GLIC_OPTIONAL_RE.match(line_stripped):
                            is_optional = True
                        if parse.GLIC_IGNORE_RE.match(line_stripped):
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


def Main():
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
        parse.SOURCE_DIR,
        'chrome/browser/resources/glic/glic_api/glic_api_generated.ts')
    generated_text = header + c.out.getvalue().rstrip() + '\n'
    _WriteFile(target_path, generated_text, args.check_only)

    glic_api_path = os.path.join(
        parse.SOURCE_DIR, 'chrome/browser/resources/glic/glic_api/glic_api.ts')
    _UpdateGlicApiExports(glic_api_path, c.generated_interfaces,
                          c.generated_enums, args.check_only)


if __name__ == '__main__':
    Main()
