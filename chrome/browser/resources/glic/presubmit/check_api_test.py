# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import subprocess
import tempfile

DEBUG = False
SCRIPT_PATH = os.path.dirname(os.path.abspath(__file__))
ROOT_PATH = os.path.join(SCRIPT_PATH, '../../../../../')
TESTS_PATH = os.path.join(SCRIPT_PATH, 'tests')


# Parses edit declarations, which are of the form
# // <test_name>:edit-remove-lines:
# // <test_name>:edit-add-lines: <number>
# and returns a list of tuples of (test_name, modified_text).
def CreateTestEdits(test_file):
    edit_re = re.compile(r'// (\w+):(edit-[a-z-]+):(.*)')
    with open(os.path.join(TESTS_PATH, test_file), 'r') as f:
        text = f.read()
    test_names = sorted(list(set(m[0] for m in edit_re.findall(text))))
    for name in test_names:
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
        yield name, '\n'.join(lines)


def DoTest(test_name, original_path, test_file, expected_pass):
    if DEBUG:
        print(f'---- Running test {test_name} ----')
    args = [
        sys.executable,
        os.path.join(SCRIPT_PATH, 'check_api.py'), '--old-stdin',
        '--api-file-path=' + test_file
    ]
    if DEBUG:
        args.append('--debug')
    result = subprocess.run(args,
                            input=open(original_path).read(),
                            text=True,
                            capture_output=True)
    if DEBUG:
        print(f'Test {test_file} stdout: {result.stdout}')
        print(f'Test {test_file} stderr: {result.stderr}')
    if expected_pass:
        if result.returncode != 0:
            print(f'Test {test_file} failed: {result.stderr}')
            if DEBUG:
                sys.exit(1)
            return False
    else:
        if result.returncode == 0:
            print(f'Test {test_file} should have reported errors,',
                  'but reported none.')
            if DEBUG:
                sys.exit(1)
            return False
    if DEBUG:
        print(f'---- Test Passesd {test_name} ----')
    return True


def main():
    passed_tests = 0
    failed_tests = 0
    for file in os.listdir(TESTS_PATH):
        if not file.endswith('.ts'):
            continue
        test_file_path = os.path.join(TESTS_PATH, file)
        for test_name, test_text in CreateTestEdits(test_file_path):
            # Note: check_api.py needs to open the file, so we must
            # close it before calling DoTest.
            f = tempfile.NamedTemporaryFile(mode='w',
                                            prefix=test_name,
                                            suffix='.ts',
                                            delete=False)
            try:
                f.write(test_text)
                f.close()
                if test_name.startswith('Error'):
                    expected_pass = False
                elif test_name.startswith('Ok'):
                    expected_pass = True
                else:
                    print(f'Tests must start with Error or Ok: {test_name}')
                    failed_tests += 1
                    continue
                if DoTest(test_name, test_file_path, f.name, expected_pass):
                    passed_tests += 1
                else:
                    failed_tests += 1
            finally:
                if not DEBUG:
                    os.remove(f.name)

    if failed_tests == 0:
        print(f'All {passed_tests} tests passed!')
    else:
        print(f'{passed_tests} tests passed, {failed_tests} tests failed.')
    return failed_tests == 0


if __name__ == '__main__':
    sys.exit(0 if main() else 1)
