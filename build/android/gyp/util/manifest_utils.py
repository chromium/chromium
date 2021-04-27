# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains common helpers for working with Android manifests."""

import hashlib
import os
import re
import shlex
import sys
import xml.dom.minidom as minidom

from util import build_utils
from xml.etree import ElementTree

ANDROID_NAMESPACE = 'http://schemas.android.com/apk/res/android'
TOOLS_NAMESPACE = 'http://schemas.android.com/tools'
DIST_NAMESPACE = 'http://schemas.android.com/apk/distribution'
EMPTY_ANDROID_MANIFEST_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', 'AndroidManifest.xml'))
# When normalizing for expectation matching, wrap these tags when they are long
# or else they become very hard to read.
_WRAP_CANDIDATES = (
    '<manifest',
    '<application',
    '<activity',
    '<provider',
    '<receiver',
    '<service',
)
# Don't wrap lines shorter than this.
_WRAP_LINE_LENGTH = 100

_xml_namespace_initialized = False


def _RegisterElementTreeNamespaces():
  global _xml_namespace_initialized
  if _xml_namespace_initialized:
    return
  _xml_namespace_initialized = True
  ElementTree.register_namespace('android', ANDROID_NAMESPACE)
  ElementTree.register_namespace('tools', TOOLS_NAMESPACE)
  ElementTree.register_namespace('dist', DIST_NAMESPACE)


def ParseManifest(path):
  """Parses an AndroidManifest.xml using ElementTree.

  Registers required namespaces, creates application node if missing, adds any
  missing namespaces for 'android', 'tools' and 'dist'.

  Returns tuple of:
    doc: Root xml document.
    manifest_node: the <manifest> node.
    app_node: the <application> node.
  """
  _RegisterElementTreeNamespaces()
  doc = ElementTree.parse(path)
  # ElementTree.find does not work if the required tag is the root.
  if doc.getroot().tag == 'manifest':
    manifest_node = doc.getroot()
  else:
    manifest_node = doc.find('manifest')

  app_node = doc.find('application')
  if app_node is None:
    app_node = ElementTree.SubElement(manifest_node, 'application')

  return doc, manifest_node, app_node


def SaveManifest(doc, path):
  with build_utils.AtomicOutput(path) as f:
    f.write(ElementTree.tostring(doc.getroot(), encoding='UTF-8'))


def GetPackage(manifest_node):
  return manifest_node.get('package')


def AssertUsesSdk(manifest_node,
                  min_sdk_version=None,
                  target_sdk_version=None,
                  max_sdk_version=None,
                  fail_if_not_exist=False):
  """Asserts values of attributes of <uses-sdk> element.

  Unless |fail_if_not_exist| is true, will only assert if both the passed value
  is not None and the value of attribute exist. If |fail_if_not_exist| is true
  will fail if passed value is not None but attribute does not exist.
  """
  uses_sdk_node = manifest_node.find('./uses-sdk')
  if uses_sdk_node is None:
    return
  for prefix, sdk_version in (('min', min_sdk_version), ('target',
                                                         target_sdk_version),
                              ('max', max_sdk_version)):
    value = uses_sdk_node.get('{%s}%sSdkVersion' % (ANDROID_NAMESPACE, prefix))
    if fail_if_not_exist and not value and sdk_version:
      assert False, (
          '%sSdkVersion in Android manifest does not exist but we expect %s' %
          (prefix, sdk_version))
    if not value or not sdk_version:
      continue
    assert value == sdk_version, (
        '%sSdkVersion in Android manifest is %s but we expect %s' %
        (prefix, value, sdk_version))


def AssertPackage(manifest_node, package):
  """Asserts that manifest package has desired value.

  Will only assert if both |package| is not None and the package is set in the
  manifest.
  """
  package_value = GetPackage(manifest_node)
  if package_value is None or package is None:
    return
  assert package_value == package, (
      'Package in Android manifest is %s but we expect %s' % (package_value,
                                                              package))


def _SortAndStripElementTree(root):
  # Sort alphabetically with two exceptions:
  # 1) Put <application> node last (since it's giant).
  # 2) Put android:name before other attributes.
  def element_sort_key(node):
    if node.tag == 'application':
      return 'z'
    ret = ElementTree.tostring(node)
    # ElementTree.tostring inserts namespace attributes for any that are needed
    # for the node or any of its descendants. Remove them so as to prevent a
    # change to a child that adds/removes a namespace usage from changing sort
    # order.
    return re.sub(r' xmlns:.*?".*?"', '', ret.decode('utf8'))

  name_attr = '{%s}name' % ANDROID_NAMESPACE

  def attribute_sort_key(tup):
    return ('', '') if tup[0] == name_attr else tup

  def helper(node):
    for child in node:
      if child.text and child.text.isspace():
        child.text = None
      helper(child)

    # Sort attributes (requires Python 3.8+).
    node.attrib = dict(sorted(node.attrib.items(), key=attribute_sort_key))

    # Sort nodes
    node[:] = sorted(node, key=element_sort_key)

  helper(root)


def _SplitElement(line):
  """Parses a one-line xml node into ('<tag', ['a="b"', ...]], '/>')."""

  # Shlex splits nicely, but removes quotes. Need to put them back.
  def restore_quotes(value):
    return value.replace('=', '="', 1) + '"'

  # Simplify restore_quotes by separating />.
  assert line.endswith('>'), line
  end_tag = '>'
  if line.endswith('/>'):
    end_tag = '/>'
  line = line[:-len(end_tag)]

  # Use shlex to avoid having to re-encode &quot;, etc.
  parts = shlex.split(line)
  start_tag = parts[0]
  attrs = parts[1:]

  return start_tag, [restore_quotes(x) for x in attrs], end_tag


def _CreateNodeHash(lines):
  """Computes a hash (md5) for the first XML node found in |lines|.

  Args:
    lines: List of strings containing pretty-printed XML.

  Returns:
    Positive 32-bit integer hash of the node (including children).
  """
  target_indent = lines[0].find('<')
  tag_closed = False
  for i, l in enumerate(lines[1:]):
    cur_indent = l.find('<')
    if cur_indent != -1 and cur_indent <= target_indent:
      tag_lines = lines[:i + 1]
      break
    elif not tag_closed and 'android:name="' in l:
      # To reduce noise of node tags changing, use android:name as the
      # basis the hash since they usually unique.
      tag_lines = [l]
      break
    tag_closed = tag_closed or '>' in l
  else:
    assert False, 'Did not find end of node:\n' + '\n'.join(lines)

  # Insecure and truncated hash as it only needs to be unique vs. its neighbors.
  return hashlib.md5(('\n'.join(tag_lines)).encode('utf8')).hexdigest()[:8]


def _IsSelfClosing(lines):
  """Given pretty-printed xml, returns whether first node is self-closing."""
  for l in lines:
    idx = l.find('>')
    if idx != -1:
      return l[idx - 1] == '/'
  assert False, 'Did not find end of tag:\n' + '\n'.join(lines)


def _AddDiffTags(lines):
  # When multiple identical tags appear sequentially, XML diffs can look like:
  # +  </tag>
  # +  <tag>
  # rather than:
  # +  <tag>
  # +  </tag>
  # To reduce confusion, add hashes to tags.
  # This also ensures changed tags show up with outer <tag> elements rather than
  # showing only changed attributes.
  hash_stack = []
  for i, l in enumerate(lines):
    stripped = l.lstrip()
    # Ignore non-indented tags and lines that are not the start/end of a node.
    if l[0] != ' ' or stripped[0] != '<':
      continue
    # Ignore self-closing nodes that fit on one line.
    if l[-2:] == '/>':
      continue
    # Ignore <application> since diff tag changes with basically any change.
    if stripped.lstrip('</').startswith('application'):
      continue

    # Check for the closing tag (</foo>).
    if stripped[1] != '/':
      cur_hash = _CreateNodeHash(lines[i:])
      if not _IsSelfClosing(lines[i:]):
        hash_stack.append(cur_hash)
    else:
      cur_hash = hash_stack.pop()
    lines[i] += '  # DIFF-ANCHOR: {}'.format(cur_hash)
  assert not hash_stack, 'hash_stack was not empty:\n' + '\n'.join(hash_stack)


def NormalizeManifest(manifest_contents):
  _RegisterElementTreeNamespaces()
  # This also strips comments and sorts node attributes alphabetically.
  root = ElementTree.fromstring(manifest_contents)
  package = GetPackage(root)

  app_node = root.find('application')
  if app_node is not None:
    # android:debuggable is added when !is_official_build. Strip it out to avoid
    # expectation diffs caused by not adding is_official_build. Play store
    # blocks uploading apps with it set, so there's no risk of it slipping in.
    debuggable_name = '{%s}debuggable' % ANDROID_NAMESPACE
    if debuggable_name in app_node.attrib:
      del app_node.attrib[debuggable_name]

    # Trichrome's static library version number is updated daily. To avoid
    # frequent manifest check failures, we remove the exact version number
    # during normalization.
    for node in app_node:
      if (node.tag in ['uses-static-library', 'static-library']
          and '{%s}version' % ANDROID_NAMESPACE in node.keys()
          and '{%s}name' % ANDROID_NAMESPACE in node.keys()):
        node.set('{%s}version' % ANDROID_NAMESPACE, '$VERSION_NUMBER')

  # We also remove the exact package name (except the one at the root level)
  # to avoid noise during manifest comparison.
  def blur_package_name(node):
    for key in node.keys():
      node.set(key, node.get(key).replace(package, '$PACKAGE'))

    for child in node:
      blur_package_name(child)

  # We only blur the package names of non-root nodes because they generate a lot
  # of diffs when doing manifest checks for upstream targets. We still want to
  # have 1 piece of package name not blurred just in case the package name is
  # mistakenly changed.
  for child in root:
    blur_package_name(child)

  _SortAndStripElementTree(root)

  # Fix up whitespace/indentation.
  dom = minidom.parseString(ElementTree.tostring(root))
  out_lines = []
  for l in dom.toprettyxml(indent='  ').splitlines():
    if not l or l.isspace():
      continue
    if len(l) > _WRAP_LINE_LENGTH and any(x in l for x in _WRAP_CANDIDATES):
      indent = ' ' * l.find('<')
      start_tag, attrs, end_tag = _SplitElement(l)
      out_lines.append('{}{}'.format(indent, start_tag))
      for attribute in attrs:
        out_lines.append('{}    {}'.format(indent, attribute))
      out_lines[-1] += '>'
      # Heuristic: Do not allow multi-line tags to be self-closing since these
      # can generally be allowed to have nested elements. When diffing, it adds
      # noise if the base file is self-closing and the non-base file is not
      # self-closing.
      if end_tag == '/>':
        out_lines.append('{}{}>'.format(indent, start_tag.replace('<', '</')))
    else:
      out_lines.append(l)

  # Make output more diff-friendly.
  _AddDiffTags(out_lines)

  return '\n'.join(out_lines) + '\n'
