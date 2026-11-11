#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Resolves a contributor email (and optional date) to domain + organization."""

import argparse
from collections.abc import Mapping, Sequence
import datetime
import json
import pathlib
import re
import sys
from typing import Any

_SRC_DIR = pathlib.Path(__file__).resolve().parents[4]
_COMMIT_STATS_DIR = (
    _SRC_DIR / 'third_party' / 'blink' / 'tools' / 'commit_stats'
)
DEFAULT_AFFILIATIONS_FILE = _COMMIT_STATS_DIR / 'affiliations.json5'
DEFAULT_ORG_LIST_FILE = _COMMIT_STATS_DIR / 'org-list.txt'

sys.path.insert(0, str(_SRC_DIR / 'third_party' / 'pyjson5' / 'src'))
import json5  # noqa: E402  # pytype: disable=import-error

SPECIAL_AFFILIATIONS = frozenset({'Individual', 'Undisclosed'})
_DATE_RE = re.compile(r'\d{4}-\d{2}-\d{2}')


def parse_date(value: str) -> datetime.date:
    """Parses a strict YYYY-MM-DD date string."""
    if not _DATE_RE.fullmatch(value):
        raise ValueError(f'Invalid YYYY-MM-DD date: {value!r}')
    return datetime.date.fromisoformat(value)


def load_affiliations(
    path: pathlib.Path = DEFAULT_AFFILIATIONS_FILE,
) -> dict[str, Any]:
    """Loads affiliations.json5 keyed by lowercase email address."""
    with path.open('r', encoding='utf-8') as f:
        data = json5.load(f)
    return {email.lower(): record for email, record in data.items()}


def load_org_list(path: pathlib.Path = DEFAULT_ORG_LIST_FILE) -> dict[str, str]:
    """Loads lowercase domain -> organization mapping from org-list.txt."""
    domain_to_org: dict[str, str] = {}
    with path.open('r', encoding='utf-8') as f:
        for raw_line in f:
            line = raw_line.strip()
            if line:
                org, domain = line.split(',', 1)
                domain_to_org[domain.strip().lower()] = org.strip()
    return domain_to_org


def _org_for_domain(
    domain: str, domain_to_org: Mapping[str, str]
) -> str | None:
    org = domain_to_org.get(domain)
    return None if org in (None, 'Unknown') else org


def resolve_affiliation(
    email: str,
    query_date: datetime.date,
    contributors: Mapping[str, Any],
    domain_to_org: Mapping[str, str],
) -> dict[str, str | None]:
    """Returns {'domain': ..., 'organization': ...} for email on query_date."""
    addr = email.strip().lower()
    local, sep, domain = addr.partition('@')
    if (
        not sep
        or not local
        or '.' not in domain.strip('.')
        or '@' in domain
        or any(c.isspace() for c in addr)
    ):
        raise ValueError(f'Invalid email address: {email!r}')

    record = contributors.get(addr)
    if record is not None:
        for entry in record.get('affiliations', ()):
            if query_date >= parse_date(entry['start']):
                value = entry['domain']
                if value in SPECIAL_AFFILIATIONS:
                    return {'domain': None, 'organization': value}
                resolved_domain = value.lower()
                return {
                    'domain': resolved_domain,
                    'organization': _org_for_domain(
                        resolved_domain, domain_to_org
                    ),
                }
        return {'domain': None, 'organization': 'Undisclosed'}

    resolved_domain = 'google.com' if domain == 'chromium.org' else domain
    return {
        'domain': resolved_domain,
        'organization': _org_for_domain(resolved_domain, domain_to_org),
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('email', help='Contributor email address.')
    parser.add_argument(
        'date',
        nargs='?',
        help='Lookup date in YYYY-MM-DD format (default: today).',
    )
    parser.add_argument(
        '--affiliations-file',
        type=pathlib.Path,
        default=DEFAULT_AFFILIATIONS_FILE,
        help='Path to affiliations.json5.',
    )
    parser.add_argument(
        '--org-list-file',
        type=pathlib.Path,
        default=DEFAULT_ORG_LIST_FILE,
        help='Path to org-list.txt.',
    )
    args = parser.parse_args(argv)

    try:
        query_date = (
            parse_date(args.date) if args.date else datetime.date.today()
        )
        contributors = load_affiliations(args.affiliations_file)
        domain_to_org = load_org_list(args.org_list_file)
        result = resolve_affiliation(
            args.email, query_date, contributors, domain_to_org
        )
        print(json.dumps(result))
    except (OSError, ValueError) as exc:
        print(f'Error: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
