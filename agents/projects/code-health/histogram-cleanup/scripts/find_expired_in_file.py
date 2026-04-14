# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds expired Chromium histograms in a specific metadata file."""

import argparse
import os
import subprocess
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta


def is_expired(exp, date_limit, m_limit):
    """Returns True if the histogram is expired by date or milestone."""
    if not exp:
        return False
    if exp.startswith("20"):
        return exp < date_limit
    if m_limit > 0 and exp.startswith("M") and exp[1:].isdigit():
        return int(exp[1:]) < m_limit
    return False


def count_recording_sites(name):
    """Counts the number of code recording sites for a histogram name."""
    search_term = name.split('.')[-1] if '.' in name else name
    try:
        # Search globally using cs for the last segment (to catch split strings)
        # Exclude matches in the metadata files themselves.
        cmd = [
            "cs", "-c",
            f"{search_term} -file:tools/metrics/histograms/metadata"
        ]
        result = subprocess.run(cmd,
                                capture_output=True,
                                text=True,
                                check=False)
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            for line in reversed(lines):
                if line.isdigit():
                    return int(line)
    except Exception:
        pass
    return 0


def get_expired_from_file(file_path, date_limit, m_limit):
    """Yields expired histograms with 0 recording sites from a file."""
    try:
        for hist in ET.parse(file_path).iter("histogram"):
            exp = hist.attrib.get("expires_after", "")
            if not is_expired(exp, date_limit, m_limit):
                continue
            if hist.find("expired_intentionally") is not None:
                continue

            name = hist.attrib.get("name", "")
            if count_recording_sites(name) > 0:
                continue

            summary = hist.findtext("summary", default="").strip()
            owners = [o.text for o in hist.findall("owner") if o.text]
            yield {
                "file": file_path,
                "name": name,
                "expires_after": exp,
                "summary": summary,
                "owners": owners
            }
    except Exception:
        pass


def main():
    parser = argparse.ArgumentParser(
        description="Find expired histograms in a file.")
    parser.add_argument("file", type=str, help="Specific XML file to scan.")
    parser.add_argument("--names-only",
                        action="store_true",
                        help="Output only the names of the histograms.")
    args = parser.parse_args()

    # Thresholds: 1 year ago or milestone - 12 (approx 1 year of milestones)
    date_limit = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")
    m_limit = 0
    version_file = os.path.join(os.getcwd(), "chrome", "VERSION")

    if os.path.exists(version_file):
        try:
            with open(version_file, encoding="utf-8") as f:
                for line in f:
                    if line.startswith("MAJOR="):
                        m_limit = int(line.split("=")[1]) - 12
                        break
        except (ValueError, IndexError):
            pass

    for h in get_expired_from_file(args.file, date_limit, m_limit):
        if "{" in h["name"]:
            continue

        if args.names_only:
            print(h["name"])
            continue

        summary = h["summary"][:150] + "..." if h["summary"] else "No summary."
        owners_str = ", ".join(h["owners"]) if h["owners"] else "No owners."

        print(f"File: {h['file']}\nName: {h['name']}")
        print(f"Owners: {owners_str}")
        print(f"Expiry: {h['expires_after']}")
        print(f"Summary: {summary}\n---")


if __name__ == "__main__":
    main()
