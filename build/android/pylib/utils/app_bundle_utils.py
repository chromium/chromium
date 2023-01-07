# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import pathlib
import re
import shutil
import sys
import zipfile

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'gyp'))

from util import build_utils
from util import md5_check
from util import resource_utils
import bundletool

# "system_apks" is "default", but with locale list and compressed dex.
_SYSTEM_MODES = ('system', 'system_apks')
BUILD_APKS_MODES = _SYSTEM_MODES + ('default', 'universal')
OPTIMIZE_FOR_OPTIONS = ('ABI', 'SCREEN_DENSITY', 'LANGUAGE',
                        'TEXTURE_COMPRESSION_FORMAT')

_ALL_ABIS = ['armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64']


def _BundleMinSdkVersion(bundle_path):
  manifest_data = bundletool.RunBundleTool(
      ['dump', 'manifest', '--bundle', bundle_path])
  return int(re.search(r'minSdkVersion.*?(\d+)', manifest_data).group(1))


def _CreateDeviceSpec(bundle_path, sdk_version, locales):
  if not sdk_version:
    sdk_version = _BundleMinSdkVersion(bundle_path)

  # Setting sdkVersion=minSdkVersion prevents multiple per-minSdkVersion .apk
  # files from being created within the .apks file.
  return {
      'screenDensity': 1000,  # Ignored since we don't split on density.
      'sdkVersion': sdk_version,
      'supportedAbis': _ALL_ABIS,  # Our .aab files are already split on abi.
      'supportedLocales': locales,
  }


def _FixBundleDexCompressionGlob(src_bundle, dst_bundle):
  # Modifies the BundleConfig.pb of the given .aab to add "classes*.dex" to the
  # "uncompressedGlob" list.
  with zipfile.ZipFile(src_bundle) as src, \
      zipfile.ZipFile(dst_bundle, 'w') as dst:
    for info in src.infolist():
      data = src.read(info)
      if info.filename == 'BundleConfig.pb':
        # A classesX.dex entry is added by create_app_bundle.py so that we can
        # modify it here in order to have it take effect. b/176198991
        data = data.replace(b'classesX.dex', b'classes*.dex')
      dst.writestr(info, data)


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
    with build_utils.TempDir() as tmp_dir:
      tmp_apks_file = os.path.join(tmp_dir, 'output.apks')
      cmd_args = [
          'build-apks',
          '--aapt2=%s' % aapt2_path,
          '--output=%s' % tmp_apks_file,
          '--ks=%s' % keystore_path,
          '--ks-pass=pass:%s' % keystore_password,
          '--ks-key-alias=%s' % keystore_alias,
          '--overwrite',
      ]
      input_bundle_path = bundle_path
      # Work around bundletool not respecting uncompressDexFiles setting.
      # b/176198991
      if mode not in _SYSTEM_MODES and _BundleMinSdkVersion(bundle_path) >= 27:
        input_bundle_path = os.path.join(tmp_dir, 'system.aab')
        _FixBundleDexCompressionGlob(bundle_path, input_bundle_path)

      cmd_args += ['--bundle=%s' % input_bundle_path]

      if local_testing:
        cmd_args += ['--local-testing']

      if mode is not None:
        if mode not in BUILD_APKS_MODES:
          raise Exception('Invalid mode parameter %s (should be in %s)' %
                          (mode, BUILD_APKS_MODES))
        if mode != 'system_apks':
          cmd_args += ['--mode=' + mode]
        else:
          # Specify --optimize-for to prevent language splits being created.
          cmd_args += ['--optimize-for=device_tier']

      if optimize_for:
        if optimize_for not in OPTIMIZE_FOR_OPTIONS:
          raise Exception('Invalid optimize_for parameter %s '
                          '(should be in %s)' %
                          (mode, OPTIMIZE_FOR_OPTIONS))
        cmd_args += ['--optimize-for=' + optimize_for]

      if device_spec:
        data = json.dumps(device_spec)
        logging.debug('Device Spec: %s', data)
        spec_file = pathlib.Path(tmp_dir) / 'device.json'
        spec_file.write_text(data)
        cmd_args += ['--device-spec=' + str(spec_file)]

      bundletool.RunBundleTool(cmd_args)

      shutil.move(tmp_apks_file, bundle_apks_path)

  if check_for_noop:
    input_paths = [
        bundle_path,
        bundletool.BUNDLETOOL_JAR_PATH,
        aapt2_path,
        keystore_path,
    ]
    input_strings = [
        keystore_password,
        keystore_alias,
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
