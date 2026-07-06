# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import os
import posixpath
import re

from pylib import constants

_EXCLUSIONS = [
    # Misc files that exist to document directories
    r'.*METADATA',
    r'.*OWNERS',
    r'.*\.md',
    r'.*\.crx',  # Chrome extension zip files.
    r'.*/\.git.*',  # Any '.git*' directories/files.
    r'.*\.so',  # Libraries packed into .apk.
    r'.*Mojo.*manifest\.json',  # Some source_set()s pull these in.
    r'.*\.py',  # Some test_support targets include python deps.
    r'.*\.apk',  # Should be installed separately.
    r'.*\.jar',  # Never need java intermediates.
    r'.*\.crx',  # Used by download_from_google_storage.
    r'.*\.wpr',  # Web-page-relay files needed only on host.
    r'.*lib.java/.*',  # Never need java intermediates.

    # Test filter files:
    r'.*/clank/build/bot/filters/.*',
    r'.*/testing/buildbot/filters/.*',

    # Chrome external extensions config file.
    r'.*external_extensions\.json',

    # v8's blobs and icu data get packaged into APKs.
    r'.*snapshot_blob.*\.bin',
    r'.*icudtl\.bin',

    # Scripts that are needed by swarming, but not on devices:
    r'.*goldctl',
    r'.*llvm-readelf',
    r'.*llvm-readobj',
    r'.*llvm-symbolizer',
    r'.*devil_util_(?:bin|dist|host)',
    r'.*md5sum_(?:bin|dist|host)',
    r'.*/development/scripts/stack',
    r'.*/build/android/pylib/symbols',
    r'.*/build/android/stacktrace',

    # Required for java deobfuscation on the host:
    r'.*build/android/stacktrace/.*',
    r'.*third_party/jdk/.*',
    r'.*third_party/proguard/.*',

    # Our tests don't need these.
    r'.*/devtools-frontend/.*front_end/.*',
    r'.*/devtools-frontend/.*inspector_overlay/.*',

    # Build artifacts:
    r'.*\.stamp',
    r'.*\.pak\.info',
    r'.*\.build_config.json',
    r'.*\.incremental\.json',
]


def _GetExclusionsRE():
  exclusions = _EXCLUSIONS + [
      re.escape(os.path.join(constants.GetOutDirectory(), 'bin'))
  ]
  return re.compile('|'.join(exclusions))


def _FilterDataDeps(abs_host_files):
  exclusions_re = _GetExclusionsRE()
  return [p for p in abs_host_files if not exclusions_re.search(p)]


def DevicePathFor(host_path, output_directory=None):
  """Returns the device path for a given host path.

  This returns the device path as a relative posix path string,
  which should be rooted at the device's external storage.

  Args:
    host_path: The absolute path to the host file.
    output_directory: The absolute path to the build output directory.
  Returns:
    A relative device path string.
  """
  output_directory = output_directory or constants.GetOutDirectory()
  if (host_path.startswith(output_directory) and
      os.path.splitext(host_path)[1] == '.pak'):
    return posixpath.join('paks', os.path.basename(host_path))

  rel_host_path = os.path.relpath(host_path, output_directory)

  # Split the path and filter out '..' components to keep it relative.
  parts = rel_host_path.split(os.sep)
  clean_parts = [p for p in parts if p and p != os.pardir]
  return posixpath.join(*clean_parts)


def GetDataDependencies(runtime_deps_path, device_data_filters=None):
  """Returns a list of device data dependencies.

  Args:
    runtime_deps_path: A str path to the .runtime_deps file.
    device_data_filters: A list of glob patterns to filter the dependencies.
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

  # TODO(crbug.com/525859933): Apply filter after ExpandDataDependencies().
  filtered_abs_host_files = _FilterDataDeps(abs_host_files)
  host_device_tuples = [(f, DevicePathFor(f, output_directory))
                        for f in filtered_abs_host_files]

  if device_data_filters:
    host_device_tuples = ExpandDataDependencies(host_device_tuples)
    host_device_tuples = FilterDataDependencies(host_device_tuples,
                                                device_data_filters)
  return host_device_tuples


def SubstituteDeviceRootSingle(device_path, device_root):
  if not device_path:
    return device_root
  return posixpath.join(device_root, device_path)


def SubstituteDeviceRoot(host_device_tuples, device_root):
  return [(h, SubstituteDeviceRootSingle(d, device_root))
          for h, d in host_device_tuples]


def ExpandDataDependencies(host_device_tuples):
  """Expands directory dependencies into file dependencies.

  Args:
    host_device_tuples: A list of (host_path, device_path) tuples,
      where:
        - host_path (str): Absolute path to a host file or directory.
        - device_path (str): Device path for the dependency.
  Returns:
    A list of (host_path, device_path) tuples where all host_paths
    are files (directories are expanded recursively).
  """
  ret = []
  for h, d in host_device_tuples:
    assert isinstance(d, str), f"Expected str for device path, got {type(d)}"
    if os.path.isdir(h):
      for root, _, filenames in os.walk(h):
        for filename in filenames:
          subpath = os.path.join(root, filename)
          rel_to_dir = os.path.relpath(subpath, h)
          # Convert rel_to_dir to posix path (device uses forward slashes)
          rel_to_dir_posix = rel_to_dir.replace(os.sep, '/')
          ret.append((subpath, posixpath.join(d, rel_to_dir_posix)))
    else:
      ret.append((h, d))
  return ret


def FilterDataDependencies(host_device_tuples, filters):
  if not filters:
    return host_device_tuples

  for f in filters:
    if not f or f[0] not in ('+', '-'):
      raise ValueError(f"Invalid filter: '{f}'. Must start with '+' or '-'.")

  filtered_tuples = []
  for host_path, device_path in host_device_tuples:
    # Make host_path relative to source root for matching
    rel_path = os.path.relpath(host_path, constants.DIR_SOURCE_ROOT)

    # Default to KEEP (blocklist approach)
    keep = True

    for f in filters:
      op = f[0]
      pattern = f[1:]
      # Strip '//' from pattern if present
      if pattern.startswith('//'):
        pattern = pattern[2:]

      if fnmatch.fnmatch(rel_path, pattern):
        if op == '+':
          keep = True
        elif op == '-':
          keep = False
        break

    if keep:
      filtered_tuples.append((host_path, device_path))

  return filtered_tuples
