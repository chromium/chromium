#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wraps an executable and any provided arguments into an executable script."""

import argparse
import os
import sys
import textwrap


# The bash template passes the python script into vpython via stdin.
# The interpreter doesn't know about the script, so we have bash
# inject the script location.
BASH_TEMPLATE = textwrap.dedent(
    """\
    #!/usr/bin/env vpython
    _SCRIPT_LOCATION = __file__
    {script}
    """)


# The batch template reruns the batch script with vpython, with the -x
# flag instructing the interpreter to ignore the first line. The interpreter
# knows about the (batch) script in this case, so it can get the file location
# directly.
BATCH_TEMPLATE = textwrap.dedent(
    """\
    @SETLOCAL ENABLEDELAYEDEXPANSION \
      & vpython.bat -x "%~f0" %* \
      & EXIT /B !ERRORLEVEL!
    _SCRIPT_LOCATION = __file__
    {script}
    """)


SCRIPT_TEMPLATES = {
    'bash': BASH_TEMPLATE,
    'batch': BATCH_TEMPLATE,
}


PY_TEMPLATE = textwrap.dedent(
    """\
    import os
    import re
    import subprocess
    import sys

    _WRAPPED_PATH_RE = re.compile(r'@WrappedPath\(([^)]+)\)')
    _PATH_TO_OUTPUT_DIR = '{path_to_output_dir}'
    _SCRIPT_DIR = os.path.dirname(os.path.realpath(_SCRIPT_LOCATION))


    def ExpandWrappedPath(arg):
      m = _WRAPPED_PATH_RE.match(arg)
      if m:
        relpath = os.path.join(
            os.path.relpath(_SCRIPT_DIR), _PATH_TO_OUTPUT_DIR, m.group(1))
        return os.path.normpath(relpath)
      return arg


    def ExpandWrappedPaths(args):
      for i, arg in enumerate(args):
        args[i] = ExpandWrappedPath(arg)
      return args


    def main(raw_args):
      executable_path = ExpandWrappedPath('{executable_path}')
      executable_args = ExpandWrappedPaths({executable_args})
      cmd = [executable_path] + executable_args + raw_args
      if executable_path.endswith('.py'):
        cmd = [sys.executable] + cmd
      return subprocess.call(cmd)


    if __name__ == '__main__':
      sys.exit(main(sys.argv[1:]))
    """)


def Wrap(args):
  """Writes a wrapped script according to the provided arguments.

  Arguments:
    args: an argparse.Namespace object containing command-line arguments
      as parsed by a parser returned by CreateArgumentParser.
  """
  path_to_output_dir = os.path.relpath(
      args.output_directory,
      os.path.dirname(args.wrapper_script))

  with open(args.wrapper_script, 'w') as wrapper_script:
    py_contents = PY_TEMPLATE.format(
        path_to_output_dir=path_to_output_dir,
        executable_path=str(args.executable),
        executable_args=str(args.executable_args))
    template = SCRIPT_TEMPLATES[args.script_language]
    wrapper_script.write(template.format(
        script=py_contents))
  os.chmod(args.wrapper_script, 0o750)

  return 0


def CreateArgumentParser():
  """Creates an argparse.ArgumentParser instance."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--executable',
      help='Executable to wrap.')
  parser.add_argument(
      '--wrapper-script',
      help='Path to which the wrapper script will be written.')
  parser.add_argument(
      '--output-directory',
      help='Path to the output directory.')
  parser.add_argument(
      '--script-language',
      choices=SCRIPT_TEMPLATES.keys(),
      help='Language in which the wrapper script will be written.')
  parser.add_argument(
      'executable_args', nargs='*',
      help='Arguments to wrap into the executable.')
  return parser


def main(raw_args):
  parser = CreateArgumentParser()
  args = parser.parse_args(raw_args)
  return Wrap(args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
