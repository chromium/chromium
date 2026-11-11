# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds expired Chromium histograms in a specific metadata file."""

import argparse
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# Allows running this script from the repo root without cd'ing to scripts/.
sys.path.insert(0, _SCRIPT_DIR)

from find_candidates import (  # noqa: E402
    SAFE_HISTOGRAM_FILES,
    get_expiry_thresholds,
    is_safe_histogram_file,
    normalize_histogram_path,
    parse_expired_histograms,
)

_CS_TIMEOUT_SECONDS = 30
_CS_MAX_RESULTS = 50
# Paths that mention histogram names without recording them.
_CS_EXCLUDE_PATHS = (
    "tools/metrics/histograms/metadata",
    "tools/metrics/histograms/test_data/",
    "expired_histograms_array",
    "analysis/chrome/kaslow",
    "chrome/finch/gcl_studies",
)
# Stale mirrors of other repos that duplicate Chromium code in search results.
_MIRROR_REPO_PREFIXES = (
    "github/youtube/cobalt/",
    "lbshell-internal/cobalt_src/",
    "android/platform/superproject/external/cronet/",
    "arsp/platform/superproject/external/cronet/",
)
_MAX_SUMMARY_LENGTH = 150


def count_recording_files(name):
    """Counts the files that contain the full literal histogram name.

    Runs a case-sensitive `cs -l` search for the full histogram name,
    excluding metadata, test_data, kaslow, finch study, and generated
    expired-histograms-array files, and dropping results from stale mirror
    repos. Recordings that build the name from split strings do not match
    this search and must still be checked during deep analysis.

    Raises:
        RuntimeError: If the count cannot be determined (cs is missing,
            times out, fails, or returns only mirror results at the result
            limit). Never returns 0 on error, since a false 0 would flag a
            histogram that is still recorded.
    """
    escaped = name.replace("\\", "\\\\").replace('"', '\\"')
    excludes = " ".join(f"-file:{p}" for p in _CS_EXCLUDE_PATHS)
    query = f'case:yes "{escaped}" {excludes}'
    cmd = ["cs", "-l", f"--max_num_results={_CS_MAX_RESULTS}", "--", query]
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False,
            timeout=_CS_TIMEOUT_SECONDS,
        )
    except FileNotFoundError as e:
        raise RuntimeError(
            f"'cs' binary not found while searching {name}"
        ) from e
    except subprocess.TimeoutExpired as e:
        raise RuntimeError(
            f"'cs' timed out after {_CS_TIMEOUT_SECONDS}s searching {name}"
        ) from e
    if result.returncode != 0:
        raise RuntimeError(
            f"'cs' exited with {result.returncode} searching {name}: "
            f"{result.stderr.strip()}"
        )
    raw_lines = [
        line.strip() for line in result.stdout.splitlines() if line.strip()
    ]
    kept_lines = [
        line
        for line in raw_lines
        if not line.replace("\\", "/").startswith(_MIRROR_REPO_PREFIXES)
    ]
    # A saturated result list of only mirrors may hide real recordings.
    if len(raw_lines) >= _CS_MAX_RESULTS and not kept_lines:
        raise RuntimeError(
            f"'cs' hit {_CS_MAX_RESULTS}-result mirror limit searching {name}"
        )
    return len(kept_lines)


def get_expired_from_file(file_path, exclude=None, search_root=None):
    """Yields expired histograms with 0 recording sites from a file.

    Histograms whose names contain a '{' token or are in `exclude` are skipped
    before searching for recording sites.

    Raises:
        RuntimeError: If file_path is not allowlisted or cs fails.
        OSError, ET.ParseError: If file_path cannot be read or parsed.
    """
    # Resolve relative paths against search_root, not the CWD.
    base_root = os.path.abspath(search_root or os.getcwd())
    abs_path = os.path.join(base_root, file_path)
    if not is_safe_histogram_file(abs_path, root=search_root):
        raise RuntimeError(
            f"'{file_path}' is not an allowlisted histograms.xml file. "
            "Only the files listed in references/safe_dirs.md may be scanned."
        )
    exclude_set = set(exclude or ())
    date_limit, m_limit = get_expiry_thresholds(search_root)
    expired = parse_expired_histograms(
        abs_path,
        normalize_histogram_path(abs_path, root=search_root),
        date_limit,
        m_limit,
    )
    for hist in expired:
        name = hist["name"]
        if "{" in name or name in exclude_set:
            continue
        if count_recording_files(name) == 0:
            yield hist


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Find expired histograms in a file."
    )
    parser.add_argument("file", type=str, help="Specific XML file to scan.")
    parser.add_argument(
        "--names-only",
        action="store_true",
        help="Output only the names of the histograms.",
    )
    parser.add_argument(
        "--exclude",
        nargs="*",
        action="extend",
        default=[],
        help="Names of histograms to exclude",
    )
    args = parser.parse_args(argv)
    if not is_safe_histogram_file(args.file):
        print(
            f"ERROR: '{args.file}' is not an allowlisted histograms.xml file. "
            "Only the files listed in references/safe_dirs.md may be scanned.",
            file=sys.stderr,
        )
        print("Allowlisted files:", file=sys.stderr)
        for allowed in SAFE_HISTOGRAM_FILES:
            print(f"  {allowed}", file=sys.stderr)
        return 2

    try:
        for h in get_expired_from_file(args.file, exclude=args.exclude):
            if args.names_only:
                print(h["name"])
                continue

            summary = h["summary"]
            if len(summary) > _MAX_SUMMARY_LENGTH:
                summary = summary[:_MAX_SUMMARY_LENGTH] + "..."
            elif not summary:
                summary = "No summary."
            owners_str = ", ".join(h["owners"]) if h["owners"] else "No owners."

            print(f"File: {h['file']}\nName: {h['name']}")
            print(f"Owners: {owners_str}")
            print(f"Expires_after: {h['expires_after']}")
            print(f"Summary: {summary}\n---")
    except (RuntimeError, OSError, ET.ParseError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
