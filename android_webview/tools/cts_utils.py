# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Stage the Chromium checkout to update CTS test version."""

import json
import os
import re

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

TOOLS_DIR = os.path.join('android_webview', 'tools')

CONFIG_FILE = os.path.join('cts_config', 'webview_cts_gcs_path.json')
CONFIG_PATH = os.path.join(SRC_DIR, TOOLS_DIR, CONFIG_FILE)


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


class ChromiumRepoHelper:
  """Performs operations on Chromium checkout."""

  def __init__(self, root_dir=SRC_DIR):
    self._root_dir = os.path.abspath(root_dir)

  def rebase(self, *rel_path_parts):
    """Construct absolute path from parts relative to root_dir.

    Args:
      rel_path_parts: Parts of the root relative path.

    Returns:
      The absolute path.
    """
    return os.path.join(self._root_dir, *rel_path_parts)
