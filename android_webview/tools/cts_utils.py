# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Stage the Chromium checkout to update CTS test version."""

import contextlib
import json
import operator
import os
import re
import sys
import tempfile
import threading
try:
  # Workaround for py2/3 compatibility.
  # TODO(pbirk): remove once py2 support is no longer needed.
  import urllib.request as urllib_request
except ImportError:
  import urllib as urllib_request
import zipfile

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
        'catapult', 'devil'))
# pylint: disable=wrong-import-position,import-error
from devil.utils import cmd_helper

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
        'catapult', 'common', 'py_utils'))
# pylint: disable=wrong-import-position,import-error
from py_utils import tempfile_ext

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

TOOLS_DIR = os.path.join('android_webview', 'tools')

CONFIG_FILE = os.path.join('cts_config', 'webview_cts_gcs_path.json')
CONFIG_PATH = os.path.join(SRC_DIR, TOOLS_DIR, CONFIG_FILE)

CIPD_FILE = os.path.join('cts_archive', 'cipd.yaml')
CIPD_PATH = os.path.join(SRC_DIR, TOOLS_DIR, CIPD_FILE)

DEPS_FILE = 'DEPS'

TEST_SUITES_FILE = os.path.join('testing', 'buildbot', 'test_suites.pyl')

CTS_DEP_NAME = 'src/android_webview/tools/cts_archive'
CTS_DEP_PACKAGE = 'chromium/android_webview/tools/cts_archive'

CIPD_REFERRERS = [DEPS_FILE, TEST_SUITES_FILE]

_GENERATE_BUILDBOT_JSON = os.path.join('testing', 'buildbot',
                                       'generate_buildbot_json.py')

_ENSURE_FORMAT = """$ParanoidMode CheckIntegrity
@Subdir cipd
{} {}"""
_ENSURE_SUBDIR = 'cipd'

_RE_COMMENT_OR_BLANK = re.compile(r'^ *(#.*)?$')


class CTSConfig:
  """Represents a CTS config file."""

  def __init__(self, file_path=CONFIG_PATH):
    """Constructs a representation of the CTS config file.

    Only read operations are provided by this object.  Users should edit the
    file manually for any modifications.

    Args:
      file_path: Path to file.
    """
    self._path = os.path.abspath(file_path)
    with open(self._path) as f:
      self._config = json.load(f)

  def save(self):
    with open(self._path, 'w') as file:
      json.dump(self._config, file, indent=2)
      file.write("\n")

  def get_platforms(self):
    return sorted(self._config.keys())

  def get_archs(self, platform):
    return sorted(self._config[platform]['arch'].keys())

  def get_git_tag_prefix(self, platform):
    return self._config[platform]['git']['tag_prefix']

  def iter_platforms(self):
    for p in self.get_platforms():
      yield p

  def iter_platform_archs(self):
    for p in self.get_platforms():
      for a in self.get_archs(p):
        yield p, a

  def get_cipd_zip(self, platform, arch):
    return self._config[platform]['arch'][arch]['filename']

  def get_origin(self, platform, arch):
    return self._config[platform]['arch'][arch]['_origin']

  def get_origin_zip(self, platform, arch):
    return os.path.basename(self.get_origin(platform, arch))

  def get_apks(self, platform):
    return sorted([r['apk'] for r in self._config[platform]['test_runs']])

  def get_additional_apks(self, platform):
    return [
        apk['apk'] for r in self._config[platform]['test_runs']
        for apk in r.get('additional_apks', [])
    ]

  def set_release_version(self, platform, arch, release):
    pattern = re.compile(r'(?<=_r)\d*')

    def update_release_version(field):
      return pattern.sub(str(release),
                         self._config[platform]['arch'][arch][field])

    self._config[platform]['arch'][arch] = {
        'filename': update_release_version('filename'),
        '_origin': update_release_version('_origin'),
        'unzip_dir': update_release_version('unzip_dir'),
    }


class CTSCIPDYaml:
  """Represents a CTS CIPD yaml file."""

  RE_PACKAGE = r'^package:\s*(\S+)\s*$'
  RE_DESC = r'^description:\s*(.+)$'
  RE_DATA = r'^data:\s*$'
  RE_FILE = r'^\s+-\s+file:\s*(.+)$'

  # TODO(crbug.com/1049432): Replace with yaml parser
  @classmethod
  def parse(cls, lines):
    result = {}
    for line in lines:
      if len(line) == 0 or line[0] == '#':
        continue
      package_match = re.match(cls.RE_PACKAGE, line)
      if package_match:
        result['package'] = package_match.group(1)
        continue
      desc_match = re.match(cls.RE_DESC, line)
      if desc_match:
        result['description'] = desc_match.group(1)
        continue
      if re.match(cls.RE_DATA, line):
        result['data'] = []
      if 'data' in result:
        file_match = re.match(cls.RE_FILE, line)
        if file_match:
          result['data'].append({'file': file_match.group(1)})
    return result

  def __init__(self, file_path=CIPD_PATH):
    """Constructs a representation of CTS CIPD yaml file.

    Note the file won't be modified unless write is called
    with its path.
    Args:
      file_path: Path to file.
    """
    self._path = os.path.abspath(file_path)
    self._header = []
    # Read header comments
    with open(self._path) as f:
      for l in f.readlines():
        if re.match(_RE_COMMENT_OR_BLANK, l):
          self._header.append(l)
        else:
          break
    # Read yaml data
    with open(self._path) as f:
      self._yaml = CTSCIPDYaml.parse(f.readlines())

  def get_file_path(self):
    """Get full file path of yaml file that this was constructed from."""
    return self._path

  def get_file_basename(self):
    """Get base file name that this was constructed from."""
    return os.path.basename(self._path)

  def get_package(self):
    """Get package name."""
    return self._yaml['package']

  def clear_files(self):
    """Clears all files in file (only in local memory, does not modify file)."""
    self._yaml['data'] = []

  def append_file(self, file_name):
    """Add file_name to list of files."""
    self._yaml['data'].append({'file': str(file_name)})

  def remove_file(self, file_name):
    """Remove file_name from list of files."""
    old_file_names = self.get_files()
    new_file_names = [name for name in old_file_names if name != file_name]
    self._yaml['data'] = [{'file': name} for name in new_file_names]

  def get_files(self):
    """Get list of files in yaml file."""
    return [e['file'] for e in self._yaml['data']]

  def write(self, file_path):
    """(Over)write file_path with the cipd.yaml representation."""
    dir_name = os.path.dirname(file_path)
    if not os.path.isdir(dir_name):
      os.makedirs(dir_name)
    with open(file_path, 'w') as f:
      f.writelines(self._get_yamls())

  def _get_yamls(self):
    """Return the cipd.yaml file contents of this object."""
    output = []
    output += self._header
    output.append('package: {}\n'.format(self._yaml['package']))
    output.append('description: {}\n'.format(self._yaml['description']))
    output.append('data:\n')
    for d in sorted(self._yaml['data'], key=operator.itemgetter('file')):
      output.append('  - file: {}\n'.format(d.get('file')))
    return output


def cipd_ensure(package, version, root_dir):
  """Ensures CIPD package is installed at root_dir.

  Args:
    package: CIPD name of package
    version: Package version
    root_dir: Directory to install package into
  """

  def _createEnsureFile(package, version, file_path):
    with open(file_path, 'w') as f:
      f.write(_ENSURE_FORMAT.format(package, version))

  def _ensure(root, ensure_file):
    ret = cmd_helper.RunCmd(
        ['cipd', 'ensure', '-root', root, '-ensure-file', ensure_file])
    if ret:
      raise IOError('Error while running cipd ensure: ' + ret)

  with tempfile.NamedTemporaryFile() as f:
    _createEnsureFile(package, version, f.name)
    _ensure(root_dir, f.name)


def cipd_download(cipd, version, download_dir):
  """Downloads CIPD package files.

  This is different from cipd ensure in that actual files will exist at
  download_dir instead of symlinks.

  Args:
    cipd: CTSCIPDYaml object
    version: Version of package
    download_dir: Destination directory
  """
  package = cipd.get_package()
  download_dir_abs = os.path.abspath(download_dir)
  if not os.path.isdir(download_dir_abs):
    os.makedirs(download_dir_abs)
  with tempfile_ext.NamedTemporaryDirectory() as workDir, chdir(workDir):
    cipd_ensure(package, version, '.')
    for file_name in cipd.get_files():
      src_path = os.path.join(_ENSURE_SUBDIR, file_name)
      dest_path = os.path.join(download_dir_abs, file_name)
      dest_dir = os.path.dirname(dest_path)
      if not os.path.isdir(dest_dir):
        os.makedirs(dest_dir)
      ret = cmd_helper.RunCmd(['cp', '--reflink=never', src_path, dest_path])
      if ret:
        raise IOError('Error file copy from ' + file_name + ' to ' + dest_path)


def filter_cts_file(cts_config, cts_zip_file, dest_dir):
  """Filters out non-webview test apks from downloaded CTS zip file.

  Args:
    cts_config: CTSConfig object
    cts_zip_file: Path to downloaded CTS zip, retaining the original filename
    dest_dir: Destination directory to filter to, filename will be unchanged
  """

  for p in cts_config.get_platforms():
    for a in cts_config.get_archs(p):
      o = cts_config.get_origin(p, a)
      base_name = os.path.basename(o)
      if base_name == os.path.basename(cts_zip_file):
        filterzip(cts_zip_file,
                  cts_config.get_apks(p) + cts_config.get_additional_apks(p),
                  os.path.join(dest_dir, base_name))
        return
  raise ValueError('Could not find platform and arch for: ' + cts_zip_file)


class ChromiumRepoHelper:
  """Performs operations on Chromium checkout."""

  def __init__(self, root_dir=SRC_DIR):
    self._root_dir = os.path.abspath(root_dir)
    self._cipd_referrers = [
        os.path.join(self._root_dir, p) for p in CIPD_REFERRERS
    ]

  @property
  def cipd_referrers(self):
    return self._cipd_referrers

  @property
  def cts_cipd_package(self):
    return CTS_DEP_PACKAGE

  def get_cipd_dependency_rev(self):
    """Return CTS CIPD revision in the checkout's DEPS file."""
    deps_file = os.path.join(self._root_dir, DEPS_FILE)

    # Use the gclient command instead of gclient_eval since the latter is not
    # intended for direct use outside of depot_tools. The .bat file extension
    # must be explicitly specified when shell=False.
    gclient = 'gclient.bat' if os.name == 'nt' else 'gclient'
    cmd = [
        gclient, 'getdep', '--revision',
        '%s:%s' % (CTS_DEP_NAME, CTS_DEP_PACKAGE), '--deps-file', deps_file
    ]
    env = os.environ

    # Disable auto-update of depot tools since update_depot_tools may not be
    # available (for example, on the presubmit bot), and it's probably best not
    # to perform surprise updates anyways.
    env.update({'DEPOT_TOOLS_UPDATE': '0'})
    status, output, err = cmd_helper.GetCmdStatusOutputAndError(cmd, env=env)

    if status != 0:
      raise Exception('Command "%s" failed: %s' % (' '.join(cmd), err))

    return output.strip()

  def update_cts_cipd_rev(self, new_version):
    """Update references to CTS CIPD revision in checkout.

    Args:
      new_version: New version to use
    """
    old_version = self.get_cipd_dependency_rev()
    for path in self.cipd_referrers:
      replace_cipd_revision(path, old_version, new_version)

  def git_status(self, path):
    """Returns canonical git status of file.

    Args:
      path: Path to file.
    Returns:
      Output of git status --porcelain.
    """
    with chdir(self._root_dir):
      output = cmd_helper.GetCmdOutput(['git', 'status', '--porcelain', path])
      return output

  def update_testing_json(self):
    """Performs generate_buildbot_json.py.

    Raises:
      IOError: If generation failed.
    """
    with chdir(self._root_dir):
      ret = cmd_helper.RunCmd(['vpython3', _GENERATE_BUILDBOT_JSON])
      if ret:
        raise IOError('Error while generating_buildbot_json.py')

  def rebase(self, *rel_path_parts):
    """Construct absolute path from parts relative to root_dir.

    Args:
      rel_path_parts: Parts of the root relative path.

    Returns:
      The absolute path.
    """
    return os.path.join(self._root_dir, *rel_path_parts)


def replace_cipd_revision(file_path, old_revision, new_revision):
  """Replaces cipd revision strings in file.

  Args:
    file_path: Path to file.
    old_revision: Old cipd revision to be replaced.
    new_revision: New cipd revision to use as replacement.

  Returns:
    Number of replaced occurrences.

  Raises:
    IOError: If no occurrences were found.
  """
  with open(file_path) as f:
    contents = f.read()
  num = contents.count(old_revision)
  if not num:
    raise IOError('Did not find old CIPD revision {} in {}'.format(
        old_revision, file_path))
  newcontents = contents.replace(old_revision, new_revision)
  with open(file_path, 'w') as f:
    f.write(newcontents)
  return num


@contextlib.contextmanager
def chdir(dirPath):
  """Context manager that changes working directory."""
  cwd = os.getcwd()
  os.chdir(dirPath)
  try:
    yield
  finally:
    os.chdir(cwd)


def filterzip(inputPath, pathList, outputPath):
  """Copy a subset of files from input archive into output archive.

  Args:
    inputPath: Input archive path
    pathList: List of file names from input archive to copy
    outputPath: Output archive path
  """
  with zipfile.ZipFile(os.path.abspath(inputPath), 'r') as inputZip,\
       zipfile.ZipFile(os.path.abspath(outputPath), 'w') as outputZip,\
       tempfile_ext.NamedTemporaryDirectory() as workDir,\
       chdir(workDir):
    for p in pathList:
      inputZip.extract(p)
      outputZip.write(p)


def download(url, destination):
  """Asynchronously download url to path specified by destination.

  Args:
    url: Url location of file.
    destination: Path where file should be saved to.

  If destination parent directories do not exist, they will be created.

  Returns the download thread which can then be joined by the caller to
  wait for download completion.
  """

  dest_dir = os.path.dirname(destination)
  if not os.path.isdir(dest_dir):
    os.makedirs(dest_dir)
  t = threading.Thread(target=urllib_request.urlretrieve,
                       args=(url, destination))
  t.start()
  return t


def update_cipd_package(cipd_yaml_path):
  """Updates the CIPD package specified by cipd_yaml_path.

  Args:
    cipd_yaml_path: Path of cipd yaml specification file
  """
  cipd_yaml_path_abs = os.path.abspath(cipd_yaml_path)
  with chdir(os.path.dirname(cipd_yaml_path_abs)),\
       tempfile.NamedTemporaryFile() as jsonOut:
    ret = cmd_helper.RunCmd([
        'cipd', 'create', '-pkg-def', cipd_yaml_path_abs, '-json-output',
        jsonOut.name
    ])
    if ret:
      raise IOError('Error during cipd create.')
    return json.load(jsonOut)['result']['instance_id']
