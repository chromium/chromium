#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Removes code coverage flags from invocations of the Clang C/C++ compiler.

If the GN arg `use_clang_coverage=true`, this script will be invoked by default.
GN will add coverage instrumentation flags to almost all source files.

This script is used to remove instrumentation flags from a subset of the source
files. By default, it will not remove flags from any files. If the option
--files-to-instrument is passed, this script will remove flags from all files
except the ones listed in --files-to-instrument.

This script also contains hard-coded exclusion lists of files to never
instrument, indexed by target operating system. Files in these lists have their
flags removed in both modes. The OS can be selected with --target-os.

This script also contains hard-coded force lists of files to always instrument,
indexed by target operating system. Files in these lists never have their flags
removed in either mode. The OS can be selected with --target-os.

The order of precedence is: force list, exclusion list, --files-to-instrument.

The path to the coverage instrumentation input file should be relative to the
root build directory, and the file consists of multiple lines where each line
represents a path to a source file, and the specified paths must be relative to
the root build directory. e.g. ../../base/task/post_task.cc for build
directory 'out/Release'. The paths should be written using OS-native path
separators for the current platform.

One caveat with this compiler wrapper is that it may introduce unexpected
behaviors in incremental builds when the file path to the coverage
instrumentation input file changes between consecutive runs, so callers of this
script are strongly advised to always use the same path such as
"${root_build_dir}/coverage_instrumentation_input.txt".

It's worth noting on try job builders, if the contents of the instrumentation
file changes so that a file doesn't need to be instrumented any longer, it will
be recompiled automatically because if try job B runs after try job A, the files
that were instrumented in A will be updated (i.e., reverted to the checked in
version) in B, and so they'll be considered out of date by ninja and recompiled.

Example usage:
  clang_code_coverage_wrapper.py \\
      --files-to-instrument=coverage_instrumentation_input.txt
"""

from __future__ import print_function

import argparse
import os
import subprocess
import sys

# Flags used to enable coverage instrumentation.
# Flags should be listed in the same order that they are added in
# build/config/coverage/BUILD.gn
_COVERAGE_FLAGS = [
    '-fprofile-instr-generate',
    '-fcoverage-mapping',
    # Following experimental flags remove unused header functions from the
    # coverage mapping data embedded in the test binaries, and the reduction
    # of binary size enables building Chrome's large unit test targets on
    # MacOS. Please refer to crbug.com/796290 for more details.
    '-mllvm',
    '-limited-coverage-experimental=true',
]

# Files that should not be built with coverage flags by default.
_DEFAULT_COVERAGE_EXCLUSION_LIST = [
    # TODO(crbug.com/1051561): angle_unittests affected by coverage.
    '../../base/message_loop/message_pump_default.cc',
    '../../base/message_loop/message_pump_libevent.cc',
    '../../base/message_loop/message_pump_win.cc',
    '../../base/task/sequence_manager/thread_controller_with_message_pump_impl.cc',  #pylint: disable=line-too-long
]

# Map of exclusion lists indexed by target OS.
# If no target OS is defined, or one is defined that doesn't have a specific
# entry, use _DEFAULT_COVERAGE_EXCLUSION_LIST.
_COVERAGE_EXCLUSION_LIST_MAP = {
    'android': [
        # This file caused webview native library failed on arm64.
        '../../device/gamepad/dualshock4_controller.cc',
    ],
    'fuchsia': [
        # TODO(crbug.com/1174725): These files caused clang to crash while
        # compiling them.
        '../../base/allocator/partition_allocator/pcscan.cc',
        '../../third_party/skia/src/core/SkOpts.cpp',
        '../../third_party/skia/src/opts/SkOpts_hsw.cpp',
        '../../third_party/skia/third_party/skcms/skcms.cc',
    ],
    'linux': [
        # These files caused a static initializer to be generated, which
        # shouldn't.
        # TODO(crbug.com/990948): Remove when the bug is fixed.
        '../../chrome/browser/media/router/providers/cast/cast_internal_message_util.cc',  #pylint: disable=line-too-long
        '../../components/cast_channel/cast_channel_enum.cc',
        '../../components/cast_channel/cast_message_util.cc',
        '../../components/media_router/common/providers/cast/cast_media_source.cc',  #pylint: disable=line-too-long
        '../../ui/events/keycodes/dom/keycode_converter.cc',
        # TODO(crbug.com/1051561): angle_unittests affected by coverage.
        '../../base/message_loop/message_pump_default.cc',
        '../../base/message_loop/message_pump_libevent.cc',
        '../../base/message_loop/message_pump_win.cc',
        '../../base/task/sequence_manager/thread_controller_with_message_pump_impl.cc',  #pylint: disable=line-too-long
    ],
    'chromeos': [
        # These files caused clang to crash while compiling them. They are
        # excluded pending an investigation into the underlying compiler bug.
        '../../third_party/webrtc/p2p/base/p2p_transport_channel.cc',
        '../../third_party/icu/source/common/uts46.cpp',
        '../../third_party/icu/source/common/ucnvmbcs.cpp',
        '../../base/android/android_image_reader_compat.cc',
        # TODO(crbug.com/1051561): angle_unittests affected by coverage.
        '../../base/message_loop/message_pump_default.cc',
        '../../base/message_loop/message_pump_libevent.cc',
        '../../base/message_loop/message_pump_win.cc',
        '../../base/task/sequence_manager/thread_controller_with_message_pump_impl.cc',  #pylint: disable=line-too-long
    ],
    'win': [
        # TODO(crbug.com/1051561): angle_unittests affected by coverage.
        '../../base/message_loop/message_pump_default.cc',
        '../../base/message_loop/message_pump_libevent.cc',
        '../../base/message_loop/message_pump_win.cc',
        '../../base/task/sequence_manager/thread_controller_with_message_pump_impl.cc',  #pylint: disable=line-too-long
    ],
}

# Map of force lists indexed by target OS.
_COVERAGE_FORCE_LIST_MAP = {
    # clang_profiling.cc refers to the symbol `__llvm_profile_dump` from the
    # profiling runtime. In a partial coverage build, it is possible for a
    # binary to include clang_profiling.cc but have no instrumented files, thus
    # causing an unresolved symbol error because the profiling runtime will not
    # be linked in. Therefore we force coverage for this file to ensure that
    # any target that includes it will also get the profiling runtime.
    'win': [r'..\..\base\test\clang_profiling.cc'],
    # TODO(crbug.com/1141727) We're seeing runtime LLVM errors in mac-rel when
    # no files are changed, so we suspect that this is similar to the other
    # problem with clang_profiling.cc on Windows. The TODO here is to force
    # coverage for this specific file on ALL platforms, if it turns out to fix
    # this issue on Mac as well. It's the only file that directly calls
    # `__llvm_profile_dump` so it warrants some special treatment.
    'mac': ['../../base/test/clang_profiling.cc'],
}


def _remove_flags_from_command(command):
  # We need to remove the coverage flags for this file, but we only want to
  # remove them if we see the exact sequence defined in _COVERAGE_FLAGS.
  # That ensures that we only remove the flags added by GN when
  # "use_clang_coverage" is true. Otherwise, we would remove flags set by
  # other parts of the build system.
  start_flag = _COVERAGE_FLAGS[0]
  num_flags = len(_COVERAGE_FLAGS)
  start_idx = 0
  try:
    while True:
      idx = command.index(start_flag, start_idx)
      if command[idx:idx + num_flags] == _COVERAGE_FLAGS:
        del command[idx:idx + num_flags]
        # There can be multiple sets of _COVERAGE_FLAGS. All of these need to be
        # removed.
        start_idx = idx
      else:
        start_idx = idx + 1
  except ValueError:
    pass


def main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__
  arg_parser.add_argument(
      '--files-to-instrument',
      type=str,
      help='Path to a file that contains a list of file names to instrument.')
  arg_parser.add_argument(
      '--target-os', required=False, help='The OS to compile for.')
  arg_parser.add_argument('args', nargs=argparse.REMAINDER)
  parsed_args = arg_parser.parse_args()

  if (parsed_args.files_to_instrument and
      not os.path.isfile(parsed_args.files_to_instrument)):
    raise Exception('Path to the coverage instrumentation file: "%s" doesn\'t '
                    'exist.' % parsed_args.files_to_instrument)

  compile_command = parsed_args.args
  if not any('clang' in s for s in compile_command):
    return subprocess.call(compile_command)

  target_os = parsed_args.target_os

  try:
    # The command is assumed to use Clang as the compiler, and the path to the
    # source file is behind the -c argument, and the path to the source path is
    # relative to the root build directory. For example:
    # clang++ -fvisibility=hidden -c ../../base/files/file_path.cc -o \
    #   obj/base/base/file_path.o
    # On Windows, clang-cl.exe uses /c instead of -c.
    source_flag = '/c' if target_os == 'win' else '-c'
    source_flag_index = compile_command.index(source_flag)
  except ValueError:
    print('%s argument is not found in the compile command.' % source_flag)
    raise

  if source_flag_index + 1 >= len(compile_command):
    raise Exception('Source file to be compiled is missing from the command.')

  # On Windows, filesystem paths should use '\', but GN creates build commands
  # that use '/'. We invoke os.path.normpath to ensure that the path uses the
  # correct separator for the current platform (i.e. '\' on Windows and '/'
  # otherwise).
  compile_source_file = os.path.normpath(compile_command[source_flag_index + 1])
  extension = os.path.splitext(compile_source_file)[1]
  if not extension in ['.c', '.cc', '.cpp', '.cxx', '.m', '.mm', '.S']:
    raise Exception('Invalid source file %s found' % compile_source_file)
  exclusion_list = _COVERAGE_EXCLUSION_LIST_MAP.get(
      target_os, _DEFAULT_COVERAGE_EXCLUSION_LIST)
  force_list = _COVERAGE_FORCE_LIST_MAP.get(target_os, [])

  should_remove_flags = False
  if compile_source_file not in force_list:
    if compile_source_file in exclusion_list:
      should_remove_flags = True
    elif parsed_args.files_to_instrument:
      with open(parsed_args.files_to_instrument) as f:
        if compile_source_file not in f.read():
          should_remove_flags = True

  if should_remove_flags:
    _remove_flags_from_command(compile_command)

  return subprocess.call(compile_command)


if __name__ == '__main__':
  sys.exit(main())
