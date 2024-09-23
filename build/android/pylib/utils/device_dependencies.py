# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import posixpath
import re

from pylib import constants

_EXCLUSIONS = [
    # Misc files that exist to document directories
    re.compile(r'.*METADATA'),
    re.compile(r'.*OWNERS'),
    re.compile(r'.*\.md'),
    re.compile(r'.*\.crx'),  # Chrome extension zip files.
    re.compile(r'.*/\.git.*'),  # Any '.git*' directories/files.
    re.compile(r'.*\.so'),  # Libraries packed into .apk.
    re.compile(r'.*Mojo.*manifest\.json'),  # Some source_set()s pull these in.
    re.compile(r'.*\.py'),  # Some test_support targets include python deps.
    re.compile(r'.*\.apk'),  # Should be installed separately.
    re.compile(r'.*\.jar'),  # Never need java intermediates.
    re.compile(r'.*\.crx'),  # Used by download_from_google_storage.
    re.compile(r'.*\.wpr'),  # Web-page-relay files needed only on host.
    re.compile(r'.*lib.java/.*'),  # Never need java intermediates.

    # Test filter files:
    re.compile(r'.*/testing/buildbot/filters/.*'),

    # Chrome external extensions config file.
    re.compile(r'.*external_extensions\.json'),

    # v8's blobs and icu data get packaged into APKs.
    re.compile(r'.*snapshot_blob.*\.bin'),
    re.compile(r'.*icudtl\.bin'),

    # Scripts that are needed by swarming, but not on devices:
    re.compile(r'.*llvm-symbolizer'),
    re.compile(r'.*md5sum_(?:bin|dist)'),
    re.compile(r'.*/development/scripts/stack'),
    re.compile(r'.*/build/android/pylib/symbols'),
    re.compile(r'.*/build/android/stacktrace'),

    # Required for java deobfuscation on the host:
    re.compile(r'.*build/android/stacktrace/.*'),
    re.compile(r'.*third_party/jdk/.*'),
    re.compile(r'.*third_party/proguard/.*'),

    # Our tests don't need these.
    re.compile(r'.*/devtools-frontend/.*front_end/.*'),

    # Build artifacts:
    re.compile(r'.*\.stamp'),
    re.compile(r'.*\.pak\.info'),
    re.compile(r'.*\.build_config.json'),
    re.compile(r'.*\.incremental\.json'),
]


def _FilterDataDeps(abs_host_files):
  exclusions = _EXCLUSIONS + [
      re.compile(os.path.join(constants.GetOutDirectory(), 'bin'))
  ]
  return [p for p in abs_host_files if not any(r.match(p) for r in exclusions)]


def DevicePathComponentsFor(host_path, output_directory=None):
  """Returns the device path components for a given host path.

  This returns the device path as a list of joinable path components,
  with None as the first element to indicate that the path should be
  rooted at $EXTERNAL_STORAGE.

  e.g., given

    '$RUNTIME_DEPS_ROOT_DIR/foo/bar/baz.txt'

  this would return

    [None, 'foo', 'bar', 'baz.txt']

  This handles a couple classes of paths differently than it otherwise would:
    - All .pak files get mapped to top-level paks/
    - All other dependencies get mapped to the top level directory
        - If a file is not in the output directory then it's relative path to
          the output directory will start with .. strings, so we remove those
          and then the path gets mapped to the top-level directory
        - If a file is in the output directory then the relative path to the
          output directory gets mapped to the top-level directory

  e.g. given

    '$RUNTIME_DEPS_ROOT_DIR/out/Release/icu_fake_dir/icudtl.dat'

  this would return

    [None, 'icu_fake_dir', 'icudtl.dat']

  Args:
    host_path: The absolute path to the host file.
  Returns:
    A list of device path components.
  """
  output_directory = output_directory or constants.GetOutDirectory()
  if (host_path.startswith(output_directory) and
      os.path.splitext(host_path)[1] == '.pak'):
    return [None, 'paks', os.path.basename(host_path)]

  rel_host_path = os.path.relpath(host_path, output_directory)

  device_path_components = [None]
  p = rel_host_path
  while p:
    p, d = os.path.split(p)
    # The relative path from the output directory to a file under the runtime
    # deps root directory may start with multiple .. strings, so they need to
    # be skipped.
    if d and d != os.pardir:
      device_path_components.insert(1, d)
  return device_path_components


def GetDataDependencies(runtime_deps_path):
  """Returns a list of device data dependencies.

  Args:
    runtime_deps_path: A str path to the .runtime_deps file.
  Returns:
    A list of (host_path, device_path) tuples.
  """
  if not runtime_deps_path:
    return []

  with open(runtime_deps_path, 'r') as runtime_deps_file:
    # .runtime_deps can contain duplicates.
    rel_host_files = sorted({l.strip() for l in runtime_deps_file if l})

  output_directory = constants.GetOutDirectory()
  abs_host_files = [
      os.path.abspath(os.path.join(output_directory, r))
      for r in rel_host_files]
  filtered_abs_host_files = _FilterDataDeps(abs_host_files)
  # TODO(crbug.com/40533647): Filter out host executables, and investigate
  # whether other files could be filtered as well.
  return [(f, DevicePathComponentsFor(f, output_directory))
          for f in filtered_abs_host_files]


def SubstituteDeviceRootSingle(device_path, device_root):
  if not device_path:
    return device_root
  if isinstance(device_path, list):
    return posixpath.join(*(p if p else device_root for p in device_path))
  return device_path


def SubstituteDeviceRoot(host_device_tuples, device_root):
  return [(h, SubstituteDeviceRootSingle(d, device_root))
          for h, d in host_device_tuples]


def ExpandDataDependencies(host_device_tuples):
  ret = []
  for h, d in host_device_tuples:
    if os.path.isdir(h):
      for subpath in glob.glob(f'{h}/**', recursive=True):
        if not os.path.isdir(subpath):
          new_part = subpath[len(h):]
          ret.append((subpath, d + new_part))
    else:
      ret.append((h, d))
  return ret
