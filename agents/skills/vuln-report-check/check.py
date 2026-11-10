#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs every check on a bug report and its attached files.

Runs every check file in the `checks/` folder and prints two answers: is the
report complete, and can Chrome's test system reproduce the bug automatically.

We endeavor to keep this script up to date, but passing this check.py does not
guarantee a report will be accepted. Do not paste the output of this script into
your reports.

Usage:
  python3 check.py --report report.md poc.html
  python3 check.py --report - --json < report.md
  python3 check.py --smoke server.py poc.html
"""

from collections.abc import Sequence
import importlib
import importlib.util
import os
import sys
import types

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
# Lets `from checks import common` work no matter which folder the script is
# started from.
if ROOT_DIR not in sys.path:
    sys.path.insert(0, ROOT_DIR)

from checks import common  # noqa: E402

# Files in `checks/` that are not checks: package setup and shared helpers.
_SKIP_MODULES = frozenset({'__init__', 'common'})


def _load_check_modules(
    checks_dir: str | None = None,
) -> tuple[list[tuple[str, types.ModuleType]], list[common.Check]]:
    """Loads every check file in the `checks/` folder.

    A check file that fails to load becomes a "needs judgment" result instead
    of stopping the whole run, so the other checks still report. Checks are
    sorted by their `ORDER` value, then by name, so the output is the same on
    every run.

    Args:
        checks_dir: Folder to load checks from. `None` means the built-in
            `checks/` folder; tests pass another folder.

    Returns:
        A pair: the loaded `(name, module)` list in run order, and a list of
        results for files that failed to load.
    """
    directory = checks_dir or os.path.join(ROOT_DIR, 'checks')
    entries = sorted(os.listdir(directory))
    loaded: list[tuple[int, str, types.ModuleType]] = []
    errors: list[common.Check] = []

    for filename in entries:
        if not filename.endswith('.py') or filename.startswith('_'):
            continue
        stem = filename[:-3]
        if stem in _SKIP_MODULES:
            continue
        try:
            # The built-in folder is a package, so a normal import works. Any
            # other folder is loaded straight from the file path.
            if checks_dir is None:
                module = importlib.import_module(f'checks.{stem}')
            else:
                spec = importlib.util.spec_from_file_location(
                    f'_discovered_{stem}', os.path.join(directory, filename)
                )
                if spec is None or spec.loader is None:
                    continue
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
            # Only files with a `check()` function count as checks. Files
            # without `ORDER` run in the middle (50).
            if callable(getattr(module, 'check', None)):
                order = int(getattr(module, 'ORDER', 50))
                loaded.append((order, stem, module))
        # A broken check file turns into an `unknown` ("needs judgment")
        # result instead of a crash.
        except Exception as exc:  # pylint: disable=broad-except
            errors.append(
                common.Check(
                    f'internal.{stem}',
                    common.UNKNOWN,
                    f'Checker error loading {filename}:'
                    f' {type(exc).__name__}: {exc}',
                    axis=common.AXIS_REPRO,
                )
            )

    loaded.sort(key=lambda item: (item[0], item[1]))
    return [(stem, module) for _, stem, module in loaded], errors


def discover_check_modules(
    checks_dir: str | None = None,
) -> list[types.ModuleType]:
    """Finds and loads all check files, in the same order on every run.

    Files that fail to load are left out here; `run_all_checks` reports them.

    Args:
        checks_dir: Folder to load checks from. `None` means the built-in
            `checks/` folder.

    Returns:
        The loaded check modules, in run order.
    """
    modules, _ = _load_check_modules(checks_dir=checks_dir)
    return [module for _, module in modules]


def run_all_checks(
    submission: common.Submission, checks_dir: str | None = None
) -> list[common.Check]:
    """Runs every check on one bug report and its files.

    A crash in one check does not stop the others. The crash becomes an
    `unknown` ("needs judgment") result, so a person looks at it.

    Args:
        submission: The bug report and its attached files.
        checks_dir: Folder to load checks from. `None` means the built-in
            `checks/` folder.

    Returns:
        All results: load errors first, then each check's results in order.
    """
    modules, errors = _load_check_modules(checks_dir=checks_dir)
    results: list[common.Check] = list(errors)
    for stem, module in modules:
        try:
            results.extend(module.check(submission))
        # Turn a crash inside one check into a "needs judgment" result.
        except Exception as exc:  # pylint: disable=broad-except
            results.append(
                common.Check(
                    f'internal.{stem}',
                    common.UNKNOWN,
                    f'Checker error: {type(exc).__name__}: {exc}',
                    axis=common.AXIS_REPRO,
                )
            )
    return results


def main(
    argv: Sequence[str] | None = None, *, checks_dir: str | None = None
) -> int:
    """Runs all checks from the command line and prints the answers.

    Reading arguments and printing is shared with the single-check scripts in
    `common.run_cli`. Only this entry point turns on the `--smoke` option.

    Args:
        argv: Command-line arguments. `None` means use `sys.argv`.
        checks_dir: Folder to load checks from. `None` means the built-in
            `checks/` folder.

    Returns:
        The process exit code.
    """
    return common.run_cli(
        lambda submission: run_all_checks(submission, checks_dir=checks_dir),
        argv=argv,
        description=__doc__,
        allow_smoke=True,
    )


if __name__ == '__main__':
    sys.exit(main())
