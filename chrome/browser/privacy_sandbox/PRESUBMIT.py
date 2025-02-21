# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for files in chrome/browser/privacy_sandbox.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

def _GetXmlElementName(input_api, element):
    """Extracts the name of an XML element string."""
    element_name = input_api.re.search(
        r"""
        ^<              # Matches the opening < of an XML element
        ([^\/> \n]*)    # Group that matches any char except "/, >, space or \n>
        """, element, input_api.re.VERBOSE).group(1)
    if not element_name:
        return "Unknown Element"

    return element_name

def _GetLineOfElement(input_api, element, file_content):
    """Determines the line number of an XML element within file content.
    Limitation: This always returns the first element for duplicate elements
    that lack an "android:id" element.
    """
    match = input_api.re.search(element, file_content)
    if match:
        start_index = match.start()
        element_line_number = file_content.count('\n', 0, start_index) + 1
        return element_line_number
    return -1 # Element not found, this shouldn't be hit.

def CheckPrivacySandboxXmlElementsHaveResourceIds(input_api, output_api):
    """Makes sure developers add resource-ids (android:id=...) to all element
    tags in Android XML files (see go/ps-android-xml-presubmit).
    """
    problems = []
    BAD_PATTERN = input_api.re.compile(
        r"""
        (?m)                    # Enables multi-line mode
        <                       # Matches the opening < of an XML element.
        (?!\?xml|\!|/)          # Excludes prolog, comments, close tags.
        (?:                     # Non capturing (NC) group for conditions
        (?!android\:id)         # Ensure android:id isn't an attribute
        .                       # Match any character except newlines
        |\n(?!\n)               # OR match newlines but not consecutive ones
        )+                      # Ensure preceding group repeats 1 or more times
        >                       # Matches the closing > of the XML element.
        """, input_api.re.VERBOSE)

    def FileFilter(affected_file):
        """Check only files in a specific directory."""
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[(
                r'^chrome\/browser\/privacy_sandbox\/android\/java\/res\/layout'
                r'\/.*.xml'
                )])

    for affected_file in input_api.AffectedFiles(include_deletes=False,
                                    file_filter=FileFilter):
        # TODO(crbug.com/394838842): consider  using ChangedContent instead of
        #  NewContents (or a mixture of both) to only flag changed XML elements
        new_file_content = '\n'.join(affected_file.NewContents())

        matches = BAD_PATTERN.findall(new_file_content)
        if len(matches) > 0:
            problems.append(f"{affected_file.LocalPath()}:")
        for match in matches:
            problems.append((
                f"\tElement <{_GetXmlElementName(input_api, match)}> on line "
                f"{_GetLineOfElement(input_api, match, new_file_content)}"
                ))

    if not problems:
        return []

    WARNING_MSG = ("Ensure all Android XML Elements have the \"android:id\" "
                    "attribute in the following .xml files:")
    return [output_api.PresubmitPromptWarning(WARNING_MSG, items=problems)]