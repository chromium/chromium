# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains common helpers for GN action()s."""

import atexit
import collections
import contextlib
import filecmp
import fnmatch
import json
import logging
import os
import re
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
import textwrap
import zipfile

sys.path.append(os.path.join(os.path.dirname(__file__),
                             os.pardir, os.pardir, os.pardir))
import gn_helpers

# Use relative paths to improved hermetic property of build scripts.
DIR_SOURCE_ROOT = os.path.relpath(
    os.environ.get(
        'CHECKOUT_SOURCE_ROOT',
        os.path.join(
            os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
            os.pardir)))
JAVA_HOME = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'jdk', 'current')
JAVA_PATH = os.path.join(JAVA_HOME, 'bin', 'java')
JAVA_PATH_FOR_INPUTS = f'{JAVA_PATH}.chromium'
JAVAC_PATH = os.path.join(JAVA_HOME, 'bin', 'javac')
JAVAP_PATH = os.path.join(JAVA_HOME, 'bin', 'javap')
KOTLIN_HOME = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'kotlinc', 'current')
KOTLINC_PATH = os.path.join(KOTLIN_HOME, 'bin', 'kotlinc')


def JavaCmd(xmx='1G'):
  ret = [JAVA_PATH]
  # Limit heap to avoid Java not GC'ing when it should, and causing
  # bots to OOM when many java commands are runnig at the same time
  # https://crbug.com/1098333
  ret += ['-Xmx' + xmx]
  # JDK17 bug.
  # See: https://chromium-review.googlesource.com/c/chromium/src/+/4705883/3
  # https://github.com/iBotPeaches/Apktool/issues/3174
  ret += ['-Djdk.util.zip.disableZip64ExtraFieldValidation=true']
  return ret


@contextlib.contextmanager
def TempDir(**kwargs):
  dirname = tempfile.mkdtemp(**kwargs)
  try:
    yield dirname
  finally:
    shutil.rmtree(dirname)


def MakeDirectory(dir_path):
  try:
    os.makedirs(dir_path)
  except OSError:
    pass


def DeleteDirectory(dir_path):
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)


def Touch(path, fail_if_missing=False):
  if fail_if_missing and not os.path.exists(path):
    raise Exception(path + ' doesn\'t exist.')

  MakeDirectory(os.path.dirname(path))
  with open(path, 'a'):
    os.utime(path, None)


def FindInDirectory(directory, filename_filter='*'):
  files = []
  for root, _dirnames, filenames in os.walk(directory):
    matched_files = fnmatch.filter(filenames, filename_filter)
    files.extend((os.path.join(root, f) for f in matched_files))
  return files


def CheckOptions(options, parser, required=None):
  if not required:
    return
  for option_name in required:
    if getattr(options, option_name) is None:
      parser.error('--%s is required' % option_name.replace('_', '-'))


def WriteJson(obj, path, only_if_changed=False):
  old_dump = None
  if os.path.exists(path):
    with open(path, 'r') as oldfile:
      old_dump = oldfile.read()

  new_dump = json.dumps(obj, sort_keys=True, indent=2, separators=(',', ': '))

  if not only_if_changed or old_dump != new_dump:
    with open(path, 'w') as outfile:
      outfile.write(new_dump)


@contextlib.contextmanager
def _AtomicOutput(path, only_if_changed=True, mode='w+b'):
  # Create in same directory to ensure same filesystem when moving.
  dirname = os.path.dirname(path)
  if not os.path.exists(dirname):
    MakeDirectory(dirname)
  with tempfile.NamedTemporaryFile(
      mode, suffix=os.path.basename(path), dir=dirname, delete=False) as f:
    try:
      yield f

      # file should be closed before comparison/move.
      f.close()
      if not (only_if_changed and os.path.exists(path) and
              filecmp.cmp(f.name, path)):
        shutil.move(f.name, path)
    finally:
      if os.path.exists(f.name):
        os.unlink(f.name)


class CalledProcessError(Exception):
  """This exception is raised when the process run by CheckOutput
  exits with a non-zero exit code."""

  def __init__(self, cwd, args, output):
    super().__init__()
    self.cwd = cwd
    self.args = args
    self.output = output

  def __str__(self):
    # A user should be able to simply copy and paste the command that failed
    # into their shell (unless it is more than 200 chars).
    # User can set PRINT_FULL_COMMAND=1 to always print the full command.
    print_full = os.environ.get('PRINT_FULL_COMMAND', '0') != '0'
    full_cmd = shlex.join(self.args)
    short_cmd = textwrap.shorten(full_cmd, width=200)
    printed_cmd = full_cmd if print_full else short_cmd
    copyable_command = '( cd {}; {} )'.format(os.path.abspath(self.cwd),
                                              printed_cmd)
    return 'Command failed: {}\n{}'.format(copyable_command, self.output)


def FilterLines(output, filter_string):
  """Output filter from build_utils.CheckOutput.

  Args:
    output: Executable output as from build_utils.CheckOutput.
    filter_string: An RE string that will filter (remove) matching
        lines from |output|.

  Returns:
    The filtered output, as a single string.
  """
  re_filter = re.compile(filter_string)
  return '\n'.join(
      line for line in output.split('\n') if not re_filter.search(line))


def FilterReflectiveAccessJavaWarnings(output):
  """Filters out warnings about illegal reflective access operation.

  These warnings were introduced in Java 9, and generally mean that dependencies
  need to be updated.
  """
  #  WARNING: An illegal reflective access operation has occurred
  #  WARNING: Illegal reflective access by ...
  #  WARNING: Please consider reporting this to the maintainers of ...
  #  WARNING: Use --illegal-access=warn to enable warnings of further ...
  #  WARNING: All illegal access operations will be denied in a future release
  return FilterLines(
      output, r'WARNING: ('
      'An illegal reflective|'
      'Illegal reflective access|'
      'Please consider reporting this to|'
      'Use --illegal-access=warn|'
      'All illegal access operations)')


# This filter applies globally to all CheckOutput calls. We use this to prevent
# messages from failing the build, without actually removing them.
def _FailureFilter(output):
  # This is a message that comes from the JDK which can't be disabled, which as
  # far as we can tell, doesn't cause any real issues. It only happens
  # occasionally on the bots. See crbug.com/1441023 for details.
  jdk_filter = (r'.*warning.*Cannot use file \S+ because'
                r' it is locked by another process')
  output = FilterLines(output, jdk_filter)
  return output


# This can be used in most cases like subprocess.check_output(). The output,
# particularly when the command fails, better highlights the command's failure.
# If the command fails, raises a build_utils.CalledProcessError.
def CheckOutput(args,
                cwd=None,
                env=None,
                print_stdout=False,
                print_stderr=True,
                stdout_filter=None,
                stderr_filter=None,
                fail_on_output=True,
                before_join_callback=None,
                fail_func=lambda returncode, stderr: returncode != 0):
  if not cwd:
    cwd = os.getcwd()

  logging.info('CheckOutput: %s', ' '.join(args))
  child = subprocess.Popen(args,
      stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd, env=env)

  if before_join_callback:
    before_join_callback()
  stdout, stderr = child.communicate()

  # For Python3 only:
  if isinstance(stdout, bytes) and sys.version_info >= (3, ):
    stdout = stdout.decode('utf-8')
    stderr = stderr.decode('utf-8')

  if stdout_filter is not None:
    stdout = stdout_filter(stdout)

  if stderr_filter is not None:
    stderr = stderr_filter(stderr)

  if fail_func and fail_func(child.returncode, stderr):
    raise CalledProcessError(cwd, args, stdout + stderr)

  if print_stdout:
    sys.stdout.write(stdout)
  if print_stderr:
    sys.stderr.write(stderr)

  has_stdout = print_stdout and stdout
  has_stderr = print_stderr and stderr
  if has_stdout or has_stderr:
    if has_stdout and has_stderr:
      stream_name = 'stdout and stderr'
    elif has_stdout:
      stream_name = 'stdout'
    else:
      stream_name = 'stderr'

    if fail_on_output and _FailureFilter(stdout + stderr):
      MSG = """
Command failed because it wrote to {}.
You can often set treat_warnings_as_errors=false to not treat output as \
failure (useful when developing locally).
"""
      raise CalledProcessError(cwd, args, MSG.format(stream_name))

    short_cmd = textwrap.shorten(shlex.join(args), width=200)
    sys.stderr.write(
        f'\nThe above {stream_name} output was from: {short_cmd}\n')

  return stdout


def GetModifiedTime(path):
  # For a symlink, the modified time should be the greater of the link's
  # modified time and the modified time of the target.
  return max(os.lstat(path).st_mtime, os.stat(path).st_mtime)


def IsTimeStale(output, inputs):
  if not os.path.exists(output):
    return True

  output_time = GetModifiedTime(output)
  for i in inputs:
    if GetModifiedTime(i) > output_time:
      return True
  return False


def _CheckZipPath(name):
  if os.path.normpath(name) != name:
    raise Exception('Non-canonical zip path: %s' % name)
  if os.path.isabs(name):
    raise Exception('Absolute zip path: %s' % name)


def _IsSymlink(zip_file, name):
  zi = zip_file.getinfo(name)

  # The two high-order bytes of ZipInfo.external_attr represent
  # UNIX permissions and file type bits.
  return stat.S_ISLNK(zi.external_attr >> 16)


def ExtractAll(zip_path, path=None, no_clobber=True, pattern=None,
               predicate=None):
  if path is None:
    path = os.getcwd()
  elif not os.path.exists(path):
    MakeDirectory(path)

  if not zipfile.is_zipfile(zip_path):
    raise Exception('Invalid zip file: %s' % zip_path)

  extracted = []
  with zipfile.ZipFile(zip_path) as z:
    for name in z.namelist():
      if name.endswith('/'):
        MakeDirectory(os.path.join(path, name))
        continue
      if pattern is not None:
        if not fnmatch.fnmatch(name, pattern):
          continue
      if predicate and not predicate(name):
        continue
      _CheckZipPath(name)
      if no_clobber:
        output_path = os.path.join(path, name)
        if os.path.exists(output_path):
          raise Exception(
              'Path already exists from zip: %s %s %s'
              % (zip_path, name, output_path))
      if _IsSymlink(z, name):
        dest = os.path.join(path, name)
        MakeDirectory(os.path.dirname(dest))
        os.symlink(z.read(name), dest)
        extracted.append(dest)
      else:
        z.extract(name, path)
        extracted.append(os.path.join(path, name))

  return extracted


def MatchesGlob(path, filters):
  """Returns whether the given path matches any of the given glob patterns."""
  return filters and any(fnmatch.fnmatch(path, f) for f in filters)


def MergeZips(output, input_zips, path_transform=None, compress=None):
  """Combines all files from |input_zips| into |output|.

  Args:
    output: Path, fileobj, or ZipFile instance to add files to.
    input_zips: Iterable of paths to zip files to merge.
    path_transform: Called for each entry path. Returns a new path, or None to
        skip the file.
    compress: Overrides compression setting from origin zip entries.
  """
  path_transform = path_transform or (lambda p: p)

  out_zip = output
  if not isinstance(output, zipfile.ZipFile):
    out_zip = zipfile.ZipFile(output, 'w')

  # Include paths in the existing zip here to avoid adding duplicate files.
  added_names = set(out_zip.namelist())

  try:
    for in_file in input_zips:
      with zipfile.ZipFile(in_file, 'r') as in_zip:
        for info in in_zip.infolist():
          # Ignore directories.
          if info.filename[-1] == '/':
            continue
          dst_name = path_transform(info.filename)
          if not dst_name:
            continue
          already_added = dst_name in added_names
          if not already_added:
            if compress is not None:
              compress_entry = compress
            else:
              compress_entry = info.compress_type != zipfile.ZIP_STORED
            AddToZipHermetic(
                out_zip,
                dst_name,
                data=in_zip.read(info),
                compress=compress_entry)
            added_names.add(dst_name)
  finally:
    if output is not out_zip:
      out_zip.close()


def GetSortedTransitiveDependencies(top, deps_func):
  """Gets the list of all transitive dependencies in sorted order.

  There should be no cycles in the dependency graph (crashes if cycles exist).

  Args:
    top: A list of the top level nodes
    deps_func: A function that takes a node and returns a list of its direct
        dependencies.
  Returns:
    A list of all transitive dependencies of nodes in top, in order (a node will
    appear in the list at a higher index than all of its dependencies).
  """
  # Find all deps depth-first, maintaining original order in the case of ties.
  deps_map = collections.OrderedDict()
  def discover(nodes):
    for node in nodes:
      if node in deps_map:
        continue
      deps = deps_func(node)
      discover(deps)
      deps_map[node] = deps

  discover(top)
  return list(deps_map)


def InitLogging(enabling_env):
  logging.basicConfig(
      level=logging.DEBUG if os.environ.get(enabling_env) else logging.WARNING,
      format='%(levelname).1s %(process)d %(relativeCreated)6d %(message)s')
  script_name = os.path.basename(sys.argv[0])
  logging.info('Started (%s)', script_name)

  my_pid = os.getpid()

  def log_exit():
    # Do not log for fork'ed processes.
    if os.getpid() == my_pid:
      logging.info("Job's done (%s)", script_name)

  atexit.register(log_exit)


def ExpandFileArgs(args):
  """Replaces file-arg placeholders in args.

  These placeholders have the form:
    @FileArg(filename:key1:key2:...:keyn)

  The value of such a placeholder is calculated by reading 'filename' as json.
  And then extracting the value at [key1][key2]...[keyn]. If a key has a '[]'
  suffix the (intermediate) value will be interpreted as a single item list and
  the single item will be returned or used for further traversal.

  Note: This intentionally does not return the list of files that appear in such
  placeholders. An action that uses file-args *must* know the paths of those
  files prior to the parsing of the arguments (typically by explicitly listing
  them in the action's inputs in build files).
  """
  new_args = list(args)
  file_jsons = dict()
  r = re.compile(r'@FileArg\((.*?)\)')
  for i, arg in enumerate(args):
    match = r.search(arg)
    if not match:
      continue

    def get_key(key):
      if key.endswith('[]'):
        return key[:-2], True
      return key, False

    lookup_path = match.group(1).split(':')
    file_path, _ = get_key(lookup_path[0])
    if not file_path in file_jsons:
      with open(file_path) as f:
        file_jsons[file_path] = json.load(f)

    expansion = file_jsons
    for k in lookup_path:
      k, flatten = get_key(k)
      expansion = expansion[k]
      if flatten:
        if not isinstance(expansion, list) or not len(expansion) == 1:
          raise Exception('Expected single item list but got %s' % expansion)
        expansion = expansion[0]

    # This should match parse_gn_list. The output is either a GN-formatted list
    # or a literal (with no quotes).
    if isinstance(expansion, list):
      new_args[i] = (arg[:match.start()] + gn_helpers.ToGNString(expansion) +
                     arg[match.end():])
    else:
      new_args[i] = arg[:match.start()] + str(expansion) + arg[match.end():]

  return new_args


def ReadSourcesList(sources_list_file_name):
  """Reads a GN-written file containing list of file names and returns a list.

  Note that this function should not be used to parse response files.
  """
  with open(sources_list_file_name) as f:
    return [file_name.strip() for file_name in f]
