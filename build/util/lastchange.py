#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
lastchange.py -- Chromium revision fetching utility.
"""

import argparse
import collections
import datetime
import logging
import os
import subprocess
import sys

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_ROOT_DIR = os.path.abspath(
    os.path.join(_THIS_DIR, "..", "..", "third_party/depot_tools"))

sys.path.insert(0, _ROOT_DIR)

import gclient_utils

VersionInfo = collections.namedtuple("VersionInfo",
                                     ("revision_id", "revision", "timestamp"))
_EMPTY_VERSION_INFO = VersionInfo('0' * 40, '0' * 40, 0)

class GitError(Exception):
  pass

# This function exists for compatibility with logic outside this
# repository that uses this file as a library.
# TODO(eliribble) remove this function after it has been ported into
# the repositories that depend on it
def RunGitCommand(directory, command):
  """
  Launches git subcommand.

  Errors are swallowed.

  Returns:
    A process object or None.
  """
  command = ['git'] + command
  # Force shell usage under cygwin. This is a workaround for
  # mysterious loss of cwd while invoking cygwin's git.
  # We can't just pass shell=True to Popen, as under win32 this will
  # cause CMD to be used, while we explicitly want a cygwin shell.
  if sys.platform == 'cygwin':
    command = ['sh', '-c', ' '.join(command)]
  try:
    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            cwd=directory,
                            shell=(sys.platform=='win32'))
    return proc
  except OSError as e:
    logging.error('Command %r failed: %s' % (' '.join(command), e))
    return None


def _RunGitCommand(directory, command):
  """Launches git subcommand.

  Returns:
    The stripped stdout of the git command.
  Raises:
    GitError on failure, including a nonzero return code.
  """
  command = ['git'] + command
  # Force shell usage under cygwin. This is a workaround for
  # mysterious loss of cwd while invoking cygwin's git.
  # We can't just pass shell=True to Popen, as under win32 this will
  # cause CMD to be used, while we explicitly want a cygwin shell.
  if sys.platform == 'cygwin':
    command = ['sh', '-c', ' '.join(command)]
  try:
    logging.info("Executing '%s' in %s", ' '.join(command), directory)
    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            cwd=directory,
                            shell=(sys.platform=='win32'))
    stdout, stderr = tuple(x.decode(encoding='utf_8')
                           for x in proc.communicate())
    stdout = stdout.strip()
    stderr = stderr.strip()
    logging.debug("returncode: %d", proc.returncode)
    logging.debug("stdout: %s", stdout)
    logging.debug("stderr: %s", stderr)
    if proc.returncode != 0 or not stdout:
      raise GitError((
          "Git command '{}' in {} failed: "
          "rc={}, stdout='{}' stderr='{}'").format(
          " ".join(command), directory, proc.returncode, stdout, stderr))
    return stdout
  except OSError as e:
    raise GitError("Git command 'git {}' in {} failed: {}".format(
        " ".join(command), directory, e))


def GetMergeBase(directory, ref):
  """
  Return the merge-base of HEAD and ref.

  Args:
    directory: The directory containing the .git directory.
    ref: The ref to use to find the merge base.
  Returns:
    The git commit SHA of the merge-base as a string.
  """
  logging.debug("Calculating merge base between HEAD and %s in %s",
                ref, directory)
  command = ['merge-base', 'HEAD', ref]
  return _RunGitCommand(directory, command)


def FetchGitRevision(directory, commit_filter, start_commit="HEAD"):
  """
  Fetch the Git hash (and Cr-Commit-Position if any) for a given directory.

  Args:
    directory: The directory containing the .git directory.
    commit_filter: A filter to supply to grep to filter commits
    start_commit: A commit identifier. The result of this function
      will be limited to only consider commits before the provided
      commit.
  Returns:
    A VersionInfo object. On error all values will be 0.
  """
  hash_ = ''

  git_args = ['log', '-1', '--format=%H %ct']
  if commit_filter is not None:
    git_args.append('--grep=' + commit_filter)

  git_args.append(start_commit)

  output = _RunGitCommand(directory, git_args)
  hash_, commit_timestamp = output.split()
  if not hash_:
    return VersionInfo('0', '0', 0)

  revision = hash_
  output = _RunGitCommand(directory, ['cat-file', 'commit', hash_])
  for line in reversed(output.splitlines()):
    if line.startswith('Cr-Commit-Position:'):
      pos = line.rsplit()[-1].strip()
      logging.debug("Found Cr-Commit-Position '%s'", pos)
      revision = "{}-{}".format(hash_, pos)
      break
  return VersionInfo(hash_, revision, int(commit_timestamp))


def GetHeaderGuard(path):
  """
  Returns the header #define guard for the given file path.
  This treats everything after the last instance of "src/" as being a
  relevant part of the guard. If there is no "src/", then the entire path
  is used.
  """
  src_index = path.rfind('src/')
  if src_index != -1:
    guard = path[src_index + 4:]
  else:
    guard = path
  guard = guard.upper()
  return guard.replace('/', '_').replace('.', '_').replace('\\', '_') + '_'


def GetHeaderContents(path, define, version):
  """
  Returns what the contents of the header file should be that indicate the given
  revision.
  """
  header_guard = GetHeaderGuard(path)

  header_contents = """/* Generated by lastchange.py, do not edit.*/

#ifndef %(header_guard)s
#define %(header_guard)s

#define %(define)s "%(version)s"

#endif  // %(header_guard)s
"""
  header_contents = header_contents % { 'header_guard': header_guard,
                                        'define': define,
                                        'version': version }
  return header_contents


def GetGitTopDirectory(source_dir):
  """Get the top git directory - the directory that contains the .git directory.

  Args:
    source_dir: The directory to search.
  Returns:
    The output of "git rev-parse --show-toplevel" as a string
  """
  return _RunGitCommand(source_dir, ['rev-parse', '--show-toplevel'])


def WriteIfChanged(file_name, contents):
  """
  Writes the specified contents to the specified file_name
  iff the contents are different than the current contents.
  Returns if new data was written.
  """
  try:
    old_contents = open(file_name, 'r').read()
  except EnvironmentError:
    pass
  else:
    if contents == old_contents:
      return False
    os.unlink(file_name)
  open(file_name, 'w').write(contents)
  return True


def GetVersion(source_dir, commit_filter, merge_base_ref):
  """
  Returns the version information for the given source directory.
  """
  if gclient_utils.IsEnvCog():
    return _EMPTY_VERSION_INFO

  git_top_dir = None
  try:
    git_top_dir = GetGitTopDirectory(source_dir)
  except GitError as e:
    logging.warning("Failed to get git top directory from '%s': %s", source_dir,
                    e)

  merge_base_sha = 'HEAD'
  if git_top_dir and merge_base_ref:
    try:
      merge_base_sha = GetMergeBase(git_top_dir, merge_base_ref)
    except GitError as e:
      logging.error(
          "You requested a --merge-base-ref value of '%s' but no "
          "merge base could be found between it and HEAD. Git "
          "reports: %s", merge_base_ref, e)
      return None

  version_info = None
  if git_top_dir:
    try:
      version_info = FetchGitRevision(git_top_dir, commit_filter,
                                      merge_base_sha)
    except GitError as e:
      logging.error("Failed to get version info: %s", e)

  if not version_info:
    logging.warning(
        "Falling back to a version of 0.0.0 to allow script to "
        "finish. This is normal if you are bootstrapping a new environment "
        "or do not have a git repository for any other reason. If not, this "
        "could represent a serious error.")
    # Use a dummy revision that has the same length as a Git commit hash,
    # same as what we use in build/util/LASTCHANGE.dummy.
    version_info = _EMPTY_VERSION_INFO

  return version_info


def main(argv=None):
  if argv is None:
    argv = sys.argv

  parser = argparse.ArgumentParser(usage="lastchange.py [options]")
  parser.add_argument("-m", "--version-macro",
                    help=("Name of C #define when using --header. Defaults to "
                          "LAST_CHANGE."))
  parser.add_argument("-o",
                      "--output",
                      metavar="FILE",
                      help=("Write last change to FILE. "
                            "Can be combined with other file-output-related "
                            "options to write multiple files."))
  parser.add_argument("--header",
                      metavar="FILE",
                      help=("Write last change to FILE as a C/C++ header. "
                            "Can be combined with other file-output-related "
                            "options to write multiple files."))
  parser.add_argument("--revision",
                      metavar="FILE",
                      help=("Write last change to FILE as a one-line revision. "
                            "Can be combined with other file-output-related "
                            "options to write multiple files."))
  parser.add_argument("--merge-base-ref",
                    default=None,
                    help=("Only consider changes since the merge "
                          "base between HEAD and the provided ref"))
  parser.add_argument("--revision-id-only", action='store_true',
                    help=("Output the revision as a VCS revision ID only (in "
                          "Git, a 40-character commit hash, excluding the "
                          "Cr-Commit-Position)."))
  parser.add_argument("--revision-id-prefix",
                      metavar="PREFIX",
                      help=("Adds a string prefix to the VCS revision ID."))
  parser.add_argument("--print-only", action="store_true",
                    help=("Just print the revision string. Overrides any "
                          "file-output-related options."))
  parser.add_argument("-s", "--source-dir", metavar="DIR",
                    help="Use repository in the given directory.")
  parser.add_argument("--filter", metavar="REGEX",
                    help=("Only use log entries where the commit message "
                          "matches the supplied filter regex. Defaults to "
                          "'^Change-Id:' to suppress local commits."),
                    default='^Change-Id:')

  args, extras = parser.parse_known_args(argv[1:])

  logging.basicConfig(level=logging.WARNING)

  out_file = args.output
  header = args.header
  revision = args.revision
  commit_filter=args.filter

  while len(extras) and out_file is None:
    if out_file is None:
      out_file = extras.pop(0)
  if extras:
    sys.stderr.write('Unexpected arguments: %r\n\n' % extras)
    parser.print_help()
    sys.exit(2)

  source_dir = args.source_dir or os.path.dirname(os.path.abspath(__file__))

  version_info = GetVersion(source_dir, commit_filter, args.merge_base_ref)

  revision_string = version_info.revision
  if args.revision_id_only:
    revision_string = version_info.revision_id

  if args.revision_id_prefix:
    revision_string = args.revision_id_prefix + revision_string

  if args.print_only:
    print(revision_string)
  else:
    lastchange_year = datetime.datetime.fromtimestamp(
        version_info.timestamp, datetime.timezone.utc).year
    contents_lines = [
        "LASTCHANGE=%s" % revision_string,
        "LASTCHANGE_YEAR=%s" % lastchange_year,
    ]
    contents = '\n'.join(contents_lines) + '\n'
    if not out_file and not header and not revision:
      sys.stdout.write(contents)
    else:
      if out_file:
        committime_file = out_file + '.committime'
        out_changed = WriteIfChanged(out_file, contents)
        if out_changed or not os.path.exists(committime_file):
          with open(committime_file, 'w') as timefile:
            timefile.write(str(version_info.timestamp))
      if header:
        WriteIfChanged(header,
                       GetHeaderContents(header, args.version_macro,
                                         revision_string))
      if revision:
        WriteIfChanged(revision, revision_string)

  return 0


if __name__ == '__main__':
  sys.exit(main())
