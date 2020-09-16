#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Add all generated lint_result.xml files to suppressions.xml"""

# pylint: disable=no-member

from __future__ import print_function

import argparse
import os
import re
import sys
from xml.dom import minidom

_BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(_BUILD_ANDROID_DIR)

_TMP_DIR_RE = re.compile(r'^/tmp/.*/(SRC_ROOT[0-9]+|PRODUCT_DIR)/')
_THIS_FILE = os.path.abspath(__file__)
_DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(_THIS_FILE),
                                    'suppressions.xml')
_INSERT_COMMENT = ('TODO: This line was added by suppress.py,'
                   ' please add an explanation.')


class _Issue(object):

  def __init__(self, dom_element):
    self.regexps = set()
    self.dom_element = dom_element


def _CollectIssuesFromDom(dom):
  issues_dict = {}
  for issue_element in dom.getElementsByTagName('issue'):
    issue_id = issue_element.attributes['id'].value
    issue = _Issue(issue_element)
    issues_dict[issue_id] = issue
    for child in issue_element.childNodes:
      if child.nodeType != minidom.Node.ELEMENT_NODE:
        continue
      if child.tagName == 'ignore' and child.getAttribute('regexp'):
        issue.regexps.add(child.getAttribute('regexp'))

  return issues_dict


def _TrimWhitespaceNodes(n):
  """Remove all whitespace-only TEXT_NODEs."""
  rm_children = []
  for c in n.childNodes:
    if c.nodeType == minidom.Node.TEXT_NODE and c.data.strip() == '':
      rm_children.append(c)
    else:
      _TrimWhitespaceNodes(c)

  for c in rm_children:
    n.removeChild(c)


def _ParseAndInsertNewSuppressions(result_path, config_path):
  print('Parsing %s' % config_path)
  config_dom = minidom.parse(config_path)
  issues_dict = _CollectIssuesFromDom(config_dom)
  print('Parsing and merging %s' % result_path)
  dom = minidom.parse(result_path)
  for issue_element in dom.getElementsByTagName('issue'):
    issue_id = issue_element.attributes['id'].value
    severity = issue_element.attributes['severity'].value
    path = issue_element.getElementsByTagName(
        'location')[0].attributes['file'].value
    # Strip temporary file path.
    path = re.sub(_TMP_DIR_RE, '', path)
    # Escape Java inner class name separator and suppress with regex instead
    # of path. Doesn't use re.escape() as it is a bit too aggressive and
    # escapes '_', causing trouble with PRODUCT_DIR.
    regexp = path.replace('$', r'\$')
    if issue_id not in issues_dict:
      element = config_dom.createElement('issue')
      element.attributes['id'] = issue_id
      element.attributes['severity'] = severity
      config_dom.documentElement.appendChild(element)
      issue = _Issue(element)
      issues_dict[issue_id] = issue
    else:
      issue = issues_dict[issue_id]
      if issue.dom_element.getAttribute('severity') == 'ignore':
        continue

    if regexp not in issue.regexps:
      issue.regexps.add(regexp)
      ignore_element = config_dom.createElement('ignore')
      ignore_element.attributes['regexp'] = regexp
      issue.dom_element.appendChild(config_dom.createComment(_INSERT_COMMENT))
      issue.dom_element.appendChild(ignore_element)

    for issue_id, issue in issues_dict.iteritems():
      if issue.dom_element.getAttribute('severity') == 'ignore':
        print('Warning: [%s] is suppressed globally.' % issue_id)

  # toprettyxml inserts whitespace, so delete whitespace first.
  _TrimWhitespaceNodes(config_dom.documentElement)

  with open(config_path, 'w') as f:
    f.write(config_dom.toprettyxml(indent='  ', encoding='utf-8'))
  print('Updated %s' % config_path)


def _Suppress(config_path, result_path):
  _ParseAndInsertNewSuppressions(result_path, config_path)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--config',
                      help='Path to suppression.xml config file',
                      default=_DEFAULT_CONFIG_PATH)
  parser.add_argument('result_path',
                      help='Lint results xml file',
                      metavar='RESULT_FILE')
  args = parser.parse_args()

  _Suppress(args.config, args.result_path)


if __name__ == '__main__':
  main()
