# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for find_candidates.py."""

import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import find_candidates as fc

_REL_A = "tools/metrics/histograms/metadata/aaa/histograms.xml"
_REL_B = "tools/metrics/histograms/metadata/bbb/histograms.xml"
_EXPIRED = "2020-01-01"
_EXPIRED_OTHER = "2020-06-01"


def _hist(name, expires=_EXPIRED, owners=("a@chromium.org",), extra=""):
    owners_xml = "".join(f"<owner>{o}</owner>" for o in owners)
    return (
        f'<histogram name="{name}" units="units" expires_after="{expires}">'
        f"{owners_xml}<summary>Summary of {name}.</summary>{extra}"
        "</histogram>"
    )


def _xml(*histograms):
    return (
        "<histogram-configuration><histograms>"
        + "".join(histograms)
        + "</histograms></histogram-configuration>"
    )


class RelatedPrefixTest(unittest.TestCase):
    def test_three_or_more_segments(self):
        self.assertEqual(
            fc._related_prefix("Download.Bubble.Shown"), "Download.Bubble"
        )
        self.assertEqual(fc._related_prefix("A.B.C.D.E"), "A.B")

    def test_two_segments(self):
        self.assertEqual(fc._related_prefix("Download.Shown"), "Download")

    def test_one_segment_or_empty(self):
        self.assertEqual(fc._related_prefix("Download"), "")
        self.assertEqual(fc._related_prefix(""), "")

    def test_token_suffix_is_stripped(self):
        self.assertEqual(
            fc._related_prefix("Download.Bubble.{Type}.Shown"),
            "Download.Bubble",
        )
        self.assertEqual(fc._related_prefix("Download.{Type}"), "Download")
        self.assertEqual(fc._related_prefix("{Type}.Shown"), "")
        self.assertEqual(fc._related_prefix("Download{Type}.Shown"), "Download")


class FindCandidatesTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.root = self._tmp.name

    def _write(self, rel_path, content):
        abs_path = os.path.join(self.root, *rel_path.split("/"))
        os.makedirs(os.path.dirname(abs_path), exist_ok=True)
        with open(abs_path, "w", encoding="utf-8") as f:
            f.write(content)

    def _find(self, files=(_REL_A,)):
        with mock.patch.object(fc, "SAFE_HISTOGRAM_FILES", tuple(files)):
            results = list(fc.find_candidates(self.root))
        return {c["name"]: c for c in results}

    def test_groups_siblings_with_same_expiry_owners_prefix(self):
        self._write(
            _REL_A,
            _xml(
                _hist("Download.Bubble.A"),
                _hist("Download.Bubble.B"),
                _hist("Download.Bubble.C"),
            ),
        )
        found = self._find()
        self.assertEqual(
            set(found),
            {"Download.Bubble.A", "Download.Bubble.B", "Download.Bubble.C"},
        )
        self.assertEqual(
            found["Download.Bubble.A"]["related_histograms"],
            ["Download.Bubble.B", "Download.Bubble.C"],
        )
        self.assertEqual(
            found["Download.Bubble.B"]["related_histograms"],
            ["Download.Bubble.A", "Download.Bubble.C"],
        )
        self.assertEqual(
            found["Download.Bubble.C"]["related_histograms"],
            ["Download.Bubble.A", "Download.Bubble.B"],
        )

    def test_owners_match_case_and_order_insensitively(self):
        self._write(
            _REL_A,
            _xml(
                _hist(
                    "Download.Bubble.A",
                    owners=("A@chromium.org", "b@chromium.org"),
                ),
                _hist(
                    "Download.Bubble.B",
                    owners=(" b@Chromium.org ", "a@chromium.org"),
                ),
            ),
        )
        found = self._find()
        self.assertEqual(
            found["Download.Bubble.A"]["related_histograms"],
            ["Download.Bubble.B"],
        )
        self.assertEqual(
            found["Download.Bubble.B"]["related_histograms"],
            ["Download.Bubble.A"],
        )

    def test_two_segment_names_share_first_segment(self):
        self._write(_REL_A, _xml(_hist("Download.A"), _hist("Download.B")))
        found = self._find()
        self.assertEqual(
            found["Download.A"]["related_histograms"], ["Download.B"]
        )

    def test_token_names_group_by_base_prefix(self):
        self._write(
            _REL_A,
            _xml(_hist("Download.Bubble.{Type}.A"), _hist("Download.Bubble.B")),
        )
        found = self._find()
        self.assertEqual(
            found["Download.Bubble.{Type}.A"]["related_histograms"],
            ["Download.Bubble.B"],
        )

    def test_no_grouping_when_expiry_differs(self):
        self._write(
            _REL_A,
            _xml(
                _hist("Download.Bubble.A", expires=_EXPIRED),
                _hist("Download.Bubble.B", expires=_EXPIRED_OTHER),
            ),
        )
        found = self._find()
        self.assertEqual(len(found), 2)
        for c in found.values():
            self.assertNotIn("related_histograms", c)

    def test_no_grouping_when_owner_sets_differ(self):
        self._write(
            _REL_A,
            _xml(
                _hist("Download.Bubble.A", owners=("a@chromium.org",)),
                _hist("Download.Bubble.B", owners=("b@chromium.org",)),
                _hist(
                    "Download.Bubble.C",
                    owners=("a@chromium.org", "b@chromium.org"),
                ),
            ),
        )
        found = self._find()
        self.assertEqual(len(found), 3)
        for c in found.values():
            self.assertNotIn("related_histograms", c)

    def test_no_grouping_when_owners_empty(self):
        self._write(
            _REL_A,
            _xml(
                _hist("Download.Bubble.A", owners=()),
                _hist("Download.Bubble.B", owners=()),
            ),
        )
        found = self._find()
        self.assertEqual(len(found), 2)
        for c in found.values():
            self.assertNotIn("related_histograms", c)

    def test_no_grouping_when_prefix_differs(self):
        self._write(
            _REL_A,
            _xml(
                _hist("Download.Bubble.A"),
                _hist("Download.Service.B"),
                _hist("Download.Foo"),
                _hist("Other"),
            ),
        )
        found = self._find()
        self.assertEqual(len(found), 4)
        for c in found.values():
            self.assertNotIn("related_histograms", c)

    def test_no_grouping_across_files(self):
        self._write(_REL_A, _xml(_hist("Download.Bubble.A")))
        self._write(_REL_B, _xml(_hist("Download.Bubble.B")))
        found = self._find(files=(_REL_A, _REL_B))
        self.assertEqual(len(found), 2)
        for c in found.values():
            self.assertNotIn("related_histograms", c)

    def test_expired_intentionally_is_excluded_from_group(self):
        self._write(
            _REL_A,
            _xml(
                _hist("Download.Bubble.A"),
                _hist(
                    "Download.Bubble.B",
                    extra="<expired_intentionally>Why.</expired_intentionally>",
                ),
            ),
        )
        found = self._find()
        self.assertEqual(set(found), {"Download.Bubble.A"})
        self.assertNotIn("related_histograms", found["Download.Bubble.A"])

    def test_related_histograms_capped_at_nine(self):
        names = [f"Download.Bubble.H{i:02d}" for i in range(12)]
        self._write(_REL_A, _xml(*[_hist(n) for n in names]))
        found = self._find()
        self.assertEqual(len(found), 12)
        for name, c in found.items():
            related = c["related_histograms"]
            self.assertEqual(len(related), 9)
            self.assertNotIn(name, related)
            self.assertEqual(related, sorted(related))
        self.assertEqual(
            found["Download.Bubble.H00"]["related_histograms"], names[1:10]
        )

    def test_omits_related_histograms_when_alone(self):
        self._write(_REL_A, _xml(_hist("Download.Bubble.A")))
        found = self._find()
        self.assertEqual(set(found), {"Download.Bubble.A"})
        self.assertNotIn("related_histograms", found["Download.Bubble.A"])

    def test_candidate_fields_preserved(self):
        self._write(_REL_A, _xml(_hist("Download.Bubble.A")))
        c = self._find()["Download.Bubble.A"]
        self.assertEqual(c["file"], _REL_A)
        self.assertEqual(c["expires_after"], _EXPIRED)
        self.assertEqual(c["owners"], ["a@chromium.org"])
        self.assertEqual(c["summary"], "Summary of Download.Bubble.A.")

    def test_malformed_xml_is_skipped_and_valid_file_still_returned(self):
        self._write(_REL_A, "<histogram-configuration><histograms><histogram")
        self._write(_REL_B, _xml(_hist("Download.Bubble.B")))
        found = self._find(files=(_REL_A, _REL_B))
        self.assertEqual(set(found), {"Download.Bubble.B"})

    def test_oserror_reading_xml_is_skipped_and_valid_file_still_returned(
        self,
    ):
        self._write(_REL_A, _xml(_hist("Download.Bubble.A")))
        self._write(_REL_B, _xml(_hist("Download.Bubble.B")))
        real_parse = fc.ET.parse

        def fake_parse(source, *args, **kwargs):
            if str(source).endswith(_REL_A.replace("/", os.sep)):
                raise OSError("Permission denied")
            return real_parse(source, *args, **kwargs)

        with mock.patch.object(fc.ET, "parse", side_effect=fake_parse):
            found = self._find(files=(_REL_A, _REL_B))
        self.assertEqual(set(found), {"Download.Bubble.B"})

    def _write_cross_file_histograms(self):
        self._write(_REL_A, _xml(_hist("Charlie.X.Y"), _hist("alpha.X.Y")))
        self._write(_REL_B, _xml(_hist("Delta.X.Y"), _hist("Bravo.X.Y")))

    def test_yields_all_candidates_across_files(self):
        self._write_cross_file_histograms()
        self.assertEqual(
            sorted(self._find(files=(_REL_A, _REL_B))),
            ["Bravo.X.Y", "Charlie.X.Y", "Delta.X.Y", "alpha.X.Y"],
        )

    def test_always_shuffles_files_and_each_files_candidates(self):
        self._write_cross_file_histograms()
        with (
            mock.patch.object(fc.random, "shuffle") as shuffle,
            mock.patch.object(fc, "SAFE_HISTOGRAM_FILES", (_REL_A, _REL_B)),
        ):
            list(fc.find_candidates(self.root))
        # Once for the file list and once per file with candidates.
        self.assertEqual(3, shuffle.call_count)

    def test_sort_argument_is_not_accepted(self):
        with self.assertRaises(TypeError):
            list(fc.find_candidates(self.root, sort="asc"))


if __name__ == "__main__":
    unittest.main()
