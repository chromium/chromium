#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for pinpoint_evaluator.py."""

import io
import itertools
from pathlib import Path
import random
import sys
import unittest
from unittest import mock

import pinpoint_evaluator

# `pp s --show-all` output of a real Speedometer 3 job (20 subtests + Score)
# with an anonymized header.
_SP3_TABLE = (
    Path(__file__).resolve().parent / "testdata" / "pp_show_all_sp3.txt"
).read_text(encoding="utf-8")

_TABLE_HEADER = (
    "  metric      base±std     exp±std    chg%       p  sig  direction\n"
    " " + "─" * 66 + "\n"
)


def _row(
    name: str,
    base_mean: float,
    exp_mean: float,
    p: float,
    direction: str = "smaller-better",
    std: float = 0.5,
) -> str:
    """Formats a table row the way `pp s --show-all` prints it."""
    chg = (exp_mean - base_mean) / base_mean * 100
    sig = "*" if p < 0.01 else ""
    return (
        f"  {name:<12} {base_mean:.2f} ±{std:.2f}  {exp_mean:.2f} ±{std:.2f}  "
        f"{chg:+.2f}%  {p:.4f}  {sig:>3}  {direction}"
    )


def _table(overrides=None, score=None) -> str:
    """Returns a table of 20 unaffected subtests, with `overrides` applied."""
    overrides = overrides or {}
    rows = []
    for i in range(20):
        name = f"Subtest-{i:02d}"
        rows.append(overrides.get(name, _row(name, 50.0, 50.01, 0.6)))
    rows.append(score or _row("Score", 30.0, 30.0, 0.9, "bigger-better"))
    return _TABLE_HEADER + "\n".join(rows) + "\n"


def _names(stats):
    return [s.name for s in stats]


class MultipleTestingTest(unittest.TestCase):
    def test_benjamini_hochberg(self):
        # Reference values from R: p.adjust(c(0.01, 0.04, 0.03, 0.005), "BH").
        for got, want in zip(
            pinpoint_evaluator.benjamini_hochberg([0.01, 0.04, 0.03, 0.005]),
            [0.02, 0.04, 0.04, 0.02],
        ):
            self.assertAlmostEqual(got, want)
        self.assertEqual(pinpoint_evaluator.benjamini_hochberg([]), [])
        self.assertEqual(
            pinpoint_evaluator.benjamini_hochberg([0.01, 0.01]), [0.01, 0.01]
        )

    def test_benjamini_hochberg_invariants(self):
        rng = random.Random(0)
        for _ in range(200):
            p_values = [rng.random() ** 3 for _ in range(rng.randint(1, 30))]
            m = len(p_values)
            pairs = list(
                zip(p_values, pinpoint_evaluator.benjamini_hochberg(p_values))
            )
            for p, q in pairs:
                # Between the raw p-value and the Bonferroni bound m * p.
                self.assertLessEqual(p, q + 1e-12)
                self.assertLessEqual(q, min(1.0, m * p) + 1e-12)
            # A smaller p-value never gets a larger q-value.
            for (p1, q1), (p2, q2) in itertools.combinations(pairs, 2):
                if p1 < p2:
                    self.assertLessEqual(q1, q2 + 1e-12)


class EvaluateResultsTest(unittest.TestCase):
    def test_parses_real_table(self):
        res = pinpoint_evaluator.evaluate_results(_SP3_TABLE)
        self.assertEqual(res["num_subtests"], 20)
        score = res["score_stat"]
        self.assertEqual(score.direction, "bigger-better")
        self.assertTrue(score.is_improved_dir)
        self.assertAlmostEqual(score.chg_pct, 0.16)
        self.assertAlmostEqual(score.p_two_sided, 0.0597)

    def test_real_table_decision(self):
        res = pinpoint_evaluator.evaluate_results(_SP3_TABLE)
        self.assertTrue(res["keep"])
        self.assertEqual(
            _names(res["confirmed_improvements"]),
            [
                "TodoMVC-jQuery",
                "TodoMVC-JavaScript-ES6-Webpack-Complex-DOM",
                "Editor-TipTap",
            ],
        )
        self.assertEqual(
            _names(res["borderline_improvements"]), ["TodoMVC-Lit-Complex-DOM"]
        )
        self.assertEqual(res["confirmed_regressions"], [])
        self.assertEqual(
            _names(res["borderline_regressions"]), ["Editor-CodeMirror"]
        )
        self.assertIn("subtest-only win", res["reasons"][-1])

    def test_se_ci_and_mde(self):
        table = _table(
            overrides={
                "Subtest-00": _row("Subtest-00", 100.0, 99.0, 0.0001, std=1.0)
            },
            score=_row("Score", 30.0, 30.0, 0.9, "bigger-better", std=0.3),
        )
        res = pinpoint_evaluator.evaluate_results(table, repeat=100)
        sub = res["confirmed_improvements"][0]
        # SE_% = 100 * sqrt((1^2 + 1^2) / 100) / 100.
        self.assertAlmostEqual(sub.se_pct, 0.141421, places=5)
        self.assertAlmostEqual(sub.ci95_low, -1.277181, places=5)
        self.assertAlmostEqual(sub.ci95_high, -0.722819, places=5)
        # Subtests use the corrected one-sided alpha / m = 0.05 / 20.
        self.assertAlmostEqual(sub.mde_80_pct / sub.se_pct, 3.6487, places=4)
        # `Score` is tested at the unadjusted two-sided alpha = 0.05.
        score = res["score_stat"]
        self.assertAlmostEqual(score.se_pct, 0.141421, places=5)
        self.assertAlmostEqual(
            score.mde_80_pct / score.se_pct, 2.8016, places=4
        )

    def test_isolated_noise_regression_is_ignored(self):
        res = pinpoint_evaluator.evaluate_results(
            _table(
                overrides={
                    "Subtest-00": _row("Subtest-00", 50.0, 49.5, 0.0001),
                    "Subtest-01": _row("Subtest-01", 50.0, 50.2, 0.03),
                }
            )
        )
        self.assertTrue(res["keep"])
        self.assertEqual(_names(res["confirmed_improvements"]), ["Subtest-00"])
        self.assertEqual(res["confirmed_regressions"], [])
        self.assertEqual(_names(res["borderline_regressions"]), ["Subtest-01"])

    def test_distributed_regressions_are_confirmed(self):
        res = pinpoint_evaluator.evaluate_results(
            _table(
                overrides={
                    "Subtest-00": _row("Subtest-00", 50.0, 49.5, 0.0001),
                    "Subtest-01": _row("Subtest-01", 50.0, 50.3, 0.006),
                    "Subtest-02": _row("Subtest-02", 50.0, 50.2, 0.008),
                    "Subtest-03": _row("Subtest-03", 50.0, 50.1, 0.009),
                }
            )
        )
        self.assertFalse(res["keep"])
        self.assertEqual(
            _names(res["confirmed_regressions"]),
            ["Subtest-01", "Subtest-02", "Subtest-03"],
        )
        # None of them would pass a Bonferroni threshold (alpha / m) on its
        # own; BH confirms them because they move together.
        for s in res["confirmed_regressions"]:
            self.assertLess(s.q_bh_reg, 0.05)
            self.assertGreater(s.p_one_sided_reg, 0.05 / 20)
        self.assertIn("confirmed subtest regression(s)", res["reasons"][-1])

    def test_score_veto(self):
        res = pinpoint_evaluator.evaluate_results(
            _table(
                overrides={
                    "Subtest-00": _row("Subtest-00", 50.0, 49.5, 0.0001)
                },
                score=_row("Score", 30.0, 29.955, 0.1, "bigger-better"),
            )
        )
        self.assertFalse(res["keep"])
        self.assertTrue(res["score_vetoed"])
        self.assertFalse(res["score_regressed_sig"])
        self.assertIn("vetoed", res["reasons"][-1])

    def test_score_improvement_alone_keeps(self):
        res = pinpoint_evaluator.evaluate_results(
            _table(score=_row("Score", 30.0, 30.09, 0.01, "bigger-better"))
        )
        self.assertTrue(res["keep"])
        self.assertTrue(res["score_improved_sig"])
        self.assertEqual(res["confirmed_improvements"], [])
        self.assertEqual(res["reasons"], ["Score +0.30% (p=0.0100) improved"])

    def test_no_improvement_abandons(self):
        res = pinpoint_evaluator.evaluate_results(_table())
        self.assertFalse(res["keep"])
        self.assertEqual(
            res["reasons"],
            ["no improvement survives multiple-testing correction"],
        )

    def test_jetstream_total_is_the_composite(self):
        rows = [
            _row(f"line-{i}", 100.0, 100.0, 0.5, "bigger-better")
            for i in range(10)
        ]
        rows.append(_row("Total", 290.0, 291.0, 0.001, "bigger-better"))
        res = pinpoint_evaluator.evaluate_results(
            _TABLE_HEADER + "\n".join(rows) + "\n"
        )
        self.assertEqual(res["num_subtests"], 10)
        self.assertEqual(res["score_stat"].name, "Total")
        self.assertTrue(res["keep"])
        self.assertEqual(res["reasons"], ["Total +0.34% (p=0.0010) improved"])

    def test_unparsable_row_raises(self):
        table = _SP3_TABLE.replace(
            "19.31 ±0.28   +0.28%", "19.31 ±0.28   n/a", 1
        )
        self.assertNotEqual(table, _SP3_TABLE)
        with self.assertRaisesRegex(ValueError, "Editor-CodeMirror"):
            pinpoint_evaluator.evaluate_results(table)

    def test_missing_score_raises(self):
        table = "\n".join(
            line
            for line in _SP3_TABLE.splitlines()
            if not line.strip().startswith("Score ")
        )
        with self.assertRaisesRegex(ValueError, "Score"):
            pinpoint_evaluator.evaluate_results(table)

    def test_no_rows_raises(self):
        with self.assertRaises(ValueError):
            pinpoint_evaluator.evaluate_results("No results found.\n")


class FetchResultsTest(unittest.TestCase):
    @mock.patch("pinpoint_evaluator.run_command")
    def test_returns_table(self, mock_run: mock.MagicMock):
        # The patch subject in the header contains "running" and "queued".
        mock_run.return_value = (0, _SP3_TABLE, "")
        self.assertEqual(pinpoint_evaluator.fetch_results("abc"), _SP3_TABLE)
        mock_run.assert_called_once_with(["pp", "s", "--show-all", "abc"])

    @mock.patch("pinpoint_evaluator.run_command")
    def test_not_ready(self, mock_run: mock.MagicMock):
        for out in (
            "Error: Job is not completed (status: Running)\n",
            "Error: Results page has no histogram data yet.\n",
        ):
            mock_run.return_value = (0, out, "")
            self.assertIsNone(pinpoint_evaluator.fetch_results("abc"))

    @mock.patch("pinpoint_evaluator.run_command")
    def test_command_failure(self, mock_run: mock.MagicMock):
        mock_run.return_value = (1, "", "boom")
        self.assertIsNone(pinpoint_evaluator.fetch_results("abc"))


class MainTest(unittest.TestCase):
    def _run_evaluate(self, table: str) -> str:
        argv = ["prog", "--action", "evaluate", "--job-id", "x"]
        with (
            mock.patch.object(sys, "argv", argv),
            mock.patch("pinpoint_evaluator.fetch_results", return_value=table),
            mock.patch("sys.stdout", new_callable=io.StringIO) as out,
        ):
            pinpoint_evaluator.main()
        return out.getvalue()

    def test_evaluate_prints_decision(self):
        out = self._run_evaluate(_SP3_TABLE)
        self.assertIn("m=20 subtests", out)
        self.assertIn("Decision: Propose / Keep CL (3 confirmed subtest", out)

    def test_evaluate_fails_on_unparsable_output(self):
        with mock.patch("sys.stderr", new_callable=io.StringIO):
            with self.assertRaises(SystemExit) as cm:
                self._run_evaluate("No results found.\n")
        self.assertEqual(cm.exception.code, 3)


if __name__ == "__main__":
    unittest.main()
