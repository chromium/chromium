#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""usage: rc.py [options] input.res
A resource compiler for .rc files.

options:
-h, --help     Print this message.
-I<dir>        Add include path, used for both headers and resources.
-imsvc<dir>    Add system include path, used for preprocessing only.
-D<sym>        Define a macro for the preprocessor.
/fo<out>       Set path of output .res file.
/nologo        Ignored (rc.py doesn't print a logo by default).
/showIncludes  Print referenced header and resource files."""

from __future__ import print_function
from collections import namedtuple
import codecs
import os
import re
import subprocess
import sys
import tempfile


THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = \
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(THIS_DIR))))


def ParseFlags():
  """Parses flags off sys.argv and returns the parsed flags."""
  # Can't use optparse / argparse because of /fo flag :-/
  includes = []
  imsvcs = []
  defines = []
  output = None
  input = None
  show_includes = False
  # Parse.
  for flag in sys.argv[1:]:
    if flag == '-h' or flag == '--help':
      print(__doc__)
      sys.exit(0)
    if flag.startswith('-I'):
      includes.append(flag)
    elif flag.startswith('-imsvc'):
      imsvcs.append(flag)
    elif flag.startswith('-D'):
      defines.append(flag)
    elif flag.startswith('/fo'):
      if output:
        print('rc.py: error: multiple /fo flags', '/fo' + output, flag,
              file=sys.stderr)
        sys.exit(1)
      output = flag[3:]
    elif flag == '/nologo':
      pass
    elif flag == '/showIncludes':
      show_includes = True
    elif (flag.startswith('-') or
          (flag.startswith('/') and not os.path.exists(flag))):
      print('rc.py: error: unknown flag', flag, file=sys.stderr)
      print(__doc__, file=sys.stderr)
      sys.exit(1)
    else:
      if input:
        print('rc.py: error: multiple inputs:', input, flag, file=sys.stderr)
        sys.exit(1)
      input = flag
  # Validate and set default values.
  if not input:
    print('rc.py: error: no input file', file=sys.stderr)
    sys.exit(1)
  if not output:
    output = os.path.splitext(input)[0] + '.res'
  Flags = namedtuple('Flags', ['includes', 'defines', 'output', 'imsvcs',
                               'input', 'show_includes'])
  return Flags(includes=includes, defines=defines, output=output, imsvcs=imsvcs,
               input=input, show_includes=show_includes)


def ReadInput(input):
  """"Reads input and returns it. For UTF-16LEBOM input, converts to UTF-8."""
  # Microsoft's rc.exe only supports unicode in the form of UTF-16LE with a BOM.
  # Our rc binary sniffs for UTF-16LE.  If that's not found, if /utf-8 is
  # passed, the input is treated as UTF-8.  If /utf-8 is not passed and the
  # input is not UTF-16LE, then our rc errors out on characters outside of
  # 7-bit ASCII.  Since the driver always converts UTF-16LE to UTF-8 here (for
  # the preprocessor, which doesn't support UTF-16LE), our rc will either see
  # UTF-8 with the /utf-8 flag (for UTF-16LE input), or ASCII input.
  # This is compatible with Microsoft rc.exe.  If we wanted, we could expose
  # a /utf-8 flag for the driver for UTF-8 .rc inputs too.
  # TODO(thakis): Microsoft's rc.exe supports BOM-less UTF-16LE. We currently
  # don't, but for chrome it currently doesn't matter.
  is_utf8 = False
  try:
    with open(input, 'rb') as rc_file:
      rc_file_data = rc_file.read()
      if rc_file_data.startswith(codecs.BOM_UTF16_LE):
        rc_file_data = rc_file_data[2:].decode('utf-16le').encode('utf-8')
        is_utf8 = True
  except IOError:
    print('rc.py: failed to open', input, file=sys.stderr)
    sys.exit(1)
  except UnicodeDecodeError:
    print('rc.py: failed to decode UTF-16 despite BOM', input, file=sys.stderr)
    sys.exit(1)
  return rc_file_data, is_utf8


def Preprocess(rc_file_data, flags):
  """Runs the input file through the preprocessor."""
  clang = os.path.join(SRC_DIR, 'third_party', 'llvm-build',
                       'Release+Asserts', 'bin', 'clang-cl')
  # Let preprocessor write to a temp file so that it doesn't interfere
  # with /showIncludes output on stdout.
  if sys.platform == 'win32':
    clang += '.exe'
  temp_handle, temp_file = tempfile.mkstemp(suffix='.i')
  # Closing temp_handle immediately defeats the purpose of mkstemp(), but I
  # can't figure out how to let write to the temp file on Windows otherwise.
  os.close(temp_handle)
  clang_cmd = [clang, '/P', '/DRC_INVOKED', '/TC', '-', '/Fi' + temp_file]
  if flags.imsvcs:
    clang_cmd += ['/X']
  if os.path.dirname(flags.input):
    # This must precede flags.includes.
    clang_cmd.append('-I' + os.path.dirname(flags.input))
  if flags.show_includes:
    clang_cmd.append('/showIncludes')
  clang_cmd += flags.imsvcs + flags.includes + flags.defines
  p = subprocess.Popen(clang_cmd, stdin=subprocess.PIPE)
  p.communicate(input=rc_file_data)
  if p.returncode != 0:
    sys.exit(p.returncode)
  preprocessed_output = open(temp_file, 'rb').read()
  os.remove(temp_file)

  # rc.exe has a wacko preprocessor:
  # https://msdn.microsoft.com/en-us/library/windows/desktop/aa381033(v=vs.85).aspx
  # """RC treats files with the .c and .h extensions in a special manner. It
  # assumes that a file with one of these extensions does not contain
  # resources. If a file has the .c or .h file name extension, RC ignores all
  # lines in the file except the preprocessor directives."""
  # Thankfully, the Microsoft headers are mostly good about putting everything
  # in the system headers behind `if !defined(RC_INVOKED)`, so regular
  # preprocessing with RC_INVOKED defined works.
  return preprocessed_output


def RunRc(preprocessed_output, is_utf8, flags):
  if sys.platform.startswith('linux'):
    rc = os.path.join(THIS_DIR, 'linux64', 'rc')
  elif sys.platform == 'darwin':
    rc = os.path.join(THIS_DIR, 'mac', 'rc')
  elif sys.platform == 'win32':
    rc = os.path.join(THIS_DIR, 'win', 'rc.exe')
  else:
    print('rc.py: error: unsupported platform', sys.platform, file=sys.stderr)
    sys.exit(1)
  rc_cmd = [rc]
  # Make sure rc-relative resources can be found:
  if os.path.dirname(flags.input):
    rc_cmd.append('/cd' + os.path.dirname(flags.input))
  rc_cmd.append('/fo' + flags.output)
  if is_utf8:
    rc_cmd.append('/utf-8')
  # TODO(thakis): cl currently always prints full paths for /showIncludes,
  # but clang-cl /P doesn't.  Which one is right?
  if flags.show_includes:
    rc_cmd.append('/showIncludes')
  # Microsoft rc.exe searches for referenced files relative to -I flags in
  # addition to the pwd, so -I flags need to be passed both to both
  # the preprocessor and rc.
  rc_cmd += flags.includes
  p = subprocess.Popen(rc_cmd, stdin=subprocess.PIPE)
  p.communicate(input=preprocessed_output)

  if flags.show_includes and p.returncode == 0:
    TOOL_DIR = os.path.dirname(os.path.relpath(THIS_DIR)).replace("\\", "/")
    # Since tool("rc") can't have deps, add deps on this script and on rc.py
    # and its deps here, so that rc edges become dirty if rc.py changes.
    print('Note: including file: {}/tool_wrapper.py'.format(TOOL_DIR))
    print('Note: including file: {}/rc/rc.py'.format(TOOL_DIR))
    print(
        'Note: including file: {}/rc/linux64/rc.sha1'.format(TOOL_DIR))
    print('Note: including file: {}/rc/mac/rc.sha1'.format(TOOL_DIR))
    print(
        'Note: including file: {}/rc/win/rc.exe.sha1'.format(TOOL_DIR))

  return p.returncode


def CompareToMsRcOutput(preprocessed_output, is_utf8, flags):
  msrc_in = flags.output + '.preprocessed.rc'

  # Strip preprocessor line markers.
  preprocessed_output = re.sub(r'^#.*$', '', preprocessed_output, flags=re.M)
  if is_utf8:
    preprocessed_output = preprocessed_output.decode('utf-8').encode('utf-16le')
  with open(msrc_in, 'wb') as f:
    f.write(preprocessed_output)

  msrc_out = flags.output + '_ms_rc'
  msrc_cmd = ['rc', '/nologo', '/x', '/fo' + msrc_out]

  # Make sure rc-relative resources can be found. rc.exe looks for external
  # resource files next to the file, but the preprocessed file isn't where the
  # input was.
  # Note that rc searches external resource files in the order of
  # 1. next to the input file
  # 2. relative to cwd
  # 3. next to -I directories
  # Changing the cwd means we'd have to rewrite all -I flags, so just add
  # the input file dir as -I flag. That technically gets the order of 1 and 2
  # wrong, but in Chromium's build the cwd is the gn out dir, and generated
  # files there are in obj/ and gen/, so this difference doesn't matter in
  # practice.
  if os.path.dirname(flags.input):
    msrc_cmd += [ '-I' + os.path.dirname(flags.input) ]

  # Microsoft rc.exe searches for referenced files relative to -I flags in
  # addition to the pwd, so -I flags need to be passed both to both
  # the preprocessor and rc.
  msrc_cmd += flags.includes

  # Input must come last.
  msrc_cmd += [ msrc_in ]

  rc_exe_exit_code = subprocess.call(msrc_cmd)
  # Assert Microsoft rc.exe and rc.py produced identical .res files.
  if rc_exe_exit_code == 0:
    import filecmp
    assert filecmp.cmp(msrc_out, flags.output)
  return rc_exe_exit_code


def main():
  # This driver has to do these things:
  # 1. Parse flags.
  # 2. Convert the input from UTF-16LE to UTF-8 if needed.
  # 3. Pass the input through a preprocessor (and clean up the preprocessor's
  #    output in minor ways).
  # 4. Call rc for the heavy lifting.
  flags = ParseFlags()
  rc_file_data, is_utf8 = ReadInput(flags.input)
  preprocessed_output = Preprocess(rc_file_data, flags)
  rc_exe_exit_code = RunRc(preprocessed_output, is_utf8, flags)

  # 5. On Windows, we also call Microsoft's rc.exe and check that we produced
  #   the same output.
  # Since Microsoft's rc has a preprocessor that only accepts 32 characters
  # for macro names, feed the clang-preprocessed source into it instead
  # of using ms rc's preprocessor.
  if sys.platform == 'win32' and rc_exe_exit_code == 0:
    rc_exe_exit_code = CompareToMsRcOutput(preprocessed_output, is_utf8, flags)

  return rc_exe_exit_code


if __name__ == '__main__':
  sys.exit(main())
