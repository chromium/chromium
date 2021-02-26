# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

from pylib import constants


_EXCLUSIONS = [
    re.compile(r'.*OWNERS'),  # Should never be included.
    re.compile(r'.*\.crx'),  # Chrome extension zip files.
    re.compile(os.path.join('.*',
                            r'\.git.*')),  # Any '.git*' directories/files.
    re.compile(r'.*\.so'),  # Libraries packed into .apk.
    re.compile(r'.*Mojo.*manifest\.json'),  # Some source_set()s pull these in.
    re.compile(r'.*\.py'),  # Some test_support targets include python deps.
    re.compile(r'.*\.apk'),  # Should be installed separately.
    re.compile(r'.*lib.java/.*'),  # Never need java intermediates.

    # Test filter files:
    re.compile(r'.*/testing/buildbot/filters/.*'),

    # Chrome external extensions config file.
    re.compile(r'.*external_extensions\.json'),

    # Exists just to test the compile, not to be run.
    re.compile(r'.*jni_generator_tests'),

    # v8's blobs and icu data get packaged into APKs.
    re.compile(r'.*snapshot_blob.*\.bin'),
    re.compile(r'.*icudtl.bin'),

    # Scripts that are needed by swarming, but not on devices:
    re.compile(r'.*llvm-symbolizer'),
    re.compile(r'.*md5sum_bin'),
    re.compile(os.path.join('.*', 'development', 'scripts', 'stack')),

    # Required for java deobfuscation on the host:
    re.compile(r'.*build/android/stacktrace/.*'),
    re.compile(r'.*third_party/jdk/.*'),
    re.compile(r'.*third_party/proguard/.*'),

    # Build artifacts:
    re.compile(r'.*\.stamp'),
    re.compile(r'.*.pak\.info'),
    re.compile(r'.*\.incremental\.json'),
]


def _FilterDataDeps(abs_host_files):
  exclusions = _EXCLUSIONS + [
      re.compile(os.path.join(constants.GetOutDirectory(), 'bin'))
  ]
  return [p for p in abs_host_files if not any(r.match(p) for r in exclusions)]


def DevicePathComponentsFor(host_path, output_directory):
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
    rel_host_files = [l.strip() for l in runtime_deps_file if l]

  output_directory = constants.GetOutDirectory()
  abs_host_files = [
      os.path.abspath(os.path.join(output_directory, r))
      for r in rel_host_files]
  filtered_abs_host_files = _FilterDataDeps(abs_host_files)
  # TODO(crbug.com/752610): Filter out host executables, and investigate
  # whether other files could be filtered as well.
  return [(f, DevicePathComponentsFor(f, output_directory))
          for f in filtered_abs_host_files]
