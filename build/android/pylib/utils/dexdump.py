# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import shutil
import sys
import tempfile
from xml.etree import ElementTree

from devil.utils import cmd_helper
from pylib import constants

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'gyp'))
from util import build_utils

DEXDUMP_PATH = os.path.join(constants.ANDROID_SDK_TOOLS, 'dexdump')


def Dump(apk_path):
  """Dumps class and method information from a APK into a dict via dexdump.

  Args:
    apk_path: An absolute path to an APK file to dump.
  Returns:
    A dict in the following format:
      {
        <package_name>: {
          'classes': {
            <class_name>: {
              'methods': [<method_1>, <method_2>]
            }
          }
        }
      }
  """
  try:
    dexfile_dir = tempfile.mkdtemp()
    parsed_dex_files = []
    for dex_file in build_utils.ExtractAll(apk_path,
                                           dexfile_dir,
                                           pattern='*classes*.dex'):
      output_xml = cmd_helper.GetCmdOutput(
          [DEXDUMP_PATH, '-l', 'xml', dex_file])
      # Dexdump doesn't escape its XML output very well; decode it as utf-8 with
      # invalid sequences replaced, then remove forbidden characters and
      # re-encode it (as etree expects a byte string as input so it can figure
      # out the encoding itself from the XML declaration)
      BAD_XML_CHARS = re.compile(
          u'[\x00-\x08\x0b-\x0c\x0e-\x1f\x7f-\x84\x86-\x9f' +
          u'\ud800-\udfff\ufdd0-\ufddf\ufffe-\uffff]')
      if sys.version_info[0] < 3:
        decoded_xml = output_xml.decode('utf-8', 'replace')
        clean_xml = BAD_XML_CHARS.sub(u'\ufffd', decoded_xml)
      else:
        # Line duplicated to avoid pylint redefined-variable-type error.
        clean_xml = BAD_XML_CHARS.sub(u'\ufffd', output_xml)
      parsed_dex_files.append(
          _ParseRootNode(ElementTree.fromstring(clean_xml.encode('utf-8'))))
    return parsed_dex_files
  finally:
    shutil.rmtree(dexfile_dir)


def _ParseRootNode(root):
  """Parses the XML output of dexdump. This output is in the following format.

  This is a subset of the information contained within dexdump output.

  <api>
    <package name="foo.bar">
      <class name="Class" extends="foo.bar.SuperClass">
        <field name="Field">
        </field>
        <constructor name="Method">
          <parameter name="Param" type="int">
          </parameter>
        </constructor>
        <method name="Method">
          <parameter name="Param" type="int">
          </parameter>
        </method>
      </class>
    </package>
  </api>
  """
  results = {}
  for child in root:
    if child.tag == 'package':
      package_name = child.attrib['name']
      parsed_node = _ParsePackageNode(child)
      if package_name in results:
        results[package_name]['classes'].update(parsed_node['classes'])
      else:
        results[package_name] = parsed_node
  return results


def _ParsePackageNode(package_node):
  """Parses a <package> node from the dexdump xml output.

  Returns:
    A dict in the format:
      {
        'classes': {
          <class_1>: {
            'methods': [<method_1>, <method_2>]
          },
          <class_2>: {
            'methods': [<method_1>, <method_2>]
          },
        }
      }
  """
  classes = {}
  for child in package_node:
    if child.tag == 'class':
      classes[child.attrib['name']] = _ParseClassNode(child)
  return {'classes': classes}


def _ParseClassNode(class_node):
  """Parses a <class> node from the dexdump xml output.

  Returns:
    A dict in the format:
      {
        'methods': [<method_1>, <method_2>]
      }
  """
  methods = []
  for child in class_node:
    if child.tag == 'method':
      methods.append(child.attrib['name'])
  return {'methods': methods, 'superclass': class_node.attrib['extends']}
