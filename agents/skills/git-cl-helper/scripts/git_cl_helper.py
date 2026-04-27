#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""git_cl_helper.py - Skill script to poll presubmit results."""

import argparse
import json
import re
import subprocess
import sys
import urllib.request


def _fetch_build_error(cq_message):
    """Extract build ID from a CQ failure message and fetch the error via bb
    get."""
    build_ids = re.findall(r'build/(\d+)', cq_message)
    for build_id in build_ids[:3]:
        try:
            r = subprocess.run(
                ["bb", "get", build_id, "-json"],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            if r.returncode == 0:
                data = json.loads(r.stdout)
                summary = data.get("summaryMarkdown", "")
                if summary:
                    return summary[:3000]
        except Exception as e:
            print(f"Failed to fetch build error for {build_id}: {e}",
                  file=sys.stderr)
    return cq_message[:3000]


def _poll_gerrit(gerrit_url):
    """Poll trybot status via the Gerrit API."""
    m = re.search(r'/(\d+)(?:/|$)', gerrit_url)
    if not m:
        print(f"Error: Invalid Gerrit URL: {gerrit_url}", file=sys.stderr)
        sys.exit(1)

    change_num = m.group(1)
    api_url = (f"https://chromium-review.googlesource.com/changes/{change_num}"
               f"/detail?o=MESSAGES&o=CURRENT_REVISION")

    print(f"Polling Gerrit API: {api_url}")
    req = urllib.request.Request(api_url,
                                 headers={"Accept": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            raw = resp.read().decode("utf-8")
            if raw.startswith(")]}'"):
                raw = raw[4:].strip()
            data = json.loads(raw)
    except Exception as e:
        print(f"Error accessing Gerrit API: {e}", file=sys.stderr)
        sys.exit(1)

    messages = data.get("messages", [])
    for msg in reversed(messages):
        text = msg.get("message", "")
        email = msg.get("author", {}).get("email", "")

        luci_email = (
            "chromium-scoped@luci-project-accounts.iam.gserviceaccount.com")
        if luci_email not in email:
            continue

        text_lower = text.lower()

        if "patch failure" in text_lower or "try rebasing" in text_lower:
            return "rebase_conflict", text[:500]

        if "this cl has failed" in text_lower or "failed tryjobs" in text_lower:
            detail = _fetch_build_error(text)
            return "failed", detail

        if ("passed the run" in text_lower
                or "this cl is ready to submit" in text_lower):
            return "passed", ""

        if "is trying" in text_lower or "dry run: cv" in text_lower:
            return "running", ""

    return "dry-run not started", ""


def _do_poll(args):
    """Poll status for a Gerrit CL."""
    url = args.gerrit_url
    status, detail = _poll_gerrit(url)
    print(f"STATUS: {status}")
    if detail:
        print(f"DETAIL:\n{detail}")


def main():
    parser = argparse.ArgumentParser(
        description="Script for Gerrit CL operations.")
    subparsers = parser.add_subparsers(dest="action", required=True)

    poll_parser = subparsers.add_parser("poll")
    poll_parser.add_argument("--gerrit_url",
                             required=True,
                             help="Gerrit CL URL")

    args = parser.parse_args()

    if args.action == "poll":
        _do_poll(args)


if __name__ == "__main__":
    main()
