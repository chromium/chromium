# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Assertion for checking if modified files build properly."""

import pathlib
import subprocess


def _repo_root() -> pathlib.Path:
    try:
        raw_output = subprocess.check_output(['gclient', 'root'], text=True)
        return pathlib.Path(raw_output.strip()) / 'src'
    except Exception:
        return pathlib.Path(__file__).resolve().parents[3]


def _format_target(target: str) -> str:
    """Formats a GN target label for ninja/autoninja."""
    target = target.split('(')[0].strip()
    return target.lstrip('/')


def _get_targets_for_file(
    repo_root: pathlib.Path, out_dir: str, rel_path: str
) -> list[str]:
    """Uses gn refs to find GN build targets for a given file."""
    cmd = ['gn', 'refs', out_dir, rel_path]
    try:
        res = subprocess.run(
            cmd,
            cwd=repo_root,
            capture_output=True,
            text=True,
            check=False,
        )
        if res.returncode != 0 or not res.stdout:
            return []
        targets = set()
        for line in res.stdout.splitlines():
            line = line.strip()
            if not line:
                continue
            formatted = _format_target(line)
            if formatted:
                targets.add(formatted)
        return list(targets)
    except Exception:
        return []


def check_build(_: str, context) -> dict:
    """Checks if specified modified files build properly.

    Assertion format in promptfoo config:
    - type: python:agents/testing/asserts/check_build.py:check_build
      config:
        files:
          - path/to/file1.cc
          - path/to/file2.h
        out_dir: out/Default
        timeout: 300
    """
    out_dir = 'out/Default'
    timeout = 60
    files = context.get('config', {}).get('files', {})
    repo_root = _repo_root()

    non_existent_files = []
    files_with_no_targets = []
    targets = set()

    for f in files:
        full_path = repo_root / f
        if not full_path.exists():
            non_existent_files.append(f)
            continue

        file_targets = _get_targets_for_file(repo_root, out_dir, f)
        if not file_targets:
            files_with_no_targets.append(f)
        else:
            targets.update(file_targets)

    if non_existent_files:
        return {
            'pass': False,
            'reason': (
                'The following files do not exist:\n'
                + '\n'.join(non_existent_files)
            ),
            'score': 0,
        }

    if not targets:
        if files_with_no_targets:
            return {
                'pass': False,
                'reason': (
                    'Could not find build targets for files:\n'
                    + '\n'.join(files_with_no_targets)
                ),
                'score': 0,
            }
        return {
            'pass': False,
            'reason': 'No modified files specified or detected.',
            'score': 0,
        }

    build_cmd = ['autoninja', '-C', out_dir] + sorted(targets)
    try:
        proc = subprocess.run(
            build_cmd,
            cwd=repo_root,
            timeout=timeout,
            capture_output=True,
            text=True,
            check=False,
        )
        if proc.returncode == 0:
            return {
                'pass': True,
                'reason': (
                    f'Build succeeded for targets: {", ".join(sorted(targets))}'
                ),
                'score': 1,
            }
        else:
            err_msg = proc.stderr.strip() or proc.stdout.strip()
            return {
                'pass': False,
                'reason': (
                    'Build failed for targets '
                    f'{", ".join(sorted(targets))}:\n{err_msg}'
                ),
                'score': 0,
            }
    except subprocess.TimeoutExpired:
        return {
            'pass': False,
            'reason': f'Build timed out after {timeout} seconds.',
            'score': 0,
        }
    except Exception as e:
        return {
            'pass': False,
            'reason': f'Error executing build command: {e}',
            'score': 0,
        }
