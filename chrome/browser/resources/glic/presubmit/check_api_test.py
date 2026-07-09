# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import sys
import subprocess
import tempfile

DEBUG = False
SCRIPT_PATH = os.path.dirname(os.path.abspath(__file__))
ROOT_PATH = os.path.join(SCRIPT_PATH, '../../../../../')
TESTS_PATH = os.path.join(SCRIPT_PATH, 'tests')


def ApplyEdits(text, name):
    edit_re = re.compile(r'// (\w+):(edit-[a-z-]+):(.*)')
    lines = text.splitlines()
    removing_lines = 0
    adding_lines = False

    for i, line in enumerate(lines):
        match = edit_re.search(line)
        if match and match.group(1) == name:
            if match.group(2) == 'edit-add-lines':
                adding_lines = True
                new_line = ''
            elif match.group(2) == 'edit-remove-lines':
                removing_lines = int(match.group(3))
                new_line = ''
            lines[i] = new_line
        elif removing_lines > 0:
            lines[i] = ''
            removing_lines -= 1
        elif adding_lines:
            new_line = lines[i].strip()
            if new_line.startswith('//'):
                new_line = new_line[2:]
                lines[i] = new_line
            else:
                adding_lines = False

    return '\n'.join(lines)


def RunTestCase(test_name, old_files, new_files, expected_pass):
    if DEBUG:
        print(f'---- Running test {test_name} ----')

    # Create a unique temporary directory for this test case
    tmp_dir = tempfile.TemporaryDirectory()
    tmp_dir_name = tmp_dir.name

    # Write the new files to the temp directory
    for filename, contents in new_files.items():
        path = os.path.join(tmp_dir_name, filename)
        with open(path, 'w') as f:
            f.write(contents)

    # Serialize the old files map for stdin
    old_contents_json = json.dumps(old_files)

    # Run check_api.py on the entry point file 'check_api_cases.ts'
    entry_point_path = os.path.join(tmp_dir_name, 'check_api_cases.ts')
    args = [
        sys.executable,
        os.path.join(SCRIPT_PATH, 'check_api.py'), '--old-stdin',
        '--api-file-path=' + entry_point_path
    ]
    if DEBUG:
        args.append('--debug')

    result = subprocess.run(args,
                            input=old_contents_json,
                            text=True,
                            capture_output=True)

    if DEBUG:
        print(f'Test {test_name} stdout: {result.stdout}')
        print(f'Test {test_name} stderr: {result.stderr}')

    # Clean up the temp directory
    tmp_dir.cleanup()

    if expected_pass:
        if result.returncode != 0:
            print(f'Test {test_name} failed: {result.stderr}')
            return False
    else:
        if result.returncode == 0:
            print(f'Test {test_name} should have reported' +
                  ' errors, but reported none.')
            return False

    if DEBUG:
        print(f'---- Test Passed {test_name} ----')
    return True


def main():
    # Read the base (unedited) content of all test files.
    base_files = {}
    for filename in os.listdir(TESTS_PATH):
        if filename.endswith('.ts') and not filename.endswith('.d.ts'):
            with open(os.path.join(TESTS_PATH, filename), 'r') as f:
                base_files[filename] = f.read()

    # Find all test names (from the edit comments in any file)
    edit_re = re.compile(r'// (\w+):(edit-[a-z-]+):(.*)')
    test_names = set()
    for contents in base_files.values():
        test_names.update(m[0] for m in edit_re.findall(contents))

    passed_tests = 0
    failed_tests = 0

    for test_name in sorted(test_names):
        # Determine the expected pass status
        if test_name.startswith('Error'):
            expected_pass = False
        elif test_name.startswith('Ok'):
            expected_pass = True
        else:
            print(f'Tests must start with Error or Ok: {test_name}')
            failed_tests += 1
            continue

        # Create the edited versions of all files for this test case
        new_files = {}
        for filename, base_content in base_files.items():
            edited_content = ApplyEdits(base_content, test_name)
            new_files[filename] = edited_content

        # Run the test
        if RunTestCase(test_name, base_files, new_files, expected_pass):
            passed_tests += 1
        else:
            failed_tests += 1

    if failed_tests == 0:
        print(f'All {passed_tests} tests passed!')
    else:
        print(f'{passed_tests} tests passed, {failed_tests} tests failed.')
    return failed_tests == 0


if __name__ == '__main__':
    sys.exit(0 if main() else 1)
