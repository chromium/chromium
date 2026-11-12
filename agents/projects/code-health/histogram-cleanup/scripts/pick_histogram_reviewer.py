# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Recommends a reviewer from a histogram's ordered XML <owner> entries.

Walks the listed owners in order and checks each one against Moma (has the
person left Google, or is the entry a team alias?) and Google Calendar (is the
person out of office for more than a week?). The first listed owner that is an
active, available Googler is recommended.

The script never modifies anything and always exits 0 in normal and fallback
cases. If credentials or the required CLIs are unavailable, it fails open: the
raw <owner> entries are returned with a warning, and nobody is ever labeled as
having "left Google" without a definitive Moma answer.

Usage:
  python3 pick_histogram_reviewer.py --owners alice@chromium.org bob@google.com
  python3 pick_histogram_reviewer.py --owners a@chromium.org,b@google.com --json
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import unicodedata
import urllib.parse
from datetime import date, datetime, time, timedelta

_DEFAULT_MOMA_CLI = "/google/bin/releases/gemini-agents-moma/moma_cli"
_DEFAULT_GCALENDAR = "/google/bin/releases/gemini-agents-gcalendar/gcalendar"

_CORP_DOMAINS = ("google.com", "chromium.org")
_COMMAND_TIMEOUT_SECONDS = 15
_GERRIT_ACCOUNT_URL = (
    "https://chromium-review.googlesource.com/accounts/{account}/detail"
)
_DEFAULT_OOO_THRESHOLD_DAYS = 7
_MAX_LOOKBACK_DAYS = 60

# A timed event counts toward a day only if it covers this many working hours.
_WORKDAY_START = time(9, 0)
_WORKDAY_END = time(17, 0)
_MIN_WORKDAY_OVERLAP_HOURS = 4.0

_OOO_TITLE_RE = re.compile(
    r"\b(out of office|ooo|pto|vacation|leave|sick|sabbatical|holiday)\b",
    re.IGNORECASE,
)
_OOO_TITLE_EXCLUSIONS = (
    "gym",
    "observance",
    "coverage",
    "catch up",
    "catch-up",
    "sync",
    "holiday party",
    "holiday tgit",
    "holiday freeze",
    "holiday giving",
    "reschedule",
)

# Statuses.
AVAILABLE = "available"
EXTERNAL = "external"
NON_INDIVIDUAL = "non_individual"
GROUP_ALIAS = "group_alias"
LEFT_GOOGLE = "left_google"
NOT_IN_MOMA = "not_in_moma"
CHECK_ERROR = "check_error"
CALENDAR_ERROR = "calendar_error"
OOO_LONG = "ooo_long"

# Internal Moma lookup states.
_MOMA_OK = "ok"
_MOMA_EMPTY = "empty"
_MOMA_NOT_FOUND = "not_found"
_MOMA_ERROR = "error"

# Internal Gerrit account lookup states.
_GERRIT_FOUND = "found"
_GERRIT_NOT_FOUND = "not_found"
_GERRIT_UNAVAILABLE = "unavailable"


class ToolsUnavailableError(Exception):
    """Raised when credentials or required CLIs are not usable."""


def parse_owner_args(values):
    """Flattens --owners values into a deduplicated, ordered list.

    Accepts space-separated items as well as comma-separated values within
    items.
    """
    owners = []
    seen = set()
    for value in values or []:
        for part in value.split(","):
            part = part.strip()
            if part and part.lower() not in seen:
                seen.add(part.lower())
                owners.append(part)
    return owners


def ordinal(n):
    """Returns n as an ordinal string, e.g. 1 -> '1st'."""
    if 10 <= n % 100 <= 20:
        suffix = "th"
    else:
        suffix = {1: "st", 2: "nd", 3: "rd"}.get(n % 10, "th")
    return f"{n}{suffix}"


def _run(runner, cmd):
    """Runs cmd and returns (returncode, stdout, stderr); never raises."""
    try:
        result = runner(
            cmd,
            capture_output=True,
            text=True,
            timeout=_COMMAND_TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return -1, "", f"timed out after {_COMMAND_TIMEOUT_SECONDS}s"
    except OSError as e:
        return -1, "", str(e)
    return result.returncode, result.stdout or "", result.stderr or ""


def _short(text):
    """Collapses text into a short single-line note."""
    text = " ".join(text.split())
    return text if len(text) <= 120 else text[:117] + "..."


def resolve_tools(which=shutil.which, exists=os.path.exists):
    """Returns (gcertstatus, moma_cli, gcalendar) paths.

    Raises ToolsUnavailableError if any of them is missing.
    """
    gcertstatus = which("gcertstatus")
    moma_cli = which("moma_cli") or _DEFAULT_MOMA_CLI
    gcalendar = which("gcalendar") or _DEFAULT_GCALENDAR
    if not gcertstatus:
        raise ToolsUnavailableError(
            "gcertstatus was not found, so Google credentials could not be "
            "verified."
        )
    if not exists(moma_cli):
        raise ToolsUnavailableError(f"moma_cli was not found at {moma_cli}.")
    if not exists(gcalendar):
        raise ToolsUnavailableError(f"gcalendar was not found at {gcalendar}.")
    return gcertstatus, moma_cli, gcalendar


def classify_owner(owner):
    """Returns (kind, ldap) for an XML <owner> entry.

    kind is one of 'owners_file', 'non_email', 'external', 'google' or
    'chromium'. ldap is only set for corp (google.com/chromium.org) entries.
    """
    if owner.rsplit("/", 1)[-1].endswith("OWNERS"):
        return "owners_file", None
    if "@" not in owner:
        return "non_email", None
    local, domain = owner.rsplit("@", 1)
    domain = domain.lower()
    if domain not in _CORP_DOMAINS:
        return "external", None
    ldap = local.split("+", 1)[0].strip().lower()
    return ("google" if domain == "google.com" else "chromium"), ldap


def _parse_dt(value, default_tz):
    if value.endswith("Z"):
        value = value[:-1] + "+00:00"
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=default_tz)
    return dt


def _attendees(event):
    """Returns the event's attendee dicts, ignoring malformed entries."""
    attendees = event.get("attendees")
    if not isinstance(attendees, list):
        return []
    return [a for a in attendees if isinstance(a, dict)]


def _is_declined(event, ldap):
    own_email = f"{ldap}@google.com"
    for attendee in _attendees(event):
        email = attendee.get("email")
        is_me = attendee.get("self") is True or (
            isinstance(email, str) and email.lower() == own_email
        )
        if is_me and attendee.get("responseStatus") == "declined":
            return True
    return False


def is_ooo_event(event, ldap):
    """Returns whether a calendar event represents out-of-office time."""
    if event.get("status") == "cancelled" or _is_declined(event, ldap):
        return False
    event_type = event.get("eventType") or "default"
    if event_type == "outOfOffice":
        return True
    if event_type != "default":
        return False
    summary = event.get("summary")
    title = summary.lower() if isinstance(summary, str) else ""
    if not _OOO_TITLE_RE.search(title):
        return False
    if any(word in title for word in _OOO_TITLE_EXCLUSIONS):
        return False
    people = [a for a in _attendees(event) if a.get("resource") is not True]
    # A multi-person meeting is not someone's personal time off.
    return len(people) < 2


def _ooo_dates_for_event(event, window_start, window_end, local_tz):
    """Returns the OOO dates the event marks, up to window_end.

    Dates before window_start are kept (back to a bounded lookback) so spans
    that began before the window still report their true start.
    """
    start = event.get("start") or {}
    end = event.get("end") or {}
    dates = set()
    # Bounds the loop for pathological, very long events.
    lookback_start = window_start - timedelta(days=_MAX_LOOKBACK_DAYS)
    if "date" in start:
        first = date.fromisoformat(start["date"])
        last_exclusive = (
            date.fromisoformat(end["date"])
            if "date" in end
            else first + timedelta(days=1)
        )
        if last_exclusive <= first:
            last_exclusive = first + timedelta(days=1)
        day = max(first, lookback_start)
        while day < last_exclusive and day <= window_end:
            dates.add(day)
            day += timedelta(days=1)
        return dates
    if "dateTime" not in start or "dateTime" not in end:
        return dates
    start_dt = _parse_dt(start["dateTime"], local_tz)
    tz = start_dt.tzinfo
    end_dt = _parse_dt(end["dateTime"], local_tz).astimezone(tz)
    if end_dt <= start_dt:
        return dates
    day = max(start_dt.date(), lookback_start)
    last_day = (end_dt - timedelta(microseconds=1)).date()
    while day <= last_day and day <= window_end:
        day_start = datetime.combine(day, _WORKDAY_START, tzinfo=tz)
        day_end = datetime.combine(day, _WORKDAY_END, tzinfo=tz)
        overlap = min(end_dt, day_end) - max(start_dt, day_start)
        if overlap.total_seconds() / 3600.0 >= _MIN_WORKDAY_OVERLAP_HOURS:
            dates.add(day)
        day += timedelta(days=1)
    return dates


def compute_ooo_spans(dates):
    """Merges OOO dates into contiguous (start, end) spans.

    Consecutive days merge, and a Friday followed by the next Monday also
    merges so that the weekend between them counts as OOO.
    """
    spans = []
    for day in sorted(set(dates)):
        if spans:
            span_start, prev = spans[-1]
            bridges_weekend = prev.weekday() == 4 and day == prev + timedelta(
                days=3
            )
            if day == prev + timedelta(days=1) or bridges_weekend:
                spans[-1] = (span_start, day)
                continue
        spans.append((day, day))
    return spans


def find_relevant_ooo_span(spans, today):
    """Returns the (start, end, remaining_days) span active now or imminent.

    remaining_days counts from max(start, today) through end, so a long leave
    that is about to finish only reports the days still left.
    """
    # Cover through the next working day; from Friday that is Monday.
    cutoff = today + timedelta(days=3 if today.weekday() == 4 else 2)
    best = None
    for span_start, span_end in spans:
        if span_end >= today and span_start <= cutoff:
            days = (span_end - max(span_start, today)).days + 1
            if best is None or days > best[2]:
                best = (span_start, span_end, days)
    return best


def _check_ooo(runner, gcalendar, ldap, now, threshold_days):
    """Returns (status, note, ooo_days, ooo_note) from the calendar check."""
    local_tz = now.tzinfo
    today = now.date()
    window_start = today - timedelta(days=1)
    window_end = today + timedelta(days=21)
    start_str = datetime.combine(
        window_start, time.min, tzinfo=local_tz
    ).isoformat()
    end_str = datetime.combine(
        window_end, time(23, 59, 59), tzinfo=local_tz
    ).isoformat()
    returncode, stdout, stderr = _run(
        runner,
        [
            gcalendar,
            "readonly",
            "events",
            "--cal",
            f"{ldap}@google.com",
            "--start",
            start_str,
            "--end",
            end_str,
            "--json",
        ],
    )
    if returncode != 0:
        return (
            CALENDAR_ERROR,
            "active in Moma; calendar unreadable: "
            f"{_short(stderr or stdout) or 'unknown error'}",
            None,
            None,
        )
    if not stdout.strip():
        # Empty output must not be mistaken for "no events".
        return (
            CALENDAR_ERROR,
            "active in Moma; calendar returned empty output",
            None,
            None,
        )
    try:
        data = json.loads(stdout)
        if isinstance(data, dict):
            if "items" in data:
                data = data["items"]
            elif "events" in data:
                data = data["events"]
            else:
                raise ValueError("calendar JSON has no items or events")
        if not isinstance(data, list):
            raise ValueError("unexpected calendar JSON")
        ooo_dates = set()
        for event in data:
            if not isinstance(event, dict):
                raise ValueError("unexpected calendar event")
            if is_ooo_event(event, ldap):
                ooo_dates |= _ooo_dates_for_event(
                    event, window_start, window_end, local_tz
                )
    except (ValueError, KeyError, TypeError, AttributeError) as e:
        return (
            CALENDAR_ERROR,
            f"active in Moma; calendar data unparseable: {_short(str(e))}",
            None,
            None,
        )
    span = find_relevant_ooo_span(compute_ooo_spans(ooo_dates), today)
    if span is None:
        return AVAILABLE, "available", None, None
    span_start, span_end, days = span  # days counts from max(start, today).
    if span_start <= today:
        desc = f"OOO {days}d left (until {span_end})"
        available_note = f"available (OOO through {span_end}, {days}d left)"
    else:
        desc = f"OOO {days}d ({span_start} to {span_end})"
        available_note = f"available ({desc})"
    if days > threshold_days:
        return OOO_LONG, desc, days, None
    return AVAILABLE, available_note, days, desc


def _names_match(name_a, name_b):
    """Returns whether two person names plausibly refer to the same person.

    Ignores accents, case, punctuation, parenthetical/bracket suffixes and
    token order ('Last, First'). Multi-token names match when their tokens
    are equal or when the shorter name (at least two tokens) is contained in
    the longer one, which tolerates middle names and initials. A single-letter
    initial also matches the start of a token in the other name, as long as
    at least one full token is shared ('J. Smith' vs 'John Smith'). A single
    token only matches an identical single token.
    """

    def tokens(name):
        decomposed = unicodedata.normalize("NFKD", name)
        text = "".join(c for c in decomposed if not unicodedata.combining(c))
        text = re.sub(r"\([^)]*\)|\[[^\]]*\]", " ", text)
        # Delete apostrophes so "O'Brien" and "O\u2019Brien" become "obrien".
        text = re.sub(r"['\u2018\u2019]", "", text)
        text = re.sub(r"[.,\-_]", " ", text)
        return set(text.lower().split())

    def initials_match(short, long):
        initials = {t for t in short if len(t) == 1}
        full = short - initials
        if not initials or not full or not full <= long:
            return False
        remaining = long - full
        return all(any(t.startswith(i) for t in remaining) for i in initials)

    set_a, set_b = tokens(name_a), tokens(name_b)
    if not set_a or not set_b:
        return False
    if set_a == set_b:
        return True
    smaller, larger = sorted((set_a, set_b), key=len)
    if len(smaller) >= 2 and smaller <= larger:
        return True
    return initials_match(set_a, set_b) or initials_match(set_b, set_a)


def _moma_query(runner, cmd):
    """Runs a Moma command; returns (state, data_or_note).

    state is _MOMA_OK (data is the parsed JSON), _MOMA_NOT_FOUND or
    _MOMA_ERROR (the second item is a note for the failure).
    """
    returncode, stdout, stderr = _run(runner, cmd)
    if returncode != 0:
        output = f"{stderr}\n{stdout}"
        if "person not found" in output.lower():
            return _MOMA_NOT_FOUND, None
        return (
            _MOMA_ERROR,
            f"Moma lookup failed: {_short(output) or 'unknown error'}",
        )
    try:
        return _MOMA_OK, json.loads(stdout)
    except ValueError:
        return _MOMA_ERROR, "Moma lookup failed: unparseable response"


def _get_profile(runner, moma_cli, ldap):
    """Runs `moma_cli get-profile`; returns (state, note).

    state is _MOMA_OK for an individual, _MOMA_EMPTY for a null or empty
    profile (a group alias), _MOMA_NOT_FOUND or _MOMA_ERROR.
    """
    state, data = _moma_query(runner, [moma_cli, "get-profile", ldap, "--json"])
    if state != _MOMA_OK:
        return state, data
    if data is None or data == []:
        return _MOMA_EMPTY, None
    if not isinstance(data, list):
        return _MOMA_ERROR, "Moma lookup failed: unexpected response"
    return _MOMA_OK, None


def _query_people(runner, moma_cli, query):
    """Runs `moma_cli person`; returns (people, error_note).

    people is a list of (lowercase username, name) for active people; entries
    without both a username and a name (stubs) are ignored.
    """
    state, data = _moma_query(runner, [moma_cli, "person", query, "--json"])
    if state == _MOMA_NOT_FOUND:
        return [], None
    if state == _MOMA_ERROR:
        return None, data
    if data is None:
        return [], None
    if not isinstance(data, list):
        return None, "Moma lookup failed: unexpected response"
    people = []
    for entry in data:
        if not isinstance(entry, dict):
            continue
        name, username = entry.get("name"), entry.get("username")
        if (
            isinstance(name, str)
            and name.strip()
            and isinstance(username, str)
            and username.strip()
        ):
            people.append((username.strip().lower(), name.strip()))
    return people, None


def _gerrit_account_name(runner, curl_bin, owner):
    """Returns (state, name) for the owner's Chromium Gerrit account.

    state is _GERRIT_FOUND (name is set), _GERRIT_NOT_FOUND or
    _GERRIT_UNAVAILABLE (no curl, a transport error or an unusable reply).
    """
    if not curl_bin:
        return _GERRIT_UNAVAILABLE, None
    account = urllib.parse.quote(owner.lower(), safe="")
    returncode, stdout, stderr = _run(
        runner,
        [
            curl_bin,
            "-sSf",
            "--max-time",
            "10",
            _GERRIT_ACCOUNT_URL.format(account=account),
        ],
    )
    if returncode != 0:
        if re.search(r"\b404\b", stderr):
            return _GERRIT_NOT_FOUND, None
        return _GERRIT_UNAVAILABLE, None
    text = stdout.lstrip()
    # Gerrit prefixes JSON replies with an XSSI guard line.
    if text.startswith(")]}'"):
        text = text[len(")]}'") :]
    try:
        data = json.loads(text)
    except ValueError:
        return _GERRIT_UNAVAILABLE, None
    name = data.get("name") if isinstance(data, dict) else None
    if isinstance(name, str) and name.strip():
        return _GERRIT_FOUND, name.strip()
    return _GERRIT_UNAVAILABLE, None


def _resolve_google_ldap(runner, moma_cli, ldap):
    """Returns (ldap, None) for an active Googler, else (None, failure).

    failure is a (status, note) tuple.
    """
    state, note = _get_profile(runner, moma_cli, ldap)
    if state == _MOMA_OK:
        return ldap, None
    if state == _MOMA_NOT_FOUND:
        return None, (LEFT_GOOGLE, "left Google")
    if state == _MOMA_EMPTY:
        return None, (GROUP_ALIAS, "team or group alias, not an individual")
    return None, (CHECK_ERROR, note)


def _resolve_chromium_ldap(runner, moma_cli, gerrit_state, gerrit_name, ldap):
    """Maps a @chromium.org owner to a Google LDAP.

    The corp username can differ from the @chromium.org prefix, so the Gerrit
    account name (from _gerrit_account_name) is matched against Moma instead of
    trusting the prefix, and gerrit_state distinguishes a missing account from
    an unavailable lookup.

    Returns (resolved_ldap, None) or (None, failure), where failure is a
    (status, note) tuple.
    """
    people, error = _query_people(runner, moma_cli, ldap)
    if error:
        return None, (CHECK_ERROR, error)
    # A Gerrit display name that is just the handle (e.g. "danakj") has no
    # personal name to compare, so the username match alone is accepted.
    gerrit_is_handle = (
        bool(gerrit_name)
        and re.sub(r"[^a-z0-9]", "", gerrit_name.lower()) == ldap
    )
    for username, name in people:
        if username == ldap and (
            gerrit_name is None
            or gerrit_is_handle
            or _names_match(gerrit_name, name)
        ):
            return ldap, None

    if gerrit_name:
        people, error = _query_people(runner, moma_cli, gerrit_name)
        if error:
            return None, (CHECK_ERROR, error)
        usernames = []
        for username, name in people:
            if _names_match(gerrit_name, name) and username not in usernames:
                usernames.append(username)
        if len(usernames) == 1:
            return usernames[0], None
        if len(usernames) > 1:
            if ldap in usernames:
                return ldap, None
            return None, (
                NOT_IN_MOMA,
                f"ambiguous Moma matches for {gerrit_name}; left unmapped",
            )
        # A Gerrit account with a personal name is never a group alias.
        return None, (NOT_IN_MOMA, "left Google or unmapped Chromium username")

    state, note = _get_profile(runner, moma_cli, ldap)
    if state == _MOMA_OK:
        return ldap, None
    if state == _MOMA_NOT_FOUND:
        return None, (NOT_IN_MOMA, "left Google or different corp username")
    if state == _MOMA_EMPTY:
        if gerrit_state == _GERRIT_NOT_FOUND:
            return None, (GROUP_ALIAS, "team or group alias, not an individual")
        return None, (NOT_IN_MOMA, "unmapped Chromium username or group alias")
    return None, (CHECK_ERROR, note)


def check_owner(
    owner, position, runner, moma_cli, gcalendar, curl_bin, now, threshold_days
):
    """Classifies one XML <owner> entry; returns a check dict."""
    check = {"owner": owner, "position": position}

    def done(status, note, **extra):
        check.update(status=status, note=note, **extra)
        return check

    kind, ldap = classify_owner(owner)
    if kind == "owners_file":
        return done(NON_INDIVIDUAL, "OWNERS file reference")
    if kind == "non_email":
        return done(NON_INDIVIDUAL, "non-email entry")
    if kind == "external":
        return done(EXTERNAL, "external contributor; cannot verify Moma/OOO")

    if kind == "google":
        resolved_ldap, failure = _resolve_google_ldap(runner, moma_cli, ldap)
    else:
        gerrit_state, gerrit_name = _gerrit_account_name(
            runner, curl_bin, owner
        )
        resolved_ldap, failure = _resolve_chromium_ldap(
            runner, moma_cli, gerrit_state, gerrit_name, ldap
        )
    if failure is not None:
        return done(*failure)

    status, note, ooo_days, ooo_note = _check_ooo(
        runner, gcalendar, resolved_ldap, now, threshold_days
    )
    extra = {"resolved_ldap": resolved_ldap}
    if resolved_ldap != ldap:
        # The Chromium username differs from the corp LDAP that was checked.
        extra["resolved_email"] = f"{resolved_ldap}@google.com"
    if ooo_days is not None:
        extra["ooo_days"] = ooo_days
    if ooo_note:
        extra["ooo_note"] = ooo_note
    return done(status, note, **extra)


def _recommendation(check, reason=None):
    recommendation = {
        "owner": check["owner"],
        "position": check["position"],
        "status": check["status"],
        "note": reason or check["note"],
    }
    if "resolved_email" in check:
        recommendation["resolved_email"] = check["resolved_email"]
    return recommendation


def _unverified_result(owners, warning):
    return {
        "recommended": None,
        "checks": [],
        "warning": warning,
        "owners": owners,
        "notes": [],
        "skipped": [],
    }


def recommend_reviewer(
    owners,
    runner=subprocess.run,
    now=None,
    threshold_days=_DEFAULT_OOO_THRESHOLD_DAYS,
    which=shutil.which,
    exists=os.path.exists,
):
    """Picks a reviewer from the ordered XML <owner> entries.

    Args:
      owners: Ordered, deduplicated <owner> strings.
      runner: subprocess.run-compatible callable (injected for tests).
      now: Timezone-aware datetime for "now"; defaults to the local time.
      threshold_days: Contiguous OOO longer than this many days is unavailable.
      which: shutil.which-compatible callable.
      exists: os.path.exists-compatible callable.

    Returns:
      A dict with keys recommended, checks, warning, owners, notes, and
      skipped.
    """
    if not owners:
        return _unverified_result([], "No XML <owner> entries provided.")
    if now is None:
        now = datetime.now().astimezone()
    elif now.tzinfo is None:
        now = now.astimezone()

    try:
        gcertstatus, moma_cli, gcalendar = resolve_tools(which, exists)
    except ToolsUnavailableError as e:
        return _unverified_result(
            owners, f"{e} Returning the raw XML <owner> entries unverified."
        )
    returncode, stdout, stderr = _run(
        runner, [gcertstatus, "--check_ssh=false", "--check_remaining=5m"]
    )
    if returncode != 0:
        return _unverified_result(
            owners,
            "Google credentials are missing or expire in under 5 "
            "minutes (gcertstatus failed), so Moma/Calendar were not checked. "
            "Run gcert and retry. Returning the raw XML <owner> entries "
            "unverified.",
        )

    checks = []
    # Optional: without curl, Gerrit name lookups are skipped.
    curl_bin = which("curl")
    primary = None  # First external owner reached; wins over later Googlers.
    chosen = None
    backup = None
    fallback_ooo = None
    fallback_calendar = None
    for position, owner in enumerate(owners, start=1):
        check = check_owner(
            owner,
            position,
            runner,
            moma_cli,
            gcalendar,
            curl_bin,
            now,
            threshold_days,
        )
        checks.append(check)
        status = check["status"]
        if status == EXTERNAL:
            if primary is None:
                primary = check
        elif status == AVAILABLE:
            # Stop once a verified available Googler is found.
            if primary is None:
                chosen = check
            else:
                backup = check
            break
        elif status == OOO_LONG:
            if (
                fallback_ooo is None
                or check["ooo_days"] < fallback_ooo["ooo_days"]
            ):
                fallback_ooo = check
        elif status == CALENDAR_ERROR and fallback_calendar is None:
            fallback_calendar = check

    notes = []
    warning = None
    recommended = None
    rec_check = None
    if primary is not None:
        rec_check = primary
        recommended = _recommendation(primary)
        if backup is not None:
            notes.append(
                f"Next active Googler: {backup['owner']} "
                f"({ordinal(backup['position'])} listed XML owner)"
            )
    elif chosen is not None:
        rec_check = chosen
        recommended = _recommendation(chosen)
        if chosen.get("ooo_note"):
            notes.append(f"{chosen['owner']} has short {chosen['ooo_note']}")
    elif fallback_ooo is not None:
        rec_check = fallback_ooo
        recommended = _recommendation(fallback_ooo)
        warning = (
            "No available owner found; recommending the active Googler "
            f"with the shortest upcoming absence: {fallback_ooo['note']}"
            "."
        )
    elif fallback_calendar is not None:
        rec_check = fallback_calendar
        recommended = _recommendation(fallback_calendar)
        warning = (
            "No verified-available owner found; recommending an active "
            "Googler whose calendar could not be read."
        )
    else:
        warning = (
            "No individual reviewer could be verified from the listed "
            "XML <owner> entries."
        )

    if rec_check is None:
        skipped = list(checks)
    elif rec_check is primary or rec_check is chosen:
        skipped = [c for c in checks if c["position"] < rec_check["position"]]
    else:
        # Fallback picks mean every other checked owner was skipped.
        skipped = [c for c in checks if c is not rec_check]

    return {
        "recommended": recommended,
        "checks": checks,
        "warning": warning,
        "owners": owners,
        "notes": notes,
        "skipped": skipped,
    }


def format_text(result):
    """Formats the result for display in chat."""
    rec = result["recommended"]
    lines = []
    if rec is not None and rec["status"] == AVAILABLE:
        lines.append(
            f"Recommended reviewer: {rec['owner']} "
            f"({ordinal(rec['position'])} listed XML owner)"
        )
    elif rec is not None:
        lines.append(
            "No clear verified-available reviewer found (closest "
            f"listed: {rec['owner']}, {rec['status']}: "
            f"{rec['note']})."
        )
    elif result["owners"]:
        lines.append("No clear reviewer found.")
    else:
        return result["warning"]
    lines.extend(f"Note: {note}" for note in result["notes"])
    if result["warning"]:
        lines.append(f"Warning: {result['warning']}")
    if result["skipped"]:
        lines.append("Skipped:")
        lines.extend(
            f"  - {c['owner']}: {c['note']}" for c in result["skipped"]
        )
    if rec is None and not result["checks"] and result["owners"]:
        lines.append(
            "Unverified XML <owner> entries (listed order): "
            + ", ".join(result["owners"])
        )
    elif rec is None and result["owners"]:
        lines.append(
            "XML <owner> entries (listed order): " + ", ".join(result["owners"])
        )
    return "\n".join(lines)


def main(
    argv=None,
    runner=subprocess.run,
    now=None,
    stdout=None,
    which=shutil.which,
    exists=os.path.exists,
):
    parser = argparse.ArgumentParser(
        description="Recommend a reviewer from ordered histograms.xml "
        "<owner> entries, skipping people who left Google or are OOO for "
        "more than a week."
    )
    parser.add_argument(
        "--owners",
        nargs="*",
        default=[],
        help="Ordered <owner> values from histograms.xml (space- or "
        "comma-separated).",
    )
    parser.add_argument(
        "--json", action="store_true", help="Emit structured JSON."
    )
    parser.add_argument(
        "--ooo-threshold-days",
        type=int,
        default=_DEFAULT_OOO_THRESHOLD_DAYS,
        help="Contiguous OOO longer than this many calendar days makes an "
        "owner unavailable (default: %(default)s).",
    )
    args = parser.parse_args(argv)
    stdout = stdout or sys.stdout

    owners = parse_owner_args(args.owners)
    result = recommend_reviewer(
        owners,
        runner=runner,
        now=now,
        threshold_days=args.ooo_threshold_days,
        which=which,
        exists=exists,
    )
    if args.json:
        output = {
            "recommended": result["recommended"],
            "checks": result["checks"],
            "warning": result["warning"],
            "owners": result["owners"],
            "notes": result["notes"],
            "skipped": result["skipped"],
        }
        print(json.dumps(output, indent=2), file=stdout)
    else:
        print(format_text(result), file=stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
