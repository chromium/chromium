# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re
import sys
import tempfile

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'gyp'))

from util import build_utils
from util import md5_check
from util import resource_utils
import bundletool

# List of valid modes for GenerateBundleApks()
BUILD_APKS_MODES = ('default', 'universal', 'system', 'system_compressed')
OPTIMIZE_FOR_OPTIONS = ('ABI', 'SCREEN_DENSITY', 'LANGUAGE',
                        'TEXTURE_COMPRESSION_FORMAT')
_SYSTEM_MODES = ('system_compressed', 'system')

_ALL_ABIS = ['armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64']


def _CreateDeviceSpec(bundle_path, sdk_version, locales):
  if not sdk_version:
    manifest_data = bundletool.RunBundleTool(
        ['dump', 'manifest', '--bundle', bundle_path])
    sdk_version = int(
        re.search(r'minSdkVersion.*?(\d+)', manifest_data).group(1))

  # Setting sdkVersion=minSdkVersion prevents multiple per-minSdkVersion .apk
  # files from being created within the .apks file.
  return {
      'screenDensity': 1000,  # Ignored since we don't split on density.
      'sdkVersion': sdk_version,
      'supportedAbis': _ALL_ABIS,  # Our .aab files are already split on abi.
      'supportedLocales': locales,
  }


def GenerateBundleApks(bundle_path,
                       bundle_apks_path,
                       aapt2_path,
                       keystore_path,
                       keystore_password,
                       keystore_alias,
                       mode=None,
                       local_testing=False,
                       minimal=False,
                       minimal_sdk_version=None,
                       check_for_noop=True,
                       system_image_locales=None,
                       optimize_for=None):
  """Generate an .apks archive from a an app bundle if needed.

  Args:
    bundle_path: Input bundle file path.
    bundle_apks_path: Output bundle .apks archive path. Name must end with
      '.apks' or this operation will fail.
    aapt2_path: Path to aapt2 build tool.
    keystore_path: Path to keystore.
    keystore_password: Keystore password, as a string.
    keystore_alias: Keystore signing key alias.
    mode: Build mode, which must be either None or one of BUILD_APKS_MODES.
    minimal: Create the minimal set of apks possible (english-only).
    minimal_sdk_version: Use this sdkVersion when |minimal| or
      |system_image_locales| args are present.
    check_for_noop: Use md5_check to short-circuit when inputs have not changed.
    system_image_locales: Locales to package in the APK when mode is "system"
      or "system_compressed".
    optimize_for: Overrides split configuration, which must be None or
      one of OPTIMIZE_FOR_OPTIONS.
  """
  device_spec = None
  if minimal_sdk_version:
    assert minimal or system_image_locales, (
        'minimal_sdk_version is only used when minimal or system_image_locales '
        'is specified')
  if minimal:
    # Measure with one language split installed. Use Hindi because it is
    # popular. resource_size.py looks for splits/base-hi.apk.
    # Note: English is always included since it's in base-master.apk.
    device_spec = _CreateDeviceSpec(bundle_path, minimal_sdk_version, ['hi'])
  elif mode in _SYSTEM_MODES:
    if not system_image_locales:
      raise Exception('system modes require system_image_locales')
    # Bundletool doesn't seem to understand device specs with locales in the
    # form of "<lang>-r<region>", so just provide the language code instead.
    locales = [
        resource_utils.ToAndroidLocaleName(l).split('-')[0]
        for l in system_image_locales
    ]
    device_spec = _CreateDeviceSpec(bundle_path, minimal_sdk_version, locales)

  def rebuild():
    logging.info('Building %s', bundle_apks_path)
    with tempfile.NamedTemporaryFile(suffix='.apks') as tmp_apks_file:
      cmd_args = [
          'build-apks',
          '--aapt2=%s' % aapt2_path,
          '--output=%s' % tmp_apks_file.name,
          '--bundle=%s' % bundle_path,
          '--ks=%s' % keystore_path,
          '--ks-pass=pass:%s' % keystore_password,
          '--ks-key-alias=%s' % keystore_alias,
          '--overwrite',
      ]

      if local_testing:
        cmd_args += ['--local-testing']

      if mode is not None:
        if mode not in BUILD_APKS_MODES:
          raise Exception('Invalid mode parameter %s (should be in %s)' %
                          (mode, BUILD_APKS_MODES))
        cmd_args += ['--mode=' + mode]

      if optimize_for:
        if optimize_for not in OPTIMIZE_FOR_OPTIONS:
          raise Exception('Invalid optimize_for parameter %s '
                          '(should be in %s)' %
                          (mode, OPTIMIZE_FOR_OPTIONS))
        cmd_args += ['--optimize-for=' + optimize_for]

      with tempfile.NamedTemporaryFile(mode='w', suffix='.json') as spec_file:
        if device_spec:
          json.dump(device_spec, spec_file)
          spec_file.flush()
          cmd_args += ['--device-spec=' + spec_file.name]
        bundletool.RunBundleTool(cmd_args)

      # Make the resulting .apks file hermetic.
      with build_utils.TempDir() as temp_dir, \
        build_utils.AtomicOutput(bundle_apks_path, only_if_changed=False) as f:
        files = build_utils.ExtractAll(tmp_apks_file.name, temp_dir)
        build_utils.DoZip(files, f, base_dir=temp_dir)

  if check_for_noop:
    # NOTE: BUNDLETOOL_JAR_PATH is added to input_strings, rather than
    # input_paths, to speed up MD5 computations by about 400ms (the .jar file
    # contains thousands of class files which are checked independently,
    # resulting in an .md5.stamp of more than 60000 lines!).
    input_paths = [bundle_path, aapt2_path, keystore_path]
    input_strings = [
        keystore_password,
        keystore_alias,
        bundletool.BUNDLETOOL_JAR_PATH,
        # NOTE: BUNDLETOOL_VERSION is already part of BUNDLETOOL_JAR_PATH, but
        # it's simpler to assume that this may not be the case in the future.
        bundletool.BUNDLETOOL_VERSION,
        device_spec,
    ]
    if mode is not None:
      input_strings.append(mode)

    # Avoid rebuilding (saves ~20s) when the input files have not changed. This
    # is essential when calling the apk_operations.py script multiple times with
    # the same bundle (e.g. out/Debug/bin/monochrome_public_bundle run).
    md5_check.CallAndRecordIfStale(
        rebuild,
        input_paths=input_paths,
        input_strings=input_strings,
        output_paths=[bundle_apks_path])
  else:
    rebuild()
