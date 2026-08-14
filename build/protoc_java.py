#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate java source files from protobuf files.

This is the action script for the proto_java_library template.

It performs the following steps:
1. Deletes all old sources (ensures deleted classes are not part of new jars).
2. Creates source directory.
3. Generates Java files using protoc (output into either --java-out-dir or
   --srcjar).
4. Creates a new stamp file.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys

import action_helpers
import zip_helpers

sys.path.append(os.path.join(os.path.dirname(__file__), 'android', 'gyp'))
from util import build_utils

_PROTOBUF_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    'third_party',
    'protobuf',
    'python',
)
if os.path.exists(_PROTOBUF_DIR) and _PROTOBUF_DIR not in sys.path:
    sys.path.insert(1, _PROTOBUF_DIR)

# See b/315080809, b/545614704 and b/336173744 for context.
# This relies on print_python_deps.py to pick up the in-tree version of the
# protobuf dependency. If not careful, print_python_deps.py can pickup the wrong
# version of google.protobuf from vpython's site-packages. This will lead to
# print_python_deps.py failing to declare the dependency on google.protobuf in
# protoc_java.pydeps. This in turns leads to less hermetic builds.
from google.protobuf import descriptor_pb2


def _HasJavaPackage(proto_lines):
    return any(
        line.strip().startswith('option java_package') for line in proto_lines
    )


def _EnforceJavaPackage(proto_srcs):
    for proto_path in proto_srcs:
        with open(proto_path) as in_proto:
            if not _HasJavaPackage(in_proto.readlines()):
                raise Exception(
                    'Proto files for java must contain a "java_package" '
                    'line: {}'.format(proto_path)
                )


def _GenerateIntDefFiles(temp_dir, descriptor_path):
    """Generates companion @IntDef files for all enums in the given descriptor.

    These are Chrome-specific @IntDef type-use annotations to allow using
    efficient primitive integers instead of Java enum objects in Chromium code.
    """
    with open(descriptor_path, 'rb') as df:
        fds = descriptor_pb2.FileDescriptorSet.FromString(df.read())

    def collect_enums(msgs):
        res = {}
        for m in msgs:
            for e in m.enum_type:
                res[e.name] = [(v.name, v.number) for v in e.value]
            res.update(collect_enums(m.nested_type))
        return res

    for file_proto in fds.file:
        enums = {}
        for e in file_proto.enum_type:
            enums[e.name] = [(v.name, v.number) for v in e.value]
        enums.update(collect_enums(file_proto.message_type))

        if not enums:
            continue

        package_name = file_proto.options.java_package or file_proto.package
        if file_proto.options.java_outer_classname:
            outer_classname = file_proto.options.java_outer_classname + 'IntDef'
        else:
            base_name = os.path.basename(file_proto.name).replace('.proto', '')
            outer_classname = (
                ''.join(part.capitalize() for part in base_name.split('_'))
                + 'IntDef'
            )

        interfaces = []
        for enum_name, val_matches in enums.items():
            values = [v for _, v in val_matches]
            min_val = min(values)
            max_val = max(values)
            is_contiguous = sorted(values) == list(range(min_val, max_val + 1))

            constants_decl = []
            constants_decl.append(f'        int MIN_VALUE = {min_val};')
            constants_decl.append(f'        int MAX_VALUE = {max_val};')
            for name, val in val_matches:
                constants_decl.append(f'        int {name} = {val};')
            # ALL_VALUES array allocation is avoided when enums are contiguous
            # since callers can iterate using MIN_VALUE and MAX_VALUE.
            if not is_contiguous:
                all_values_list = ', '.join(
                    [f'{name}' for name, _ in val_matches]
                )
                constants_decl.append(
                    f'        int[] ALL_VALUES = new int[] {{{all_values_list}}};'
                )
            constants_block = '\n'.join(constants_decl)

            intdef_values = ',\n'.join(
                [f'        {enum_name}.{name}' for name, _ in val_matches]
            )

            intdef_interface = (
                f'    @androidx.annotation.IntDef({{\n'
                f'{intdef_values}\n'
                f'    }})\n'
                f'    @java.lang.annotation.Retention(\n'
                f'        java.lang.annotation.RetentionPolicy.SOURCE)\n'
                f'    @java.lang.annotation.Target({{\n'
                f'        java.lang.annotation.ElementType.TYPE_USE\n'
                f'    }})\n'
                f'    public @interface {enum_name} {{\n'
                f'{constants_block}\n'
                f'    }}'
            )
            interfaces.append(intdef_interface)

        interfaces_block = '\n\n'.join(interfaces)
        file_content = (
            f'// Generated by //build/protoc_java.py. Do not edit.\n\n'
            f'package {package_name};\n\n'
            f'public final class {outer_classname} {{\n'
            f'    private {outer_classname}() {{}}\n\n'
            f'{interfaces_block}\n'
            f'}}\n'
        )

        package_dir = os.path.join(temp_dir, *package_name.split('.'))
        os.makedirs(package_dir, exist_ok=True)
        out_file = os.path.join(package_dir, f'{outer_classname}.java')
        with open(out_file, 'w') as f:
            f.write(file_content)


def main(argv):
    parser = argparse.ArgumentParser()
    action_helpers.add_depfile_arg(parser)
    parser.add_argument(
        '--protoc', required=True, help='Path to protoc binary.'
    )
    parser.add_argument('--plugin', help='Path to plugin executable')
    parser.add_argument(
        '--proto-path', required=True, help='Path to proto directory.'
    )
    parser.add_argument(
        '--java-out-dir', help='Path to output directory for java files.'
    )
    parser.add_argument('--srcjar', help='Path to output srcjar.')
    parser.add_argument('--stamp', help='File to touch on success.')
    parser.add_argument(
        '--import-dir',
        action='append',
        default=[],
        help='Extra import directory for protos, can be repeated.',
    )
    parser.add_argument(
        '--generate-intdefs',
        action='store_true',
        help='Generate @IntDef annotations for all proto enums.',
    )
    parser.add_argument('protos', nargs='+', help='proto source files')
    options = parser.parse_args(argv)

    if not options.java_out_dir and not options.srcjar:
        raise Exception('One of --java-out-dir or --srcjar must be specified.')

    _EnforceJavaPackage(options.protos)

    with build_utils.TempDir() as temp_dir:
        protoc_args = []

        generator = 'java'
        if options.plugin:
            generator = 'plugin'
            protoc_args += ['--plugin', 'protoc-gen-plugin=' + options.plugin]

        protoc_args += ['--proto_path', options.proto_path]
        for path in options.import_dir:
            protoc_args += ['--proto_path', path]

        protoc_args += ['--' + generator + '_out=lite:' + temp_dir]

        if options.generate_intdefs:
            descriptor_file = os.path.join(temp_dir, 'proto_descriptor.pb')
            protoc_args += ['--descriptor_set_out=' + descriptor_file]
        else:
            descriptor_file = None

        # Generate Java files using protoc.
        build_utils.CheckOutput(
            [options.protoc] + protoc_args + options.protos,
            # protoc generates superfluous warnings about LITE_RUNTIME deprecation
            # even though we are using the new non-deprecated method.
            stderr_filter=lambda output: build_utils.FilterLines(
                output,
                '|'.join(
                    [
                        r'optimize_for = LITE_RUNTIME',
                        r'java/lite\.md',
                        r'hugepage_text\.cc',
                    ]
                ),
            ),
        )

        if options.generate_intdefs:
            _GenerateIntDefFiles(temp_dir, descriptor_file)

        if options.java_out_dir:
            build_utils.DeleteDirectory(options.java_out_dir)
            shutil.copytree(temp_dir, options.java_out_dir)
        else:
            with action_helpers.atomic_output(options.srcjar) as f:
                zip_helpers.zip_directory(f, temp_dir)

    if options.depfile:
        assert options.srcjar
        deps = options.protos + [options.protoc]
        action_helpers.write_depfile(options.depfile, options.srcjar, deps)

    if options.stamp:
        build_utils.Touch(options.stamp)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
