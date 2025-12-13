# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Asserts for checking changed files in git."""

import os
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


def check_files_exist(_: str, context):
    """Checks if specific files exist on the filesystem."""
    files = context.get('config', {}).get('files', {})
    files_that_do_not_exist = [
        file for file in files if not os.path.exists(file)
    ]
    if files_that_do_not_exist:
        non_existent_files = '\n'.join(files_that_do_not_exist)
        return {
            'pass': False,
            'reason': f'Expected files do not exist:\n{non_existent_files}',
            'score': 0
        }
    return {'pass': True, 'reason': 'All expected files exist.', 'score': 1}


def check_file_content(_: str, context):
    """Checks if files contain or do not contain specific strings."""
    file_configs = context.get('config', {}).get('files', [])
    errors = []

    for config in file_configs:
        file_path = config.get('path')
        if not file_path:
            continue

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except FileNotFoundError:
            errors.append(f'File not found: {file_path}')
            continue
        except Exception as e:
            errors.append(f'Error reading file {file_path}: {e}')
            continue

        for s in config.get('present', []):
            if s not in content:
                errors.append(
                    f'Expected to find "{s}" in {file_path}, but it was not '
                    'found.')

        for s in config.get('absent', []):
            if s in content:
                errors.append(
                    f'Expected to not find "{s}" in {file_path}, but it was '
                    'found.')

    if errors:
        return {'pass': False, 'reason': '\n'.join(errors), 'score': 0}

    return {
        'pass': True,
        'reason': 'All file content checks passed.',
        'score': 1
    }
