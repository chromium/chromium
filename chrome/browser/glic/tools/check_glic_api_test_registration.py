#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validates that all Glic WebUI test methods are registered in C++ tests.

This script replaces runtime AssertAllTestsRegistered checks by statically
extracting all test methods from TypeScript test files and ensuring that
corresponding C++ IN_PROC_BROWSER_TEST_P or IN_PROC_BROWSER_TEST_F declarations
exist.
"""

import argparse
import functools
import os
from pathlib import Path
import re
import sys

IGNORED_TS_METHODS = frozenset([
    'testMain',
    'testStepper',
])


def find_repo_root(start_path: str | Path | None = None) -> str:
    """Finds the Chromium repo root, or uses the explicitly provided root."""
    if start_path:
        return str(Path(start_path).resolve())
    current = Path(__file__).resolve()
    for parent in [current] + list(current.parents):
        if (parent / '.gn').exists():
            return str(parent)
    raise RuntimeError(
        'Could not locate Chromium repo root (.gn not found in parent '
        f'hierarchy of {current}). Please supply --repo-root explicitly.')


TS_TEST_REL_DIR = os.path.join('chrome', 'test', 'data', 'webui', 'glic',
                               'browser_tests')
# Matches top-level C++ test reference comment in TypeScript files:
# e.g., // cc_file_path: chrome/browser/glic/host/glic_api_browsertest.cc
_RX_CC_TEST_COMMENT = re.compile(
    r'//\s*cc_file_path:(?:\s*\n\s*//)?\s*'
    r'([a-zA-Z0-9_\-/\\]+test\.cc)',
    flags=re.MULTILINE,
)

# Matches constructor arguments referencing the JS test bundle:
# e.g., GlicTestJsPath("./glic_api_browsertest.js") or multiline.
_RX_JS_BUNDLE = re.compile(
    r'GlicTestJsPath\(\s*"(?:\./)?([^"]+\.js)"\s*\)',
    flags=re.DOTALL,
)

# Matches GoogleTest and Chromium browser test declarations:
# e.g., IN_PROC_BROWSER_TEST_P(GlicApiTest, testFoo)
_RX_CC_TEST = re.compile(
    r'^\s*(?:TYPED_)?(?:IN_PROC_BROWSER_)?'
    r'TEST(?:_F|_P)?\(\s*(\w+)\s*,\s*(\w+)\s*\)',
    flags=re.DOTALL | re.M,
)

# Matches TypeScript test method definitions inside classes:
# e.g., async testFoo() { ... }, public testBar(): void { ... },
# or testBaz = async () => { ... }
_RX_TS_TEST_METHOD = re.compile(
    r'(?:^|\n|[{;])\s*(?:(?:public|protected|override|readonly)\s+)*'
    r'(?:async\s+)?(test[A-Za-z0-9_]+)\s*(?:<[^>]+>)?\s*\(', )
_RX_TS_ARROW_TEST_METHOD = re.compile(
    r'(?:^|\n|[{;])\s*(?:(?:public|protected|override|readonly)\s+)*'
    r'(?:async\s+)?(test[A-Za-z0-9_]+)\s*=\s*(?:async\s*)?(?:<[^>]+>)?\s*\(', )


def is_ts_test_file(path: str) -> bool:
    """Returns True if path represents a WebUI test file."""
    return path.endswith('test.ts')


def is_cc_test_file(path: str) -> bool:
    """Returns True if path represents a C++ test file."""
    return path.endswith('test.cc')


def strip_comments(content: str) -> str:
    """Removes single-line (//) and multi-line (/* ... */) comments."""
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    content = re.sub(r'//.*$', '', content, flags=re.M)
    return content


def extract_ts_tests(content: str) -> set[str]:
    """Extracts all test method names from a TypeScript test file."""
    content = strip_comments(content)
    tests = set()
    for match in _RX_TS_TEST_METHOD.finditer(content):
        name = match.group(1)
        if name not in IGNORED_TS_METHODS:
            tests.add(name)
    for match in _RX_TS_ARROW_TEST_METHOD.finditer(content):
        name = match.group(1)
        if name not in IGNORED_TS_METHODS:
            tests.add(name)
    return tests


def extract_cc_tests(content: str) -> set[str]:
    """Extracts all test names declared in a C++ test file."""
    content = strip_comments(content)
    tests = set()
    for match in _RX_CC_TEST.finditer(content):
        test_name = match.group(2)
        test_name = re.sub(r'^(?:MAYBE_|DISABLED_|MANUAL_)', '', test_name)
        if test_name not in IGNORED_TS_METHODS:
            tests.add(test_name)
    return tests


def normalize_abs_path(path: str, repo_root: str) -> str:
    """Returns normalized absolute path, resolving relative against root."""
    norm_repo = os.path.normpath(os.path.abspath(repo_root))
    if os.path.isabs(path):
        return os.path.normpath(path)
    return os.path.normpath(os.path.join(norm_repo, path))


def clear_caches() -> None:
    """Clears memoized LRU caches."""
    find_ts_path_from_cc.cache_clear()
    find_cc_path_from_ts.cache_clear()


@functools.lru_cache(maxsize=None)
def find_ts_path_from_cc(cc_path: str,
                         repo_root: str) -> tuple[str | None, str | None]:
    """Finds matching TypeScript test file path from a C++ test file.

    Returns:
        tuple of (ts_path, error_message)
    """
    norm_repo = os.path.normpath(os.path.abspath(repo_root))
    abs_cc = normalize_abs_path(cc_path, norm_repo)

    if not is_cc_test_file(abs_cc) or not os.path.exists(abs_cc):
        return None, None
    try:
        with open(abs_cc, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except OSError:
        return None, None

    content_no_comments = strip_comments(content)
    match = _RX_JS_BUNDLE.search(content_no_comments)
    if not match:
        # If GlicTestJsPath is not found, it's not a Glic API test file.
        return None, None

    js_filename = match.group(1)
    ts_filename = js_filename.replace('.js', '.ts')
    ts_path = os.path.normpath(
        os.path.join(norm_repo, TS_TEST_REL_DIR, ts_filename))
    if not os.path.exists(ts_path):
        rel_cc = os.path.relpath(abs_cc, norm_repo)
        return None, (
            f"ERROR: {rel_cc} references JS test bundle '{js_filename}', but "
            f"corresponding TypeScript file does not exist: {ts_path}\n")

    return ts_path, None


@functools.lru_cache(maxsize=None)
def find_cc_path_from_ts(ts_path: str,
                         repo_root: str) -> tuple[str | None, str | None]:
    """Finds matching C++ test file path from a TypeScript test file.

    Returns:
        tuple of (cc_path, error_message)
    """
    norm_repo = os.path.normpath(os.path.abspath(repo_root))
    abs_ts = normalize_abs_path(ts_path, norm_repo)

    if not is_ts_test_file(abs_ts) or not os.path.exists(abs_ts):
        return None, None

    try:
        with open(abs_ts, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except OSError:
        return None, None

    rel_ts = os.path.relpath(abs_ts, norm_repo)
    match = _RX_CC_TEST_COMMENT.search(content)
    if not match:
        return None, (
            f"ERROR: {rel_ts} is missing the required C++ test "
            f"reference comment.\nPlease add '// cc_file_path: "
            f"<path/to/test_browsertest.cc>' at the top of {rel_ts}.\n")

    cc_rel = match.group(1).strip()
    cc_path = normalize_abs_path(cc_rel, norm_repo)
    if not os.path.exists(cc_path):
        return None, (
            f"ERROR: {rel_ts} references non-existent C++ test file '{cc_rel}'.\n"
            f"Resolved path does not exist: {cc_path}\n")

    return cc_path, None


def discover_all_test_pairs(
        repo_root: str) -> tuple[list[tuple[str, str]], list[str]]:
    """Discovers all C++ and TS test pairs from the WebUI test folder.

    Returns:
        tuple of (pairs, error_messages)
    """
    norm_repo = os.path.normpath(os.path.abspath(repo_root))
    ts_dir = os.path.join(norm_repo, TS_TEST_REL_DIR)
    if not os.path.exists(ts_dir):
        return [], []

    pairs = set()
    errors = []
    for entry in sorted(os.scandir(ts_dir), key=lambda e: e.name):
        if not entry.is_file() or not is_ts_test_file(entry.name):
            continue
        cc_path, err = find_cc_path_from_ts(entry.path, norm_repo)
        if err:
            errors.append(err)
        elif cc_path:
            pairs.add((cc_path, os.path.normpath(entry.path)))

    return sorted(pairs), errors


def resolve_test_pair(file_path: str,
                      repo_root: str) -> tuple[str, str] | None:
    """Resolves a file path (C++ or TS) to its (cc_path, ts_path) pair."""
    norm_repo = os.path.normpath(os.path.abspath(repo_root))
    abs_path = normalize_abs_path(file_path, norm_repo)

    if is_cc_test_file(abs_path):
        ts_path, _ = find_ts_path_from_cc(abs_path, norm_repo)
        if ts_path:
            return (abs_path, ts_path)

    if is_ts_test_file(abs_path):
        cc_path, _ = find_cc_path_from_ts(abs_path, norm_repo)
        if cc_path:
            return (cc_path, abs_path)

    return None


def get_test_pairs_to_check(
        files: list[str],
        repo_root: str) -> tuple[list[tuple[str, str]], list[str]]:
    """Returns test pairs to check from specific files or full discovery.

    Returns:
        tuple of (test_pairs, error_messages)
    """
    if not files or any('check_glic_api_test_registration.py' in f
                        for f in files):
        return discover_all_test_pairs(repo_root)

    pairs = set()
    errors = []
    norm_repo = os.path.normpath(os.path.abspath(repo_root))
    for file in files:
        abs_path = normalize_abs_path(file, norm_repo)
        if is_ts_test_file(abs_path):
            _, err = find_cc_path_from_ts(abs_path, norm_repo)
            if err:
                errors.append(err)
        elif is_cc_test_file(abs_path):
            _, err = find_ts_path_from_cc(abs_path, norm_repo)
            if err:
                errors.append(err)
        pair = resolve_test_pair(abs_path, norm_repo)
        if pair:
            pairs.add(pair)

    return sorted(pairs), errors


def check_test_pair(cc_path: str,
                    ts_path: str) -> tuple[set[str], set[str], set[str]]:
    """Checks registration between a C++ and a TypeScript test file.

    Returns:
        tuple of (missing_in_cc, ts_tests, cc_tests)
    """
    with open(cc_path, 'r', encoding='utf-8', errors='ignore') as f:
        cc_content = f.read()
    with open(ts_path, 'r', encoding='utf-8', errors='ignore') as f:
        ts_content = f.read()

    ts_tests = extract_ts_tests(ts_content)
    cc_tests = extract_cc_tests(cc_content)
    missing_in_cc = ts_tests - cc_tests
    return missing_in_cc, ts_tests, cc_tests


def report_missing_registrations(rel_cc: str, rel_ts: str,
                                 missing: set[str]) -> None:
    """Prints a formatted error message for missing test registrations."""
    print(
        f'ERROR: Missing C++ test registration in {rel_cc} for tests '
        f'declared in {rel_ts}:',
        file=sys.stderr,
    )
    for test in sorted(missing):
        print(f'  - {test}', file=sys.stderr)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description='Validate C++ Glic API test registrations.')
    parser.add_argument(
        '--check-only',
        action='store_true',
        help=
        'Run checks and exit with non-zero if missing registrations are found.',
    )
    parser.add_argument(
        '--quiet',
        action='store_true',
        help='Suppress success messages.',
    )
    parser.add_argument(
        '--repo-root',
        default=None,
        help='Path to Chromium repository root.',
    )
    parser.add_argument(
        'files',
        nargs='*',
        help='Specific C++ or TypeScript test files to check.',
    )

    args = parser.parse_args(argv)
    repo_root = find_repo_root(args.repo_root)

    test_pairs, errors = get_test_pairs_to_check(args.files, repo_root)
    if errors:
        for err in errors:
            print(err, file=sys.stderr)
        return 1

    if not test_pairs:
        if not args.quiet:
            print('No Glic API test pairs to check.')
        return 0

    has_errors = False
    for cc_path, ts_path in test_pairs:
        rel_cc = os.path.relpath(cc_path, repo_root)
        rel_ts = os.path.relpath(ts_path, repo_root)
        missing, ts_tests, _ = check_test_pair(cc_path, ts_path)

        if missing:
            has_errors = True
            report_missing_registrations(rel_cc, rel_ts, missing)
        elif not args.quiet:
            print(f'OK: {rel_cc} (checked {len(ts_tests)} TypeScript tests)')

    return 1 if has_errors else 0


def CheckGlicApiTestRegistration(input_api, output_api):
    """Presubmit check function to validate Glic API test registrations."""

    def file_filter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[
                r'.*test\.cc$',
                r'^chrome/test/data/webui/glic/browser_tests/.*\.ts$',
                r'^chrome/browser/glic/tools/'
                r'check_glic_api_test_registration\.py$',
            ],
        )

    affected_files = list(
        input_api.AffectedFiles(include_deletes=False,
                                file_filter=file_filter))
    if not affected_files:
        return []

    repo_root = input_api.change.RepositoryRoot()
    file_paths = [f.UnixLocalPath() for f in affected_files]
    test_pairs, errors = get_test_pairs_to_check(file_paths, repo_root)

    results = []
    if errors:
        for err in errors:
            results.append(
                output_api.PresubmitError(
                    f'Glic API test registration check error:\n{err}'))
        return results

    for cc_path, ts_path in test_pairs:
        rel_cc = os.path.relpath(cc_path, repo_root)
        rel_ts = os.path.relpath(ts_path, repo_root)
        missing, _, _ = check_test_pair(cc_path, ts_path)
        if missing:
            missing_formatted = '\n'.join(f'  - {test}'
                                          for test in sorted(missing))
            results.append(
                output_api.PresubmitError(
                    f'Glic API test registration check failed:\n'
                    f'Missing C++ test registration in {rel_cc} for tests '
                    f'declared in {rel_ts}:\n{missing_formatted}'))

    return results


if __name__ == '__main__':
    sys.exit(main())
