#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Update CTS Tests to a new version."""

from __future__ import print_function

import argparse
import os
import re
import sys

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
        'catapult', 'devil'))
# pylint: disable=wrong-import-position,import-error
from devil.utils import cmd_helper
from devil.utils import logging_common

import cts_utils


def _query_git_for_cts_tags():
  cts_git_url = 'https://android.googlesource.com/platform/cts/'

  tags = cmd_helper.GetCmdOutput(['git', 'ls-remote', '--tags',
                                  cts_git_url]).splitlines()

  print('[Updating CTS versions] Retrieved the CTS git tags')

  return tags


class UpdateCTS:
  """Updates CTS archive to a new version.

  Performs the following tasks to simplify the CTS test update process:
  (TODO(crbug.com/40259004): - Update the CTS versions in fetch.py / install.sh)
  - Update the CTS versions in webview_cts_gcs_path.json

  After these steps are completed, the user can commit and upload
  the CL to Chromium Gerrit.
  """

  def __init__(self, repo_root):
    """Construct UpdateCTS instance.

    Args:
      repo_root: Repository root (e.g. /path/to/chromium/src) to base
                 all configuration files
    """
    self._repo_root = os.path.abspath(repo_root)
    helper = cts_utils.ChromiumRepoHelper(self._repo_root)
    self._repo_helper = helper
    self._cts_config_path = helper.rebase(cts_utils.TOOLS_DIR,
                                          cts_utils.CONFIG_FILE)
    self._CTSConfig = cts_utils.CTSConfig(self._cts_config_path)

  def _check_for_latest_cts_versions(self, cts_tags):
    """Query for the latest cts versions per platform

    We can retrieve the newest CTS versions by searching through the git tags
    for each CTS version and looking for the latest
    """

    prefixes = [(platform, self._CTSConfig.get_git_tag_prefix(platform))
                for platform in self._CTSConfig.iter_platforms()]
    release_versions = dict()

    tag_prefix_regexes = {
        # Do a forward lookup for the tag prefix plus an '_r'
        # Eg: 'android-cts-7.0_r2'
        # Then retrieve the digits after this
        tag_prefix: re.compile('(?<=/%s_r)\\d*' % re.escape(tag_prefix))
        for _, tag_prefix in prefixes
    }

    for tag in cts_tags:
      for platform, prefix in prefixes:
        matches = tag_prefix_regexes[prefix].search(tag)
        if matches:
          version = int(matches.group(0))
          if release_versions.get(platform, -1) < version:
            release_versions[platform] = version

    print('[Updating CTS versions] Retrieved the latest CTS versions')

    return release_versions

  def _update_cts_config_file_download_origins(self, release_versions):
    """ Update the CTS release version for each architecture
    and then save the config json
    """
    for platform, arch in self._CTSConfig.iter_platform_archs():
      self._CTSConfig.set_release_version(platform, arch,
                                          release_versions[platform])

    self._CTSConfig.save()

    print('[Updating CTS versions] Updated cts config')

  def update_cts_download_origins_cmd(self):
    """Performs the cts download origins update command"""
    tags = _query_git_for_cts_tags()
    release_versions = self._check_for_latest_cts_versions(tags)
    self._update_cts_config_file_download_origins(release_versions)


DESC = """Updates the WebView CTS tests to a new version.

See https://source.android.com/compatibility/cts/downloads for the latest
versions.

Please create a new branch, then edit the
{}
file with updated origin and file name before running this script.

After performing all steps, perform git add then commit.""".format(
    os.path.join(cts_utils.TOOLS_DIR, cts_utils.CONFIG_FILE))

UPDATE_CONFIG = 'update-config'


def main():
  parser = argparse.ArgumentParser(
      description=DESC, formatter_class=argparse.RawTextHelpFormatter)

  logging_common.AddLoggingArguments(parser)

  subparsers = parser.add_subparsers(dest='cmd')

  subparsers.add_parser(
      UPDATE_CONFIG,
      help='Update the CTS config to the newest release versions.')

  args = parser.parse_args()
  logging_common.InitializeLogging(args)

  cts_updater = UpdateCTS(repo_root=cts_utils.SRC_DIR)

  if args.cmd == UPDATE_CONFIG:
    cts_updater.update_cts_download_origins_cmd()


if __name__ == '__main__':
  main()
