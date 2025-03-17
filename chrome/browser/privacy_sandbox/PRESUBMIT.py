# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for files in chrome/browser/privacy_sandbox.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import xml.etree.ElementTree as ElementTree

PRESUBMIT_VERSION = '2.0.0'


def _FindChildrenWithoutAttribute(element, attribute_name):
    """Returns children of an element that do not have the given attribute.
    Uses a stack to perform a DFS of the element tree from top to bottom in the
    same order as a recursive implementation would do."""
    results = []
    stack = [element]

    while stack:
        current_element = stack.pop()
        if not isinstance(current_element.tag, str):
            continue
        if current_element.get(attribute_name) is None:
            results.append(current_element)

        # current_element is an iterator, so we need to reverse it before
        # iterating over it to maintain the top-down order.
        children = reversed(current_element)
        for child in children:
            stack.append(child)

    return results


def _SubstitutePrefixes(input_api, attribute):
    """Replaces namespace names in an attribute with its matching prefix."""
    res = input_api.re.sub('{http://schemas.android.com/apk/res/android}',
                           'android:', attribute)
    res = input_api.re.sub('{http://schemas.android.com/apk/res-auto}', 'app:',
                           res)
    return res


def _FormatElement(input_api, element):
    res = ['<', element.tag]
    for k, v in element.attrib.items():
        key = _SubstitutePrefixes(input_api, k)
        value = _SubstitutePrefixes(input_api, v)
        res.append(f'\n\t{key}="{value}"')
    res.append('>' if len(element) > 0 else '/>')
    return ''.join(res)


def CheckPrivacySandboxXmlElementsHaveResourceIds(input_api, output_api):
    """Makes sure developers add resource-ids (android:id=...) to all element
    tags in Android XML files (see go/ps-android-xml-presubmit)."""
    problems = []

    def FileFilter(affected_file):
        """Check only files in a specific directory."""
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[(
                r'^chrome\/browser\/privacy_sandbox\/android\/java\/res\/layout'
                r'\/.*.xml')])

    for affected_file in input_api.AffectedFiles(include_deletes=False,
                                                 file_filter=FileFilter):
        # TODO(crbug.com/394838842): Consider using ChangedContent instead of
        #  NewContents (or a mixture of both) to only flag changed XML elements
        parser = ElementTree.XMLParser(
            target=ElementTree.TreeBuilder(insert_comments=True))

        affected_file_lines = affected_file.NewContents()
        affected_file_string = "\n".join(affected_file_lines)

        tree = ElementTree.fromstring(affected_file_string, parser)

        # The XML library expands the namespace to the full URI usually defined
        # in the root element of the XML file, so we need to match that. In this
        # case, the namespace is "android" and the URI is
        # "http://schemas.android.com/apk/res/android".
        ANDROID_ID_ATTRIBUTE = '{http://schemas.android.com/apk/res/android}id'

        bad_elements = _FindChildrenWithoutAttribute(tree,
                                                     ANDROID_ID_ATTRIBUTE)
        if len(bad_elements) > 0:
            problems.append(f'{"*" * 100}')
            problems.append(f'{affected_file.LocalPath()}')
            problems.extend([
                _FormatElement(input_api, element) for element in bad_elements
            ])

    if not problems:
        return []

    WARNING_MSG = ('Ensure all Android XML Elements have the "android:id" '
                   'attribute in the following .xml files:')
    return [output_api.PresubmitPromptWarning(WARNING_MSG, items=problems)]
