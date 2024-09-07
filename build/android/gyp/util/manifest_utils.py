# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains common helpers for working with Android manifests."""

import hashlib
import os
import re
import shlex
import sys
import xml.dom.minidom as minidom
from xml.etree import ElementTree

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.

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


def NamespacedGet(node, key):
  return node.get('{%s}%s' % (ANDROID_NAMESPACE, key))


def NamespacedSet(node, key, value):
  node.set('{%s}%s' % (ANDROID_NAMESPACE, key), value)


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
  assert manifest_node is not None, 'Manifest is none for path ' + path

  app_node = doc.find('application')
  if app_node is None:
    app_node = ElementTree.SubElement(manifest_node, 'application')

  return doc, manifest_node, app_node


def SaveManifest(doc, path):
  with action_helpers.atomic_output(path) as f:
    f.write(ElementTree.tostring(doc.getroot(), encoding='UTF-8'))


def GetPackage(manifest_node):
  return manifest_node.get('package')


def SetUsesSdk(manifest_node,
               target_sdk_version,
               min_sdk_version,
               max_sdk_version=None):
  uses_sdk_node = manifest_node.find('./uses-sdk')
  if uses_sdk_node is None:
    uses_sdk_node = ElementTree.SubElement(manifest_node, 'uses-sdk')
  NamespacedSet(uses_sdk_node, 'targetSdkVersion', target_sdk_version)
  NamespacedSet(uses_sdk_node, 'minSdkVersion', min_sdk_version)
  if max_sdk_version:
    NamespacedSet(uses_sdk_node, 'maxSdkVersion', max_sdk_version)


def SetTargetApiIfUnset(manifest_node, target_sdk_version):
  uses_sdk_node = manifest_node.find('./uses-sdk')
  if uses_sdk_node is None:
    uses_sdk_node = ElementTree.SubElement(manifest_node, 'uses-sdk')
  curr_target_sdk_version = NamespacedGet(uses_sdk_node, 'targetSdkVersion')
  if curr_target_sdk_version is None:
    NamespacedSet(uses_sdk_node, 'targetSdkVersion', target_sdk_version)
  return curr_target_sdk_version is None


def OverrideMinSdkVersionIfPresent(manifest_node, min_sdk_version):
  uses_sdk_node = manifest_node.find('./uses-sdk')
  if uses_sdk_node is not None:
    NamespacedSet(uses_sdk_node, 'minSdkVersion', min_sdk_version)


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
    if not tag_closed and 'android:name="' in l:
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
  raise RuntimeError('Did not find end of tag:\n%s' % '\n'.join(lines))


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


def NormalizeManifest(manifest_contents, version_code_offset,
                      library_version_offset):
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

    version_code = NamespacedGet(root, 'versionCode')
    if version_code and version_code_offset:
      version_code = int(version_code) - int(version_code_offset)
      NamespacedSet(root, 'versionCode', f'OFFSET={version_code}')
    version_name = NamespacedGet(root, 'versionName')
    if version_name:
      version_name = re.sub(r'\d+', '#', version_name)
      NamespacedSet(root, 'versionName', version_name)

    # Trichrome's static library version number is updated daily. To avoid
    # frequent manifest check failures, we remove the exact version number
    # during normalization.
    for node in app_node:
      if node.tag in ['uses-static-library', 'static-library']:
        version = NamespacedGet(node, 'version')
        if version and library_version_offset:
          version = int(version) - int(library_version_offset)
          NamespacedSet(node, 'version', f'OFFSET={version}')

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
