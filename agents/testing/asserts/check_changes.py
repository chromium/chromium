# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Asserts for checking changed files in git."""

import subprocess


def _get_changed_files():
    """Returns the file to status for the current branch"""
    result = subprocess.run(
        ['git', 'status', '--short'],
        check=True,
        capture_output=True,
        text=True,
    )
    lines = result.stdout.strip().splitlines()
    lines = [line.strip() for line in lines]
    file_statuses = {
        line[line.index(' ') + 1:].strip(): line[:line.index(' ')]
        for line in lines if line
    }
    return file_statuses


def _check_files_status(context, expected_statuses, verb):
    """Helper function to check for files with specific git statuses."""

    files = context.get('config', {}).get('files', {})

    file_statuses = _get_changed_files()
    files_with_status = {
        file
        for file, status in file_statuses.items()
        if status in expected_statuses
    }

    files_without_status = [
        file for file in files if file not in files_with_status
    ]
    if len(files_without_status) != 0:
        unexected_files = '\n'.join(files_without_status)
        actual_files = '\n'.join(files_with_status)
        return {
            'pass': False,
            'reason':
            f'Expected {verb} files were not {verb}:\n{unexected_files}'
            f'\nActual {verb} files:\n{actual_files}',
            'score': 0
        }
    return {
        'pass': True,
        'reason': f'All expected {verb} files were {verb}.',
        'score': 1
    }


def check_files_changed(_: str, context):
    """Checks if specific files have been changed and uncommitted."""
    return _check_files_status(context, ['M', 'T', 'R', 'U'], 'changed')


def check_files_added(_: str, context):
    """Checks if specific files have been added and uncommitted."""
    return _check_files_status(context, ['A', '??'], 'added')
