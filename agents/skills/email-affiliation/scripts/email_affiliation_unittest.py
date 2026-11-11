#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for email_affiliation.py."""

import datetime
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import email_affiliation  # noqa: E402  # pytype: disable=import-error

SAMPLE_JSON5 = """\
/* Comment header */
{
  "alice@chromium.org": {
    affiliations: [
      { start: "2025-04-11", domain: "microsoft.com" },
    ],
  },
  "bob@chromium.org": {
    affiliations: [
      { start: "2024-06-01", domain: "Undisclosed" },
      { start: "2021-03-15", domain: "igalia.com" },
      { start: "2015-01-10", domain: "google.com" },
    ],
  },
  "carol@chromium.org": {
    affiliations: [
      { start: "2023-01-01", domain: "Individual" },
    ],
  },
  "dave@chromium.org": {
    affiliations: [
      { start: "2020-05-01", domain: "gmail.com" },
    ],
  },
  "eve@yoav.ws": {
    affiliations: [
      { start: "2022-10-01", domain: "google.com" },
      { start: "2018-01-01", domain: "unlisted-startup.ai" },
    ],
  },
}
"""

SAMPLE_ORG_LIST = """\
AMD,amd.corp-partner.google.com
Google,chromium.org
Google,google.com
Google,webrtc.org
Igalia,igalia.com
Individual,gmail.com
Individual,yoav.ws
LunarG,LunarG.com
Microsoft,microsoft.com
Samsung,partner.samsung.com
Samsung,samsung.com
Unknown,mystery.example
"""


class EmailAffiliationTest(unittest.TestCase):
    def setUp(self) -> None:
        super().setUp()
        tmp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(tmp_dir.cleanup)
        base = pathlib.Path(tmp_dir.name)

        self.affiliations_path = base / 'affiliations.json5'
        self.affiliations_path.write_text(SAMPLE_JSON5, encoding='utf-8')
        self.org_list_path = base / 'org-list.txt'
        self.org_list_path.write_text(SAMPLE_ORG_LIST, encoding='utf-8')

        self.contributors = email_affiliation.load_affiliations(
            self.affiliations_path
        )
        self.domain_to_org = email_affiliation.load_org_list(self.org_list_path)

    def _resolve(self, email: str, date_str: str) -> dict[str, str | None]:
        return email_affiliation.resolve_affiliation(
            email,
            datetime.date.fromisoformat(date_str),
            self.contributors,
            self.domain_to_org,
        )

    def _cli_args(self, *extra: str) -> list[str]:
        return [
            '--affiliations-file',
            str(self.affiliations_path),
            '--org-list-file',
            str(self.org_list_path),
            *extra,
        ]

    def test_unlisted_chromium_defaults_to_google(self) -> None:
        expected = {'domain': 'google.com', 'organization': 'Google'}
        self.assertEqual(
            self._resolve('unlisted@chromium.org', '2026-01-01'), expected
        )
        self.assertEqual(
            self._resolve('  MixedCase@Chromium.ORG ', '2010-01-01'), expected
        )

    def test_org_list_mappings_for_unlisted_emails(self) -> None:
        self.assertEqual(
            self._resolve('dev@Microsoft.COM', '2026-01-01'),
            {'domain': 'microsoft.com', 'organization': 'Microsoft'},
        )
        self.assertEqual(
            self._resolve('author@webrtc.org', '2026-01-01'),
            {'domain': 'webrtc.org', 'organization': 'Google'},
        )
        self.assertEqual(
            self._resolve(
                'contractor@amd.corp-partner.google.com', '2026-01-01'
            ),
            {'domain': 'amd.corp-partner.google.com', 'organization': 'AMD'},
        )
        self.assertEqual(
            self._resolve('eng@lunarg.com', '2026-01-01'),
            {'domain': 'lunarg.com', 'organization': 'LunarG'},
        )
        self.assertEqual(
            self._resolve('someone@gmail.com', '2026-01-01'),
            {'domain': 'gmail.com', 'organization': 'Individual'},
        )

    def test_unmapped_and_unknown_domains_have_null_organization(self) -> None:
        self.assertEqual(
            self._resolve('user@newcompany.io', '2026-01-01'),
            {'domain': 'newcompany.io', 'organization': None},
        )
        self.assertEqual(
            self._resolve('user@mystery.example', '2026-01-01'),
            {'domain': 'mystery.example', 'organization': None},
        )

    def test_date_ranges_and_exact_start_boundaries(self) -> None:
        self.assertEqual(
            self._resolve('bob@chromium.org', '2026-01-01'),
            {'domain': None, 'organization': 'Undisclosed'},
        )
        self.assertEqual(
            self._resolve('bob@chromium.org', '2024-06-01'),
            {'domain': None, 'organization': 'Undisclosed'},
        )
        self.assertEqual(
            self._resolve('bob@chromium.org', '2024-05-31'),
            {'domain': 'igalia.com', 'organization': 'Igalia'},
        )
        self.assertEqual(
            self._resolve('bob@chromium.org', '2021-03-15'),
            {'domain': 'igalia.com', 'organization': 'Igalia'},
        )
        self.assertEqual(
            self._resolve('bob@chromium.org', '2021-03-14'),
            {'domain': 'google.com', 'organization': 'Google'},
        )
        self.assertEqual(
            self._resolve('bob@chromium.org', '2015-01-10'),
            {'domain': 'google.com', 'organization': 'Google'},
        )

    def test_preceding_earliest_start_returns_undisclosed(self) -> None:
        undisclosed = {'domain': None, 'organization': 'Undisclosed'}
        self.assertEqual(
            self._resolve('alice@chromium.org', '2025-04-10'), undisclosed
        )
        self.assertEqual(
            self._resolve('bob@chromium.org', '2015-01-09'), undisclosed
        )
        self.assertEqual(
            self._resolve('eve@yoav.ws', '2017-12-31'), undisclosed
        )

    def test_special_tokens_and_personal_domains_in_affiliations(self) -> None:
        self.assertEqual(
            self._resolve('CAROL@chromium.org', '2025-01-01'),
            {'domain': None, 'organization': 'Individual'},
        )
        self.assertEqual(
            self._resolve('dave@chromium.org', '2025-01-01'),
            {'domain': 'gmail.com', 'organization': 'Individual'},
        )
        self.assertEqual(
            self._resolve('eve@yoav.ws', '2023-01-01'),
            {'domain': 'google.com', 'organization': 'Google'},
        )
        self.assertEqual(
            self._resolve('eve@yoav.ws', '2020-01-01'),
            {'domain': 'unlisted-startup.ai', 'organization': None},
        )

    def test_cli_outputs_json_and_defaults_to_today(self) -> None:
        fake_today = datetime.date(2022, 1, 1)
        with (
            mock.patch(
                'email_affiliation.datetime.date', wraps=datetime.date
            ) as mock_date,
            mock.patch('sys.stdout', new_callable=io.StringIO) as out,
        ):
            mock_date.today.return_value = fake_today
            rc = email_affiliation.main(self._cli_args('bob@chromium.org'))
        self.assertEqual(rc, 0)
        self.assertEqual(
            json.loads(out.getvalue()),
            {'domain': 'igalia.com', 'organization': 'Igalia'},
        )

        with mock.patch('sys.stdout', new_callable=io.StringIO) as out:
            rc = email_affiliation.main(
                self._cli_args('alice@chromium.org', '2025-04-11')
            )
        self.assertEqual(rc, 0)
        self.assertEqual(
            json.loads(out.getvalue()),
            {'domain': 'microsoft.com', 'organization': 'Microsoft'},
        )

    def test_invalid_inputs_return_error(self) -> None:
        bad_cases = [
            ['not-an-email'],
            ['user@localhost'],
            ['two@@example.com'],
            ['has space@example.com'],
            ['alice@chromium.org', '2025-13-01'],
            ['alice@chromium.org', '20250411'],
            ['alice@chromium.org', 'not-a-date'],
        ]
        for args in bad_cases:
            with (
                self.subTest(args=args),
                mock.patch('sys.stderr', new_callable=io.StringIO) as err,
            ):
                rc = email_affiliation.main(self._cli_args(*args))
                self.assertEqual(rc, 1)
                self.assertIn('Error:', err.getvalue())

    def test_real_checkout_files_load_and_resolve(self) -> None:
        contributors = email_affiliation.load_affiliations()
        domain_to_org = email_affiliation.load_org_list()
        self.assertGreater(len(contributors), 0)
        self.assertGreater(len(domain_to_org), 0)
        for email in contributors:
            result = email_affiliation.resolve_affiliation(
                email, datetime.date(2030, 1, 1), contributors, domain_to_org
            )
            self.assertIn('domain', result)
            self.assertIn('organization', result)


if __name__ == '__main__':
    unittest.main()
