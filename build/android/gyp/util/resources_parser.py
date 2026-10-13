# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os
from xml.etree import ElementTree
import zipfile

from util import build_utils
from util import resource_utils
import action_helpers  # build_utils adds //build to sys.path.

_TextSymbolEntry = collections.namedtuple(
    'RTextEntry', ('java_type', 'resource_type', 'name', 'value')
)

_DUMMY_RTXT_ID = '0x7f010001'
_DUMMY_RTXT_INDEX = '1'


def _ResourceNameToJavaSymbol(resource_name):
    return resource_name.replace('.', '_').replace(':', '_')


class RTxtGenerator:
    def __init__(
        self,
        res_dirs=(),
        ignore_pattern=resource_utils.AAPT_IGNORE_PATTERN,
        res_zips=(),
    ):
        self.res_dirs = res_dirs
        self.res_zips = res_zips
        self.ignore_globs = resource_utils.GenerateGlobs(ignore_pattern)

    def _ParseDeclareStyleable(self, node):
        ret = set()
        stylable_name = _ResourceNameToJavaSymbol(node.attrib['name'])
        ret.add(
            _TextSymbolEntry(
                'int[]',
                'styleable',
                stylable_name,
                '{{{}}}'.format(_DUMMY_RTXT_ID),
            )
        )
        for child in node:
            if child.tag == 'eat-comment':
                continue
            if child.tag != 'attr':
                # This parser expects everything inside <declare-stylable/> to be either
                # an attr or an eat-comment. If new resource xml files are added that do
                # not conform to this, this parser needs updating.
                raise Exception(
                    'Unexpected tag {} inside <delcare-stylable/>'.format(
                        child.tag
                    )
                )
            entry_name = '{}_{}'.format(
                stylable_name, _ResourceNameToJavaSymbol(child.attrib['name'])
            )
            ret.add(
                _TextSymbolEntry(
                    'int', 'styleable', entry_name, _DUMMY_RTXT_INDEX
                )
            )
            if not child.attrib['name'].startswith('android:'):
                resource_name = _ResourceNameToJavaSymbol(child.attrib['name'])
                ret.add(
                    _TextSymbolEntry(
                        'int', 'attr', resource_name, _DUMMY_RTXT_ID
                    )
                )
            for entry in child:
                if entry.tag not in ('enum', 'flag'):
                    # This parser expects everything inside <attr/> to be either an
                    # <enum/> or an <flag/>. If new resource xml files are added that do
                    # not conform to this, this parser needs updating.
                    raise Exception(
                        'Unexpected tag {} inside <attr/>'.format(entry.tag)
                    )
                resource_name = _ResourceNameToJavaSymbol(entry.attrib['name'])
                ret.add(
                    _TextSymbolEntry('int', 'id', resource_name, _DUMMY_RTXT_ID)
                )
        return ret

    def _ExtractNewIdsFromNode(self, node):
        ret = set()
        # Sometimes there are @+id/ in random attributes (not just in android:id)
        # and apparently that is valid. See:
        # https://developer.android.com/reference/android/widget/RelativeLayout.LayoutParams.html
        for value in node.attrib.values():
            if value.startswith('@+id/'):
                resource_name = value[5:]
                ret.add(
                    _TextSymbolEntry('int', 'id', resource_name, _DUMMY_RTXT_ID)
                )
        for child in node:
            ret.update(self._ExtractNewIdsFromNode(child))
        return ret

    def _ParseXml(self, xml_file):
        try:
            return ElementTree.parse(xml_file).getroot()
        except Exception as e:
            raise RuntimeError(f'Failure parsing {xml_file}:\n') from e

    def _ExtractNewIdsFromXml(self, xml_file):
        return self._ExtractNewIdsFromNode(self._ParseXml(xml_file))

    def _ParseValuesXml(self, xml_file):
        ret = set()
        root = self._ParseXml(xml_file)

        assert root.tag == 'resources'
        for child in root:
            if child.tag in ('eat-comment', 'skip', 'overlayable', 'macro'):
                # These tags do not create real resources
                continue
            if child.tag == 'declare-styleable':
                ret.update(self._ParseDeclareStyleable(child))
            else:
                if child.tag in ('item', 'public'):
                    resource_type = child.attrib['type']
                elif child.tag in ('array', 'integer-array', 'string-array'):
                    resource_type = 'array'
                else:
                    resource_type = child.tag
                assert resource_type in resource_utils.ALL_RESOURCE_TYPES, (
                    f'Infered resource type ({resource_type}) from xml entry ('
                    f'{ElementTree.tostring(child, encoding="unicode").strip()}'
                    f') (found in {xml_file}) is not listed in '
                    'resource_utils.ALL_RESOURCE_TYPES. Teach '
                    'resources_parser.py how to parse this entry and then add '
                    'to the list.'
                )
                name = _ResourceNameToJavaSymbol(child.attrib['name'])
                ret.add(
                    _TextSymbolEntry('int', resource_type, name, _DUMMY_RTXT_ID)
                )
        return ret

    def _CollectResourcesFromEntry(self, subdir, filename, entry_path, open_fn):
        if build_utils.MatchesGlob(filename, self.ignore_globs):
            return ()
        resource_type = os.path.basename(subdir)
        if '-' in resource_type:
            if (
                resource_type.startswith('values-')
                and resource_type != 'values-mdpi'
            ):
                # Skip non-base values-* directories (such as translation
                # strings.xml files), since all resources should also be
                # declared in the base values/ (or values-mdpi/) directory.
                # Compile or TraceReferences build steps will fail if this
                # assumption does not hold (and we miss a resource).
                return ()
            resource_type = resource_type[: resource_type.index('-')]
        if resource_type == 'values':
            with open_fn(entry_path) as xml_file:
                return self._ParseValuesXml(xml_file)
        ret = set()
        if '.' in filename:
            resource_name = filename[: filename.index('.')]
        else:
            resource_name = filename
        ret.add(
            _TextSymbolEntry(
                'int', resource_type, resource_name, _DUMMY_RTXT_ID
            )
        )
        # Other types not just layouts can contain new ids (eg: Menus and
        # Drawables). Just in case, look for new ids in all files.
        if filename.endswith('.xml'):
            with open_fn(entry_path) as xml_file:
                ret.update(self._ExtractNewIdsFromXml(xml_file))
        return ret

    def _CollectResourcesListFromDirectory(self, res_dir):
        ret = set()
        open_fn = lambda p: open(p, 'rb')  # noqa: E731
        for root, _, files in os.walk(res_dir):
            for f in files:
                path = os.path.join(root, f)
                ret.update(
                    self._CollectResourcesFromEntry(root, f, path, open_fn)
                )
        return ret

    def _CollectResourcesListFromZip(self, res_zip):
        ret = set()
        with zipfile.ZipFile(res_zip) as z:
            for name in z.namelist():
                if name.endswith('/'):
                    continue
                root, f = os.path.split(name)
                ret.update(
                    self._CollectResourcesFromEntry(root, f, name, z.open)
                )
        return ret

    def _CollectResourcesList(self):
        ret = set()
        for res_dir in self.res_dirs:
            ret.update(self._CollectResourcesListFromDirectory(res_dir))
        for res_zip in self.res_zips:
            ret.update(self._CollectResourcesListFromZip(res_zip))
        return sorted(ret)

    def WriteRTxtFile(self, rtxt_path):
        resources = self._CollectResourcesList()
        with action_helpers.atomic_output(rtxt_path, mode='w') as f:
            for resource in resources:
                line = '{0.java_type} {0.resource_type} {0.name} {0.value}\n'.format(
                    resource
                )
                f.write(line)
