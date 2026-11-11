# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plugin to find expired histograms."""

import os
import random
import re
import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta

# Configuration for main runner
MODE = "atomic"

# Canonical Markdown allowlist, relative to this script.
SAFE_DIRS_MD = os.path.normpath(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "..",
        "references",
        "safe_dirs.md",
    )
)

# Matches allowlisted metadata file paths (POSIX separators).
_SAFE_PATH_RE = re.compile(
    r"tools/metrics/histograms/metadata/[A-Za-z0-9_]+/histograms\.xml"
)


def load_safe_histogram_files(md_path=SAFE_DIRS_MD):
    """Returns the allowlisted histograms.xml paths from safe_dirs.md."""
    with open(md_path, encoding="utf-8") as f:
        parsed = tuple(dict.fromkeys(_SAFE_PATH_RE.findall(f.read())))
    if not parsed:
        raise RuntimeError(f"No allowlisted paths found in {md_path}")
    return parsed


SAFE_HISTOGRAM_FILES = load_safe_histogram_files()


def normalize_histogram_path(file_path, root=None):
    """Returns file_path as a POSIX path relative to root (default: cwd)."""
    root = os.path.abspath(root or os.getcwd())
    try:
        rel = os.path.relpath(os.path.abspath(file_path), root)
    except ValueError:  # e.g. different drives on Windows
        rel = file_path
    return os.path.normpath(rel).replace(os.sep, "/")


def is_safe_histogram_file(file_path, root=None):
    """True iff file_path is one of the allowlisted histograms.xml files."""
    return normalize_histogram_path(file_path, root) in SAFE_HISTOGRAM_FILES


def is_expired(exp, date_limit, m_limit):
    """Returns True if the histogram is expired by date or milestone."""
    if not exp:
        return False
    if exp.startswith("20"):
        return exp < date_limit
    if m_limit > 0 and exp.startswith("M") and exp[1:].isdigit():
        return int(exp[1:]) < m_limit
    return False


# Max related siblings per candidate (batch of at most 10 histograms).
_MAX_SIBLINGS = 9


def _related_prefix(name: str) -> str:
    """Returns the feature prefix used to relate histograms, or ''."""
    base = name.split("{", 1)[0]
    parts = [p for p in base.split(".") if p]
    if "{" in name:
        # The token replaces a segment, so the segments before it are all
        # concrete and two of them are enough to identify the feature.
        if len(parts) >= 2:
            return ".".join(parts[:2])
        if len(parts) == 1:
            return parts[0]
        return ""
    if len(parts) >= 3:
        return ".".join(parts[:2])
    if len(parts) == 2:
        return parts[0]
    return ""


def _group_key(candidate):
    """Returns (expiry, owners, prefix) for grouping, or None if ungroupable."""
    exp = candidate["expires_after"]
    owner_key = frozenset(
        o.strip().lower() for o in candidate["owners"] if o.strip()
    )
    prefix = _related_prefix(candidate["name"])
    if not exp or not owner_key or not prefix:
        return None
    return (exp, owner_key, prefix)


def get_expiry_thresholds(search_root=None):
    """Returns the (date_limit, m_limit) expiry thresholds.

    date_limit is the date one year ago (YYYY-MM-DD). m_limit is the
    milestone one year ago (chrome/VERSION MAJOR - 12), or 0 if unknown.
    """
    root = os.path.abspath(search_root or os.getcwd())
    date_limit = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")
    m_limit = 0
    version_file = os.path.join(root, "chrome", "VERSION")

    if os.path.exists(version_file):
        try:
            with open(version_file, encoding="utf-8") as f:
                for line in f:
                    if line.startswith("MAJOR="):
                        m_limit = int(line.split("=")[1]) - 12
                        break
        except (OSError, ValueError, IndexError):
            pass
    return date_limit, m_limit


def parse_expired_histograms(abs_path, rel_path, date_limit, m_limit):
    """Returns the expired, non-intentional histograms in an XML file.

    Raises ET.ParseError or OSError if the file cannot be read or parsed.
    """
    expired = []
    for hist in ET.parse(abs_path).iter("histogram"):
        exp = hist.attrib.get("expires_after", "")
        if not is_expired(exp, date_limit, m_limit):
            continue
        if hist.find("expired_intentionally") is not None:
            continue

        summary = hist.findtext("summary", default="").strip()
        owners = [o.text for o in hist.findall("owner") if o.text]

        expired.append(
            {
                "file": rel_path,
                "name": hist.attrib.get("name", ""),
                "expires_after": exp,
                "owners": owners,
                "summary": summary,
            }
        )
    return expired


def find_candidates(search_root=None):
    """Yields expired histograms from the allowlisted histograms.xml files.

    Only SAFE_HISTOGRAM_FILES (references/safe_dirs.md) are scanned.

    The file order and each file's results are shuffled so the hub's atomic
    pool (which may stop after a handful of candidates) samples across all
    allowlisted files instead of draining the first one. Ordering for
    asc/desc is applied by the hub, not by this plugin.
    """
    root = os.path.abspath(search_root or os.getcwd())
    date_limit, m_limit = get_expiry_thresholds(root)

    files = list(SAFE_HISTOGRAM_FILES)
    random.shuffle(files)
    for rel_path in files:
        abs_path = os.path.join(root, *rel_path.split("/"))
        if not os.path.isfile(abs_path):
            continue
        try:
            file_candidates = parse_expired_histograms(
                abs_path, rel_path, date_limit, m_limit
            )
        except (ET.ParseError, OSError):
            continue

        groups = {}
        for candidate in file_candidates:
            key = _group_key(candidate)
            if key is not None:
                groups.setdefault(key, []).append(candidate)
        for candidate in file_candidates:
            key = _group_key(candidate)
            if key is None:
                continue
            name = candidate["name"]
            siblings = sorted(
                other["name"] for other in groups[key] if other["name"] != name
            )[:_MAX_SIBLINGS]
            if siblings:
                candidate["related_histograms"] = siblings

        random.shuffle(file_candidates)
        yield from file_candidates


if __name__ == "__main__":
    print(
        "ERROR: This script is a plugin and cannot be run directly.",
        file=sys.stderr,
    )
    print("Please run the central hub runner instead:", file=sys.stderr)
    print(
        f"  python3 agents/projects/code-health/hub/scripts/candidate_finder.py find --plugin {__file__}",
        file=sys.stderr,
    )
    sys.exit(1)
