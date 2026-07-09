# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Checks the Glic API for common mistakes and backwards compatibility
issues. Used in PRESUBMIT.py.
'''

import json
import os
import re
import sys
import tempfile
import subprocess
from dataclasses import dataclass

DEBUG = False

API_FILE = 'chrome/browser/resources/glic/glic_api/glic_api.ts'
SCRIPT_PATH = os.path.dirname(os.path.abspath(__file__))
ROOT_PATH = os.path.join(SCRIPT_PATH, '../../../../../')


@dataclass
class InterfaceInfo:
    is_declare: bool
    type_parameters: list[str]


def StripComments(source_text):
    # Strip comments.
    source_text = re.sub(r'//.*', '', source_text)
    source_text = re.sub(r'/\*.*?\*/', '', source_text)
    return source_text


# Returns the set of types defined in the given interface.
def ReadTypeSet(source_text: str, set_name: str) -> set[str]:
    # Matches the interface body.
    pattern = r'export\s+interface\s+' + set_name + r'\s*\{([\s\S]*?)\}'
    match = re.search(pattern, source_text)
    if not match:
        raise Exception('Could not find interface ' + set_name)
    interface_body = StripComments(match.group(1))
    # Example declarations supported:
    #   foo: Type;
    #   bar: Type<any>;
    #   baz: typeof Type; (needed for enums)
    decl_pattern = r'\w+\s*:\s*(?:typeof\s+)?(\w+)(?:\s*<.*?>)?\s*;'
    return set(re.findall(decl_pattern, interface_body))


def _GetAllExportedInterfacesInString(
        source_text: str) -> dict[str, InterfaceInfo]:
    pattern = r'\n\s*export\s+(declare\s+)?interface\s+(\w+)(<[^<>]*>)?'
    matches = re.finditer(pattern, StripComments(source_text))
    result = {}
    for match in matches:
        type_parameters = []
        if match.group(3):
            type_parameters = [
                p.strip() for p in match.group(3)[1:-1].split(',')
            ]
        result[match.group(2)] = InterfaceInfo(
            match.group(1) is not None, type_parameters)
    return result


def GetPublicNamesInGeneratedBlock(glic_api_contents: str) -> set[str]:
    START_MARKER = '/// BEGIN_GENERATED - DO NOT MODIFY BELOW'
    END_MARKER = '/// END_GENERATED - DO NOT MODIFY ABOVE'
    start = glic_api_contents.find(START_MARKER)
    end = glic_api_contents.find(END_MARKER)
    if start < 0 or end < 0:
        return set()
    block_content = glic_api_contents[start:end]
    return set(re.findall(r'export\s+import\s+(\w+)\s*=', block_content))


def GetBackwardsCompatibleTypes(
        api_files: dict[str, str]) -> dict[str, InterfaceInfo]:
    all_types = {}
    for contents in api_files.values():
        for name, info in _GetAllExportedInterfacesInString(contents).items():
            if name in all_types:
                if not all_types[name].type_parameters and info.type_parameters:
                    all_types[name] = info
            else:
                all_types[name] = info
    if 'glic_api.ts' in api_files:
        glic_api_contents = api_files['glic_api.ts']
        public_interface_names = GetPublicNamesInGeneratedBlock(
            glic_api_contents)
        interfaces_in_api = _GetAllExportedInterfacesInString(
            glic_api_contents)
        public_interface_names.update(interfaces_in_api.keys())
        all_types = {
            name: info
            for name, info in all_types.items()
            if name in public_interface_names
        }
        for private_type in ReadTypeSet(glic_api_contents, 'PrivateTypes'):
            if private_type in all_types:
                del all_types[private_type]
    return all_types


def CheckWithEslint(api_file_path: str) -> list[str]:
    eslint_cmd = [
        sys.executable,
        os.path.join(ROOT_PATH, 'third_party/node/node.py'),
        os.path.join(ROOT_PATH,
                     'third_party/node/node_modules/eslint/bin/eslint.js'),
        '--config',
        os.path.join(SCRIPT_PATH, 'eslint.config.mjs'),
        api_file_path,
    ]
    try:
        subprocess.check_output(eslint_cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        return [e.output.decode('utf-8')]
    return []


# returns a dict of enum name to enum declaration text.
def GetAllEnumDefinitions(source_text: str) -> dict[str, str]:
    # Use a multiline regex to extract all enum definitions
    # from glic_api.ts.
    enums = {}
    pattern = r'export\s+enum\s+(\w+)\s*\{([\s\S]*?)\}'
    for match in re.finditer(pattern, source_text):
        name = match.group(1)
        full_text = match.group(0)
        enums[name] = full_text
    return enums


def BuildBackwardsCompatibleTypesDeclaration(api_files: dict[str, str]) -> str:
    types = GetBackwardsCompatibleTypes(api_files)

    def MakeDecl(name):
        type_suffix = ''
        if types[name].type_parameters:
            type_suffix = '<' + ','.join(
                ['any'] * len(types[name].type_parameters)) + '>'
        return f'  {name}: {name}{type_suffix};'

    declarations = [MakeDecl(t) for t in sorted(types.keys())]
    return 'export interface TheBackwardsCompatibleTypes {\n' + '\n'.join(
        declarations) + '\n}'


def BuildExtensibleEnumsTypeDeclaration(api_files: dict[str, str]) -> str:
    enums = {}
    for contents in api_files.values():
        enums.update(GetAllEnumDefinitions(contents))
    closed_enums = set()
    if 'glic_api.ts' in api_files:
        glic_api_contents = api_files['glic_api.ts']
        closed_enums = ReadTypeSet(glic_api_contents, 'ClosedEnums')
        public_enum_names = GetPublicNamesInGeneratedBlock(glic_api_contents)
        enums_in_api = GetAllEnumDefinitions(glic_api_contents)
        public_enum_names.update(enums_in_api.keys())
        enums = {
            name: text
            for name, text in enums.items() if name in public_enum_names
        }
    extensible_enums = enums.keys() - closed_enums

    def MakeDecl(name):
        return f'  {name}: typeof {name};'

    declarations = [MakeDecl(t) for t in sorted(extensible_enums)]
    return 'export interface TheExtensibleEnums {\n' + '\n'.join(
        declarations) + '\n}'


def BuildHelpers(api_files: dict[str, str]) -> str:
    return '\n'.join([
        BuildBackwardsCompatibleTypesDeclaration(api_files),
        BuildExtensibleEnumsTypeDeclaration(api_files)
    ])


def CheckCompatibility(old_files: dict[str, str],
                       new_files: dict[str, str]) -> list[str]:
    tmp_dir = tempfile.TemporaryDirectory()
    tmp_dir_name = tmp_dir.name

    # For debugging, use a temporary directory that won't be deleted.
    if DEBUG:
        tmp_dir_name = tempfile.mkdtemp()

    # Create subdirectories for each revision to isolate compilation
    os.makedirs(os.path.join(tmp_dir_name, 'new'))
    os.makedirs(os.path.join(tmp_dir_name, 'old_original'))
    os.makedirs(os.path.join(tmp_dir_name, 'old_edited'))

    # Write new files
    for filename, contents in new_files.items():
        path = os.path.join(tmp_dir_name, 'new', filename)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if filename == 'glic_api.ts':
            contents = contents + '\n' + BuildHelpers(new_files)
        with open(path, 'w') as f:
            f.write(contents)

    # Write old original files
    for filename, contents in old_files.items():
        path = os.path.join(tmp_dir_name, 'old_original', filename)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if filename == 'glic_api.ts':
            contents = contents + '\n' + BuildHelpers(old_files)
        with open(path, 'w') as f:
            f.write(contents)

    # Write old edited files (with extensible enums replaced by new versions)
    closed_enums = set()
    if 'glic_api.ts' in old_files:
        closed_enums = ReadTypeSet(old_files['glic_api.ts'], 'ClosedEnums')

    new_enums = {}
    for contents in new_files.values():
        new_enums.update(GetAllEnumDefinitions(contents))

    old_edited_files = {}
    for filename, contents in old_files.items():
        old_enums = GetAllEnumDefinitions(contents)
        for enum_name, enum_text in old_enums.items():
            if enum_name in closed_enums:
                continue
            if enum_name in new_enums:
                contents = contents.replace(enum_text, new_enums[enum_name])
        old_edited_files[filename] = contents

    for filename, contents in old_edited_files.items():
        path = os.path.join(tmp_dir_name, 'old_edited', filename)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if filename == 'glic_api.ts':
            contents = contents + '\n' + BuildHelpers(old_edited_files)
        with open(path, 'w') as f:
            f.write(contents)

    tsconfig_path = os.path.join(tmp_dir_name, 'tsconfig.json')
    with open(tsconfig_path, 'w') as tsconfigfile:
        tsconfigfile.write('''{
  "extends": "$ROOT/chrome/browser/resources/glic/presubmit/tsconfig.json",
  "compilerOptions": {
    "ignoreDeprecations": "6.0",
    "baseUrl": "$ROOT",
    "paths": {
      "@tmp/new_glic_api.js": ["$TMP/new/glic_api.ts"],
      "@tmp/old_edited_glic_api.js": ["$TMP/old_edited/glic_api.ts"],
      "@tmp/old_glic_api.js": ["$TMP/old_original/glic_api.ts"]
    }
  }
}
'''.replace("$TMP",
            tmp_dir_name.replace('\\',
                                 '/')).replace('$ROOT',
                                               ROOT_PATH.replace('\\', '/')))

    message = (
        '** Your changelist is a backwards-incompatible Glic API change!\n' +
        '** Did you add a non-optional field or function, or change the\n' +
        '** type of an existing field or function?\n' +
        '** Please fix, or add Bypass-Glic-Api-Compatibility-Check: <reason>' +
        ' to your changelist description if this is intended. See ' +
        'http://shortn/_sMpo1Bq6sw for more information. Error:\n  ')

    tsc_cmd = [
        sys.executable,
        os.path.join(ROOT_PATH, 'third_party/node/node.py'),
        os.path.join(ROOT_PATH,
                     'third_party/node/node_modules/typescript/bin/tsc'),
        '--noEmit', '-p', tsconfig_path
    ]

    if DEBUG:
        print('Running', ' '.join(tsc_cmd))

    try:
        subprocess.check_output(tsc_cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        return [message + e.output.decode('utf-8')]
    return []


def main():
    """
    Sets up a temporary directory with a copy of the glic_api.ts before
    modification, and a tsconfig.json file to build
    chrome/browser/resources/glic/presubmit/check_api_compatibility.ts.
    """
    old_api_stdin = False
    skip_compatibility_check = False
    glic_api_path = os.path.join(
        ROOT_PATH, 'chrome/browser/resources/glic/glic_api/glic_api.ts')
    for arg in sys.argv[1:]:
        if arg == '--old-stdin':
            old_api_stdin = True
        if arg == '--skip-compatibility-check':
            skip_compatibility_check = True
        if arg.startswith('--api-file-path='):
            glic_api_path = arg.split('=')[1]
        if arg.startswith('--debug'):
            DEBUG = True
    errors = []

    # Read new files from disk
    api_dir = os.path.dirname(glic_api_path)
    entry_point_basename = os.path.basename(glic_api_path)
    new_files = {}

    for filename in os.listdir(api_dir):
        if filename.endswith('.ts') and not filename.endswith('.d.ts'):
            with open(os.path.join(api_dir, filename), 'r') as f:
                content = f.read()
            if filename == entry_point_basename:
                new_files['glic_api.ts'] = content
            else:
                new_files[filename] = content

    # Read old files from stdin or disk
    old_files = {}
    if old_api_stdin:
        stdin_data = sys.stdin.read()
        try:
            data = json.loads(stdin_data)
            for k, v in data.items():
                basename = os.path.basename(k)
                if basename == entry_point_basename:
                    old_files['glic_api.ts'] = v
                else:
                    old_files[basename] = v
        except json.JSONDecodeError:
            # Fallback if raw manual file is piped
            old_files['glic_api.ts'] = stdin_data
    else:
        # If no stdin, old is the same as new
        old_files = dict(new_files)

    if not skip_compatibility_check:
        errors.extend(CheckCompatibility(old_files, new_files))
    errors.extend(CheckWithEslint(glic_api_path))

    for error in errors:
        print(error, file=sys.stderr)

    if errors:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
