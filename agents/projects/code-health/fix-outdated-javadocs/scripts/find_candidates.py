# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plugin to find candidates for fix-outdated-javadocs."""

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
# pylint: enable=wrong-import-position,import-error

# Configuration for main runner
MODE = 'grouped'
FILE_EXTENSIONS = ['.java']
SKIP_DIRS = {'javatests', 'junit', 'third_party', 'out', 'build', '.git'}

# Regex matching any Javadoc block in the file
JAVADOC_BLOCK_PATTERN = re.compile(r'/\*\*(?P<doc>.*?)\*/', re.DOTALL)

# Regex capturing a Javadoc block and its declaration up to { or ;.
# Matches public, protected, private, package-private, and interface methods.
DECL_AFTER_DOC_PATTERN = re.compile(
    r'/\*\*(?P<doc>.*?)\*/\s*'
    r'(?P<decl>[^;{]+(?:\([^;{]*\)[^;{]*)?)[;{]',
    re.DOTALL,
)

PARAM_TAG_PATTERN = re.compile(r'@param\s+([a-zA-Z0-9_]+)')
RETURN_TAG_PATTERN = re.compile(r'@return\b')
PIPE_PARAM_PATTERN = re.compile(r'\|[a-zA-Z0-9_]+\|')
INLINE_PARAM_PATTERN = re.compile(r'\{@param\b')
TAG_ORDER_PATTERN = re.compile(r'@return\b.*?@param\b', re.DOTALL)


def extract_params(param_str):
    """Extracts formal parameter names from a Java method parameter string."""
    param_str = param_str.strip()
    if not param_str:
        return []

    # Strip string literals inside annotations
    cleaned = re.sub(r'"[^"]*"', '""', param_str)
    # Strip annotations like @Nullable, @JniType("")
    cleaned = re.sub(r'@[a-zA-Z0-9_]+(?:\([^)]*\))?', '', cleaned)
    # Remove type generics recursively <...>
    while '<' in cleaned:
        cleaned = re.sub(r'<[^<>]*>', '', cleaned)

    params = []
    for item in cleaned.split(','):
        item = item.strip()
        if not item:
            continue
        tokens = item.split()
        if tokens:
            name = tokens[-1].rstrip('[]').rstrip('...')
            if re.match(r'^[a-zA-Z0-9_]+$', name):
                params.append(name)
    return params


def extract_method_params_from_decl(decl):
    """Extracts parameter string from a method or constructor declaration."""
    # Skip classes, interfaces, enums, records, or annotations
    if re.search(r'\b(class|interface|enum|record|@interface)\b', decl):
        return None
    # Must have parentheses representing argument list
    m = re.search(r'[a-zA-Z0-9_]+\s*\((?P<params>[^)]*)\)', decl)
    if not m:
        return None
    return m.group('params')


def is_return_only_doc(doc_content):
    """Checks if a Javadoc contains only a @return tag without summary."""
    if not RETURN_TAG_PATTERN.search(doc_content):
        return False
    pre_return = doc_content.split('@return', 1)[0]
    cleaned_pre = re.sub(r'[\s\*]', '', pre_return)
    return len(cleaned_pre) == 0


def check_file(file_path, search_root):
    """Checks a single file and returns metadata if it is a candidate.

    Args:
        file_path: Relative path to the file to check.
        search_root: Absolute path to the repository root.

    Returns:
        A dict containing candidate metadata if the file is a candidate,
        or None otherwise.
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

        no_line_comments = re.sub(r'//.*', '', raw_content)

        # 1. Broad scan: Check all Javadoc blocks in the file for style issues
        return_only_count = 0
        pipe_count = 0
        inline_param_count = 0
        tag_order_count = 0

        for m in JAVADOC_BLOCK_PATTERN.finditer(no_line_comments):
            doc = m.group('doc')
            if is_return_only_doc(doc):
                return_only_count += 1
            if TAG_ORDER_PATTERN.search(doc):
                tag_order_count += 1
            pipe_count += len(PIPE_PARAM_PATTERN.findall(doc))
            inline_param_count += len(INLINE_PARAM_PATTERN.findall(doc))

        # 2. Targeted scan: Check method declarations for @param alignment
        mismatched_count = 0
        for m in DECL_AFTER_DOC_PATTERN.finditer(no_line_comments):
            doc = m.group('doc')
            doc_params = PARAM_TAG_PATTERN.findall(doc)
            if doc_params:
                raw_params = extract_method_params_from_decl(m.group('decl'))
                if raw_params is not None:
                    actual_params = extract_params(raw_params)
                    stale = [p for p in doc_params if p not in actual_params]
                    missing = [p for p in actual_params if p not in doc_params]
                    if stale or missing:
                        mismatched_count += 1

        issues = []
        if mismatched_count > 0:
            issues.append(f'{mismatched_count} mismatched @param tag(s)')
        if return_only_count > 0:
            issues.append(f'{return_only_count} return-only Javadoc(s)')
        if pipe_count > 0:
            issues.append(f'{pipe_count} pipe parameter reference(s)')
        if inline_param_count > 0:
            issues.append(f'{inline_param_count} invalid inline @param tag(s)')
        if tag_order_count > 0:
            issues.append(f'{tag_order_count} out-of-order tag(s)')

        if issues:
            return {
                'issues': (
                    'fix-outdated-javadocs-candidates: ' + ', '.join(issues)
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
