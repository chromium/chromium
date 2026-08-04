# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plugin to find expired histograms."""
# pylint: disable=line-too-long

import glob
import os
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta

# Configuration for main runner
MODE = "atomic"


def is_expired(exp, date_limit, m_limit):
    """Returns True if the histogram is expired by date or milestone."""
    if not exp:
        return False
    if exp.startswith("20"):
        return exp < date_limit
    if m_limit > 0 and exp.startswith("M") and exp[1:].isdigit():
        return int(exp[1:]) < m_limit
    return False


def find_candidates(search_root=None):
    """Generator yielding expired histograms from the metadata directory."""
    del search_root  # unused
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

    pattern = os.path.join("tools", "metrics", "histograms", "metadata", "*",
                           "histograms.xml")
    for f in glob.iglob(pattern):
        try:
            for hist in ET.parse(f).iter("histogram"):
                exp = hist.attrib.get("expires_after", "")
                if not is_expired(exp, date_limit, m_limit):
                    continue
                if hist.find("expired_intentionally") is not None:
                    continue

                summary = hist.findtext("summary", default="").strip()
                owners = [o.text for o in hist.findall("owner") if o.text]

                yield {
                    "file": f,
                    "name": hist.attrib.get("name", ""),
                    "expires_after": exp,
                    "owners": owners,
                    "summary": summary,
                }
        except Exception:
            continue


if __name__ == "__main__":
    import sys
    print("ERROR: This script is a plugin and cannot be run directly.",
          file=sys.stderr)
    print("Please run the central hub runner instead:", file=sys.stderr)
    print(
        f"  python3 agents/projects/code-health/hub/scripts/candidate_finder.py find --plugin {__file__}",
        file=sys.stderr)
    sys.exit(1)
