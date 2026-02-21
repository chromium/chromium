# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Checks the Glic API for common mistakes and backwards compatibility
issues. Used in PRESUBMIT.py.
'''

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


# Returns a dict of interface name to whether it is declared with declare.
def GetAllExportedInterfaces(source_text: str) -> dict[str, InterfaceInfo]:
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


def GetBackwardsCompatibleTypes(source_text: str) -> dict[str, InterfaceInfo]:
    all_types = GetAllExportedInterfaces(source_text)
    for private_type in ReadTypeSet(source_text, 'PrivateTypes'):
        del all_types[private_type]
    return all_types


def CheckForInterfacesWithoutDeclare(source_text: str) -> list[str]:
    interfaces = GetAllExportedInterfaces(source_text)
    interfaces_without_declare = set(
        interface_name
        for interface_name, interface_info in interfaces.items()
        if not interface_info.is_declare)
    interfaces_without_declare -= ReadTypeSet(source_text, 'PrivateTypes')
    if interfaces_without_declare:
        return [
            'All exported interfaces in glic_api.ts must be declared with ' +
            '`declare`. Please update the following interfaces: ' +
            ', '.join(interfaces_without_declare)
        ]
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


def ReplaceEnums(old_source: str, new_source: str,
                 excluded_enums: set[str]) -> str:
    old_enums = GetAllEnumDefinitions(old_source)
    new_enums = GetAllEnumDefinitions(new_source)
    for enum_name, enum_text in old_enums.items():
        if enum_name in excluded_enums:
            continue
        if enum_name in new_enums:
            old_source = old_source.replace(enum_text, new_enums[enum_name])
    return old_source


def BuildBackwardsCompatibleTypesDeclaration(source_text: str) -> str:
    types = GetBackwardsCompatibleTypes(source_text)

    def MakeDecl(name):
        type_suffix = ''
        if types[name].type_parameters:
            type_suffix = '<' + ','.join(
                ['any'] * len(types[name].type_parameters)) + '>'
        return f'  {name}: {name}{type_suffix};'

    declarations = [MakeDecl(t) for t in sorted(types.keys())]
    return 'export interface TheBackwardsCompatibleTypes {\n' + '\n'.join(
        declarations) + '\n}'


def BuildExtensibleEnumsTypeDeclaration(source_text: str) -> str:
    enums = GetAllEnumDefinitions(source_text)
    closed_enums = ReadTypeSet(source_text, 'ClosedEnums')
    extensible_enums = enums.keys() - closed_enums

    def MakeDecl(name):
        return f'  {name}: typeof {name};'

    declarations = [MakeDecl(t) for t in sorted(extensible_enums)]
    return 'export interface TheExtensibleEnums {\n' + '\n'.join(
        declarations) + '\n}'


def AddAnnotations(source_text: str) -> str:
    return '\n'.join([
        source_text,
        BuildBackwardsCompatibleTypesDeclaration(source_text),
        BuildExtensibleEnumsTypeDeclaration(source_text)
    ])


def CheckCompatibility(old_contents: str, new_contents: str) -> list[str]:
    tmp_dir = tempfile.TemporaryDirectory()
    tmp_dir_name = tmp_dir.name

    # For debugging, use a temporary directory that won't be deleted.
    if DEBUG:
        tmp_dir_name = tempfile.mkdtemp()

    with open(os.path.join(tmp_dir_name, 'old_glic_api.ts'), 'w') as oldfile:
        oldfile.write(AddAnnotations(old_contents))
    old_edited_contents = ReplaceEnums(
        old_contents, new_contents, ReadTypeSet(old_contents, 'ClosedEnums'))
    with open(os.path.join(tmp_dir_name, 'old_edited_glic_api.ts'),
              'w') as old_edited_file:
        old_edited_file.write(AddAnnotations(old_edited_contents))
    with open(os.path.join(tmp_dir_name, 'new_glic_api.ts'), 'w') as newfile:
        newfile.write(AddAnnotations(new_contents))

    tsconfig_path = os.path.join(tmp_dir_name, 'tsconfig.json')
    with open(os.path.join(tmp_dir_name, 'tsconfig.json'),
              'w') as tsconfigfile:
        tsconfigfile.write('''{
  "extends": "$ROOT/chrome/browser/resources/glic/presubmit/tsconfig.json",
    "compilerOptions": {
      "baseUrl": "$ROOT",
      "paths": {
        "@tmp/*": ["$TMP/*"]
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
    old_contents = None
    skip_compatibility_check = False
    glic_api_path = os.path.join(
        ROOT_PATH, 'chrome/browser/resources/glic/glic_api/glic_api.ts')
    for arg in sys.argv[1:]:
        if arg == '--old-stdin':
            old_contents = sys.stdin.read()
        if arg == '--skip-compatibility-check':
            skip_compatibility_check = True
        if arg.startswith('--api-file-path='):
            glic_api_path = arg.split('=')[1]
        if arg.startswith('--debug'):
            DEBUG = True
    presubmit_results = []
    errors = []

    with open(glic_api_path, 'r') as glic_api_file:
        new_contents = glic_api_file.read()

    errors.extend(CheckForInterfacesWithoutDeclare(new_contents))
    if old_contents is None:
        old_contents = new_contents
    if not skip_compatibility_check:
        errors.extend(CheckCompatibility(old_contents, new_contents))

    for error in errors:
        print(error, file=sys.stderr)

    if errors:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
