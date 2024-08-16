#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

SRC = os.path.abspath(
    os.path.join(os.path.dirname(__file__), *([os.pardir] * 6)))
BLINK_WEB_TESTS = os.path.join(SRC, 'third_party', 'blink', 'web_tests')
BLINK_WEB_EXPOSED_TESTS = os.path.join(BLINK_WEB_TESTS, 'webexposed')
BLINK_INTERFACES_PATH = os.path.join(BLINK_WEB_EXPOSED_TESTS,
                                     'global-interface-listing-expected.txt')
NOT_WEBVIEW_EXPOSED_PATH = os.path.join(os.path.dirname(__file__),
                                        'not-webview-exposed.txt')
NOT_WEBVIEW_EXPOSED_REPO_RELPATH = (
    '//' + os.path.relpath(NOT_WEBVIEW_EXPOSED_PATH, SRC))

# Keywords used to find interfaces
INTERFACE_KEYWORDS = ['interface', '[GLOBAL OBJECT]']
# Keywords to find properties
PROPERTIES_KEYWORDS = ['getter', 'setter', 'method', 'attribute']


def build_interfaces_map(file_obj, interfaces_map=None):
  """Build a dictionary mapping interfaces to their set of properties

  Args:
    file_obj: File object for file containing interfaces and properties.
    interfaces_map: Existing dictionary to merge interfaces and
      properties into.

  Returns:
    A dictionary mapping interfaces to their properties.
  """
  interfaces_map = interfaces_map or {}
  props = None

  for line in file_obj.read().splitlines():
    line = re.sub(r'#.*$', '', line).strip()
    if any(line.startswith(keyword) for keyword in INTERFACE_KEYWORDS):
      props = interfaces_map.setdefault(line, set())
    elif any(line.startswith(keyword) for keyword in PROPERTIES_KEYWORDS):
      assert props is not None, ('Property %r has no parent interface in %s' %
                                 (line, file_obj.name))
      props.add(line)

  return interfaces_map


def construct_error_message(missing_interfaces, missing_properties):
  """Constructs an error message from missing interfaces and properties

  Args:
    missing_interfaces: List of missing interfaces.
    missing_properties: Dictionary mapping missing properties to an interface.

  Returns:
    A string containing an error message describing missing interfaces and
      properties.
  """
  error_msg = ''
  if missing_interfaces:
    error_msg += (
        ('\nThe following interfaces are missing in blink '
         'expectations files.\nIn order to suppress this message '
         'remove them and their properties from\n%s.\n%s') %
        (NOT_WEBVIEW_EXPOSED_REPO_RELPATH, '\n'.join(
            ['\t- %s' % interface for interface in missing_interfaces])))
    error_msg += '\n'

  if missing_properties:
    error_msg += (('\nThe following interface properties are missing in blink '
                   'expectations files.\nIn order to suppress this message '
                   'remove them from \n%s.\n') %
                  NOT_WEBVIEW_EXPOSED_REPO_RELPATH)

    for interface, properties in missing_properties.items():
      error_msg += (
          'Interface %r is missing the following properties\n%s\n' %
          (interface, '\n'.join(['\t- %s' % prop for prop in properties])))

    error_msg += '\n'

  return error_msg


def main():
  missing_blink_interfaces = []
  missing_blink_properties = {}
  error_msg = ''

  with open(NOT_WEBVIEW_EXPOSED_PATH) as excluded_interfaces_obj, \
      open(BLINK_INTERFACES_PATH) as blink_interfaces_obj:

    excluded_interfaces = build_interfaces_map(excluded_interfaces_obj)
    blink_interfaces = build_interfaces_map(blink_interfaces_obj)

    # Check that all not WebView exposed interfaces and properties are in the
    # set of blink interfaces and properties.
    for interface in excluded_interfaces:
      if interface not in blink_interfaces:
        missing_blink_interfaces.append(interface)
      else:
        non_blink_props = (excluded_interfaces.get(interface, set()) -
                           blink_interfaces.get(interface, set()))
        if non_blink_props:
          missing_blink_properties.update({interface: non_blink_props})

    error_msg = construct_error_message(missing_blink_interfaces,
                                        missing_blink_properties)
    assert not (missing_blink_interfaces or missing_blink_properties), error_msg


if __name__ == '__main__':
  main()
