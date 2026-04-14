# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds expired Chromium histograms from metadata directories."""

import argparse
import glob
import os
import random
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


def find_expired_histograms(date_limit, m_limit):
    """Generator yielding expired histograms from the metadata directory."""
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
                    "summary": summary,
                    "owners": owners
                }
        except Exception:
            continue


def main():
    parser = argparse.ArgumentParser(description="Find expired histograms.")
    parser.add_argument("--count", type=int, default=3)
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

    all_histograms = list(find_expired_histograms(date_limit, m_limit))
    if not all_histograms:
        print("No expired histograms found.")
        return

    # Randomly sample candidates to reduce duplicate effort between users.
    sampled = random.sample(all_histograms,
                            k=min(len(all_histograms), args.count))

    for h in sampled:
        summary = h["summary"][:150] + "..." if h["summary"] else "No summary."
        owners_str = ", ".join(h["owners"]) if h["owners"] else "No owners."

        print(f"File: {h['file']}\nName: {h['name']}")
        print(f"Owners: {owners_str}")
        print(f"Expiry: {h['expires_after']}")
        print(f"Summary: {summary}\n---")


if __name__ == "__main__":
    main()
