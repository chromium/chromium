# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Reads ai_overlay_dialog_tools.h and outputs generated tool definitions to
generated_tool_definitions.h.
'''

import argparse
import io
import re
import json


def _CamelToSnake(name):
    name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', name).lower()


def _ParseComments(comments_str):
    lines = []
    for line in comments_str.splitlines():
        line = line.strip()
        # Remove leading slashes and whitespace
        line = re.sub(r'^//\s*', '', line)
        if line:
            lines.append(line)
    return " ".join(lines)


def _CppTypeToGeminiType(cpp_type):
    if 'string' in cpp_type: return 'STRING'
    if 'bool' in cpp_type: return 'BOOLEAN'
    if 'int' in cpp_type: return 'INTEGER'
    if 'float' in cpp_type or 'double' in cpp_type: return 'NUMBER'
    return 'STRING'


def GenerateToolsJson(header_path):
    with open(header_path, 'r', encoding='utf-8') as f:
        text = f.read()

    start_marker = "// --- AI OVERLAY TOOLS START ---"
    end_marker = "// --- AI OVERLAY TOOLS END ---"
    start_idx = text.find(start_marker)
    end_idx = text.find(end_marker)
    if start_idx != -1 and end_idx != -1:
        text = text[start_idx + len(start_marker):end_idx]

    tool_declarations = []

    pattern = re.compile(
        r'((?:[ \t]*//[^\n]*\n)+)[ \t]*(?:virtual\s+)?void\s+'
        r'([A-Z]\w*)\((.*?)\)(?:\s*override)?(?:\s*=\s*0)?;',
        re.DOTALL)

    for m in pattern.finditer(text):
        comments, name, args_str = m.groups()
        description = _ParseComments(comments)

        decl = {
            "name": _CamelToSnake(name),
            "description": description,
        }

        # The last arg is always the callback, so ignore it
        args = [arg.strip() for arg in args_str.split(',')]
        if len(args) > 1:
            decl["parameters"] = {
                "type": "OBJECT",
                "properties": {},
                "required": []
            }
            # All args except the last callback
            for arg in args[:-1]:
                # Split type and name, taking the last word as name.
                # E.g. "const std::string& query" -> "query"
                parts = arg.split()
                param_name = parts[-1].lstrip('*&')
                cpp_type = " ".join(parts[:-1])

                decl["parameters"]["properties"][param_name] = {
                    "type": _CppTypeToGeminiType(cpp_type)
                }
                decl["parameters"]["required"].append(param_name)

            if not decl["parameters"]["required"]:
                del decl["parameters"]["required"]

        tool_declarations.append(decl)

    full_json_obj = [{"functionDeclarations": tool_declarations}]
    json_str = json.dumps(full_json_obj, indent=2)

    output = io.StringIO()
    output.write("// Copyright 2026 The Chromium Authors\n")
    output.write("// Use of this source code is governed by a BSD-style "
                 "license that can be\n")
    output.write("// found in the LICENSE file.\n\n")
    output.write("// THIS FILE IS GENERATED FROM ai_overlay_dialog_tools.h. "
                 "DO NOT EDIT.\n\n")
    output.write("#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_"
                 "TOOL_DEFINITIONS_H_\n")
    output.write("#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_"
                 "TOOL_DEFINITIONS_H_\n\n")
    output.write("#include <string_view>\n\n")
    output.write("namespace ai_overlay_dialog {\n\n")
    output.write("/**\n")
    output.write(" * Returns the built-in tool definitions for the Gemini "
                 "Live API based on the\n")
    output.write(" * AiOverlayDialogTools C++ header.\n")
    output.write(" */\n")
    output.write("inline constexpr std::string_view "
                 "kBuiltInToolDefinitions =\n")

    for line in json_str.splitlines(True):
        escaped = line.replace('\\', '\\\\').replace('"', '\\"')
        escaped = escaped.replace('\n', '\\n')

        chunk_size = 73
        i = 0
        while i < len(escaped):
            chunk = escaped[i:i + chunk_size]

            bs_count = 0
            for char in reversed(chunk):
                if char == '\\':
                    bs_count += 1
                else:
                    break

            if bs_count % 2 != 0:
                chunk = chunk[:-1]

            output.write(f'    "{chunk}"\n')
            i += len(chunk)

    output.write("    ;\n\n")
    output.write("}  // namespace ai_overlay_dialog\n\n")
    output.write("#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_"
                 "TOOL_DEFINITIONS_H_\n")

    return output.getvalue()


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--in-file',
                        required=True,
                        help='Path to ai_overlay_dialog_tools.h')
    parser.add_argument('--out-file',
                        required=True,
                        help='Path to write generated_tool_definitions.h')
    args = parser.parse_args()

    tools_text = GenerateToolsJson(args.in_file)
    target_path = args.out_file

    with open(target_path, 'w', newline='') as f:
        f.write(tools_text)


if __name__ == '__main__':
    Main()
