# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plugin to find candidates for remove-unused-object-overrides."""

import os
import re
import sys

# Add general hub utilities to path
# pylint: disable=wrong-import-position,import-error
sys.path.append(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '../../hub/scripts'
    )
)
from hub_utils import strip_comments
from hub_utils import strip_strings
# pylint: enable=wrong-import-position,import-error

# Configuration for main runner
MODE = 'grouped'
FILE_EXTENSIONS = ['.java']
SKIP_DIRS = {'javatests', 'junit', 'third_party', 'out', 'build', '.git'}

OVERRIDE_PATTERN = re.compile(
    r'@Override\s+public\s+(?:boolean\s+equals\s*\(\s*'
    r'(?:@Nullable\s+)?Object\b|int\s+hashCode\s*\(\s*\)|'
    r'String\s+toString\s*\(\s*\))'
)


def check_file(file_path, search_root):
    """Checks a single file and returns metadata if it is a candidate.

    Args:
        file_path: Relative path to the file to check.
        search_root: Absolute path to the repository root.

    Returns:
        A dict containing candidate metadata (e.g., {"issues": "description"})
        if the file is a candidate, or None otherwise.
    """
    test_suffixes = (
        'Test.java',
        'UnitTest.java',
        'RenderTest.java',
        'TestCase.java',
    )
    if any(file_path.endswith(suffix) for suffix in test_suffixes):
        return None

    if os.path.isabs(file_path) or os.path.exists(file_path):
        full_path = file_path
    else:
        full_path = os.path.join(search_root, file_path)
    try:
        with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
            raw_content = f.read()
            syntax_content = strip_strings(strip_comments(raw_content))

            matches = OVERRIDE_PATTERN.findall(syntax_content)
            if matches:
                return {
                    'issues': (
                        'remove-unused-object-overrides-candidates: '
                        f'{len(matches)} Object override(s)'
                    )
                }
    except Exception:  # pylint: disable=broad-except
        pass
    return None


if __name__ == '__main__':
    print(
        'ERROR: This script is a plugin and cannot be run directly.',
        file=sys.stderr,
    )
    print('Please run the central hub runner instead:', file=sys.stderr)
    print(
        '  python3 agents/projects/code-health/hub/scripts/'
        f'candidate_finder.py find --plugin {__file__}',
        file=sys.stderr,
    )
    sys.exit(1)
