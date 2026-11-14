# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plugin to find candidates for remove-unused-object-overrides."""

import os
import re
import sys
import subprocess

# Add general hub utilities to path
sys.path.append(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '../../hub/scripts'
    )
)
from hub_utils import strip_comments
from hub_utils import strip_strings

# Configuration for main runner
MODE = 'grouped'
FILE_EXTENSIONS = ['.java']
SKIP_DIRS = {'javatests', 'junit', 'third_party', 'out', 'build', '.git'}

OVERRIDE_PATTERN = re.compile(
    r'@Override\s+public\s+(?:boolean\s+equals\s*\(\s*'
    r'(?:@Nullable\s+)?Object\b|int\s+hashCode\s*\(\s*\)|'
    r'String\s+toString\s*\(\s*\))'
)

EQUALS_HASHCODE_PATTERN = re.compile(
    r'@Override\s+public\s+(?:boolean\s+equals\s*\(\s*'
    r'(?:@Nullable\s+)?Object\b|int\s+hashCode\s*\(\s*\))'
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

    excluded_paths = [
        'components/autofill',
        'components/payments',
        'components/password_manager',
        'components/sync',
        'components/signin',
    ]
    rel_path = (
        os.path.relpath(file_path, search_root)
        if os.path.isabs(file_path)
        else file_path
    )
    path_normalized = rel_path.replace(os.sep, '/')
    if any(
        path_normalized.startswith(excluded + '/')
        for excluded in excluded_paths
    ):
        return None

    if os.path.isabs(file_path) or os.path.exists(file_path):
        full_path = file_path
    else:
        full_path = os.path.join(search_root, file_path)
    try:
        with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
            raw_content = f.read()

            syntax_content = strip_strings(strip_comments(raw_content))

            if (
                '@CalledByNative' in syntax_content
                or '@NativeMethods' in syntax_content
                or '@JniType' in syntax_content
                or re.search(
                    r'\bimplements\b[^{]*\bParcelable\b', syntax_content
                )
                or re.search(
                    r'\bextends\b\s+[A-Za-z0-9_.]*Exception\b', syntax_content
                )
            ):
                return None

            matches = OVERRIDE_PATTERN.findall(syntax_content)
            if matches:
                class_name = os.path.splitext(os.path.basename(file_path))[0]
                var_name = (
                    class_name[0].lower() + class_name[1:] if class_name else ""
                )

                # Classes that only override toString() are not affected by
                # collection usage, so only check equals()/hashCode().
                if class_name and EQUALS_HASHCODE_PATTERN.search(
                    syntax_content
                ):
                    grep_pattern = (
                        rf"(?:Collection|List|Set|Map|Queue|Deque)<[^>]*\b{class_name}\b\s*[,>]|"
                        rf"\.(?:indexOf|contains|containsKey|containsValue|"
                        rf"remove|removeAll|retainAll)\([^)]*"
                        rf"\b(?:m?{class_name}|{var_name})\b"
                    )
                    try:
                        result = subprocess.run(
                            [
                                'git',
                                'grep',
                                '-P',
                                '-q',
                                grep_pattern,
                                '--',
                                ':/*.java',
                            ],
                            cwd=search_root,
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                        )
                        if result.returncode == 0:
                            return None
                    except Exception:
                        pass

                return {
                    'issues': (
                        'remove-unused-object-overrides-candidates: '
                        f'{len(matches)} Object override(s)'
                    )
                }
    except Exception:
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
