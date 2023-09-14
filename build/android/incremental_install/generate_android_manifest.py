#!/usr/bin/env python3
#
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates an AndroidManifest.xml for an incremental APK.

Given the manifest file for the real APK, generates an AndroidManifest.xml with
the application class changed to IncrementalApplication.
"""

import argparse
import os
import sys
from xml.etree import ElementTree

sys.path.append(os.path.join(os.path.dirname(__file__), os.path.pardir, 'gyp'))
from util import build_utils
from util import manifest_utils
import action_helpers  # build_utils adds //build to sys.path.

_DEFAULT_APPLICATION_CLASS = 'android.app.Application'
_INCREMENTAL_APP_NAME = 'org.chromium.incrementalinstall.BootstrapApplication'
_INCREMENTAL_APP_COMPONENT_FACTORY = (
    'org.chromium.incrementalinstall.BootstrapAppComponentFactory')
_META_DATA_APP_NAME = 'incremental-install-application'
_META_DATA_APP_COMPONENT_FACTORY = 'incremental-install-app-component-factory'
_META_DATA_INSTRUMENTATION_NAMES = [
    'incremental-install-instrumentation-0',
    'incremental-install-instrumentation-1',
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
  parser.add_argument('--src-manifest',
                      required=True,
                      help='The main manifest of the app.')
  parser.add_argument('--dst-manifest',
                      required=True,
                      help='The output modified manifest.')
  parser.add_argument('--disable-isolated-processes',
                      help='Changes all android:isolatedProcess to false. '
                           'This is required on Android M+',
                      action='store_true')

  ret = parser.parse_args(build_utils.ExpandFileArgs(args))
  return ret


def _CreateMetaData(parent, name, value):
  meta_data_node = ElementTree.SubElement(parent, 'meta-data')
  meta_data_node.set(_AddNamespace('name'), name)
  meta_data_node.set(_AddNamespace('value'), value)


def _ProcessManifest(path, disable_isolated_processes):
  doc, _, app_node = manifest_utils.ParseManifest(path)

  # Pylint for some reason things app_node is an int.
  # pylint: disable=no-member
  real_app_class = app_node.get(_AddNamespace('name'),
                                _DEFAULT_APPLICATION_CLASS)
  app_node.set(_AddNamespace('name'), _INCREMENTAL_APP_NAME)
  # pylint: enable=no-member
  _CreateMetaData(app_node, _META_DATA_APP_NAME, real_app_class)

  real_acf = app_node.get(_AddNamespace('appComponentFactory'))
  if real_acf:
    app_node.set(_AddNamespace('appComponentFactory'),
                 _INCREMENTAL_APP_COMPONENT_FACTORY)
    _CreateMetaData(app_node, _META_DATA_APP_COMPONENT_FACTORY, real_acf)

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
  ret = ret.replace(b'extractNativeLibs="false"', b'extractNativeLibs="true"')
  if disable_isolated_processes:
    ret = ret.replace(b'isolatedProcess="true"', b'isolatedProcess="false"')
    # externalService only matters for isolatedProcess="true". See:
    # https://developer.android.com/reference/android/R.attr#externalService
    ret = ret.replace(b'externalService="true"', b'externalService="false"')
  return ret


def main(raw_args):
  options = _ParseArgs(raw_args)

  new_manifest_data = _ProcessManifest(options.src_manifest,
                                       options.disable_isolated_processes)
  with action_helpers.atomic_output(options.dst_manifest) as out_manifest:
    out_manifest.write(new_manifest_data)


if __name__ == '__main__':
  main(sys.argv[1:])
