# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Reads tools/*_tools.mojom and outputs generated tool definitions to
generated_tool_definitions.ts.
'''

import argparse
import io
import re
import json


def _CamelToSnake(name):
    name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', name).lower()


def _ToParagraphs(lines_array):
    paragraphs = []
    current_para = []
    for line in lines_array:
        if not line.strip():
            if current_para:
                paragraphs.append(" ".join(l.strip() for l in current_para))
                current_para = []
        else:
            current_para.append(line)
    if current_para:
        paragraphs.append(" ".join(l.strip() for l in current_para))
    return "\n\n".join(paragraphs)


def _ParseComments(comments_str, param_names):
    lines = []
    for line in comments_str.splitlines():
        line = line.strip()
        # Remove leading slashes and at most one whitespace
        line = re.sub(r'^//\s?', '', line)
        lines.append(line)

    main_desc_lines = []
    param_lines = {}
    current_param = None

    for line in lines:
        m = re.match(r'^(\w+):\s*(.*)', line)
        if m and m.group(1) in param_names:
            current_param = m.group(1)
            param_lines[current_param] = [m.group(2)]
        elif current_param is not None:
            param_lines[current_param].append(line)
        else:
            main_desc_lines.append(line)

    main_desc = _ToParagraphs(main_desc_lines)
    param_descs = {p: _ToParagraphs(lines) for p, lines in param_lines.items()}

    return main_desc, param_descs


def _MojomTypeToGeminiType(mojom_type):
    if 'string' in mojom_type:
        return 'STRING'
    if 'bool' in mojom_type:
        return 'BOOLEAN'
    if 'int' in mojom_type:
        return 'INTEGER'
    if 'double' in mojom_type or 'float' in mojom_type: return 'NUMBER'
    return 'STRING'


def ParseMojomFile(header_path):
    with open(header_path, 'r', encoding='utf-8') as f:
        text = f.read()

    tool_declarations = []

    pattern = re.compile(
        r'((?:[ \t]*//[^\n]*\n)+)[ \t]*'
        r'([A-Z]\w*)\((.*?)\)\s*=>\s*(?:result<.*?>|\(.*?\));', re.DOTALL)

    for m in pattern.finditer(text):
        comments, name, args_str = m.groups()

        # Parse mojom arguments
        args = [arg.strip() for arg in args_str.split(',') if arg.strip()]
        param_names = [arg.split()[-1] for arg in args] if args else []

        main_desc, param_descs = _ParseComments(comments, param_names)

        decl = {
            "name": _CamelToSnake(name),
            "description": main_desc,
        }

        if len(args) > 0:
            decl["parameters"] = {
                "type": "OBJECT",
                "properties": {},
                "required": []
            }
            for arg in args:
                # E.g. "string query", "bool new_tab"
                parts = arg.split()
                param_name = parts[-1]
                mojom_type = " ".join(parts[:-1])

                param_schema = {"type": _MojomTypeToGeminiType(mojom_type)}
                if param_name in param_descs:
                    param_schema["description"] = param_descs[param_name]

                decl["parameters"]["properties"][param_name] = param_schema
                decl["parameters"]["required"].append(param_name)

            if not decl["parameters"]["required"]:
                del decl["parameters"]["required"]

        tool_declarations.append(decl)

    return tool_declarations


def GenerateToolsJson(header_paths):
    tool_declarations = []
    for path in header_paths:
        tool_declarations.extend(ParseMojomFile(path))

    full_json_obj = [{"functionDeclarations": tool_declarations}]
    json_str = json.dumps(full_json_obj, indent=2)

    output = io.StringIO()
    output.write("// Copyright 2026 The Chromium Authors\n")
    output.write("// Use of this source code is governed by a BSD-style "
                 "license that can be\n")
    output.write("// found in the LICENSE file.\n\n")
    output.write("// THIS FILE IS GENERATED FROM tools/*_tools.mojom. "
                 "DO NOT EDIT.\n\n")
    output.write("/**\n")
    output.write(" * Returns the built-in tool definitions for the Gemini "
                 "Live API based on the\n")
    output.write(" * AiOverlayTools mojom interfaces.\n")
    output.write(" */\n")
    output.write("export const kBuiltInToolDefinitions = JSON.stringify(\n")
    output.write(json_str)
    output.write("\n);\n")

    return output.getvalue()


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--in-files',
                        nargs='+',
                        required=True,
                        help='Path to mojom files')
    parser.add_argument('--out-file',
                        required=True,
                        help='Path to write generated_tool_definitions.ts')
    args = parser.parse_args()

    tools_text = GenerateToolsJson(args.in_files)
    target_path = args.out_file

    with open(target_path, 'w', newline='') as f:
        f.write(tools_text)


if __name__ == '__main__':
    Main()
