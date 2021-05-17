# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class for storing Skia Gold comparison properties.

Examples:
* git revision being tested
* Whether the test is being run locally or on a bot
* What the continuous integration system is
"""

import logging
import os
import subprocess
import sys


class SkiaGoldProperties(object):
  def __init__(self, args):
    """Abstract class to validate and store properties related to Skia Gold.

    Args:
      args: The parsed arguments from an argparse.ArgumentParser.
    """
    self._git_revision = None
    self._issue = None
    self._patchset = None
    self._job_id = None
    self._local_pixel_tests = None
    self._no_luci_auth = None
    self._bypass_skia_gold_functionality = None
    self._code_review_system = None
    self._continuous_integration_system = None

    self._InitializeProperties(args)

  def IsTryjobRun(self):
    return self.issue is not None

  @property
  def continuous_integration_system(self):
    return self._continuous_integration_system or 'buildbucket'

  @property
  def code_review_system(self):
    return self._code_review_system or 'gerrit'

  @property
  def git_revision(self):
    return self._GetGitRevision()

  @property
  def issue(self):
    return self._issue

  @property
  def job_id(self):
    return self._job_id

  @property
  def local_pixel_tests(self):
    return self._IsLocalRun()

  @property
  def no_luci_auth(self):
    return self._no_luci_auth

  @property
  def patchset(self):
    return self._patchset

  @property
  def bypass_skia_gold_functionality(self):
    return self._bypass_skia_gold_functionality

  @staticmethod
  def _GetGitOriginMasterHeadSha1():
    raise NotImplementedError()

  def _GetGitRevision(self):
    if not self._git_revision:
      # Automated tests should always pass the revision, so assume we're on
      # a workstation and try to get the local origin/master HEAD.
      if not self._IsLocalRun():
        raise RuntimeError(
            '--git-revision was not passed when running on a bot')
      revision = self._GetGitOriginMasterHeadSha1()
      if not revision or len(revision) != 40:
        raise RuntimeError(
            '--git-revision not passed and unable to determine from git')
      self._git_revision = revision
    return self._git_revision

  def _IsLocalRun(self):
    if self._local_pixel_tests is None:
      # Look for the presence of the SWARMING_SERVER environment variable as a
      # heuristic to determine whether we're running on a workstation or a bot.
      # This should always be set on swarming, but would be strange to be set on
      # a workstation.
      self._local_pixel_tests = 'SWARMING_SERVER' not in os.environ
      if self._local_pixel_tests:
        logging.warning(
            'Automatically determined that test is running on a workstation')
      else:
        logging.warning(
            'Automatically determined that test is running on a bot')
    return self._local_pixel_tests

  def _InitializeProperties(self, args):
    if hasattr(args, 'local_pixel_tests'):
      # If not set, will be automatically determined later if needed.
      self._local_pixel_tests = args.local_pixel_tests

    if hasattr(args, 'no_luci_auth'):
      self._no_luci_auth = args.no_luci_auth

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
