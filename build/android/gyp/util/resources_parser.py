# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os
import re
from xml.etree import ElementTree

from util import build_utils
from util import resource_utils

_TextSymbolEntry = collections.namedtuple(
    'RTextEntry', ('java_type', 'resource_type', 'name', 'value'))

_DUMMY_RTXT_ID = '0x7f010001'
_DUMMY_RTXT_INDEX = '1'


def _ResourceNameToJavaSymbol(resource_name):
  return re.sub('[\.:]', '_', resource_name)


class RTxtGenerator(object):
  def __init__(self,
               res_dirs,
               ignore_pattern=resource_utils.AAPT_IGNORE_PATTERN):
    self.res_dirs = res_dirs
    self.ignore_pattern = ignore_pattern

  def _ParseDeclareStyleable(self, node):
    ret = set()
    stylable_name = _ResourceNameToJavaSymbol(node.attrib['name'])
    ret.add(
        _TextSymbolEntry('int[]', 'styleable', stylable_name,
                         '{{{}}}'.format(_DUMMY_RTXT_ID)))
    for child in node:
      if child.tag == 'eat-comment':
        continue
      if child.tag != 'attr':
        # This parser expects everything inside <declare-stylable/> to be either
        # an attr or an eat-comment. If new resource xml files are added that do
        # not conform to this, this parser needs updating.
        raise Exception('Unexpected tag {} inside <delcare-stylable/>'.format(
            child.tag))
      entry_name = '{}_{}'.format(
          stylable_name, _ResourceNameToJavaSymbol(child.attrib['name']))
      ret.add(
          _TextSymbolEntry('int', 'styleable', entry_name, _DUMMY_RTXT_INDEX))
      if not child.attrib['name'].startswith('android:'):
        resource_name = _ResourceNameToJavaSymbol(child.attrib['name'])
        ret.add(_TextSymbolEntry('int', 'attr', resource_name, _DUMMY_RTXT_ID))
      for entry in child:
        if entry.tag not in ('enum', 'flag'):
          # This parser expects everything inside <attr/> to be either an
          # <enum/> or an <flag/>. If new resource xml files are added that do
          # not conform to this, this parser needs updating.
          raise Exception('Unexpected tag {} inside <attr/>'.format(entry.tag))
        resource_name = _ResourceNameToJavaSymbol(entry.attrib['name'])
        ret.add(_TextSymbolEntry('int', 'id', resource_name, _DUMMY_RTXT_ID))
    return ret

  def _ExtractNewIdsFromNode(self, node):
    ret = set()
    # Sometimes there are @+id/ in random attributes (not just in android:id)
    # and apparently that is valid. See:
    # https://developer.android.com/reference/android/widget/RelativeLayout.LayoutParams.html
    for value in node.attrib.values():
      if value.startswith('@+id/'):
        resource_name = value[5:]
        ret.add(_TextSymbolEntry('int', 'id', resource_name, _DUMMY_RTXT_ID))
    for child in node:
      ret.update(self._ExtractNewIdsFromNode(child))
    return ret

  def _ExtractNewIdsFromXml(self, xml_path):
    root = ElementTree.parse(xml_path).getroot()
    return self._ExtractNewIdsFromNode(root)

  def _ParseValuesXml(self, xml_path):
    ret = set()
    root = ElementTree.parse(xml_path).getroot()
    assert root.tag == 'resources'
    for child in root:
      if child.tag == 'eat-comment':
        # eat-comment is just a dummy documentation element.
        continue
      if child.tag == 'skip':
        # skip is just a dummy element.
        continue
      if child.tag == 'declare-styleable':
        ret.update(self._ParseDeclareStyleable(child))
      else:
        if child.tag == 'item':
          resource_type = child.attrib['type']
        elif child.tag in ('array', 'integer-array', 'string-array'):
          resource_type = 'array'
        else:
          resource_type = child.tag
        name = _ResourceNameToJavaSymbol(child.attrib['name'])
        ret.add(_TextSymbolEntry('int', resource_type, name, _DUMMY_RTXT_ID))
    return ret

  def _CollectResourcesListFromDirectory(self, res_dir):
    ret = set()
    globs = resource_utils._GenerateGlobs(self.ignore_pattern)
    for root, _, files in os.walk(res_dir):
      resource_type = os.path.basename(root)
      if '-' in resource_type:
        resource_type = resource_type[:resource_type.index('-')]
      for f in files:
        if build_utils.MatchesGlob(f, globs):
          continue
        if resource_type == 'values':
          ret.update(self._ParseValuesXml(os.path.join(root, f)))
        else:
          if '.' in f:
            resource_name = f[:f.index('.')]
          else:
            resource_name = f
          ret.add(
              _TextSymbolEntry('int', resource_type, resource_name,
                               _DUMMY_RTXT_ID))
          # Other types not just layouts can contain new ids (eg: Menus and
          # Drawables). Just in case, look for new ids in all files.
          if f.endswith('.xml'):
            ret.update(self._ExtractNewIdsFromXml(os.path.join(root, f)))
    return ret

  def _CollectResourcesListFromDirectories(self):
    ret = set()
    for res_dir in self.res_dirs:
      ret.update(self._CollectResourcesListFromDirectory(res_dir))
    return ret

  def WriteRTxtFile(self, rtxt_path):
    resources = self._CollectResourcesListFromDirectories()
    with build_utils.AtomicOutput(rtxt_path, mode='w') as f:
      for resource in resources:
        line = '{0.java_type} {0.resource_type} {0.name} {0.value}\n'.format(
            resource)
        f.write(line)
