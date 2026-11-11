# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for find_expired_in_file.py."""

import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest
from datetime import datetime, timedelta
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import find_candidates as fc  # noqa: E402
import find_expired_in_file as fef  # noqa: E402

_REL_A = "tools/metrics/histograms/metadata/aaa/histograms.xml"
_REL_OTHER = "tools/metrics/histograms/metadata/zzz/histograms.xml"
_EXPIRED = "2020-01-01"
_FUTURE = "2999-01-01"


def _hist(name, expires=_EXPIRED, summary=None, extra=""):
    if summary is None:
        summary = f"Summary of {name}."
    return (
        f'<histogram name="{name}" units="units" expires_after="{expires}">'
        f"<owner>a@chromium.org</owner><summary>{summary}</summary>{extra}"
        "</histogram>"
    )


def _xml(*histograms):
    return (
        "<histogram-configuration><histograms>"
        + "".join(histograms)
        + "</histograms></histogram-configuration>"
    )


def _cs_result(returncode=0, stdout="", stderr=""):
    return mock.Mock(returncode=returncode, stdout=stdout, stderr=stderr)


class TempRootTestCase(unittest.TestCase):
    """Creates a temp repo root and makes it the current directory."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.root = os.path.realpath(self._tmp.name)
        old_cwd = os.getcwd()
        os.chdir(self.root)
        self.addCleanup(os.chdir, old_cwd)

    def _write(self, rel_path, content):
        abs_path = os.path.join(self.root, *rel_path.split("/"))
        os.makedirs(os.path.dirname(abs_path), exist_ok=True)
        with open(abs_path, "w", encoding="utf-8") as f:
            f.write(content)


class GetExpiryThresholdsTest(TempRootTestCase):
    def test_date_limit_is_one_year_ago(self):
        date_limit, _ = fc.get_expiry_thresholds(self.root)
        expected = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")
        self.assertIn(
            date_limit,
            {
                expected,
                (datetime.now() - timedelta(days=366)).strftime("%Y-%m-%d"),
            },
        )

    def test_reads_major_from_version_file(self):
        self._write("chrome/VERSION", "MAJOR=150\nMINOR=0\nBUILD=1\n")
        _, m_limit = fc.get_expiry_thresholds(self.root)
        self.assertEqual(m_limit, 138)

    def test_defaults_to_current_directory(self):
        self._write("chrome/VERSION", "MAJOR=130\n")
        _, m_limit = fc.get_expiry_thresholds()
        self.assertEqual(m_limit, 118)

    def test_missing_version_file_falls_back_to_zero(self):
        _, m_limit = fc.get_expiry_thresholds(self.root)
        self.assertEqual(m_limit, 0)

    def test_malformed_version_file_falls_back_to_zero(self):
        self._write("chrome/VERSION", "MAJOR=abc\n")
        _, m_limit = fc.get_expiry_thresholds(self.root)
        self.assertEqual(m_limit, 0)

    def test_milestone_expiry_uses_major_minus_12(self):
        self._write("chrome/VERSION", "MAJOR=150\n")
        date_limit, m_limit = fc.get_expiry_thresholds(self.root)
        self.assertTrue(fc.is_expired("M137", date_limit, m_limit))
        self.assertFalse(fc.is_expired("M138", date_limit, m_limit))
        self.assertFalse(fc.is_expired("M150", date_limit, m_limit))

    def test_milestone_never_expires_without_version(self):
        date_limit, m_limit = fc.get_expiry_thresholds(self.root)
        self.assertFalse(fc.is_expired("M1", date_limit, m_limit))


class CountRecordingFilesTest(unittest.TestCase):
    _REAL_PATH = "chromium/chromium/src/base/metrics/foo.cc"
    _REAL_PATH_2 = "platform2/metrics/bar.cc"
    _MIRROR_PATH = "github/youtube/cobalt/base/metrics/foo.cc"

    def _count(self, result=None, side_effect=None, name="Foo.Bar.Baz"):
        with mock.patch.object(
            fef.subprocess, "run", return_value=result, side_effect=side_effect
        ) as run:
            count = fef.count_recording_files(name)
        return count, run

    def test_builds_expected_command(self):
        _, run = self._count(_cs_result(stdout=self._REAL_PATH + "\n"))
        self.assertEqual(
            run.call_args.args[0],
            [
                "cs",
                "-l",
                "--max_num_results=50",
                "--",
                'case:yes "Foo.Bar.Baz"'
                " -file:tools/metrics/histograms/metadata"
                " -file:tools/metrics/histograms/test_data/"
                " -file:expired_histograms_array"
                " -file:analysis/chrome/kaslow"
                " -file:chrome/finch/gcl_studies",
            ],
        )
        self.assertEqual(
            run.call_args.kwargs["timeout"], fef._CS_TIMEOUT_SECONDS
        )

    def test_escapes_quotes_and_backslashes_in_name(self):
        _, run = self._count(_cs_result(stdout=""), name='A\\B"C')
        self.assertTrue(
            run.call_args.args[0][4].startswith('case:yes "A\\\\B\\"C" -file:')
        )

    def test_counts_real_paths(self):
        count, _ = self._count(
            _cs_result(stdout=f"{self._REAL_PATH}\n{self._REAL_PATH_2}\n")
        )
        self.assertEqual(count, 2)

    def test_empty_output_returns_zero(self):
        count, _ = self._count(_cs_result(stdout=""))
        self.assertEqual(count, 0)

    def test_whitespace_only_lines_are_ignored(self):
        count, _ = self._count(_cs_result(stdout="  \n\n\t\n"))
        self.assertEqual(count, 0)

    def test_mirror_only_results_return_zero(self):
        count, _ = self._count(
            _cs_result(
                stdout="\n".join(
                    prefix + "base/foo.cc"
                    for prefix in fef._MIRROR_REPO_PREFIXES
                )
            )
        )
        self.assertEqual(count, 0)

    def test_mixed_mirror_and_real_paths_count_only_real(self):
        count, _ = self._count(
            _cs_result(
                stdout="\n".join(
                    [self._MIRROR_PATH, self._REAL_PATH, self._REAL_PATH_2]
                )
            )
        )
        self.assertEqual(count, 2)

    def test_saturated_mirror_only_results_raise(self):
        stdout = "\n".join(
            f"{self._MIRROR_PATH}.{i}" for i in range(fef._CS_MAX_RESULTS)
        )
        with self.assertRaises(RuntimeError):
            self._count(_cs_result(stdout=stdout))

    def test_saturated_results_with_real_path_do_not_raise(self):
        lines = [self._REAL_PATH] + [
            f"{self._MIRROR_PATH}.{i}" for i in range(fef._CS_MAX_RESULTS - 1)
        ]
        count, _ = self._count(_cs_result(stdout="\n".join(lines)))
        self.assertEqual(count, 1)

    def test_nonzero_returncode_raises(self):
        with self.assertRaises(RuntimeError):
            self._count(_cs_result(returncode=1, stdout="0\n", stderr="boom"))

    def test_nonzero_returncode_with_paths_raises(self):
        with self.assertRaises(RuntimeError):
            self._count(
                _cs_result(
                    returncode=2, stdout=self._REAL_PATH + "\n", stderr="boom"
                )
            )

    def test_missing_binary_raises(self):
        with self.assertRaises(RuntimeError):
            self._count(side_effect=FileNotFoundError("cs"))

    def test_timeout_raises(self):
        with self.assertRaises(RuntimeError):
            self._count(side_effect=subprocess.TimeoutExpired("cs", 30))


class GetExpiredFromFileTest(TempRootTestCase):
    def setUp(self):
        super().setUp()
        for module in (fc, fef):
            patcher = mock.patch.object(
                module, "SAFE_HISTOGRAM_FILES", (_REL_A,)
            )
            patcher.start()
            self.addCleanup(patcher.stop)

    def _get(self, counts, exclude=None):
        """Returns (names, count_mock) for the given per-name site counts."""
        with mock.patch.object(
            fef, "count_recording_files", side_effect=lambda n: counts[n]
        ) as count_mock:
            hists = list(
                fef.get_expired_from_file(
                    _REL_A, exclude=exclude, search_root=self.root
                )
            )
        return [h["name"] for h in hists], count_mock

    def test_yields_only_histograms_without_recording_sites(self):
        self._write(
            _REL_A,
            _xml(
                _hist("Foo.Unused"),
                _hist("Foo.Used"),
                _hist("Foo.Future", expires=_FUTURE),
                _hist(
                    "Foo.Intentional",
                    extra="<expired_intentionally>x</expired_intentionally>",
                ),
            ),
        )
        names, count_mock = self._get({"Foo.Unused": 0, "Foo.Used": 3})
        self.assertEqual(names, ["Foo.Unused"])
        self.assertEqual(
            [c.args[0] for c in count_mock.call_args_list],
            ["Foo.Unused", "Foo.Used"],
        )

    def test_skips_token_and_excluded_before_searching(self):
        self._write(
            _REL_A,
            _xml(
                _hist("Foo.{Type}.Shown"),
                _hist("Foo.Excluded"),
                _hist("Foo.Kept"),
            ),
        )
        names, count_mock = self._get({"Foo.Kept": 0}, exclude=["Foo.Excluded"])
        self.assertEqual(names, ["Foo.Kept"])
        count_mock.assert_called_once_with("Foo.Kept")

    def test_result_has_normalized_relative_file_and_fields(self):
        self._write(_REL_A, _xml(_hist("Foo.Unused", summary="Sum.")))
        with mock.patch.object(fef, "count_recording_files", return_value=0):
            (hist,) = fef.get_expired_from_file(
                os.path.join(self.root, *_REL_A.split("/"))
            )
        self.assertEqual(
            hist,
            {
                "file": _REL_A,
                "name": "Foo.Unused",
                "expires_after": _EXPIRED,
                "owners": ["a@chromium.org"],
                "summary": "Sum.",
            },
        )

    def test_relative_path_resolved_against_search_root_not_cwd(self):
        self._write(_REL_A, _xml(_hist("Foo.Unused")))
        other_cwd = tempfile.TemporaryDirectory()
        self.addCleanup(other_cwd.cleanup)
        # Cleanups run in reverse order: restore the CWD before deleting it.
        self.addCleanup(os.chdir, self.root)
        os.chdir(os.path.realpath(other_cwd.name))
        # The file does not exist relative to the CWD.
        self.assertFalse(os.path.exists(_REL_A))

        with mock.patch.object(fef, "count_recording_files", return_value=0):
            (hist,) = fef.get_expired_from_file(_REL_A, search_root=self.root)

        self.assertEqual(hist["name"], "Foo.Unused")
        self.assertEqual(hist["file"], _REL_A)

    def test_cs_failure_propagates(self):
        self._write(_REL_A, _xml(_hist("Foo.Unused")))
        with mock.patch.object(
            fef, "count_recording_files", side_effect=RuntimeError("cs down")
        ):
            with self.assertRaises(RuntimeError):
                list(fef.get_expired_from_file(_REL_A))

    def test_non_allowlisted_file_raises(self):
        self._write(_REL_OTHER, _xml(_hist("Foo.Unused")))
        with self.assertRaises(RuntimeError):
            list(fef.get_expired_from_file(_REL_OTHER))

    def test_malformed_xml_raises_parse_error(self):
        self._write(_REL_A, "<histogram-configuration><histogram")
        with self.assertRaises(fef.ET.ParseError):
            list(fef.get_expired_from_file(_REL_A))


class MainTest(TempRootTestCase):
    def setUp(self):
        super().setUp()
        for module in (fc, fef):
            patcher = mock.patch.object(
                module, "SAFE_HISTOGRAM_FILES", (_REL_A,)
            )
            patcher.start()
            self.addCleanup(patcher.stop)

    def _main(self, argv, counts=None, count_side_effect=None):
        if count_side_effect is None:
            count_side_effect = (counts or {}).__getitem__
        out, err = io.StringIO(), io.StringIO()
        with (
            mock.patch.object(
                fef, "count_recording_files", side_effect=count_side_effect
            ),
            contextlib.redirect_stdout(out),
            contextlib.redirect_stderr(err),
        ):
            code = fef.main(argv)
        return code, out.getvalue(), err.getvalue()

    def test_prints_matches_with_zero_sites(self):
        self._write(
            _REL_A,
            _xml(_hist("Foo.Unused"), _hist("Foo.Used"), _hist("Foo.{T}.X")),
        )
        code, out, err = self._main([_REL_A], {"Foo.Unused": 0, "Foo.Used": 2})
        self.assertEqual(code, 0)
        self.assertEqual(err, "")
        self.assertIn(f"File: {_REL_A}\nName: Foo.Unused\n", out)
        self.assertIn("Owners: a@chromium.org\n", out)
        self.assertIn(f"Expires_after: {_EXPIRED}\n", out)
        self.assertIn("Summary: Summary of Foo.Unused.\n---", out)
        self.assertNotIn("Foo.Used", out)
        self.assertNotIn("{", out)

    def test_names_only(self):
        self._write(_REL_A, _xml(_hist("Foo.A"), _hist("Foo.B")))
        code, out, _ = self._main(
            [_REL_A, "--names-only"], {"Foo.A": 0, "Foo.B": 0}
        )
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines(), ["Foo.A", "Foo.B"])

    def test_exclude(self):
        self._write(_REL_A, _xml(_hist("Foo.A"), _hist("Foo.B")))
        code, out, _ = self._main(
            [_REL_A, "--names-only", "--exclude", "Foo.A"], {"Foo.B": 0}
        )
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines(), ["Foo.B"])

    def test_repeated_exclude_flags_accumulate(self):
        self._write(
            _REL_A, _xml(_hist("Foo.A"), _hist("Foo.B"), _hist("Foo.C"))
        )
        code, out, _ = self._main(
            [
                _REL_A,
                "--names-only",
                "--exclude",
                "Foo.A",
                "--exclude",
                "Foo.B",
            ],
            {"Foo.C": 0},
        )
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines(), ["Foo.C"])

    def test_multi_value_exclude(self):
        self._write(
            _REL_A, _xml(_hist("Foo.A"), _hist("Foo.B"), _hist("Foo.C"))
        )
        code, out, _ = self._main(
            [_REL_A, "--names-only", "--exclude", "Foo.A", "Foo.B"],
            {"Foo.C": 0},
        )
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines(), ["Foo.C"])

    def test_cs_failure_returns_1_without_false_positives(self):
        self._write(_REL_A, _xml(_hist("Foo.A"), _hist("Foo.B")))
        code, out, err = self._main(
            [_REL_A, "--names-only"],
            count_side_effect=RuntimeError("'cs' timed out"),
        )
        self.assertEqual(code, 1)
        self.assertEqual(out, "")
        self.assertIn("ERROR: 'cs' timed out", err)

    def test_malformed_xml_returns_1(self):
        self._write(_REL_A, "<histogram-configuration><histogram")
        code, out, err = self._main([_REL_A], {})
        self.assertEqual(code, 1)
        self.assertEqual(out, "")
        self.assertIn("ERROR:", err)

    def test_non_allowlisted_file_returns_2(self):
        self._write(_REL_OTHER, _xml(_hist("Foo.A")))
        code, out, err = self._main([_REL_OTHER], {"Foo.A": 0})
        self.assertEqual(code, 2)
        self.assertEqual(out, "")
        self.assertIn("not an allowlisted", err)
        self.assertIn(_REL_A, err)

    def test_summary_ellipsis_only_when_longer_than_150(self):
        exact = "a" * 150
        longer = "b" * 151
        self._write(
            _REL_A,
            _xml(
                _hist("Foo.Exact", summary=exact),
                _hist("Foo.Longer", summary=longer),
            ),
        )
        code, out, _ = self._main([_REL_A], {"Foo.Exact": 0, "Foo.Longer": 0})
        self.assertEqual(code, 0)
        self.assertIn(f"Summary: {exact}\n---", out)
        self.assertIn(f"Summary: {'b' * 150}...\n---", out)
        self.assertNotIn("b" * 151, out)

    def test_empty_summary(self):
        self._write(_REL_A, _xml(_hist("Foo.A", summary="")))
        code, out, _ = self._main([_REL_A], {"Foo.A": 0})
        self.assertEqual(code, 0)
        self.assertIn("Summary: No summary.\n---", out)


if __name__ == "__main__":
    unittest.main()
