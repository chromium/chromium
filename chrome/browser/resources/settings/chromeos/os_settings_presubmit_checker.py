# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _TypeScriptPrepCheckNoAddSingletonGetter(file):
    """Checks that there are no uses of addSingletonGetter()

    Args:
        file: A changed file

    Returns:
        A list of error messages (strings)
    """

    error_messages = []
    if file.LocalPath().endswith('js'):
        for line_num, line in file.ChangedContents():
            if 'addSingletonGetter' in line:
                error_messages.append(
                    "%s:%d:\n%s\n\n"
                    "Avoid using addSingletonGetter() in ChromeOS Settings app"
                    "due to incompatibility with TypeScript. Refer to "
                    "crrev.com/c/3582712 for an alternate solution." %
                    (file.LocalPath(), line_num, line.strip()))

    return error_messages


def _TypeScriptPrepCheckNoLegacyPolymerSyntax(file):
    """Checks that there are no uses of the legacy Polymer element syntax

    Args:
        file: A changed file

    Returns:
        A list of error messages (strings)
    """

    error_messages = []
    if file.LocalPath().endswith('js'):
        for line_num, line in file.ChangedContents():
            if 'Polymer({' in line:
                error_messages.append(
                    "%s:%d:\n%s\n\n"
                    "Avoid using the legacy Polymer element syntax in "
                    "ChromeOS Settings app due to incompatibility with "
                    "TypeScript. Instead use the class-based syntax documented "
                    "in Polymer 3." %
                    (file.LocalPath(), line_num, line.strip()))

    return error_messages


def _EnforceSchemeSpecificURLs(file):
    """Checks that only scheme-specific URLs are used and scheme relative URLs
      are avoided. (i.e. 'chrome://' is preferred over '//')

    Args:
        file: A changed file

    Returns:
        A list of error messages (strings)
    """

    error_messages = []
    if file.LocalPath().endswith('js'):
        for line_num, line in file.ChangedContents():
            if '\'//' in line:
                error_messages.append(
                    "%s:%d:\n%s\n\n"
                    "Prefer using scheme-specific URLs (i.e. 'chrome://')" %
                    (file.LocalPath(), line_num, line.strip()))

    return error_messages


class OSSettingsPresubmitChecker(object):
    """Checks that the changes comply with ChromeOS Settings code style.

    Checks:
      - addSingletonGetter() is not used
      - No legacy Polymer syntax (1.x, 2.x) is used
    """

    @staticmethod
    def RunChecks(input_api, output_api):
        """Runs checks for ChromeOS Settings

        Args:
            input_api: presubmit.InputApi containing information of the files
            in the change.
            output_api: presubmit.OutputApi used to display the warnings.

        Returns:
            A list of presubmit warnings, each containing the line the violation
            occurred and the warning message.
        """

        error_messages = []
        for file in input_api.AffectedFiles():
            error_messages += _TypeScriptPrepCheckNoAddSingletonGetter(file)
            error_messages += _TypeScriptPrepCheckNoLegacyPolymerSyntax(file)
            error_messages += _EnforceSchemeSpecificURLs(file)

        errors = list(map(output_api.PresubmitPromptWarning, error_messages))
        return errors
