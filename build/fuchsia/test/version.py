# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Reads version files from the chromium source code. """

import argparse
import logging
import os.path

from typing import Dict

from common import DIR_SRC_ROOT


def chrome_version() -> Dict[str, int]:
    """ Returns a replica of //chrome/VERSION, crashes if the file does not
    exist. This function does not assume the existence of all the fields, but
    treats missing ones as 0; on the other hand, the unexpected fields would be
    also ignored."""
    file = os.path.join(DIR_SRC_ROOT, 'chrome', 'VERSION')
    assert os.path.exists(file)
    result = {}

    def parse_line(field: str, line: str) -> bool:
        if line.startswith(field.upper() + '='):
            result[field] = int(line[len(field.upper()) + 1:].rstrip())
            return True
        return False

    with open(file, 'r') as reader:
        for line in reader:
            if (not parse_line('major', line)
                    and not parse_line('minor', line)
                    and not parse_line('build', line)
                    and not parse_line('patch', line)):
                logging.warning('Unexpected line %s in the VERSION file', line)
    return result


def chrome_version_str() -> str:
    """ Returns the chrome_version in a string representation. """
    version = chrome_version()
    return (f'{version["major"]}.{version["minor"]}.'
            f'{version["build"]}.{version["patch"]}')


def _load_git_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--git-revision', default=None)
    parser.add_argument('--gerrit-issue', type=int, default=None)
    parser.add_argument('--gerrit-patchset', type=int, default=None)
    parser.add_argument('--buildbucket-id', type=int, default=None)
    args, _ = parser.parse_known_args()
    # The args look like
    # '--git-revision=e98127af84bf5b33a6e657c90dfd3f3a731eb28c'
    # '--gerrit-issue=5009604'
    # '--gerrit-patchset=16'
    # '--buildbucket-id=8756180599882888289'
    # on a try build. CI builds have only git-revision.
    return args


_GIT_ARGS: argparse.Namespace = _load_git_args()


def is_try_build() -> bool:
    """ Returns whether current build is running as a try-build, or unmerged
    change. This function crashes if the info cannot be retrieved. """
    assert _GIT_ARGS.git_revision
    return _GIT_ARGS.gerrit_issue is not None


def git_revision() -> str:
    """ Returns the git revision to identify the current change list or the
    commit info of the CI build. This function crashes if the info cannot be
    retrieved. """
    assert _GIT_ARGS.git_revision
    if not is_try_build():
        return _GIT_ARGS.git_revision
    return (f'{_GIT_ARGS.git_revision}/{_GIT_ARGS.gerrit_issue}/'
            f'{_GIT_ARGS.gerrit_patchset}')
