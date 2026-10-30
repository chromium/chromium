#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fetches public Chromium issue details and comments from issues.chromium.org.

Supports both migrated IssueTracker IDs (>= 10,000,000) and legacy Monorail IDs
(< 10,000,000) by resolving crbug.com redirects.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import re
import sys
from typing import Any
import urllib.error
import urllib.request

ISSUES_BASE_URL = "https://issues.chromium.org"
CRBUG_BASE_URL = "https://crbug.com"
CHROMIUM_TRACKER_ID = 157
MONORAIL_MAX_ID = 10_000_000

ISSUE_TYPE_MAP = {
    1: "BUG",
    2: "FEATURE_REQUEST",
    3: "CUSTOMER_ISSUE",
    4: "INTERNAL_CLEANUP",
    5: "PROCESS",
    6: "VULNERABILITY",
    7: "PRIVACY_ISSUE",
    8: "PORTFOLIO",
    9: "PROGRAM",
    10: "PROJECT",
    11: "FEATURE",
    12: "MILESTONE",
    13: "EPIC",
    14: "STORY",
    15: "TASK",
}

ISSUE_STATUS_MAP = {
    1: "NEW",
    2: "ASSIGNED",
    3: "ACCEPTED",
    4: "FIXED",
    5: "VERIFIED",
    6: "NOT_REPRODUCIBLE",
    7: "INTENDED_BEHAVIOR",
    8: "OBSOLETE",
    9: "INFEASIBLE",
    10: "DUPLICATE",
    11: "INACTIVE",
}

ISSUE_PRIORITY_MAP = {
    1: "P0",
    2: "P1",
    3: "P2",
    4: "P3",
    5: "P4",
}

ISSUE_SEVERITY_MAP = {
    1: "S0",
    2: "S1",
    3: "S2",
    4: "S3",
    5: "S4",
}

# Markdown code fence, and `<details>` open/close tags on their own lines.
_FENCE_RE = re.compile(r"^ {0,3}(`{3,}|~{3,})")
_DETAILS_OPEN_RE = re.compile(
    r"^ {0,3}<details(?:\s[^<>]*)?>\s*$", re.IGNORECASE
)
_DETAILS_CLOSE_RE = re.compile(r"^ {0,3}</details>\s*$", re.IGNORECASE)
_SUMMARY_RE = re.compile(
    r"^\s*<summary>(.*?)</summary>", re.IGNORECASE | re.DOTALL
)


def extract_issue_id(raw_id: str) -> str:
    """Extracts the issue ID (or project/ID) from an ID string or URL."""
    # Prefer matching explicit issue/detail URL patterns if a full URL is given.
    if monorail_match := re.search(
        r"bugs\.chromium\.org/p/([a-zA-Z0-9_-]+)/issues/detail"
        r"\?[^#]*\bid=(\d+)",
        raw_id,
    ):
        project, num_id = monorail_match.groups()
        return num_id if project == "chromium" else f"{project}/{num_id}"
    if crbug_match := re.search(
        r"crbug(?:\.com)?/(?:([a-zA-Z0-9_-]+)/)?(\d+)", raw_id
    ):
        project, num_id = crbug_match.groups()
        return (
            f"{project}/{num_id}"
            if project and project != "chromium"
            else num_id
        )
    url_match = re.search(
        r"(?:issues\.chromium\.org/(?:u/\d+/)?(?:issues/)?|[?&]id=)(\d+)",
        raw_id,
    )
    if url_match:
        return url_match.group(1)
    match = re.search(r"(?:(?<!/)\b([a-zA-Z0-9_-]+)/)?(\d+)", raw_id)
    if not match:
        raise ValueError(
            f"Invalid Chromium issue ID '{raw_id}'. "
            "ID must contain numeric digits."
        )
    project, num_id = match.groups()
    if project and project.lower() not in (
        "chromium",
        "crbug",
        "issue",
        "issues",
        "bug",
        "bugs",
        "b",
    ):
        return f"{project}/{num_id}"
    return num_id


def strip_xssi_prefix(text: str) -> str:
    """Strips the leading XSSI prefix (')]}\\'\\n') from a JSPB response."""
    return re.sub(r"^\)\]\}'\r?\n*", "", text.lstrip())


def format_timestamp(ts: Any) -> str | None:
    """Formats a JSPB [seconds, nanos] timestamp as an ISO-8601 UTC string."""
    if not isinstance(ts, list) or not ts or not isinstance(ts[0], int):
        return None
    dt = datetime.fromtimestamp(ts[0], tz=timezone.utc)
    return dt.strftime("%Y-%m-%dT%H:%M:%SZ")


def _safe_get(seq: Any, idx: int, default: Any = None) -> Any:
    """Safely indexes into a list, returning default if out of bounds."""
    if isinstance(seq, list) and 0 <= idx < len(seq):
        val = seq[idx]
        return default if val is None else val
    return default


def _parse_user(user_raw: Any) -> dict[str, str] | None:
    """Extracts a User dict from a JSPB User array."""
    email = _safe_get(user_raw, 1)
    if isinstance(email, str) and email:
        return {"emailAddress": email}
    return None


def _collapse_details_block(block: list[str]) -> list[str]:
    """Replaces the body of a `<details>` block, keeping its summary."""
    collapsed = [block[0]]
    if summary_match := _SUMMARY_RE.match("\n".join(block[1:-1])):
        summary = " ".join(summary_match.group(1).split())
        collapsed.append(f"<summary>{summary}</summary>")
    collapsed.append("[Elided. Rerun with --expand-details to show.]")
    collapsed.append(block[-1])
    return collapsed


def elide_details_blocks(text: str) -> str:
    """Collapses the bodies of top-level `<details>` blocks in Markdown.

    These blocks (e.g. the full commit details posted by the Git Watcher bot)
    can bloat the output with content that is rarely relevant. Similar to
    what the Buganizer MCP server does, only the `<summary>` is kept. Tags
    inside fenced code blocks are ignored.
    """
    out: list[str] = []
    block: list[str] = []
    fence: str | None = None
    depth = 0
    for line in text.split("\n"):
        if fence_match := _FENCE_RE.match(line):
            marker = fence_match.group(1)
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
        elif fence is None:
            if _DETAILS_OPEN_RE.match(line):
                depth += 1
            elif depth and _DETAILS_CLOSE_RE.match(line):
                depth -= 1
                if depth == 0:
                    block.append(line)
                    out.extend(_collapse_details_block(block))
                    block = []
                    continue
        (block if depth else out).append(line)
    # Leave unterminated blocks untouched.
    out.extend(block)
    return "\n".join(out)


def resolve_monorail_id(issue_id: str) -> str:
    """Resolves a legacy Monorail ID (< 10,000,000) to an IssueTracker ID."""
    num_id = int(issue_id.rsplit("/", 1)[-1])
    if num_id >= MONORAIL_MAX_ID:
        return str(num_id)

    url = f"{CRBUG_BASE_URL}/{issue_id}"
    req = urllib.request.Request(url, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            final_url = resp.geturl()
            body = resp.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        raise RuntimeError(
            f"Failed to resolve legacy Monorail ID {issue_id} (HTTP {e.code})."
        ) from e
    except (urllib.error.URLError, PermissionError) as e:
        raise RuntimeError(
            f"Network error resolving legacy Monorail ID {issue_id}: {e}"
        ) from e

    for text in (final_url, body):
        match = re.search(
            r"https://issues\.chromium\.org/(?:issues/)?(\d+)", text
        )
        if match:
            return match.group(1)

    raise RuntimeError(
        f"Could not resolve legacy Monorail issue ID {issue_id} to an "
        "issues.chromium.org ID."
    )


def http_request_jspb(
    url: str,
    method: str = "GET",
    json_body: Any = None,
) -> Any:
    """Executes an HTTP request to issues.chromium.org and decodes JSPB."""
    headers: dict[str, str] = {}
    data: bytes | None = None
    if json_body is not None:
        headers["Content-Type"] = "application/json"
        data = json.dumps(json_body).encode("utf-8")

    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        if e.code in (401, 403):
            raise RuntimeError(
                f"Access denied fetching Chromium issue (HTTP {e.code}). "
                "The issue may be restricted to signed-in/trusted users "
                "(e.g., histogram-expiry bugs, security bugs, or internal "
                "visibility). If working in an internal checkout, try the "
                "'buganizer-ticket' skill."
            ) from e
        if e.code == 404:
            raise RuntimeError(
                "Issue not found (HTTP 404). Please verify that the issue ID "
                "is correct and publicly accessible."
            ) from e
        raise RuntimeError(f"HTTP request failed (HTTP {e.code}): {url}") from e
    except PermissionError as e:
        raise RuntimeError(
            f"Failed to connect to issues.chromium.org due to sandbox or "
            f"network restrictions ({e}). Re-run outside the sandbox if "
            "outbound network access is restricted."
        ) from e
    except urllib.error.URLError as e:
        raise RuntimeError(
            f"Network error connecting to issues.chromium.org: {e}"
        ) from e

    cleaned = strip_xssi_prefix(raw)
    try:
        return json.loads(cleaned)
    except json.JSONDecodeError as e:
        raise RuntimeError(
            f"Failed to decode response from {url} as JSON."
        ) from e


def parse_issue_jspb(data: Any, issue_id: str) -> dict[str, Any]:
    """Converts an IssueFetchResponse JSPB array into an Issue dict."""
    response_body = _safe_get(_safe_get(data, 0), 1)
    model = _safe_get(response_body, 22)
    if not isinstance(model, list):
        raise RuntimeError(
            f"Issue {issue_id} not found or inaccessible on "
            "issues.chromium.org (may require authentication)."
        )

    resolved_id = str(_safe_get(model, 1, issue_id))
    state_raw = _safe_get(model, 2, [])

    comp_id = _safe_get(state_raw, 0)
    type_id = _safe_get(state_raw, 1)
    status_id = _safe_get(state_raw, 2)
    priority_id = _safe_get(state_raw, 3)
    severity_id = _safe_get(state_raw, 4)
    title = _safe_get(state_raw, 5, "")

    issue_state: dict[str, Any] = {
        "componentId": str(comp_id) if comp_id is not None else "",
        "type": ISSUE_TYPE_MAP.get(type_id, str(type_id or "TYPE_UNSPECIFIED")),
        "status": ISSUE_STATUS_MAP.get(
            status_id, str(status_id or "STATUS_UNSPECIFIED")
        ),
        "priority": ISSUE_PRIORITY_MAP.get(
            priority_id, str(priority_id or "PRIORITY_UNSPECIFIED")
        ),
        "severity": ISSUE_SEVERITY_MAP.get(
            severity_id, str(severity_id or "SEVERITY_UNSPECIFIED")
        ),
        "title": title,
    }

    if reporter := _parse_user(_safe_get(state_raw, 6)):
        issue_state["reporter"] = reporter
    if assignee := _parse_user(_safe_get(state_raw, 7)):
        issue_state["assignee"] = assignee
    if verifier := _parse_user(_safe_get(state_raw, 8)):
        issue_state["verifier"] = verifier

    ccs_raw = _safe_get(state_raw, 9, [])
    if isinstance(ccs_raw, list):
        ccs = [u for item in ccs_raw if (u := _parse_user(item)) is not None]
        if ccs:
            issue_state["ccs"] = ccs

    if canonical_id := _safe_get(state_raw, 10):
        issue_state["canonicalIssueId"] = str(canonical_id)
    if blocked_by := _safe_get(state_raw, 11):
        issue_state["blockedByIssueIds"] = [str(i) for i in blocked_by]
    if blocking := _safe_get(state_raw, 12):
        issue_state["blockingIssueIds"] = [str(i) for i in blocking]

    description_raw = _safe_get(model, 43, [])
    description_text = _safe_get(description_raw, 0, "")

    issue: dict[str, Any] = {
        "issueId": resolved_id,
        "issueState": issue_state,
        "issueComment": {"comment": description_text},
    }

    if created := format_timestamp(_safe_get(model, 4)):
        issue["createdTime"] = created
    if modified := format_timestamp(_safe_get(model, 5)):
        issue["modifiedTime"] = modified
    if resolved := format_timestamp(_safe_get(model, 6)):
        issue["resolvedTime"] = resolved
    if verified := format_timestamp(_safe_get(model, 7)):
        issue["verifiedTime"] = verified

    return issue


def parse_comments_jspb(data: Any) -> list[dict[str, Any]]:
    """Converts a ListIssueUpdatesResponse JSPB array into a comments list."""
    updates = _safe_get(_safe_get(_safe_get(data, 0), 1), 0, [])
    if not isinstance(updates, list):
        return []

    comments: list[dict[str, Any]] = []
    for update in updates:
        if not isinstance(update, list):
            continue
        comment_raw = _safe_get(update, 2)
        comment_text = _safe_get(comment_raw, 0)
        if not isinstance(comment_text, str) or not comment_text:
            continue

        comment_num = _safe_get(update, 3)
        entry: dict[str, Any] = {
            "commentNumber": comment_num,
            "comment": comment_text,
        }
        if author := _parse_user(_safe_get(update, 0)):
            entry["author"] = author
        if ts := format_timestamp(_safe_get(update, 1)):
            entry["timestamp"] = ts
        comments.append(entry)

    comments.sort(
        key=lambda c: (
            c["commentNumber"] if isinstance(c.get("commentNumber"), int) else 0
        )
    )
    return comments


def fetch_issue(raw_id: str, expand_details: bool = False) -> dict[str, Any]:
    """Fetches full issue details and comments from issues.chromium.org.

    Unless `expand_details` is set, `<details>` blocks are collapsed.
    """
    extracted_id = extract_issue_id(raw_id)
    issue_id = resolve_monorail_id(extracted_id)

    issue_url = f"{ISSUES_BASE_URL}/action/issues/{issue_id}"
    issue_jspb = http_request_jspb(issue_url, method="GET")
    issue_data = parse_issue_jspb(issue_jspb, issue_id)
    if extracted_id.rsplit("/", 1)[-1] != issue_id:
        issue_data["monorailId"] = extracted_id

    comments_url = (
        f"{ISSUES_BASE_URL}/action/issues/{issue_id}/updates"
        f"?currentTrackerId={CHROMIUM_TRACKER_ID}"
    )
    try:
        comments_jspb = http_request_jspb(
            comments_url,
            method="POST",
            json_body=[int(issue_id)],
        )
        comments = parse_comments_jspb(comments_jspb)
    except Exception as e:
        sys.stderr.write(f"Warning: Failed to fetch comments: {e}\n")
        comments = []

    if not expand_details:
        description = issue_data["issueComment"]
        description["comment"] = elide_details_blocks(description["comment"])
        for comment in comments:
            comment["comment"] = elide_details_blocks(comment["comment"])

    return {
        "issue": issue_data,
        "comments": comments,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Fetch public Chromium issue details and comments from "
            "issues.chromium.org."
        )
    )
    parser.add_argument(
        "issue_id",
        help="Chromium Issue ID or URL (e.g. 40256175 or crbug.com/1422675)",
    )
    parser.add_argument(
        "--expand-details",
        action="store_true",
        help="Show the bodies of <details> blocks instead of eliding them.",
    )
    args = parser.parse_args()

    try:
        data = fetch_issue(args.issue_id, expand_details=args.expand_details)
    except Exception as e:
        sys.stderr.write(f"Error: {e}\n")
        sys.exit(1)
    print(json.dumps(data, indent=2))


if __name__ == "__main__":
    main()
