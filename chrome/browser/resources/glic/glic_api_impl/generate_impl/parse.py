# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Parses mojom files and outputs JSON representation of the AST.
This is a generic parser that outputs raw comments and structural info
(camelCase).
"""
import codecs
import os
import sys
import json
import argparse


def GetDirAbove(dirname: str):
    path = os.path.abspath(__file__)
    while True:
        path, tail = os.path.split(path)
        if not tail:
            return None
        if tail == dirname:
            return path


SOURCE_DIR = GetDirAbove('chrome')
if SOURCE_DIR:
    sys.path.append(
        os.path.abspath(os.path.join(SOURCE_DIR, 'mojo/public/tools/mojom')))

from mojom.parse import parser  # type: ignore
from mojom.parse import ast  # type: ignore
from mojom.generate import generator  # type: ignore

MOJOM_PRIMITIVE_TYPES = {
    'int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64',
    'double', 'float', 'bool', 'string', 'handle'
}

INPUT_MOJOM_FILES = [
    'chrome/browser/glic/host/glic.mojom',
    'chrome/common/actor_webui.mojom',
    'chrome/common/glic_enums.mojom',
    'third_party/blink/public/mojom/content_extraction/'
    'ai_page_content_metadata.mojom',
]


def _GetComments(node) -> list[str]:
    if not getattr(node, 'comments_before', None):
        return []
    comment_lines: list[str] = []
    for c in node.comments_before:
        comment_lines.extend(c.value.splitlines())
    return comment_lines


def _GetCommentsAfter(node) -> list[str]:
    if not getattr(node, 'comments_after', None):
        return []
    comment_lines: list[str] = []
    for c in node.comments_after:
        comment_lines.extend(c.value.splitlines())
    return comment_lines


def _ConvertTypename(typename) -> dict:
    if not typename:
        return {'kind': 'primitive', 'name': ''}
    ident = typename.identifier
    if isinstance(ident, ast.Map):
        key_type = ident.key_type.id
        value_type = _ConvertTypename(ident.value_type)
        return {'kind': 'map', 'keyType': key_type, 'valueType': value_type}
    if isinstance(ident, ast.Array):
        item_type = _ConvertTypename(ident.value_type)
        return {'kind': 'array', 'elementType': item_type}
    if isinstance(ident, ast.Identifier):
        name = ident.id
        if name in MOJOM_PRIMITIVE_TYPES:
            return {'kind': 'primitive', 'name': name}
        return {'kind': 'named', 'name': name}
    return {'kind': 'primitive', 'name': ''}


def _ConvertField(field) -> dict:
    comments = _GetComments(field)
    typename = field.typename
    mojom_type = _ConvertTypename(typename)

    return {
        'name': field.mojom_name.name,
        'mojomType': mojom_type,
        'isNullable': typename.nullable,
        'comments': comments
    }


def _ConvertEnum(enum_node) -> dict:
    comments = _GetComments(enum_node)

    values = []
    next_value = 0
    for ev in enum_node.enum_value_list:
        ev_comments = _GetComments(ev)
        is_default = False
        if ev.attribute_list:
            for attr in ev.attribute_list:
                if attr.key.name == 'Default':
                    is_default = True
                    break
        if ev.value is not None:
            try:
                next_value = int(ev.value.value, 0)
            except ValueError:
                pass

        values.append({
            'name': ev.mojom_name.name,
            'value': next_value,
            'isDefault': is_default,
            'comments': ev_comments
        })
        next_value += 1

    return {
        'name': enum_node.mojom_name.name,
        'values': values,
        'comments': comments
    }


def _ConvertStruct(struct_node) -> dict:
    comments = _GetComments(struct_node)
    body_comments = _GetCommentsAfter(struct_node)

    fields = []
    if struct_node.body:
        for f in struct_node.body:
            if isinstance(f, ast.StructField):
                fields.append(_ConvertField(f))

    return {
        'name': struct_node.mojom_name.name,
        'fields': fields,
        'comments': comments,
        'bodyComments': body_comments
    }


def _ConvertUnion(union_node) -> dict:
    comments = _GetComments(union_node)

    fields = []
    if union_node.body:
        for f in union_node.body:
            if isinstance(f, ast.UnionField):
                fields.append(_ConvertField(f))

    return {
        'name': union_node.mojom_name.name,
        'fields': fields,
        'comments': comments
    }


def _ConvertModule(mojom_node, filename: str) -> dict:
    imports = []
    if mojom_node.import_list:
        for imp in mojom_node.import_list:
            imports.append(imp.import_filename)

    enums = []
    structs = []
    unions = []
    interfaces: list[dict] = []

    for defn in mojom_node.definition_list:
        if isinstance(defn, ast.Enum):
            enums.append(_ConvertEnum(defn))
        elif isinstance(defn, ast.Struct):
            structs.append(_ConvertStruct(defn))
        elif isinstance(defn, ast.Union):
            unions.append(_ConvertUnion(defn))

    return {
        'filename': filename,
        'imports': imports,
        'enums': enums,
        'structs': structs,
        'unions': unions,
        'interfaces': interfaces
    }


def ParseAst(mojom_abspath: str) -> dict:
    with codecs.open(mojom_abspath, encoding='utf-8') as f:
        tree = parser.Parse(f.read(), mojom_abspath, with_comments=True)
        return _ConvertModule(tree, mojom_abspath)


def Main():
    parser_cli = argparse.ArgumentParser(
        description="Parse Mojom files to JSON AST")
    parser_cli.add_argument("--output", help="Path to output JSON file")
    args = parser_cli.parse_args()

    modules = []
    for f in INPUT_MOJOM_FILES:
        abspath = os.path.join(SOURCE_DIR, f)
        modules.append(ParseAst(abspath))

    ast_dict = {'modules': modules}

    if args.output:
        with open(args.output, 'w', encoding='utf-8') as out:
            json.dump(ast_dict, out, indent=2)
    else:
        json.dump(ast_dict, sys.stdout, indent=2)


if __name__ == '__main__':
    Main()
