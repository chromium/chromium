# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import subprocess


def _get_changed_files():
    """Returns the file to status for the current branch"""
    result = subprocess.run(['git', 'status', '--short'],
                            capture_output=True,
                            text=True)
    file_statuses = {
        line[line.index(' ') + 1:].strip(): line[:line.index(' ')]
        for line in result.stdout.strip().split('\n') if line
    }
    return file_statuses


def check_files_changed(output: str, context):
    """Checks if specific files have been changed and uncommitted."""
    files = context.get('config', {}).get('files', {})

    file_statuses = _get_changed_files()
    changed_files = set([
        file for file, status in file_statuses.items()
        if status in ['M', 'T', 'R', 'U']
    ])
    print(f'changed_files: {changed_files}')
    return all(file in changed_files for file in files)


def check_files_added(output: str, context):
    """Checks if specific files have been added and uncommitted."""
    files = context.get('config', {}).get('files', {})

    file_statuses = _get_changed_files()
    added_files = set([
        file for file, status in file_statuses.items()
        if status in ['A', '??']
    ])
    print(f'added_files: {added_files}')
    return all(file in added_files for file in files)
