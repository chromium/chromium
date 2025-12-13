#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Linter for promptfoo.yaml files."""

import argparse
import os
import sys
import yaml


def _get_chromium_src_path():
    """Returns the path to the Chromium src directory."""
    # This script is in chromium/agents/testing, so three levels up is the src
    # root.
    return os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def _check_extension_reference(extension_name, test_case_path):
    """Checks a single extension reference."""
    errors = []
    if not isinstance(extension_name, str):
        errors.append(f'{test_case_path} contains a non-string extension '
                      f'reference: {extension_name}')
        return errors

    if extension_name.startswith('file://'):
        msg = (f'{test_case_path} contains a file path for an extension. '
               'Please use the extension name instead: '
               f'{extension_name}')
        errors.append(msg)
        return errors

    chromium_src_path = _get_chromium_src_path()
    extension_path = os.path.join(chromium_src_path, 'agents', 'extensions',
                                  extension_name)

    if not os.path.exists(extension_path):
        msg = (f'{test_case_path} refers to a non-existent extension: '
               f'{extension_name}')
        errors.append(msg)
    return errors


def _check_file_reference(file_url, test_case_path):
    """Checks a single file reference."""
    errors = []
    if not isinstance(file_url, str):
        errors.append(f'{test_case_path} contains a non-string file '
                      f'reference: {file_url}')
        return errors
    # The file URL should be a path relative to chromium/src.
    file_dir = file_url.removeprefix('file://')
    chromium_src_path = _get_chromium_src_path()
    # The file path from yaml may contain forward slashes, which is not the
    # native path separator on Windows. We need to split the path and join it
    # back to get a path with the correct separators.
    abs_path = os.path.join(chromium_src_path, *file_dir.split('/'))

    if not os.path.exists(abs_path):
        msg = (f'{test_case_path} refers to a non-existent file: '
               f'{file_dir}')
        errors.append(msg)
    return errors


def check_test_case(data, test_case_path):
    """Checks that promptfoo.yaml data is valid.
      1. Check providers.config.changes.*.apply points to valid files.
      2. Check providers.config.templates points to an array of valid files.
      3. Check providers.config.extensions points to valid extensions in
         //agents/extensions/.
    """
    errors = []
    if not isinstance(data, dict):
        errors.append(f'{test_case_path} must be a dictionary.')
        return errors
    providers = data.get('providers')
    if not providers:
        return [f'{test_case_path} must contain at least one provider.']
    elif not isinstance(providers, list):
        return [f'{test_case_path} "providers" field must be a list.']

    for provider in providers:
        if not isinstance(provider, dict):
            errors.append(f'{test_case_path} "providers" field must be a '
                          'list of dicts.')
            continue
        config = provider.get('config')
        if config is None:
            continue
        if not isinstance(config, dict):
            errors.append(f'{test_case_path} "providers" field must have a '
                          'dict "config" field.')
            continue

        # Check providers.config.changes.*.apply
        changes = config.get('changes')
        if changes is not None:
            if isinstance(changes, list):
                for change in changes:
                    if not isinstance(change, dict):
                        errors.append(f'{test_case_path} "changes" items '
                                      'must be dicts.')
                        continue
                    if 'apply' in change:
                        errors.extend(
                            _check_file_reference(change['apply'],
                                                  test_case_path))
            else:
                errors.append(f'{test_case_path} "changes" field must be a '
                              'list.')

        # Check providers.config.templates
        templates = config.get('templates')
        if templates is not None:
            if isinstance(templates, list):
                for template in templates:
                    errors.extend(
                        _check_file_reference(template, test_case_path))
            else:
                errors.append(f'{test_case_path} "templates" field must be '
                              'a list.')

        # Check providers.config.extensions
        extensions = config.get('extensions')
        if extensions is not None:
            if isinstance(extensions, list):
                for extension in extensions:
                    errors.extend(
                        _check_extension_reference(extension, test_case_path))
            else:
                errors.append(f'{test_case_path} "extensions" field must '
                              'be a list.')
    return errors


def main(argv):
    """Entrypoint for the linter script."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files',
                        nargs='+',
                        help='promptfoo.yaml files to lint.')
    args = parser.parse_args(argv[1:])

    all_errors = []
    for f_path in args.files:
        try:
            with open(f_path, 'r', encoding='utf-8') as f:
                data = yaml.safe_load(f)
        except yaml.YAMLError as e:
            all_errors.append(f'Invalid YAML in {f_path}: {e}')
            continue
        except OSError as e:
            all_errors.append(f'Could not read file {f_path}: {e}')
            continue

        try:
            all_errors.extend(check_test_case(data, f_path))
        except Exception as e:
            # Broad exception for unexpected data structures.
            all_errors.append(
                f'Error linting {f_path}: {e}. This may be from a '
                'malformed file.')

    if all_errors:
        for error in all_errors:
            # The presubmit wrapper will show this to the user.
            print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
