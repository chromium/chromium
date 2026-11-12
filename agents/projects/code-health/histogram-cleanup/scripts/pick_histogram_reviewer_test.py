# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for pick_histogram_reviewer.py (fully offline)."""

import io
import json
import os
import subprocess
import sys
import unittest
import urllib.parse
from datetime import date, datetime, timezone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pick_histogram_reviewer as phr  # noqa: E402

# Monday 2026-11-09 and Wednesday 2026-11-11.
_NOW_MON = datetime(2026, 11, 9, 10, 0, tzinfo=timezone.utc)
_NOW_WED = datetime(2026, 11, 11, 10, 0, tzinfo=timezone.utc)
_NOW_FRI = datetime(2026, 11, 13, 10, 0, tzinfo=timezone.utc)

_MOMA_ACTIVE = (0, '[{"ldap": "someone"}]', "")
_MOMA_NOT_FOUND = (1, "", "rpc error: person not found")
_MOMA_GROUP = (0, "null", "")
_GERRIT_404 = (22, "", "curl: (22) The requested URL returned error: 404")
_GERRIT_DOWN = (6, "", "curl: (6) Could not resolve host")


def _gerrit(name, xssi=True):
    """Gerrit /detail reply for an account with the given full name."""
    body = json.dumps({"_account_id": 1000, "name": name})
    return 0, (")]}'\n" if xssi else "") + body, ""


def _people(*pairs):
    """Moma `person` reply with active (username, name) entries."""
    return (
        0,
        json.dumps(
            [{"username": username, "name": name} for username, name in pairs]
        ),
        "",
    )


def _stub(username):
    """Moma `person` reply that only echoes the username."""
    return 0, json.dumps([{"username": username}]), ""


class _CalendarError:
    def __init__(self, stderr="403 forbidden"):
        self.stderr = stderr


class _CalendarRaw:
    """Calendar CLI that exits 0 with the given raw stdout."""

    def __init__(self, stdout):
        self.stdout = stdout


def _which(name):
    return f"/usr/bin/{name}"


def _exists(_path):
    return True


def _ooo(start, end, **extra):
    """All-day outOfOffice event; `end` is exclusive."""
    event = {
        "eventType": "outOfOffice",
        "summary": "Out of office",
        "start": {"date": start},
        "end": {"date": end},
    }
    event.update(extra)
    return event


def _timed(start, end, summary="Vacation", event_type="default", **extra):
    event = {
        "eventType": event_type,
        "summary": summary,
        "start": {"dateTime": start},
        "end": {"dateTime": end},
    }
    event.update(extra)
    return event


class FakeRunner:
    """Stand-in for subprocess.run that serves canned CLI responses.

    moma maps get-profile LDAPs to replies. persons maps `moma_cli person`
    queries to replies; by default an LDAP-like query returns one active
    person with that username and a name derived from it, and a name query
    returns no one. gerrit maps lowercase @chromium.org emails to curl replies
    and defaults to a 404.
    """

    def __init__(
        self, moma=None, calendars=None, gcert_rc=0, persons=None, gerrit=None
    ):
        self.moma = moma or {}
        self.calendars = calendars or {}
        self.gcert_rc = gcert_rc
        self.persons = persons or {}
        self.gerrit = gerrit or {}
        self.calls = []

    def __call__(self, cmd, **kwargs):
        self.calls.append(list(cmd))
        exe = os.path.basename(cmd[0])
        if exe == "gcertstatus":
            return subprocess.CompletedProcess(
                cmd, self.gcert_rc, "", "no cert"
            )
        if exe == "curl":
            assert cmd[1:4] == ["-sSf", "--max-time", "10"], cmd
            assert len(cmd) == 5, cmd
            url = cmd[-1]
            prefix = "https://chromium-review.googlesource.com/accounts/"
            assert url.startswith(prefix) and url.endswith("/detail"), cmd
            account = urllib.parse.unquote(url[len(prefix) : -len("/detail")])
            rc, out, err = self.gerrit.get(account, _GERRIT_404)
            return subprocess.CompletedProcess(cmd, rc, out, err)
        if exe == "moma_cli":
            assert cmd[1] in ("get-profile", "person"), cmd
            assert cmd[3] == "--json", cmd
            if cmd[1] == "person":
                if cmd[2] in self.persons:
                    rc, out, err = self.persons[cmd[2]]
                elif " " in cmd[2]:
                    rc, out, err = 0, "[]", ""
                else:
                    rc, out, err = _people((cmd[2], cmd[2].title()))
            else:
                rc, out, err = self.moma.get(cmd[2], _MOMA_ACTIVE)
            return subprocess.CompletedProcess(cmd, rc, out, err)
        if exe == "gcalendar":
            assert cmd[1:3] == ["readonly", "events"], cmd
            assert "--json" in cmd
            ldap = cmd[cmd.index("--cal") + 1].split("@")[0]
            entry = self.calendars.get(ldap, [])
            if isinstance(entry, _CalendarError):
                return subprocess.CompletedProcess(cmd, 1, "", entry.stderr)
            if isinstance(entry, _CalendarRaw):
                return subprocess.CompletedProcess(cmd, 0, entry.stdout, "")
            return subprocess.CompletedProcess(cmd, 0, json.dumps(entry), "")
        raise AssertionError(f"Unexpected command: {cmd}")

    def moma_ldaps(self):
        return [
            c[2] for c in self.calls if os.path.basename(c[0]) == "moma_cli"
        ]

    def person_queries(self):
        return [
            c[2]
            for c in self.calls
            if os.path.basename(c[0]) == "moma_cli" and c[1] == "person"
        ]

    def profile_ldaps(self):
        return [
            c[2]
            for c in self.calls
            if os.path.basename(c[0]) == "moma_cli" and c[1] == "get-profile"
        ]

    def gerrit_accounts(self):
        return [
            urllib.parse.unquote(c[-1].split("/accounts/")[1].rsplit("/", 1)[0])
            for c in self.calls
            if os.path.basename(c[0]) == "curl"
        ]

    def calendar_ldaps(self):
        return [
            c[c.index("--cal") + 1].split("@")[0]
            for c in self.calls
            if os.path.basename(c[0]) == "gcalendar"
        ]


def _recommend(owners, runner, now=_NOW_MON, **kwargs):
    return phr.recommend_reviewer(
        owners, runner=runner, now=now, which=_which, exists=_exists, **kwargs
    )


def _statuses(result):
    return {c["owner"]: c["status"] for c in result["checks"]}


class OwnerSelectionTest(unittest.TestCase):
    def test_first_owner_available_short_circuits(self):
        runner = FakeRunner()
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        self.assertEqual(result["recommended"]["position"], 1)
        self.assertIsNone(result["warning"])
        self.assertEqual(runner.profile_ldaps(), ["a"])
        self.assertNotIn("b", runner.moma_ldaps())
        self.assertEqual(runner.calendar_ldaps(), ["a"])
        self.assertEqual(result["skipped"], [])

    def test_chromium_org_owner_maps_to_google_com_calendar(self):
        runner = FakeRunner()
        result = _recommend(["a@chromium.org"], runner)
        # The display email keeps the original XML <owner> string.
        self.assertEqual(result["recommended"]["owner"], "a@chromium.org")
        self.assertEqual(runner.moma_ldaps(), ["a"])
        cal_cmd = [c for c in runner.calls if c[0].endswith("gcalendar")][0]
        self.assertEqual(cal_cmd[cal_cmd.index("--cal") + 1], "a@google.com")

    def test_left_google_picks_second_owner(self):
        runner = FakeRunner(moma={"a": _MOMA_NOT_FOUND})
        result = _recommend(["a@google.com", "b@chromium.org"], runner)
        self.assertEqual(result["recommended"]["owner"], "b@chromium.org")
        self.assertEqual(result["recommended"]["position"], 2)
        self.assertEqual(_statuses(result)["a@google.com"], phr.LEFT_GOOGLE)
        self.assertEqual(result["skipped"][0]["note"], "left Google")
        self.assertEqual(runner.calendar_ldaps(), ["b"])

    def test_unmapped_chromium_org_owner_is_not_called_left_google(self):
        runner = FakeRunner(
            moma={"a": _MOMA_NOT_FOUND}, persons={"a": _MOMA_NOT_FOUND}
        )
        result = _recommend(["a@chromium.org", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "b@google.com")
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.NOT_IN_MOMA)
        self.assertEqual(
            check["note"], "left Google or different corp username"
        )

    def test_team_alias_is_skipped_as_group(self):
        runner = FakeRunner(moma={"team": _MOMA_GROUP})
        result = _recommend(["team@google.com", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "b@google.com")
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.GROUP_ALIAS)
        self.assertEqual(
            check["note"], "team or group alias, not an individual"
        )
        # Aliases never reach Calendar.
        self.assertEqual(runner.calendar_ldaps(), ["b"])

    def test_empty_list_profile_is_group_alias(self):
        runner = FakeRunner(moma={"team": (0, "[]", "")})
        result = _recommend(["team@google.com", "b@google.com"], runner)
        self.assertEqual(_statuses(result)["team@google.com"], phr.GROUP_ALIAS)

    def test_external_owner_recommended_without_lookup_and_notes_googler(self):
        runner = FakeRunner()
        result = _recommend(
            ["x@microsoft.com", "b@google.com", "c@google.com"], runner
        )
        self.assertEqual(result["recommended"]["owner"], "x@microsoft.com")
        self.assertEqual(result["recommended"]["position"], 1)
        self.assertEqual(runner.moma_ldaps(), ["b"])
        self.assertEqual(runner.calendar_ldaps(), ["b"])
        self.assertEqual(
            result["notes"],
            ["Next active Googler: b@google.com (2nd listed XML owner)"],
        )
        self.assertEqual(
            result["checks"][0]["note"],
            "external contributor; cannot verify Moma/OOO",
        )

    def test_external_owner_alone(self):
        runner = FakeRunner()
        result = _recommend(["x@microsoft.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "x@microsoft.com")
        self.assertEqual(result["notes"], [])
        self.assertEqual(runner.moma_ldaps(), [])

    def test_owners_file_and_non_email_entries_are_skipped(self):
        runner = FakeRunner()
        result = _recommend(
            [
                "src/chrome/browser/OWNERS",
                "per-file_OWNERS",
                "nobody",
                "a@google.com",
            ],
            runner,
        )
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        self.assertEqual(result["recommended"]["position"], 4)
        notes = [c["note"] for c in result["checks"][:3]]
        self.assertEqual(
            notes,
            [
                "OWNERS file reference",
                "OWNERS file reference",
                "non-email entry",
            ],
        )
        self.assertEqual(runner.profile_ldaps(), ["a"])

    def test_moma_rpc_failure_is_check_error_not_left_google(self):
        runner = FakeRunner(moma={"a": (2, "", "deadline exceeded")})
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.CHECK_ERROR)
        self.assertNotIn("left Google", result["checks"][0]["note"])
        self.assertEqual(result["recommended"]["owner"], "b@google.com")

    def test_unexpected_moma_json_is_check_error(self):
        runner = FakeRunner(moma={"a": (0, '{"ldap": "a"}', "")})
        result = _recommend(["a@google.com"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.CHECK_ERROR)
        self.assertIsNone(result["recommended"])

    def test_runner_timeout_is_check_error(self):

        def runner(cmd, **kwargs):
            if cmd[0].endswith("gcertstatus"):
                return subprocess.CompletedProcess(cmd, 0, "", "")
            raise subprocess.TimeoutExpired(cmd, 15)

        result = _recommend(["a@google.com"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.CHECK_ERROR)
        self.assertIsNone(result["recommended"])


class OutOfOfficeTest(unittest.TestCase):
    def test_long_all_day_ooo_is_skipped(self):
        runner = FakeRunner(
            calendars={"a": [_ooo("2026-11-10", "2026-11-19")]}
        )  # 9 days.
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "b@google.com")
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.OOO_LONG)
        self.assertEqual(check["note"], "OOO 9d (2026-11-10 to 2026-11-18)")

    def test_five_day_ooo_is_kept(self):
        runner = FakeRunner(
            calendars={"a": [_ooo("2026-11-09", "2026-11-14")]}
        )  # Mon-Fri.
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        self.assertEqual(result["checks"][0]["ooo_days"], 5)
        self.assertEqual(len(result["notes"]), 1)
        self.assertIn("OOO 5d", result["notes"][0])

    def test_seven_days_kept_eight_days_skipped(self):
        seven = FakeRunner(calendars={"a": [_ooo("2026-11-09", "2026-11-16")]})
        self.assertEqual(
            _recommend(["a@google.com"], seven)["recommended"]["owner"],
            "a@google.com",
        )
        eight = FakeRunner(calendars={"a": [_ooo("2026-11-09", "2026-11-17")]})
        self.assertEqual(
            _statuses(_recommend(["a@google.com"], eight))["a@google.com"],
            phr.OOO_LONG,
        )

    def test_threshold_flag_is_respected(self):
        runner = FakeRunner(calendars={"a": [_ooo("2026-11-10", "2026-11-19")]})
        result = _recommend(["a@google.com"], runner, threshold_days=10)
        self.assertEqual(result["recommended"]["owner"], "a@google.com")

    def test_weekend_bridge_makes_eight_day_span(self):
        events = [
            _ooo("2026-11-12", "2026-11-14"),  # Thu-Fri.
            _ooo("2026-11-16", "2026-11-20"),  # Mon-Thu.
        ]
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(
            ["a@google.com", "b@google.com"], runner, now=_NOW_WED
        )
        self.assertEqual(result["checks"][0]["status"], phr.OOO_LONG)
        self.assertEqual(result["checks"][0]["ooo_days"], 8)
        self.assertEqual(result["recommended"]["owner"], "b@google.com")

    def test_weekend_bridge_five_day_span_is_kept(self):
        events = [
            _ooo("2026-11-13", "2026-11-14"),  # Fri.
            _ooo("2026-11-16", "2026-11-18"),  # Mon-Tue.
        ]
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(
            ["a@google.com", "b@google.com"], runner, now=_NOW_WED
        )
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        self.assertEqual(result["checks"][0]["ooo_days"], 5)

    def test_compute_ooo_spans_does_not_bridge_midweek_gaps(self):
        spans = phr.compute_ooo_spans(
            [
                date(2026, 11, 10),
                date(2026, 11, 11),
                date(2026, 11, 13),
                date(2026, 11, 16),
            ]
        )
        self.assertEqual(
            spans,
            [
                (date(2026, 11, 10), date(2026, 11, 11)),
                (date(2026, 11, 13), date(2026, 11, 16)),
            ],
        )

    def test_future_ooo_beyond_next_working_day_is_ignored(self):
        events = [_ooo("2026-11-16", "2026-11-28")]  # 12 days, starts Monday.
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(["a@google.com"], runner, now=_NOW_WED)
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        self.assertEqual(result["notes"], [])

    def test_friday_counts_ooo_starting_next_monday(self):
        events = [_ooo("2026-11-16", "2026-11-28")]
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(
            ["a@google.com", "b@google.com"], runner, now=_NOW_FRI
        )
        self.assertEqual(result["checks"][0]["status"], phr.OOO_LONG)
        self.assertEqual(result["recommended"]["owner"], "b@google.com")

    def test_ignored_events_do_not_make_owner_unavailable(self):
        events = [
            _ooo("2026-11-10", "2026-11-25", status="cancelled"),
            _ooo(
                "2026-11-10",
                "2026-11-25",
                attendees=[
                    {"email": "a@google.com", "responseStatus": "declined"}
                ],
            ),
            _ooo(
                "2026-11-10",
                "2026-11-25",
                attendees=[
                    {
                        "email": "other@google.com",
                        "self": True,
                        "responseStatus": "declined",
                    }
                ],
            ),
            {
                "eventType": "workingLocation",
                "summary": "Out of office",
                "start": {"date": "2026-11-10"},
                "end": {"date": "2026-11-25"},
            },
        ]
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(["a@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        self.assertEqual(result["notes"], [])

    def test_is_ooo_event_title_matching(self):
        def event(summary, **extra):
            return _timed(
                "2026-11-10T09:00:00+00:00",
                "2026-11-10T17:00:00+00:00",
                summary=summary,
                **extra,
            )

        self.assertTrue(phr.is_ooo_event(event("Vacation"), "a"))
        self.assertTrue(phr.is_ooo_event(event("PTO"), "a"))
        self.assertTrue(phr.is_ooo_event(event("Sabbatical"), "a"))
        self.assertFalse(phr.is_ooo_event(event("OOO - Gym"), "a"))
        self.assertFalse(phr.is_ooo_event(event("Holiday party"), "a"))
        self.assertFalse(phr.is_ooo_event(event("Team sync"), "a"))
        self.assertFalse(phr.is_ooo_event(event("Lunch"), "a"))
        meeting = event(
            "Vacation planning leave",
            attendees=[
                {"email": "a@google.com", "responseStatus": "accepted"},
                {"email": "b@google.com", "responseStatus": "accepted"},
            ],
        )
        self.assertFalse(phr.is_ooo_event(meeting, "a"))
        with_room = event(
            "Vacation",
            attendees=[
                {"email": "a@google.com"},
                {
                    "email": "room@resource.calendar.google.com",
                    "resource": True,
                },
            ],
        )
        self.assertTrue(phr.is_ooo_event(with_room, "a"))
        self.assertTrue(
            phr.is_ooo_event(event("Anything", eventType="outOfOffice"), "a")
        )

    def test_default_vacation_event_counts_toward_long_ooo(self):
        events = [
            _timed("2026-11-10T09:00:00+00:00", "2026-11-20T17:00:00+00:00")
        ]
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.OOO_LONG)
        self.assertEqual(result["checks"][0]["ooo_days"], 11)

    def test_timed_event_workday_overlap_threshold(self):
        window = (date(2026, 11, 8), date(2026, 11, 30))

        def dates(start, end):
            return phr._ooo_dates_for_event(
                _timed(start, end), *window, timezone.utc
            )

        self.assertEqual(
            dates("2026-11-10T09:00:00+00:00", "2026-11-10T13:00:00+00:00"),
            {date(2026, 11, 10)},
        )
        self.assertEqual(
            dates("2026-11-10T09:00:00+00:00", "2026-11-10T12:00:00+00:00"),
            set(),
        )
        # Overnight hold overlaps no working hours.
        self.assertEqual(
            dates("2026-11-10T18:00:00+00:00", "2026-11-11T08:00:00+00:00"),
            set(),
        )
        # Multi-day: partial first and last days still count when >= 4h.
        self.assertEqual(
            dates("2026-11-10T13:00:00+00:00", "2026-11-12T13:00:00+00:00"),
            {date(2026, 11, 10), date(2026, 11, 11), date(2026, 11, 12)},
        )
        # 3h on the first day and 2h on the second: neither day counts.
        self.assertEqual(
            dates("2026-11-10T14:00:00+00:00", "2026-11-11T11:00:00+00:00"),
            set(),
        )

    def test_timed_event_uses_event_timezone(self):
        # Evaluated in the event's own UTC+11 timezone, not the local one.
        event = _timed("2026-11-10T09:00:00+11:00", "2026-11-10T17:00:00+11:00")
        dates = phr._ooo_dates_for_event(
            event, date(2026, 11, 8), date(2026, 11, 30), timezone.utc
        )
        self.assertEqual(dates, {date(2026, 11, 10)})

    def test_z_suffix_and_naive_datetimes_parse(self):
        event = _timed("2026-11-10T09:00:00Z", "2026-11-10T17:00:00")
        dates = phr._ooo_dates_for_event(
            event, date(2026, 11, 8), date(2026, 11, 30), timezone.utc
        )
        self.assertEqual(dates, {date(2026, 11, 10)})

    def test_long_ooo_ending_in_two_days_is_available_with_note(self):
        # Fri 11-13 is the last OOO day; Wed 11-11 has 3 days left.
        events = [_ooo("2026-10-28", "2026-11-14")]  # 17 days in total.
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(
            ["a@google.com", "b@google.com"], runner, now=_NOW_WED
        )
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.AVAILABLE)
        self.assertEqual(check["ooo_days"], 3)
        self.assertEqual(
            check["note"], "available (OOO through 2026-11-13, 3d left)"
        )
        self.assertEqual(len(result["notes"]), 1)
        self.assertIn("OOO 3d left (until 2026-11-13)", result["notes"][0])

    def test_active_ooo_with_more_than_seven_days_left_is_skipped(self):
        events = [_ooo("2026-11-04", "2026-11-23")]  # Through Sun 11-22.
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(
            ["a@google.com", "b@google.com"], runner, now=_NOW_WED
        )
        self.assertEqual(result["recommended"]["owner"], "b@google.com")
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.OOO_LONG)
        self.assertEqual(check["ooo_days"], 12)
        self.assertEqual(check["note"], "OOO 12d left (until 2026-11-22)")

    def test_active_ooo_with_seven_days_left_is_kept(self):
        events = [_ooo("2026-11-02", "2026-11-18")]  # Through Tue 11-17.
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(["a@google.com"], runner, now=_NOW_WED)
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        self.assertEqual(result["checks"][0]["ooo_days"], 7)

    def test_event_start_before_window_is_not_clamped(self):
        event = _ooo("2026-11-01", "2026-11-14")
        dates = phr._ooo_dates_for_event(
            event, date(2026, 11, 10), date(2026, 11, 30), timezone.utc
        )
        self.assertIn(date(2026, 11, 1), dates)
        self.assertEqual(max(dates), date(2026, 11, 13))

    def test_malformed_attendees_and_summary_do_not_raise(self):
        events = [
            _timed(
                "2026-11-10T09:00:00+00:00",
                "2026-11-20T17:00:00+00:00",
                summary=123,
                attendees=["bogus", None],
            ),
            _timed(
                "2026-11-10T09:00:00+00:00",
                "2026-11-20T17:00:00+00:00",
                summary=None,
                attendees="bogus",
            ),
        ]
        runner = FakeRunner(calendars={"a": events})
        result = _recommend(["a@google.com"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertFalse(phr.is_ooo_event(events[0], "a"))
        self.assertFalse(phr.is_ooo_event(events[1], "a"))
        malformed = _ooo(
            "2026-11-10", "2026-11-25", attendees=[None, "x", {"email": 5}]
        )
        self.assertTrue(phr.is_ooo_event(malformed, "a"))


class FallbackTest(unittest.TestCase):
    def test_empty_calendar_output_is_calendar_error(self):
        for stdout in ("", "  \n"):
            runner = FakeRunner(calendars={"a": _CalendarRaw(stdout)})
            result = _recommend(["a@google.com", "b@google.com"], runner)
            check = result["checks"][0]
            self.assertEqual(check["status"], phr.CALENDAR_ERROR)
            self.assertIn("empty output", check["note"])
            self.assertEqual(result["recommended"]["owner"], "b@google.com")

    def test_error_dict_calendar_response_is_calendar_error(self):
        runner = FakeRunner(calendars={"a": {"error": "permission denied"}})
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.CALENDAR_ERROR)
        self.assertIn("unparseable", result["checks"][0]["note"])
        self.assertEqual(result["recommended"]["owner"], "b@google.com")

    def test_non_list_calendar_payloads_are_calendar_errors(self):
        payloads = [
            _CalendarRaw("not json"),
            _CalendarRaw('"text"'),
            {"items": None},
            {"events": "oops"},
            ["not-an-event"],
        ]
        for payload in payloads:
            runner = FakeRunner(calendars={"a": payload})
            result = _recommend(["a@google.com"], runner)
            self.assertEqual(
                result["checks"][0]["status"], phr.CALENDAR_ERROR, payload
            )

    def test_calendar_items_and_events_dicts_are_parsed(self):
        long_ooo = [_ooo("2026-11-10", "2026-11-19")]
        for key in ("items", "events"):
            runner = FakeRunner(calendars={"a": {key: long_ooo}})
            result = _recommend(["a@google.com"], runner)
            self.assertEqual(result["checks"][0]["status"], phr.OOO_LONG)
        runner = FakeRunner(calendars={"a": {"items": []}})
        result = _recommend(["a@google.com"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)

    def test_calendar_error_falls_back_to_available_second_owner(self):
        runner = FakeRunner(calendars={"a": _CalendarError()})
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "b@google.com")
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.CALENDAR_ERROR)
        self.assertIn("calendar unreadable", check["note"])
        self.assertEqual(result["skipped"][0]["owner"], "a@google.com")

    def test_calendar_error_for_everyone_recommends_first_active(self):
        runner = FakeRunner(
            calendars={
                "a": _CalendarError(),
                "b": _CalendarError("404 not found"),
            }
        )
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "a@google.com")
        self.assertIn("calendar could not be read", result["warning"])
        self.assertNotIn("verified available", result["recommended"]["note"])

    def test_everyone_long_ooo_recommends_shortest(self):
        runner = FakeRunner(
            calendars={
                "a": [_ooo("2026-11-10", "2026-11-22")],  # 12 days.
                "b": [_ooo("2026-11-10", "2026-11-19")],  # 9 days.
                "c": [_ooo("2026-11-10", "2026-11-25")],  # 15 days.
            }
        )
        result = _recommend(
            ["a@google.com", "b@google.com", "c@google.com"], runner
        )
        self.assertEqual(result["recommended"]["owner"], "b@google.com")
        self.assertEqual(result["recommended"]["position"], 2)
        self.assertIn("OOO 9d", result["warning"])
        self.assertEqual(
            [c["owner"] for c in result["skipped"]],
            ["a@google.com", "c@google.com"],
        )

    def test_long_ooo_preferred_over_calendar_error_fallback(self):
        runner = FakeRunner(
            calendars={
                "a": _CalendarError(),
                "b": [_ooo("2026-11-10", "2026-11-19")],
            }
        )
        result = _recommend(["a@google.com", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "b@google.com")

    def test_nobody_usable_returns_raw_entries(self):
        runner = FakeRunner(moma={"a": _MOMA_NOT_FOUND, "team": _MOMA_GROUP})
        owners = ["src/foo/OWNERS", "a@google.com", "team@google.com"]
        result = _recommend(owners, runner)
        self.assertIsNone(result["recommended"])
        self.assertIsNotNone(result["warning"])
        text = phr.format_text(result)
        for owner in owners:
            self.assertIn(owner, text)

    def test_expired_gcert_fails_open_without_lookups(self):
        runner = FakeRunner(gcert_rc=1)
        owners = ["a@google.com", "b@chromium.org"]
        result = _recommend(owners, runner)
        self.assertIsNone(result["recommended"])
        self.assertEqual(result["owners"], owners)
        self.assertIn("credentials", result["warning"])
        self.assertEqual(runner.moma_ldaps(), [])
        self.assertEqual(runner.calendar_ldaps(), [])
        self.assertEqual(len(runner.calls), 1)
        self.assertEqual(
            runner.calls[0],
            [
                "/usr/bin/gcertstatus",
                "--check_ssh=false",
                "--check_remaining=5m",
            ],
        )
        text = phr.format_text(result)
        self.assertNotIn("left Google", text)
        self.assertIn("a@google.com, b@chromium.org", text)

    def test_missing_cli_fails_open(self):
        runner = FakeRunner()
        result = phr.recommend_reviewer(
            ["a@google.com"],
            runner=runner,
            now=_NOW_MON,
            which=lambda name: None if name == "moma_cli" else f"/bin/{name}",
            exists=lambda path: "moma_cli" not in path,
        )
        self.assertIsNone(result["recommended"])
        self.assertIn("moma_cli was not found", result["warning"])
        self.assertEqual(runner.calls, [])

    def test_missing_gcertstatus_fails_open(self):
        runner = FakeRunner()
        result = phr.recommend_reviewer(
            ["a@google.com"],
            runner=runner,
            now=_NOW_MON,
            which=lambda name: None,
            exists=_exists,
        )
        self.assertIsNone(result["recommended"])
        self.assertIn("gcertstatus", result["warning"])
        self.assertEqual(runner.calls, [])


class NamesMatchTest(unittest.TestCase):
    def test_matches_reordered_and_middle_names(self):
        self.assertTrue(phr._names_match("Eugene Zemtsov", "Zemtsov, Eugene"))
        self.assertTrue(phr._names_match("Eugene Zemtsov", "Eugene I. Zemtsov"))
        self.assertTrue(phr._names_match("Eugene Zemtsov", "eugene zemtsov"))

    def test_ignores_accents_punctuation_and_suffixes(self):
        self.assertTrue(phr._names_match("José García", "Jose Garcia"))
        self.assertTrue(
            phr._names_match("Mary-Jane Watson", "Mary Jane Watson")
        )
        self.assertTrue(
            phr._names_match("Eugene Zemtsov", "Eugene Zemtsov (OOO)")
        )
        self.assertTrue(
            phr._names_match("Eugene Zemtsov", "Eugene Zemtsov [Google]")
        )

    def test_ignores_ascii_and_curly_apostrophes(self):
        self.assertTrue(phr._names_match("O'Brien Pat", "OBrien Pat"))
        self.assertTrue(phr._names_match("Pat O\u2019Brien", "Pat O'Brien"))
        self.assertTrue(phr._names_match("Ann D\u2019Souza", "Ann DSouza"))

    def test_single_letter_initial_matches_full_token(self):
        self.assertTrue(phr._names_match("J. Smith", "John Smith"))
        self.assertTrue(phr._names_match("John Smith", "J. Smith"))
        self.assertTrue(phr._names_match("J. Smith", "John Q. Smith"))
        self.assertFalse(phr._names_match("J. Smith", "Mary Smith"))
        self.assertFalse(phr._names_match("J. Smith", "John Doe"))
        self.assertFalse(phr._names_match("J.", "John"))

    def test_rejects_different_or_partial_names(self):
        self.assertFalse(phr._names_match("John Doe", "John Smith"))
        self.assertFalse(phr._names_match("Alex", "Alex Chen"))
        self.assertFalse(phr._names_match("Alex Chen", "Alex"))
        self.assertFalse(phr._names_match("", "Alex Chen"))
        self.assertFalse(phr._names_match("Alex Chen", "(OOO)"))

    def test_single_token_names_must_be_identical(self):
        self.assertTrue(phr._names_match("Cher", "cher"))
        self.assertFalse(phr._names_match("Cher", "Madonna"))


class ChromiumOwnerResolutionTest(unittest.TestCase):
    def _eugene_runner(self, **kwargs):
        return FakeRunner(
            gerrit={"eugene@chromium.org": _gerrit("Eugene Zemtsov")},
            persons={
                "eugene": _stub("eugene"),
                "Eugene Zemtsov": _people(("ezemtsov", "Eugene Zemtsov")),
            },
            **kwargs,
        )

    def test_different_corp_username_is_resolved_via_gerrit_name(self):
        runner = self._eugene_runner()
        result = _recommend(["eugene@chromium.org", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "eugene@chromium.org")
        self.assertEqual(
            result["recommended"]["resolved_email"], "ezemtsov@google.com"
        )
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.AVAILABLE)
        self.assertEqual(check["resolved_email"], "ezemtsov@google.com")
        self.assertEqual(runner.gerrit_accounts(), ["eugene@chromium.org"])
        self.assertEqual(runner.person_queries(), ["eugene", "Eugene Zemtsov"])
        # No slow or ambiguous get-profile lookup is needed.
        self.assertEqual(runner.profile_ldaps(), [])
        self.assertEqual(runner.calendar_ldaps(), ["ezemtsov"])

    def test_long_ooo_of_resolved_account_skips_owner(self):
        runner = self._eugene_runner(
            calendars={"ezemtsov": [_ooo("2026-11-10", "2026-11-19")]}
        )
        result = _recommend(["eugene@chromium.org", "b@google.com"], runner)
        self.assertEqual(result["recommended"]["owner"], "b@google.com")
        self.assertEqual(_statuses(result)["eugene@chromium.org"], phr.OOO_LONG)
        self.assertEqual(result["skipped"][0]["owner"], "eugene@chromium.org")
        self.assertEqual(runner.calendar_ldaps(), ["ezemtsov", "b"])

    def test_matching_moma_name_for_same_username_skips_name_search(self):
        runner = FakeRunner(
            gerrit={"eugene@chromium.org": _gerrit("Eugene Zemtsov")},
            persons={"eugene": _people(("eugene", "Zemtsov, Eugene"))},
        )
        result = _recommend(["eugene@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertNotIn("resolved_email", result["checks"][0])
        self.assertEqual(runner.person_queries(), ["eugene"])
        self.assertEqual(runner.calendar_ldaps(), ["eugene"])

    def test_reversed_and_middle_name_forms_match(self):
        for gerrit_name in ("Eugene Zemtsov", "Eugene I. Zemtsov"):
            runner = FakeRunner(
                gerrit={"eugene@chromium.org": _gerrit(gerrit_name)},
                persons={
                    "eugene": _stub("eugene"),
                    gerrit_name: _people(("ezemtsov", "Zemtsov, Eugene")),
                },
            )
            result = _recommend(["eugene@chromium.org"], runner)
            self.assertEqual(
                result["checks"][0]["status"], phr.AVAILABLE, gerrit_name
            )
            self.assertEqual(runner.calendar_ldaps(), ["ezemtsov"], gerrit_name)

    def test_name_collision_never_checks_the_wrong_googlers_calendar(self):
        runner = FakeRunner(
            gerrit={"john@chromium.org": _gerrit("John Smith")},
            persons={
                "john": _people(("john", "John Doe")),
                "John Smith": _people(("jsmith", "John Smith")),
            },
        )
        result = _recommend(["john@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertEqual(
            result["checks"][0]["resolved_email"], "jsmith@google.com"
        )
        self.assertEqual(runner.calendar_ldaps(), ["jsmith"])
        self.assertNotIn(
            "john@google.com",
            [
                c[c.index("--cal") + 1]
                for c in runner.calls
                if c[0].endswith("gcalendar")
            ],
        )

    def test_name_collision_without_real_match_is_not_in_moma(self):
        runner = FakeRunner(
            gerrit={"john@chromium.org": _gerrit("John Smith")},
            persons={"john": _people(("john", "John Doe"))},
        )
        result = _recommend(["john@chromium.org", "b@google.com"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.NOT_IN_MOMA)
        self.assertEqual(runner.calendar_ldaps(), ["b"])

    def test_ambiguous_name_matches_do_not_match_by_substring(self):
        runner = FakeRunner(
            gerrit={"alice@chromium.org": _gerrit("Alice Smith")},
            persons={
                "alice": _stub("alice"),
                "Alice Smith": _people(
                    ("alicesmith", "Alice Smith"), ("alice2", "Alice Smith")
                ),
            },
        )
        result = _recommend(["alice@chromium.org", "b@google.com"], runner)
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.NOT_IN_MOMA)
        self.assertEqual(
            check["note"],
            "ambiguous Moma matches for Alice Smith; left unmapped",
        )
        self.assertEqual(runner.calendar_ldaps(), ["b"])

    def test_common_name_prefers_exact_local_part_among_matches(self):
        runner = FakeRunner(
            gerrit={"alexwchen@chromium.org": _gerrit("Alex Chen")},
            persons={
                "alexwchen": _stub("alexwchen"),
                "Alex Chen": _people(
                    ("alexchen", "Alex Chen"),
                    ("achen", "Alex Chen"),
                    ("alexwchen", "Alex Chen"),
                    ("alexwchen", "Alex Chen"),
                ),
            },
        )
        result = _recommend(["alexwchen@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertNotIn("resolved_email", result["checks"][0])
        self.assertEqual(runner.calendar_ldaps(), ["alexwchen"])

    def test_individual_who_left_google_is_not_a_group_alias(self):
        runner = FakeRunner(
            gerrit={"jane@chromium.org": _gerrit("Jane Roe")},
            persons={"jane": _MOMA_NOT_FOUND},
            moma={"jane": _MOMA_GROUP},
        )
        result = _recommend(["jane@chromium.org", "b@google.com"], runner)
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.NOT_IN_MOMA)
        self.assertEqual(
            check["note"], "left Google or unmapped Chromium username"
        )
        self.assertNotIn("group", check["note"])
        # Only the second owner (b@google.com) gets a get-profile lookup.
        self.assertEqual(runner.profile_ldaps(), ["b"])
        self.assertNotIn("jane", runner.profile_ldaps())
        self.assertEqual(result["recommended"]["owner"], "b@google.com")

    def test_gerrit_handle_display_name_is_accepted(self):
        runner = FakeRunner(
            gerrit={"danakj@chromium.org": _gerrit("danakj")},
            persons={"danakj": _people(("danakj", "Dana Jansens"))},
        )
        result = _recommend(["danakj@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertNotIn("resolved_email", result["checks"][0])
        self.assertEqual(runner.person_queries(), ["danakj"])
        self.assertEqual(runner.calendar_ldaps(), ["danakj"])

    def test_gerrit_initial_name_resolves_via_name_search(self):
        runner = FakeRunner(
            gerrit={"jsmith2@chromium.org": _gerrit("J. Smith")},
            persons={
                "jsmith2": _stub("jsmith2"),
                "J. Smith": _people(("jsmith", "John Smith")),
            },
        )
        result = _recommend(["jsmith2@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertEqual(
            result["checks"][0]["resolved_email"], "jsmith@google.com"
        )
        self.assertEqual(runner.calendar_ldaps(), ["jsmith"])

    def test_chromium_mailing_list_is_group_alias(self):
        runner = FakeRunner(
            persons={"media-dev": (0, "[]", "")},
            moma={"media-dev": _MOMA_GROUP},
        )
        result = _recommend(["media-dev@chromium.org", "b@google.com"], runner)
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.GROUP_ALIAS)
        self.assertEqual(
            check["note"], "team or group alias, not an individual"
        )
        self.assertEqual(runner.gerrit_accounts(), ["media-dev@chromium.org"])
        self.assertEqual(runner.calendar_ldaps(), ["b"])

    def test_gerrit_failure_falls_back_to_moma_profile(self):
        down = FakeRunner(gerrit={"a@chromium.org": _GERRIT_DOWN})
        result = _recommend(["a@chromium.org"], down)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertEqual(down.calendar_ldaps(), ["a"])

        # Without Gerrit proof that the account is missing, an empty Moma
        # profile is not called a group alias.
        runner = FakeRunner(
            gerrit={"x@chromium.org": _GERRIT_DOWN},
            persons={"x": (0, "[]", "")},
            moma={"x": _MOMA_GROUP},
        )
        result = _recommend(["x@chromium.org"], runner)
        check = result["checks"][0]
        self.assertEqual(check["status"], phr.NOT_IN_MOMA)
        self.assertEqual(
            check["note"], "unmapped Chromium username or group alias"
        )

    def test_gerrit_timeout_and_unusable_replies_fall_back(self):
        replies = [
            (0, "<html>not json</html>", ""),
            (0, ")]}'\n[1, 2]", ""),
            (0, ")]}'\n" + json.dumps({"name": "  "}), ""),
            (0, "", ""),
            (22, "", "curl: (22) The requested URL returned error: 500"),
        ]
        for reply in replies:
            runner = FakeRunner(gerrit={"a@chromium.org": reply})
            result = _recommend(["a@chromium.org"], runner)
            self.assertEqual(
                result["checks"][0]["status"], phr.AVAILABLE, reply
            )
            self.assertEqual(runner.calendar_ldaps(), ["a"], reply)

        def runner(cmd, **kwargs):
            if cmd[0].endswith("gcertstatus"):
                return subprocess.CompletedProcess(cmd, 0, "", "")
            if cmd[0].endswith("curl"):
                raise subprocess.TimeoutExpired(cmd, 15)
            return FakeRunner()(cmd, **kwargs)

        result = _recommend(["a@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)

    def test_gerrit_reply_without_xssi_prefix_is_parsed(self):
        runner = FakeRunner(
            gerrit={
                "eugene@chromium.org": _gerrit("Eugene Zemtsov", xssi=False)
            },
            persons={
                "eugene": _stub("eugene"),
                "Eugene Zemtsov": _people(("ezemtsov", "Eugene Zemtsov")),
            },
        )
        result = _recommend(["eugene@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertEqual(runner.calendar_ldaps(), ["ezemtsov"])

    def test_uppercase_owner_is_lowercased_and_url_encoded_for_gerrit(self):
        runner = self._eugene_runner()
        result = _recommend(["Eugene@Chromium.org"], runner)
        self.assertEqual(result["recommended"]["owner"], "Eugene@Chromium.org")
        self.assertEqual(runner.calendar_ldaps(), ["ezemtsov"])
        curl_cmd = [c for c in runner.calls if c[0].endswith("curl")][0]
        self.assertEqual(curl_cmd[1], "-sSf")
        self.assertEqual(
            curl_cmd[-1],
            "https://chromium-review.googlesource.com/accounts/"
            "eugene%40chromium.org/detail",
        )

    def test_missing_curl_skips_gerrit_lookup(self):
        runner = FakeRunner()
        result = phr.recommend_reviewer(
            ["a@chromium.org"],
            runner=runner,
            now=_NOW_MON,
            which=lambda name: None if name == "curl" else f"/usr/bin/{name}",
            exists=_exists,
        )
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertEqual(runner.gerrit_accounts(), [])
        self.assertEqual(runner.calendar_ldaps(), ["a"])

    def test_google_com_owner_skips_gerrit_and_person_lookups(self):
        runner = FakeRunner()
        now = _NOW_MON
        for owner in ("a@google.com", "A@Google.com"):
            check = phr.check_owner(
                owner,
                1,
                runner,
                "/usr/bin/moma_cli",
                "/usr/bin/gcalendar",
                "/usr/bin/curl",
                now,
                7,
            )
            self.assertEqual(check["status"], phr.AVAILABLE)
        self.assertEqual(runner.gerrit_accounts(), [])
        self.assertEqual(runner.person_queries(), [])
        self.assertEqual(runner.profile_ldaps(), ["a", "a"])

    def test_moma_person_rpc_failure_is_check_error(self):
        deadline = (2, "", "deadline exceeded")
        for persons in ({"a": deadline}, {"a": _stub("a"), "A B": deadline}):
            runner = FakeRunner(
                gerrit={"a@chromium.org": _gerrit("A B")}, persons=persons
            )
            result = _recommend(["a@chromium.org"], runner)
            check = result["checks"][0]
            self.assertEqual(check["status"], phr.CHECK_ERROR, persons)
            self.assertNotIn("left Google", check["note"])
            self.assertEqual(runner.calendar_ldaps(), [])

    def test_unparseable_moma_person_reply_is_check_error(self):
        for reply in ((0, "not json", ""), (0, '{"username": "a"}', "")):
            runner = FakeRunner(persons={"a": reply})
            result = _recommend(["a@chromium.org"], runner)
            self.assertEqual(
                result["checks"][0]["status"], phr.CHECK_ERROR, reply
            )

    def test_not_found_without_gerrit_name_falls_back_to_profile(self):
        runner = FakeRunner(
            persons={"a": _MOMA_NOT_FOUND}, moma={"a": _MOMA_NOT_FOUND}
        )
        result = _recommend(["a@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.NOT_IN_MOMA)
        self.assertEqual(runner.profile_ldaps(), ["a"])

    def test_stub_without_gerrit_name_falls_back_to_active_profile(self):
        runner = FakeRunner(persons={"a": _stub("a")})
        result = _recommend(["a@chromium.org"], runner)
        self.assertEqual(result["checks"][0]["status"], phr.AVAILABLE)
        self.assertEqual(runner.profile_ldaps(), ["a"])
        self.assertEqual(runner.calendar_ldaps(), ["a"])


class ParsingAndOutputTest(unittest.TestCase):
    def test_parse_owner_args_handles_commas_spaces_and_duplicates(self):
        self.assertEqual(
            phr.parse_owner_args(
                [
                    "a@google.com,b@chromium.org",
                    " c@x.com ",
                    "a@google.com",
                    ",",
                    "",
                ]
            ),
            ["a@google.com", "b@chromium.org", "c@x.com"],
        )
        self.assertEqual(phr.parse_owner_args([]), [])
        self.assertEqual(phr.parse_owner_args(None), [])

    def test_parse_owner_args_dedups_case_insensitively(self):
        self.assertEqual(
            phr.parse_owner_args(
                [
                    "Alice@Google.com,bob@chromium.org",
                    "alice@google.com",
                    "BOB@chromium.org",
                ]
            ),
            ["Alice@Google.com", "bob@chromium.org"],
        )

    def test_ordinal(self):
        self.assertEqual(
            [phr.ordinal(n) for n in (1, 2, 3, 4, 11, 12, 13, 21)],
            ["1st", "2nd", "3rd", "4th", "11th", "12th", "13th", "21st"],
        )

    def test_classify_owner_strips_plus_alias(self):
        self.assertEqual(
            phr.classify_owner("Alice+tag@google.com"), ("google", "alice")
        )
        self.assertEqual(
            phr.classify_owner("bob+work@chromium.org"), ("chromium", "bob")
        )

    def test_run_returns_error_tuple_on_oserror(self):
        def failing_runner(*args, **kwargs):
            raise FileNotFoundError("no such binary")

        self.assertEqual(
            phr._run(failing_runner, ["moma_cli"]), (-1, "", "no such binary")
        )

    def test_format_text_first_owner(self):
        runner = FakeRunner(gerrit={"a@chromium.org": _gerrit("A")})
        result = _recommend(["a@chromium.org", "b@google.com"], runner)
        self.assertEqual(
            phr.format_text(result),
            "Recommended reviewer: a@chromium.org (1st listed XML owner)",
        )

    def test_format_text_second_owner_lists_skipped(self):
        runner = FakeRunner(calendars={"a": [_ooo("2026-11-10", "2026-11-22")]})
        result = _recommend(["a@chromium.org", "b@google.com"], runner)
        self.assertEqual(
            phr.format_text(result),
            "\n".join(
                [
                    "Recommended reviewer: b@google.com (2nd listed XML owner)",
                    "Skipped:",
                    "  - a@chromium.org: OOO 12d (2026-11-10 to 2026-11-21)",
                ]
            ),
        )

    def test_format_text_non_available_and_unverified(self):
        ext = _recommend(["a@microsoft.com", "b@google.com"], FakeRunner())
        ext_text = phr.format_text(ext)
        self.assertIn(
            "No clear verified-available reviewer found "
            "(closest listed: a@microsoft.com, external: ",
            ext_text,
        )
        self.assertNotIn("--reviewers", ext_text)

        none_rec = _recommend(
            ["src/foo/OWNERS", "a@google.com"],
            FakeRunner(moma={"a": _MOMA_NOT_FOUND}),
        )
        self.assertTrue(
            phr.format_text(none_rec).startswith("No clear reviewer found.")
        )

        unverified = _recommend(["a@google.com"], FakeRunner(gcert_rc=1))
        unverified_text = phr.format_text(unverified)
        self.assertTrue(unverified_text.startswith("No clear reviewer found."))
        self.assertIn(
            "Unverified XML <owner> entries (listed order): a@google.com",
            unverified_text,
        )

    def test_main_empty_owners(self):
        def runner(*args, **kwargs):
            raise AssertionError("No command should run.")

        out = io.StringIO()
        self.assertEqual(phr.main(["--owners"], runner=runner, stdout=out), 0)
        self.assertEqual(
            out.getvalue().strip(), "No XML <owner> entries provided."
        )
        out = io.StringIO()
        self.assertEqual(phr.main([], runner=runner, stdout=out), 0)
        self.assertIn("No XML <owner> entries provided.", out.getvalue())

    def test_main_empty_owners_json(self):
        out = io.StringIO()
        phr.main(["--owners", "--json"], runner=FakeRunner(), stdout=out)
        data = json.loads(out.getvalue())
        self.assertIsNone(data["recommended"])
        self.assertEqual(data["checks"], [])
        self.assertEqual(data["warning"], "No XML <owner> entries provided.")

    def test_main_comma_separated_owners(self):
        runner = FakeRunner()
        out = io.StringIO()
        code = phr.main(
            ["--owners", "a@chromium.org,b@google.com"],
            runner=runner,
            now=_NOW_MON,
            stdout=out,
            which=_which,
            exists=_exists,
        )
        self.assertEqual(code, 0)
        self.assertIn("Recommended reviewer: a@chromium.org", out.getvalue())

    def test_main_json_output(self):
        runner = FakeRunner(moma={"a": _MOMA_NOT_FOUND})
        out = io.StringIO()
        code = phr.main(
            ["--owners", "a@google.com", "b@google.com", "--json"],
            runner=runner,
            now=_NOW_MON,
            stdout=out,
            which=_which,
            exists=_exists,
        )
        self.assertEqual(code, 0)
        data = json.loads(out.getvalue())
        self.assertEqual(
            set(data),
            {"recommended", "checks", "warning", "owners", "notes", "skipped"},
        )
        self.assertEqual(data["recommended"]["owner"], "b@google.com")
        self.assertEqual(
            [c["status"] for c in data["checks"]],
            [phr.LEFT_GOOGLE, phr.AVAILABLE],
        )
        self.assertEqual(
            [c["owner"] for c in data["skipped"]], ["a@google.com"]
        )

    def test_main_threshold_flag(self):
        runner = FakeRunner(calendars={"a": [_ooo("2026-11-10", "2026-11-19")]})
        out = io.StringIO()
        phr.main(
            ["--owners", "a@google.com", "--ooo-threshold-days", "10"],
            runner=runner,
            now=_NOW_MON,
            stdout=out,
            which=_which,
            exists=_exists,
        )
        self.assertIn("Recommended reviewer: a@google.com", out.getvalue())


if __name__ == "__main__":
    unittest.main()
