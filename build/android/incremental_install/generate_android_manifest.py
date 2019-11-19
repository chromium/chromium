#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates an AndroidManifest.xml for an incremental APK.

Given the manifest file for the real APK, generates an AndroidManifest.xml with
the application class changed to IncrementalApplication.
"""

import argparse
import os
import subprocess
import sys
import tempfile
import zipfile
from xml.etree import ElementTree

sys.path.append(os.path.join(os.path.dirname(__file__), os.path.pardir, 'gyp'))
from util import build_utils
from util import manifest_utils
from util import resource_utils

_INCREMENTAL_APP_NAME = 'org.chromium.incrementalinstall.BootstrapApplication'
_META_DATA_APP_NAME = 'incremental-install-real-app'
_DEFAULT_APPLICATION_CLASS = 'android.app.Application'
_META_DATA_INSTRUMENTATION_NAMES = [
    'incremental-install-real-instrumentation-0',
    'incremental-install-real-instrumentation-1',
]
_INCREMENTAL_INSTRUMENTATION_CLASSES = [
    'android.app.Instrumentation',
    'org.chromium.incrementalinstall.SecondInstrumentation',
]


def _AddNamespace(name):
  """Adds the android namespace prefix to the given identifier."""
  return '{%s}%s' % (manifest_utils.ANDROID_NAMESPACE, name)


def _ParseArgs(args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--src-manifest', required=True, help='The main manifest of the app')
  parser.add_argument('--disable-isolated-processes',
                      help='Changes all android:isolatedProcess to false. '
                           'This is required on Android M+',
                      action='store_true')
  parser.add_argument(
      '--out-apk', required=True, help='Path to output .ap_ file')
  parser.add_argument(
      '--in-apk', required=True, help='Path to non-incremental .ap_ file')
  parser.add_argument(
      '--aapt2-path', required=True, help='Path to the Android aapt tool')
  parser.add_argument(
      '--android-sdk-jars', help='GN List of resource apks to include.')

  ret = parser.parse_args(build_utils.ExpandFileArgs(args))
  ret.android_sdk_jars = build_utils.ParseGnList(ret.android_sdk_jars)
  return ret


def _CreateMetaData(parent, name, value):
  meta_data_node = ElementTree.SubElement(parent, 'meta-data')
  meta_data_node.set(_AddNamespace('name'), name)
  meta_data_node.set(_AddNamespace('value'), value)


def _ProcessManifest(path, arsc_package_name, disable_isolated_processes):
  doc, manifest_node, app_node = manifest_utils.ParseManifest(path)

  # Ensure the manifest package matches that of the apk's arsc package
  # So that resource references resolve correctly. The actual manifest
  # package name is set via --rename-manifest-package.
  manifest_node.set('package', arsc_package_name)

  # Pylint for some reason things app_node is an int.
  # pylint: disable=no-member
  real_app_class = app_node.get(_AddNamespace('name'),
                                _DEFAULT_APPLICATION_CLASS)
  app_node.set(_AddNamespace('name'), _INCREMENTAL_APP_NAME)
  # pylint: enable=no-member
  _CreateMetaData(app_node, _META_DATA_APP_NAME, real_app_class)

  # Seems to be a bug in ElementTree, as doc.find() doesn't work here.
  instrumentation_nodes = doc.findall('instrumentation')
  assert len(instrumentation_nodes) <= 2, (
      'Need to update incremental install to support >2 <instrumentation> tags')
  for i, instrumentation_node in enumerate(instrumentation_nodes):
    real_instrumentation_class = instrumentation_node.get(_AddNamespace('name'))
    instrumentation_node.set(_AddNamespace('name'),
                             _INCREMENTAL_INSTRUMENTATION_CLASSES[i])
    _CreateMetaData(app_node, _META_DATA_INSTRUMENTATION_NAMES[i],
                    real_instrumentation_class)

  ret = ElementTree.tostring(doc.getroot(), encoding='UTF-8')
  # Disable check for page-aligned native libraries.
  ret = ret.replace('extractNativeLibs="false"', 'extractNativeLibs="true"')
  if disable_isolated_processes:
    ret = ret.replace('isolatedProcess="true"', 'isolatedProcess="false"')
  return ret


def main(raw_args):
  options = _ParseArgs(raw_args)

  arsc_package, _ = resource_utils.ExtractArscPackage(options.aapt2_path,
                                                      options.in_apk)
  # Extract version from the compiled manifest since it might have been set
  # via aapt, and not exist in the manifest's text form.
  version_code, version_name, manifest_package = (
      resource_utils.ExtractBinaryManifestValues(options.aapt2_path,
                                                 options.in_apk))

  new_manifest_data = _ProcessManifest(options.src_manifest, arsc_package,
                                       options.disable_isolated_processes)
  with tempfile.NamedTemporaryFile() as tmp_manifest, \
      tempfile.NamedTemporaryFile() as tmp_apk:
    tmp_manifest.write(new_manifest_data)
    tmp_manifest.flush()
    cmd = [
        options.aapt2_path, 'link', '-o', tmp_apk.name, '--manifest',
        tmp_manifest.name, '-I', options.in_apk, '--replace-version',
        '--version-code', version_code, '--version-name', version_name,
        '--rename-manifest-package', manifest_package, '--debug-mode'
    ]
    for j in options.android_sdk_jars:
      cmd += ['-I', j]
    subprocess.check_call(cmd)
    with zipfile.ZipFile(options.out_apk, 'w') as z:
      path_transform = lambda p: None if p != 'AndroidManifest.xml' else p
      build_utils.MergeZips(z, [tmp_apk.name], path_transform=path_transform)
      path_transform = lambda p: None if p == 'AndroidManifest.xml' else p
      build_utils.MergeZips(z, [options.in_apk], path_transform=path_transform)


if __name__ == '__main__':
  main(sys.argv[1:])
