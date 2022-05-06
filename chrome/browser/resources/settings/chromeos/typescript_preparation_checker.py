# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CheckNoAddSingletonGetter(file):
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


def _CheckNoLegacyPolymerSyntax(file):
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


class TypescriptPreparationChecker(object):
    """Checks that the changes are in line with the upcoming TypeScript
    migrationfor ChromeOS Settings.

    Checks:
      - addSingletonGetter() is not used
    """

    @staticmethod
    def RunChecks(input_api, output_api):
        """Runs checks for compatibility with the upcoming TypeScript migration

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
            error_messages += _CheckNoAddSingletonGetter(file)
            error_messages += _CheckNoLegacyPolymerSyntax(file)

        errors = list(map(output_api.PresubmitPromptWarning, error_messages))
        return errors
