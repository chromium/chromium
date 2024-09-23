#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs 'ld -shared' and generates a .TOC file that's untouched when unchanged.

This script exists to avoid using complex shell commands in
gcc_toolchain.gni's tool("solink"), in case the host running the compiler
does not have a POSIX-like shell (e.g. Windows).
"""

import argparse
import os
import shlex
import subprocess
import sys

import wrapper_utils


def CollectSONAME(args):
  """Replaces: readelf -d $sofile | grep SONAME"""
  # TODO(crbug.com/40797404): Come up with a way to get this info without having
  # to bundle readelf in the toolchain package.
  toc = ''
  readelf = subprocess.Popen(wrapper_utils.CommandToRun(
      [args.readelf, '-d', args.sofile]),
                             stdout=subprocess.PIPE,
                             bufsize=-1,
                             universal_newlines=True)
  for line in readelf.stdout:
    if 'SONAME' in line:
      toc += line
  return readelf.wait(), toc


def CollectDynSym(args):
  """Replaces: nm --format=posix -g -D -p $sofile | cut -f1-2 -d' '"""
  toc = ''
  nm = subprocess.Popen(wrapper_utils.CommandToRun(
      [args.nm, '--format=posix', '-g', '-D', '-p', args.sofile]),
                        stdout=subprocess.PIPE,
                        bufsize=-1,
                        universal_newlines=True)
  for line in nm.stdout:
    toc += ' '.join(line.split(' ', 2)[:2]) + '\n'
  return nm.wait(), toc


def CollectTOC(args):
  result, toc = CollectSONAME(args)
  if result == 0:
    result, dynsym = CollectDynSym(args)
    toc += dynsym
  return result, toc


def UpdateTOC(tocfile, toc):
  if os.path.exists(tocfile):
    old_toc = open(tocfile, 'r').read()
  else:
    old_toc = None
  if toc != old_toc:
    open(tocfile, 'w').write(toc)


def CollectInputs(out, args):
  for x in args:
    if x.startswith('@'):
      with open(x[1:]) as rsp:
        CollectInputs(out, shlex.split(rsp.read()))
    elif not x.startswith('-') and (x.endswith('.o') or x.endswith('.a')):
      out.write(x)
      out.write('\n')


def InterceptFlag(flag, command):
  ret = flag in command
  if ret:
    command.remove(flag)
  return ret


def SafeDelete(path):
  try:
    os.unlink(path)
  except OSError:
    pass


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--readelf',
                      required=True,
                      help='The readelf binary to run',
                      metavar='PATH')
  parser.add_argument('--nm',
                      required=True,
                      help='The nm binary to run',
                      metavar='PATH')
  parser.add_argument('--strip',
                      help='The strip binary to run',
                      metavar='PATH')
  parser.add_argument('--dwp', help='The dwp binary to run', metavar='PATH')
  parser.add_argument('--sofile',
                      required=True,
                      help='Shared object file produced by linking command',
                      metavar='FILE')
  parser.add_argument('--tocfile',
                      required=True,
                      help='Output table-of-contents file',
                      metavar='FILE')
  parser.add_argument('--map-file',
                      help=('Use --Wl,-Map to generate a map file. Will be '
                            'gzipped if extension ends with .gz'),
                      metavar='FILE')
  parser.add_argument('--output',
                      required=True,
                      help='Final output shared object file',
                      metavar='FILE')
  parser.add_argument('command', nargs='+',
                      help='Linking command')
  args = parser.parse_args()

  # Work-around for gold being slow-by-default. http://crbug.com/632230
  fast_env = dict(os.environ)
  fast_env['LC_ALL'] = 'C'

  # Extract flags passed through ldflags but meant for this script.
  # https://crbug.com/954311 tracks finding a better way to plumb these.
  partitioned_library = InterceptFlag('--partitioned-library', args.command)
  collect_inputs_only = InterceptFlag('--collect-inputs-only', args.command)

  # Partitioned .so libraries are used only for splitting apart in a subsequent
  # step.
  #
  # - The TOC file optimization isn't useful, because the partition libraries
  #   must always be re-extracted if the combined library changes (and nothing
  #   should be depending on the combined library's dynamic symbol table).
  # - Stripping isn't necessary, because the combined library is not used in
  #   production or published.
  #
  # Both of these operations could still be done, they're needless work, and
  # tools would need to be updated to handle and/or not complain about
  # partitioned libraries. Instead, to keep Ninja happy, simply create dummy
  # files for the TOC and stripped lib.
  if collect_inputs_only or partitioned_library:
    open(args.output, 'w').close()
    open(args.tocfile, 'w').close()

  # Instead of linking, records all inputs to a file. This is used by
  # enable_resource_allowlist_generation in order to avoid needing to
  # link (which is slow) to build the resources allowlist.
  if collect_inputs_only:
    if args.map_file:
      open(args.map_file, 'w').close()
    if args.dwp:
      open(args.sofile + '.dwp', 'w').close()

    with open(args.sofile, 'w') as f:
      CollectInputs(f, args.command)
    return 0

  # First, run the actual link.
  command = wrapper_utils.CommandToRun(args.command)
  result = wrapper_utils.RunLinkWithOptionalMapFile(command,
                                                    env=fast_env,
                                                    map_file=args.map_file)

  if result != 0:
    return result

  # If dwp is set, then package debug info for this SO.
  dwp_proc = None
  if args.dwp:
    # Explicit delete to account for symlinks (when toggling between
    # debug/release).
    SafeDelete(args.sofile + '.dwp')
    # Suppress warnings about duplicate CU entries (https://crbug.com/1264130)
    dwp_proc = subprocess.Popen(wrapper_utils.CommandToRun(
        [args.dwp, '-e', args.sofile, '-o', args.sofile + '.dwp']),
                                stderr=subprocess.DEVNULL)

  if not partitioned_library:
    # Next, generate the contents of the TOC file.
    result, toc = CollectTOC(args)
    if result != 0:
      return result

    # If there is an existing TOC file with identical contents, leave it alone.
    # Otherwise, write out the TOC file.
    UpdateTOC(args.tocfile, toc)

    # Finally, strip the linked shared object file (if desired).
    if args.strip:
      result = subprocess.call(
          wrapper_utils.CommandToRun(
              [args.strip, '-o', args.output, args.sofile]))

  if dwp_proc:
    dwp_result = dwp_proc.wait()
    if dwp_result != 0:
      sys.stderr.write('dwp failed with error code {}\n'.format(dwp_result))
      return dwp_result

  return result


if __name__ == "__main__":
  sys.exit(main())
