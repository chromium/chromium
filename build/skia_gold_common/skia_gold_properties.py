# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class for storing Skia Gold comparison properties.

Examples:
* git revision being tested
* Whether the test is being run locally or on a bot
* What the continuous integration system is
"""

import argparse
import logging
import optparse
import os
import subprocess
import sys
from typing import Optional, Union

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))

ParsedCmdArgs = Union[argparse.Namespace, optparse.Values]


def _IsWin() -> bool:
  return sys.platform == 'win32'


class SkiaGoldProperties():
  def __init__(self, args: ParsedCmdArgs):
    """Class to validate and store properties related to Skia Gold.

    The base implementation is usable on its own, but is meant to be overridden
    as necessary.

    Args:
      args: The parsed arguments from an argparse.ArgumentParser.
    """
    self._git_revision: Optional[str] = None
    self._issue: Optional[int] = None
    self._patchset: Optional[int] = None
    self._job_id: Optional[str] = None
    self._local_pixel_tests: Optional[bool] = None
    self._no_luci_auth: Optional[bool] = None
    self._service_account: Optional[str] = None
    self._bypass_skia_gold_functionality: Optional[bool] = None
    self._code_review_system: Optional[str] = None
    self._continuous_integration_system: Optional[str] = None
    self._local_png_directory: Optional[str] = None

    self._InitializeProperties(args)

  def IsTryjobRun(self) -> bool:
    return self.issue is not None

  @property
  def continuous_integration_system(self) -> str:
    return self._continuous_integration_system or 'buildbucket'

  @property
  def code_review_system(self) -> str:
    return self._code_review_system or 'gerrit'

  @property
  def git_revision(self) -> str:
    return self._GetGitRevision()

  @property
  def issue(self) -> Optional[int]:
    return self._issue

  @property
  def job_id(self) -> Optional[str]:
    return self._job_id

  @property
  def local_pixel_tests(self) -> bool:
    return self._IsLocalRun()

  @property
  def local_png_directory(self) -> Optional[str]:
    return self._local_png_directory

  @property
  def no_luci_auth(self) -> Optional[bool]:
    return self._no_luci_auth

  @property
  def service_account(self) -> Optional[str]:
    return self._service_account

  @property
  def patchset(self) -> Optional[int]:
    return self._patchset

  @property
  def bypass_skia_gold_functionality(self) -> Optional[bool]:
    return self._bypass_skia_gold_functionality

  def _GetGitOriginMainHeadSha1(self) -> Optional[str]:
    try:
      return subprocess.check_output(
          ['git', 'rev-parse', 'origin/main'],
          shell=_IsWin(),
          cwd=self._GetGitRepoDirectory()).decode('utf-8').strip()
    except subprocess.CalledProcessError:
      return None

  def _GetGitRepoDirectory(self) -> str:
    return CHROMIUM_SRC_DIR

  def _GetGitRevision(self) -> str:
    if not self._git_revision:
      # Automated tests should always pass the revision, so assume we're on
      # a workstation and try to get the local origin/master HEAD.
      if not self._IsLocalRun():
        raise RuntimeError(
            '--git-revision was not passed when running on a bot')
      revision = self._GetGitOriginMainHeadSha1()
      if not revision or len(revision) != 40:
        raise RuntimeError(
            '--git-revision not passed and unable to determine from git')
      self._git_revision = revision
    return self._git_revision

  def _IsLocalRun(self) -> bool:
    if self._local_pixel_tests is None:
      # Look for the presence of the SWARMING_SERVER environment variable as a
      # heuristic to determine whether we're running on a workstation or a bot.
      # This should always be set on swarming, but would be strange to be set on
      # a workstation.
      # However, since Skylab technically isn't swarming, we need to look for
      # an alternative environment variable there.
      in_swarming = 'SWARMING_SERVER' in os.environ
      in_skylab = bool(int(os.environ.get('RUNNING_IN_SKYLAB', '0')))
      self._local_pixel_tests = not (in_swarming or in_skylab)
      if self._local_pixel_tests:
        logging.warning(
            'Automatically determined that test is running on a workstation')
      else:
        logging.warning(
            'Automatically determined that test is running on a bot')
    return self._local_pixel_tests

  @staticmethod
  def AddCommandLineArguments(parser: argparse.ArgumentParser) -> None:
    """ Add command line arguments to an ArgumentParser instance

    Args:
      parser: ArgumentParser instance

    Returns:
      None
    """
    parser.add_argument('--git-revision', type=str, help='Git revision')
    parser.add_argument('--gerrit-issue', type=int, help='Gerrit issue number')
    parser.add_argument('--gerrit-patchset',
                        type=int,
                        help='Gerrit patchset number')
    parser.add_argument('--buildbucket-id',
                        type=int,
                        help='Buildbucket ID of builder')
    parser.add_argument('--code-review-system',
                        type=str,
                        help='Code review system')
    parser.add_argument('--continuous-integration-system',
                        type=str,
                        help='Continuous integration system')

  def _InitializeProperties(self, args: ParsedCmdArgs) -> None:
    if hasattr(args, 'local_pixel_tests'):
      # If not set, will be automatically determined later if needed.
      self._local_pixel_tests = args.local_pixel_tests

    if hasattr(args, 'skia_gold_local_png_write_directory'):
      self._local_png_directory = args.skia_gold_local_png_write_directory

    if hasattr(args, 'no_luci_auth'):
      self._no_luci_auth = args.no_luci_auth

    if hasattr(args, 'service_account'):
      self._service_account = args.service_account
      if self._service_account:
        self._no_luci_auth = True

    if hasattr(args, 'bypass_skia_gold_functionality'):
      self._bypass_skia_gold_functionality = args.bypass_skia_gold_functionality

    if hasattr(args, 'code_review_system'):
      self._code_review_system = args.code_review_system

    if hasattr(args, 'continuous_integration_system'):
      self._continuous_integration_system = args.continuous_integration_system

    # Will be automatically determined later if needed.
    if not hasattr(args, 'git_revision') or not args.git_revision:
      return
    self._git_revision = args.git_revision

    # Only expected on tryjob runs.
    if not hasattr(args, 'gerrit_issue') or not args.gerrit_issue:
      return
    self._issue = args.gerrit_issue
    if not hasattr(args, 'gerrit_patchset') or not args.gerrit_patchset:
      raise RuntimeError(
          '--gerrit-issue passed, but --gerrit-patchset not passed.')
    self._patchset = args.gerrit_patchset
    if not hasattr(args, 'buildbucket_id') or not args.buildbucket_id:
      raise RuntimeError(
          '--gerrit-issue passed, but --buildbucket-id not passed.')
    self._job_id = args.buildbucket_id
