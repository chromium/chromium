#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Update CTS Tests to a new version."""

from __future__ import print_function

import argparse
import logging
import os
import shutil
import sys
import tempfile
import zipfile

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
        'catapult', 'devil'))
from devil.utils import cmd_helper
from devil.utils import logging_common

import cts_utils


class PathError(IOError):
  def __init__(self, path, err_desc):
    super(PathError, self).__init__('"%s": %s' % (path, err_desc))


class MissingDirError(PathError):
  """An expected directory is missing, usually indicates a step was missed
     during the CTS update process.  Try to perform the missing step.
  """

  def __init__(self, path):
    super(MissingDirError, self).__init__(path, 'directory is missing.')


class DirExistsError(PathError):
  """A directory is already present, usually indicates a step was repeated
     in the same working directory. Try to delete the reported directory.
  """

  def __init__(self, path):
    super(DirExistsError, self).__init__(path, 'directory already exists.')


class MissingFileError(PathError):
  """Files are missing during CIPD staging, ensure that all files were
  downloaded and that CIPD download worked properly.
  """

  def __init__(self, path):
    super(MissingFileError, self).__init__(path, 'file is missing.')


class InconsistentFilesException(Exception):
  """Test files in CTS config and cipd yaml have gotten out of sync."""


class UncommittedChangeException(Exception):
  """Files are about to be modified but previously uncommitted changes exist."""

  def __init__(self, path):
    super(UncommittedChangeException, self).__init__(
        path, 'has uncommitted changes.')


class UpdateCTS(object):
  """Updates CTS archive to a new version.

  Prereqs:
  - Update the tools/cts_config/webview_cts_gcs_path.json file with origin,
    and filenames for each platform.  See:
    https://source.android.com/compatibility/cts/downloads for the latest
    versions.

  Performs the following tasks to simplify the CTS test update process:
  - Read the desired CTS versions from
    tools/cts_config/webview_cts_gcs_path.json file.
  - Download CTS test zip files from Android's public repository.
  - Filter only the WebView CTS test apks into smaller zip files.
  - Update the CTS CIPD package with the filtered zip files.
  - Update DEPS and testing/buildbot/test_suites.pyl with updated CTS CIPD
    package version.
  - Regenerate the buildbot json files.

  After these steps are completed, the user can commit and upload
  the CL to Chromium Gerrit.
  """

  def __init__(self, work_dir, repo_root):
    """Construct UpdateCTS instance.

    Args:
      work_dir: Directory used to download and stage cipd updates
      repo_root: Repository root (e.g. /path/to/chromium/src) to base
                 all configuration files
    """
    self._work_dir = os.path.abspath(work_dir)
    self._download_dir = os.path.join(self._work_dir, 'downloaded')
    self._filter_dir = os.path.join(self._work_dir, 'filtered')
    self._cipd_dir = os.path.join(self._work_dir, 'cipd')
    self._stage_dir = os.path.join(self._work_dir, 'staged')
    self._version_file = os.path.join(self._work_dir, 'cipd_version.txt')
    self._repo_root = os.path.abspath(repo_root)
    helper = cts_utils.ChromiumRepoHelper(self._repo_root)
    self._repo_helper = helper
    self._CTSConfig = cts_utils.CTSConfig(
        helper.rebase(cts_utils.TOOLS_DIR, cts_utils.CONFIG_FILE))
    self._CIPDYaml = cts_utils.CTSCIPDYaml(
        helper.rebase(cts_utils.TOOLS_DIR, cts_utils.CIPD_FILE))

  @property
  def download_dir(self):
    """Full directory path where full test zips are to be downloaded to."""
    return self._download_dir

  def download_cts_cmd(self, platforms=None):
    """Performs the download sub-command."""
    if platforms is None:
      all_platforms = self._CTSConfig.get_platforms()
      platforms = list(
          set(all_platforms) - set(cts_utils.END_OF_SERVICE_DESSERTS))
    print('Downloading CTS tests for %d platforms, could take a few'
          ' minutes ...' % len(platforms))
    self.download_cts(platforms)

  def create_cipd_cmd(self):
    """Performs the create-cipd sub-command."""
    print('Updating WebView CTS package in CIPD.')
    self.filter_downloaded_cts()
    self.download_cipd()
    self.stage_cipd_update()
    self.commit_staged_cipd()

  def update_repository_cmd(self):
    """Performs the update-checkout sub-command."""
    print('Updating current checkout with changes.')
    self.update_repository()

  def download_cts(self, platforms=None):
    """Download full test zip files to <work_dir>/downloaded/.

    It is an error to call this if work_dir already contains downloaded/.

    Args:
      platforms: List of platforms (e.g. ['O', 'P']), defaults to all

    Raises:
      DirExistsError: If downloaded/ already exists in work_dir.
    """
    if platforms is None:
      platforms = self._CTSConfig.get_platforms()
    if os.path.exists(self._download_dir):
      raise DirExistsError(self._download_dir)
    threads = []

    for p, a in self._CTSConfig.iter_platform_archs():
      if p not in platforms:
        continue
      origin = self._CTSConfig.get_origin(p, a)
      destination = os.path.join(self._download_dir,
                                 self._CTSConfig.get_origin_zip(p, a))
      logging.info('Starting download from %s to %s.', origin, destination)
      threads.append((origin, cts_utils.download(origin, destination)))
    for t in threads:
      t[1].join()
      logging.info('Finished download from ' + t[0])

  def filter_downloaded_cts(self):
    """Filter files from downloaded/ to filtered/ to contain only WebView apks.

    It is an error to call this if downloaded/ doesn't exist or if filtered/
    already exists.

    Raises:
      DirExistsError: If filtered/ already exists in work_dir.
      MissingDirError: If downloaded/ does not exist in work_dir.
    """
    if os.path.exists(self._filter_dir):
      raise DirExistsError(self._filter_dir)
    if not os.path.isdir(self._download_dir):
      raise MissingDirError(self._download_dir)
    os.makedirs(self._filter_dir)
    with cts_utils.chdir(self._download_dir):
      downloads = os.listdir('.')
      for download in downloads:
        logging.info('Filtering %s to %s/', download, self._filter_dir)
        cts_utils.filter_cts_file(self._CTSConfig, download, self._filter_dir)

  def download_cipd(self):
    """Download cts archive of the version found in DEPS to cipd/ directory.

    It is an error to call this if cipd/ already exists under work_cir.

    Raises:
      DirExistsError: If cipd/ already exists in work_dir.
    """
    if os.path.exists(self._cipd_dir):
      raise DirExistsError(self._cipd_dir)
    version = self._repo_helper.get_cipd_dependency_rev()
    logging.info('Download current CIPD version %s to %s/', version,
                 self._cipd_dir)
    cts_utils.cipd_download(self._CIPDYaml, version, self._cipd_dir)

  def stage_cipd_update(self):
    """Stage CIPD package for update by combining CIPD and filtered CTS files.

    It is an error to call this if filtered/ and cipd/ do not already exist
    under work_dir, or if staged already exists under work_dir.

    Raises:
      DirExistsError: If staged/ already exists in work_dir.
      MissingDirError: If filtered/ or cipd/ does not exist in work_dir.
    """
    if not os.path.isdir(self._filter_dir):
      raise MissingDirError(self._filter_dir)
    if not os.path.isdir(self._cipd_dir):
      raise MissingDirError(self._cipd_dir)
    if os.path.isdir(self._stage_dir):
      raise DirExistsError(self._stage_dir)
    os.makedirs(self._stage_dir)
    filtered = os.listdir(self._filter_dir)
    self._CIPDYaml.clear_files()
    for p, a in self._CTSConfig.iter_platform_archs():
      origin_base = self._CTSConfig.get_origin_zip(p, a)
      cipd_zip = self._CTSConfig.get_cipd_zip(p, a)
      dest_path = os.path.join(self._stage_dir, cipd_zip)
      if not os.path.isdir(os.path.dirname(dest_path)):
        os.makedirs(os.path.dirname(dest_path))
      self._CIPDYaml.append_file(cipd_zip)
      if origin_base in filtered:
        logging.info('Staging downloaded and filtered version of %s to %s.',
                     origin_base, dest_path)
        cmd_helper.RunCmd(
            ['cp', os.path.join(self._filter_dir, origin_base), dest_path])
      else:
        logging.info('Staging reused %s to %s/',
                     os.path.join(self._cipd_dir, cipd_zip), dest_path)
        cmd_helper.RunCmd(
            ['cp', os.path.join(self._cipd_dir, cipd_zip), dest_path])
    self._CIPDYaml.write(
        os.path.join(self._stage_dir, self._CIPDYaml.get_file_basename()))

  def commit_staged_cipd(self):
    """Upload the staged CIPD files to CIPD.

    Raises:
      MissingDirError: If staged/ does not exist in work_dir.
      InconsistentFilesException: If errors are detected in staged config files.
      MissingFileExcepition: If files are missing from CTS zip files.
    """
    if not os.path.isdir(self._stage_dir):
      raise MissingDirError(self._stage_dir)
    staged_yaml_path = os.path.join(self._stage_dir,
                                    self._CIPDYaml.get_file_basename())
    staged_yaml = cts_utils.CTSCIPDYaml(file_path=staged_yaml_path)
    staged_yaml_files = staged_yaml.get_files()
    if cts_utils.CTS_DEP_PACKAGE != staged_yaml.get_package():
      raise InconsistentFilesException('Bad CTS package name in staged yaml '
                                       '{}: {} '.format(
                                           staged_yaml_path,
                                           staged_yaml.get_package()))
    for p, a in self._CTSConfig.iter_platform_archs():
      cipd_zip = self._CTSConfig.get_cipd_zip(p, a)
      cipd_zip_path = os.path.join(self._stage_dir, cipd_zip)
      if not os.path.exists(cipd_zip_path):
        raise MissingFileError(cipd_zip_path)
      with zipfile.ZipFile(cipd_zip_path) as zf:
        cipd_zip_contents = zf.namelist()
      missing_apks = set(self._CTSConfig.get_apks(p)) - set(cipd_zip_contents)
      if missing_apks:
        raise MissingFileError('%s in %s' % (str(missing_apks), cipd_zip_path))
      if cipd_zip not in staged_yaml_files:
        raise InconsistentFilesException(cipd_zip +
                                         ' missing from staged cipd.yaml file')
    logging.info('Updating CIPD CTS version using %s', staged_yaml_path)
    new_cipd_version = cts_utils.update_cipd_package(staged_yaml_path)
    with open(self._version_file, 'w') as vf:
      logging.info('Saving new CIPD version %s to %s', new_cipd_version,
                   vf.name)
      vf.write(new_cipd_version)

  def update_repository(self):
    """Update chromium checkout with changes for this update.

    After this is called, git add -u && git commit && git cl upload
    will still be needed to generate the CL.

    Raises:
      MissingFileError: If CIPD has not yet been staged or updated.
      UncommittedChangeException: If repo files have uncommitted changes.
      InconsistentFilesException: If errors are detected in staged config files.
    """
    if not os.path.exists(self._version_file):
      raise MissingFileError(self._version_file)

    staged_yaml_path = os.path.join(self._stage_dir,
                                    self._CIPDYaml.get_file_basename())

    if not os.path.exists(staged_yaml_path):
      raise MissingFileError(staged_yaml_path)

    with open(self._version_file) as vf:
      new_cipd_version = vf.read()
      logging.info('Read in new CIPD version %s from %s', new_cipd_version,
                   vf.name)

    repo_cipd_yaml = self._CIPDYaml.get_file_path()
    for f in self._repo_helper.cipd_referrers + [repo_cipd_yaml]:
      git_status = self._repo_helper.git_status(f)
      if git_status:
        raise UncommittedChangeException(f)

    repo_cipd_package = self._repo_helper.cts_cipd_package
    staged_yaml = cts_utils.CTSCIPDYaml(file_path=staged_yaml_path)
    if repo_cipd_package != staged_yaml.get_package():
      raise InconsistentFilesException(
          'Inconsistent CTS package name, {} in {}, but {} in {}'.format(
              repo_cipd_package, cts_utils.DEPS_FILE, staged_yaml.get_package(),
              staged_yaml.get_file_path()))

    logging.info('Updating files that reference %s under %s.',
                 cts_utils.CTS_DEP_PACKAGE, self._repo_root)
    self._repo_helper.update_cts_cipd_rev(new_cipd_version)
    logging.info('Regenerate buildbot json files under %s.', self._repo_root)
    self._repo_helper.update_testing_json()
    logging.info('Copy staged %s to  %s.', staged_yaml_path, repo_cipd_yaml)
    cmd_helper.RunCmd(['cp', staged_yaml_path, repo_cipd_yaml])
    logging.info('Ensure CIPD CTS package at %s to the new version %s',
                 repo_cipd_yaml, new_cipd_version)
    cts_utils.cipd_ensure(self._CIPDYaml.get_package(), new_cipd_version,
                          os.path.dirname(repo_cipd_yaml))


DESC = """Updates the WebView CTS tests to a new version.

See https://source.android.com/compatibility/cts/downloads for the latest
versions.

Please create a new branch, then edit the
{}
file with updated origin and file name before running this script.

After performing all steps, perform git add then commit.""".format(
    os.path.join(cts_utils.TOOLS_DIR, cts_utils.CONFIG_FILE))

ALL_CMD = 'all-steps'
DOWNLOAD_CMD = 'download'
CIPD_UPDATE_CMD = 'create-cipd'
CHECKOUT_UPDATE_CMD = 'update-checkout'


def add_dessert_arg(parser):
  """Add --dessert argument to a parser.

  Args:
    parser: The parser object to add to
  """
  parser.add_argument(
      '--dessert',
      '-d',
      action='append',
      help='Android dessert letter(s) for which to perform CTS update.')


def add_workdir_arg(parser, is_required):
  """Add --work-dir argument to a parser.

  Args:
    parser: The parser object to add to
    is_required: Is this a required argument
  """
  parser.add_argument(
      '--workdir',
      '-w',
      required=is_required,
      help='Use this directory for'
      ' intermediate files.')


def main():
  parser = argparse.ArgumentParser(
      description=DESC, formatter_class=argparse.RawTextHelpFormatter)

  logging_common.AddLoggingArguments(parser)

  subparsers = parser.add_subparsers(dest='cmd')

  all_subparser = subparsers.add_parser(
      ALL_CMD,
      help='Performs all other sub-commands, in the correct order. This is'
      ' usually what you want.')
  add_dessert_arg(all_subparser)
  add_workdir_arg(all_subparser, False)

  download_subparser = subparsers.add_parser(
      DOWNLOAD_CMD,
      help='Only downloads files to workdir for later use by other'
      ' sub-commands.')
  add_dessert_arg(download_subparser)
  add_workdir_arg(download_subparser, True)

  cipd_subparser = subparsers.add_parser(
      CIPD_UPDATE_CMD,
      help='Create a new CIPD package version for CTS tests.  This requires'
      ' that {} was completed in the same workdir.'.format(DOWNLOAD_CMD))
  add_workdir_arg(cipd_subparser, True)

  checkout_subparser = subparsers.add_parser(
      CHECKOUT_UPDATE_CMD,
      help='Updates files in the current git branch. This requires that {} was'
      ' completed in the same workdir.'.format(CIPD_UPDATE_CMD))
  add_workdir_arg(checkout_subparser, True)

  args = parser.parse_args()
  logging_common.InitializeLogging(args)

  temp_workdir = None
  if args.workdir is None:
    temp_workdir = tempfile.mkdtemp()
    workdir = temp_workdir
  else:
    workdir = args.workdir
    if not os.path.isdir(workdir):
      raise ValueError(
          '--workdir {} should already be a directory.'.format(workdir))
    if not os.access(workdir, os.W_OK | os.X_OK):
      raise ValueError('--workdir {} is not writable.'.format(workdir))

  try:
    cts_updater = UpdateCTS(work_dir=workdir, repo_root=cts_utils.SRC_DIR)

    if args.cmd == DOWNLOAD_CMD:
      cts_updater.download_cts_cmd(platforms=args.dessert)
    elif args.cmd == CIPD_UPDATE_CMD:
      cts_updater.create_cipd_cmd()
    elif args.cmd == CHECKOUT_UPDATE_CMD:
      cts_updater.update_repository_cmd()
    elif args.cmd == ALL_CMD:
      cts_updater.download_cts_cmd()
      cts_updater.create_cipd_cmd()
      cts_updater.update_repository_cmd()
  finally:
    if temp_workdir is not None:
      logging.info('Removing temporary workdir %s', temp_workdir)
      shutil.rmtree(temp_workdir)


if __name__ == '__main__':
  main()
