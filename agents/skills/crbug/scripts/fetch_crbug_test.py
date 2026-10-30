#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for fetch_crbug.py."""

from __future__ import annotations

import copy
import io
import json
import os
import sys
import unittest
from unittest import mock
import urllib.error

# Ensure scripts directory is on sys.path for direct or module execution.
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

import fetch_crbug


def _make_sample_issue_jspb() -> list:
    """Builds a minimal IssueFetchResponse JSPB structure for testing."""
    model: list = [None] * 45
    model[1] = 40256175
    state: list = [None] * 15
    state[0] = 1456128  # component_id
    state[1] = 1  # BUG
    state[2] = 4  # FIXED
    state[3] = 2  # P1
    state[4] = 3  # S2
    state[5] = "Sample issue title"
    state[6] = [None, "reporter@chromium.org"]
    state[7] = [None, "assignee@chromium.org"]
    state[8] = [None, "verifier@chromium.org"]
    state[9] = [[None, "cc1@chromium.org"], [None, "cc2@chromium.org"]]
    state[10] = 40000000
    state[11] = [40000001]
    state[12] = [40000002]
    model[2] = state
    model[4] = [1700000000, 0]  # created_time
    model[5] = [1700003600, 0]  # modified_time
    model[6] = [1700007200, 0]  # resolved_time
    model[7] = [1700010800, 0]  # verified_time
    model[43] = ["Initial issue description"]

    response_body: list = [None] * 23
    response_body[22] = model
    return [["b.IssueFetchResponse", response_body]]


def _make_sample_updates_jspb() -> list:
    """Builds a minimal ListIssueUpdatesResponse JSPB structure for testing."""
    updates = [
        [
            [None, "dev@chromium.org"],
            [1700003600, 0],
            ["Follow-up comment"],
            2,
        ],
        [
            [None, "bot@chromium.org"],
            [1700001800, 0],
            None,  # Update without comment text
            None,
        ],
        [
            [None, "reporter@chromium.org"],
            [1700000000, 0],
            ["Initial issue description"],
            1,
        ],
    ]
    return [["b.ListIssueUpdatesResponse", [updates]]]


class ExtractIssueIdTest(unittest.TestCase):
    """Tests for extract_issue_id()."""

    def test_numeric_id(self):
        self.assertEqual(fetch_crbug.extract_issue_id("40256175"), "40256175")
        self.assertEqual(fetch_crbug.extract_issue_id("v8/12345"), "v8/12345")
        self.assertEqual(
            fetch_crbug.extract_issue_id("issues/40256175"), "40256175"
        )

    def test_crbug_url(self):
        self.assertEqual(
            fetch_crbug.extract_issue_id("https://crbug.com/1414530"),
            "1414530",
        )
        self.assertEqual(
            fetch_crbug.extract_issue_id("crbug.com/40256175"),
            "40256175",
        )
        self.assertEqual(
            fetch_crbug.extract_issue_id("https://crbug.com/v8/12345"),
            "v8/12345",
        )
        self.assertEqual(
            fetch_crbug.extract_issue_id("crbug/" + "40256175"),
            "40256175",
        )

    def test_issues_chromium_org_url(self):
        self.assertEqual(
            fetch_crbug.extract_issue_id(
                "https://issues.chromium.org/issues/40256175"
            ),
            "40256175",
        )
        self.assertEqual(
            fetch_crbug.extract_issue_id(
                "https://issues.chromium.org/u/0/issues/40256175"
            ),
            "40256175",
        )

    def test_monorail_detail_url(self):
        self.assertEqual(
            fetch_crbug.extract_issue_id(
                "https://bugs.chromium.org/p/chromium/issues/detail?id=1414530"
            ),
            "1414530",
        )
        self.assertEqual(
            fetch_crbug.extract_issue_id(
                "https://bugs.chromium.org/p/v8/issues/detail?id=12345"
            ),
            "v8/12345",
        )

    def test_invalid_id_raises(self):
        with self.assertRaises(ValueError) as ctx:
            fetch_crbug.extract_issue_id("not-an-id")
        self.assertIn("Invalid Chromium issue ID", str(ctx.exception))


class StripXssiPrefixTest(unittest.TestCase):
    """Tests for strip_xssi_prefix()."""

    def test_strips_prefix(self):
        raw = ")]}'\n\n[[1, 2]]"
        self.assertEqual(fetch_crbug.strip_xssi_prefix(raw), "[[1, 2]]")

    def test_no_prefix_unchanged(self):
        raw = "[[1, 2]]"
        self.assertEqual(fetch_crbug.strip_xssi_prefix(raw), "[[1, 2]]")


class ResolveMonorailIdTest(unittest.TestCase):
    """Tests for resolve_monorail_id()."""

    def test_modern_id_not_resolved(self):
        self.assertEqual(
            fetch_crbug.resolve_monorail_id("40256175"), "40256175"
        )
        self.assertEqual(
            fetch_crbug.resolve_monorail_id("v8/42202300"), "42202300"
        )

    @mock.patch("urllib.request.urlopen")
    def test_legacy_id_resolved_from_html_body(self, mock_urlopen):
        mock_resp = mock.MagicMock()
        mock_resp.geturl.return_value = (
            "https://bugs.chromium.org/p/chromium/issues/detail?id=1414530"
        )
        mock_resp.read.return_value = (
            b'<script>const url = "https://issues.chromium.org/40256175";'
            b"</script>"
        )
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        self.assertEqual(fetch_crbug.resolve_monorail_id("1414530"), "40256175")

    @mock.patch("urllib.request.urlopen")
    def test_legacy_id_resolved_from_redirect_url(self, mock_urlopen):
        mock_resp = mock.MagicMock()
        mock_resp.geturl.return_value = (
            "https://issues.chromium.org/issues/40256175"
        )
        mock_resp.read.return_value = b""
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        self.assertEqual(fetch_crbug.resolve_monorail_id("1414530"), "40256175")

    @mock.patch("urllib.request.urlopen")
    def test_legacy_id_unresolvable_raises(self, mock_urlopen):
        mock_resp = mock.MagicMock()
        mock_resp.geturl.return_value = "https://bugs.chromium.org/p/chromium"
        mock_resp.read.return_value = b"<html>No migration link</html>"
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.resolve_monorail_id("12345")
        self.assertIn("Could not resolve legacy Monorail", str(ctx.exception))

    @mock.patch(
        "urllib.request.urlopen",
        side_effect=urllib.error.HTTPError(
            url="https://crbug.com/123",
            code=404,
            msg="Not Found",
            hdrs=None,  # type: ignore[arg-type]
            fp=None,
        ),
    )
    def test_legacy_id_http_error_raises(self, _mock_urlopen):
        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.resolve_monorail_id("123")
        self.assertIn("HTTP 404", str(ctx.exception))

    @mock.patch(
        "urllib.request.urlopen",
        side_effect=urllib.error.URLError("Connection refused"),
    )
    def test_legacy_id_network_error_raises(self, _mock_urlopen):
        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.resolve_monorail_id("123")
        self.assertIn("Network error", str(ctx.exception))


class HttpRequestJspbTest(unittest.TestCase):
    """Tests for http_request_jspb()."""

    @mock.patch("urllib.request.urlopen")
    def test_get_success(self, mock_urlopen):
        mock_resp = mock.MagicMock()
        mock_resp.read.return_value = b")]}'\n\n[[\"ok\", 123]]"
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        result = fetch_crbug.http_request_jspb("https://issues.chromium.org/x")
        self.assertEqual(result, [["ok", 123]])
        req = mock_urlopen.call_args[0][0]
        self.assertEqual(req.get_method(), "GET")
        self.assertNotIn("Content-type", req.headers)

    @mock.patch("urllib.request.urlopen")
    def test_post_with_json_body(self, mock_urlopen):
        mock_resp = mock.MagicMock()
        mock_resp.read.return_value = b")]}'\n\n[[\"ok\"]]"
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        result = fetch_crbug.http_request_jspb(
            "https://issues.chromium.org/x",
            method="POST",
            json_body=[40256175],
        )
        self.assertEqual(result, [["ok"]])
        req = mock_urlopen.call_args[0][0]
        self.assertEqual(req.get_method(), "POST")
        self.assertEqual(req.data, b"[40256175]")
        self.assertEqual(req.headers.get("Content-type"), "application/json")

    @mock.patch("urllib.request.urlopen")
    def test_401_and_403_raise_access_denied(self, mock_urlopen):
        for code in (401, 403):
            with self.subTest(code=code):
                mock_urlopen.side_effect = urllib.error.HTTPError(
                    url="https://issues.chromium.org/x",
                    code=code,
                    msg="Forbidden",
                    hdrs=None,  # type: ignore[arg-type]
                    fp=None,
                )
                with self.assertRaises(RuntimeError) as ctx:
                    fetch_crbug.http_request_jspb(
                        "https://issues.chromium.org/x"
                    )
                self.assertIn("Access denied", str(ctx.exception))
                self.assertIn("buganizer-ticket", str(ctx.exception))

    @mock.patch(
        "urllib.request.urlopen",
        side_effect=urllib.error.HTTPError(
            url="https://issues.chromium.org/x",
            code=404,
            msg="Not Found",
            hdrs=None,  # type: ignore[arg-type]
            fp=None,
        ),
    )
    def test_404_raises_not_found(self, _mock_urlopen):
        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.http_request_jspb("https://issues.chromium.org/x")
        self.assertIn("Issue not found (HTTP 404)", str(ctx.exception))

    @mock.patch(
        "urllib.request.urlopen",
        side_effect=urllib.error.HTTPError(
            url="https://issues.chromium.org/x",
            code=500,
            msg="Internal Server Error",
            hdrs=None,  # type: ignore[arg-type]
            fp=None,
        ),
    )
    def test_500_raises_http_error(self, _mock_urlopen):
        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.http_request_jspb("https://issues.chromium.org/x")
        self.assertIn("HTTP request failed (HTTP 500)", str(ctx.exception))

    @mock.patch(
        "urllib.request.urlopen",
        side_effect=PermissionError("Operation not permitted"),
    )
    def test_permission_error_raises_sandbox_error(self, _mock_urlopen):
        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.http_request_jspb("https://issues.chromium.org/x")
        self.assertIn("sandbox or network restrictions", str(ctx.exception))

    @mock.patch(
        "urllib.request.urlopen",
        side_effect=urllib.error.URLError("DNS lookup failed"),
    )
    def test_url_error_raises_network_error(self, _mock_urlopen):
        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.http_request_jspb("https://issues.chromium.org/x")
        self.assertIn("Network error", str(ctx.exception))

    @mock.patch("urllib.request.urlopen")
    def test_invalid_json_raises(self, mock_urlopen):
        mock_resp = mock.MagicMock()
        mock_resp.read.return_value = b"not valid json"
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.http_request_jspb("https://issues.chromium.org/x")
        self.assertIn("Failed to decode response", str(ctx.exception))


class ParseJspbTest(unittest.TestCase):
    """Tests for parse_issue_jspb() and parse_comments_jspb()."""

    def test_parse_issue_jspb_full(self):
        jspb = _make_sample_issue_jspb()
        issue = fetch_crbug.parse_issue_jspb(jspb, "40256175")
        self.assertEqual(issue["issueId"], "40256175")
        self.assertEqual(
            issue["issueComment"], {"comment": "Initial issue description"}
        )
        self.assertEqual(issue["createdTime"], "2023-11-14T22:13:20Z")
        self.assertEqual(issue["modifiedTime"], "2023-11-14T23:13:20Z")
        self.assertEqual(issue["resolvedTime"], "2023-11-15T00:13:20Z")
        self.assertEqual(issue["verifiedTime"], "2023-11-15T01:13:20Z")

        state = issue["issueState"]
        self.assertEqual(state["componentId"], "1456128")
        self.assertEqual(state["type"], "BUG")
        self.assertEqual(state["status"], "FIXED")
        self.assertEqual(state["priority"], "P1")
        self.assertEqual(state["severity"], "S2")
        self.assertEqual(state["title"], "Sample issue title")
        self.assertEqual(
            state["reporter"], {"emailAddress": "reporter@chromium.org"}
        )
        self.assertEqual(
            state["assignee"], {"emailAddress": "assignee@chromium.org"}
        )
        self.assertEqual(
            state["verifier"], {"emailAddress": "verifier@chromium.org"}
        )
        self.assertEqual(
            state["ccs"],
            [
                {"emailAddress": "cc1@chromium.org"},
                {"emailAddress": "cc2@chromium.org"},
            ],
        )
        self.assertEqual(state["canonicalIssueId"], "40000000")
        self.assertEqual(state["blockedByIssueIds"], ["40000001"])
        self.assertEqual(state["blockingIssueIds"], ["40000002"])

    def test_parse_issue_jspb_missing_model_raises(self):
        with self.assertRaises(RuntimeError) as ctx:
            fetch_crbug.parse_issue_jspb([["b.IssueFetchResponse", []]], "999")
        self.assertIn("not found or inaccessible", str(ctx.exception))

    def test_parse_comments_jspb_sorts_and_filters(self):
        jspb = _make_sample_updates_jspb()
        comments = fetch_crbug.parse_comments_jspb(jspb)
        self.assertEqual(len(comments), 2)
        self.assertEqual(comments[0]["commentNumber"], 1)
        self.assertEqual(comments[0]["comment"], "Initial issue description")
        self.assertEqual(
            comments[0]["author"], {"emailAddress": "reporter@chromium.org"}
        )
        self.assertEqual(comments[1]["commentNumber"], 2)
        self.assertEqual(comments[1]["comment"], "Follow-up comment")
        self.assertEqual(
            comments[1]["author"], {"emailAddress": "dev@chromium.org"}
        )

    def test_parse_comments_jspb_empty(self):
        self.assertEqual(fetch_crbug.parse_comments_jspb([]), [])


class ElideDetailsBlocksTest(unittest.TestCase):
    """Tests for elide_details_blocks()."""

    def test_collapses_body_and_keeps_summary(self):
        text = (
            "Intro\n\n<details>\n\n<summary>Expand for full commit details"
            "</summary>\n\n```\nCommit message\n```\n\n</details>\n\n---"
        )
        self.assertEqual(
            fetch_crbug.elide_details_blocks(text),
            "Intro\n\n<details>\n"
            "<summary>Expand for full commit details</summary>\n"
            "[Elided. Rerun with --expand-details to show.]\n"
            "</details>\n\n---",
        )

    def test_multiline_summary_and_nested_blocks(self):
        text = (
            "<details open>\n<summary>\nOuter\nsummary\n</summary>\n"
            "<details>\n<summary>Inner</summary>\nBody\n</details>\n"
            "</details>\nAfter"
        )
        self.assertEqual(
            fetch_crbug.elide_details_blocks(text),
            "<details open>\n<summary>Outer summary</summary>\n"
            "[Elided. Rerun with --expand-details to show.]\n"
            "</details>\nAfter",
        )

    def test_without_summary(self):
        self.assertEqual(
            fetch_crbug.elide_details_blocks("<details>\nBody\n</details>"),
            "<details>\n[Elided. Rerun with --expand-details to show.]\n"
            "</details>",
        )

    def test_ignores_fenced_and_unterminated_blocks(self):
        fenced = "```\n<details>\nBody\n</details>\n```"
        self.assertEqual(fetch_crbug.elide_details_blocks(fenced), fenced)
        unterminated = "<details>\n<summary>S</summary>\nBody"
        self.assertEqual(
            fetch_crbug.elide_details_blocks(unterminated), unterminated
        )

    def test_ignores_inline_tags(self):
        text = "Use <details>Body</details> inline."
        self.assertEqual(fetch_crbug.elide_details_blocks(text), text)


class FetchIssueTest(unittest.TestCase):
    """Tests for fetch_issue()."""

    @mock.patch("fetch_crbug.resolve_monorail_id", return_value="40256175")
    @mock.patch("fetch_crbug.http_request_jspb")
    def test_fetch_issue_with_monorail_id(self, mock_http, mock_resolve):
        mock_http.side_effect = [
            _make_sample_issue_jspb(),
            _make_sample_updates_jspb(),
        ]
        result = fetch_crbug.fetch_issue("crbug.com/1414530")
        mock_resolve.assert_called_once_with("1414530")
        self.assertEqual(result["issue"]["issueId"], "40256175")
        self.assertEqual(result["issue"]["monorailId"], "1414530")
        self.assertEqual(len(result["comments"]), 2)

    @mock.patch("fetch_crbug.http_request_jspb")
    def test_fetch_issue_elides_details_unless_expanded(self, mock_http):
        details = "<details>\n<summary>S</summary>\nBody\n</details>"
        issue_jspb = _make_sample_issue_jspb()
        issue_jspb[0][1][22][43] = [details]
        updates_jspb = _make_sample_updates_jspb()
        updates_jspb[0][1][0][0][2] = [details]

        for expand_details in (False, True):
            mock_http.side_effect = [
                copy.deepcopy(issue_jspb),
                copy.deepcopy(updates_jspb),
            ]
            result = fetch_crbug.fetch_issue(
                "40256175", expand_details=expand_details
            )
            texts = [
                result["issue"]["issueComment"]["comment"],
                result["comments"][1]["comment"],
            ]
            for text in texts:
                self.assertEqual("Body" in text, expand_details)

    @mock.patch("fetch_crbug.resolve_monorail_id", return_value="40256175")
    @mock.patch("fetch_crbug.http_request_jspb")
    def test_fetch_issue_comments_failure_graceful_fallback(
        self, mock_http, _mock_resolve
    ):
        mock_http.side_effect = [
            _make_sample_issue_jspb(),
            RuntimeError("Updates endpoint timeout"),
        ]
        with mock.patch("sys.stderr", new_callable=io.StringIO) as mock_stderr:
            result = fetch_crbug.fetch_issue("40256175")
        self.assertEqual(result["issue"]["issueId"], "40256175")
        self.assertEqual(result["comments"], [])
        self.assertIn(
            "Warning: Failed to fetch comments", mock_stderr.getvalue()
        )


class MainTest(unittest.TestCase):
    """Tests for main() CLI entrypoint."""

    @mock.patch("sys.argv", ["fetch_crbug.py", "40256175"])
    @mock.patch(
        "fetch_crbug.fetch_issue",
        return_value={"issue": {"issueId": "40256175"}, "comments": []},
    )
    def test_main_success(self, mock_fetch):
        with mock.patch("sys.stdout", new_callable=io.StringIO) as mock_stdout:
            fetch_crbug.main()
            mock_fetch.assert_called_once_with("40256175", expand_details=False)
            output = json.loads(mock_stdout.getvalue())
            self.assertEqual(output["issue"]["issueId"], "40256175")

    @mock.patch("sys.argv", ["fetch_crbug.py", "invalid_id"])
    @mock.patch(
        "fetch_crbug.fetch_issue",
        side_effect=ValueError("Invalid Chromium issue ID"),
    )
    def test_main_error_exits(self, _mock_fetch):
        with mock.patch("sys.stderr", new_callable=io.StringIO) as mock_stderr:
            with self.assertRaises(SystemExit) as ctx:
                fetch_crbug.main()
        self.assertEqual(ctx.exception.code, 1)
        self.assertIn(
            "Error: Invalid Chromium issue ID", mock_stderr.getvalue()
        )


class SkillDocTest(unittest.TestCase):
    """Verifies SKILL.md documentation integrity and metadata."""

    def test_skill_md_exists_and_contains_valid_frontmatter(self):
        scripts_dir = os.path.dirname(__file__)
        skill_md_path = os.path.join(scripts_dir, "..", "SKILL.md")
        self.assertTrue(os.path.isfile(skill_md_path))
        with open(skill_md_path, encoding="utf-8") as f:
            content = f.read()

        self.assertTrue(content.startswith("---"))
        self.assertIn("name: crbug", content)
        self.assertIn("description:", content)
        self.assertIn("fetch_crbug.py", content)


if __name__ == "__main__":
    unittest.main()
