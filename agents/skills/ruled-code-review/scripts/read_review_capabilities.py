#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads review capabilities (rules and skills) for local changes."""

import argparse
import collections
from collections.abc import Iterable
import dataclasses
import io
import json
import pathlib
import re
import subprocess
import sys
from typing import Any, Self

import yaml


class NonGitWorkspaceError(RuntimeError):
    """The workspace doesn't support Git (e.g., CoG).

    Since `dirmd` relies on `git` too, there's nothing this script can do.
    """


def _as_posix(path: pathlib.PurePath) -> str:
    """Returns a POSIX-style path with forward slashes on all platforms."""
    return path.as_posix().replace('\\', '/')


@dataclasses.dataclass(frozen=True)
class CapabilityGroup:
    """A review capability group (i.e., customizations [0]).

    [0]: https://chromium.googlesource.com/infra/infra/+/HEAD/go/src/infra/tools/dirmd/proto/dir_metadata.proto
    """

    path_expression: list[re.Pattern[str]] = dataclasses.field(
        default_factory=list
    )
    path_expression_exclusion: list[re.Pattern[str]] = dataclasses.field(
        default_factory=list
    )
    content_patterns: list[re.Pattern[str]] = dataclasses.field(
        default_factory=list
    )
    rules: set[pathlib.PurePosixPath] = dataclasses.field(default_factory=set)
    skills: set[pathlib.PurePosixPath] = dataclasses.field(default_factory=set)

    @classmethod
    def from_dirmd_json(cls, data: dict[str, Any]) -> Self:
        return cls(
            path_expression=list(
                map(re.compile, data.get('pathExpression', []))
            ),
            path_expression_exclusion=list(
                map(re.compile, data.get('pathExpressionExclusion', []))
            ),
            content_patterns=list(
                map(re.compile, data.get('contentPatterns', []))
            ),
            rules=set(map(pathlib.PurePosixPath, data.get('rules', []))),
            skills=set(
                map(
                    pathlib.PurePosixPath,
                    data.get('reviewCapabilitySkills', []),
                )
            ),
        )

    def matches(
        self,
        file_path: pathlib.PurePosixPath,
        added_lines: list[str],
    ) -> bool:
        """Check if a file and its diff match this capability group's
        filters."""
        path_str = _as_posix(file_path)
        if self.path_expression and not any(
            pattern.fullmatch(path_str) for pattern in self.path_expression
        ):
            return False

        if any(
            pattern.fullmatch(path_str)
            for pattern in self.path_expression_exclusion
        ):
            return False

        return not self.content_patterns or any(
            pattern.search(line)
            for pattern in self.content_patterns
            for line in added_lines
        )


def find_repo_root() -> pathlib.Path:
    """Finds the root of the Chromium git repository from the current working
    directory."""
    try:
        path = subprocess.check_output(
            ['git', 'rev-parse', '--show-toplevel'],
            text=True,
            stderr=subprocess.DEVNULL,
        )
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        raise NonGitWorkspaceError(
            f'"{pathlib.Path.cwd()}" is not in a git repository. This script'
            ' only supports git workspaces.'
        ) from e

    return pathlib.Path(path.strip()).resolve()


def git_diff(
    repo_root: pathlib.Path,
    base: str = 'origin/main',
    files: list[str] | None = None,
) -> subprocess.Popen[str]:
    commits = base if '..' in base else f'{base}...'
    command = [
        'git',
        'diff',
        '--no-color',
        '--no-ext-diff',
        '--unified=0',
        '--src-prefix=a/',
        '--dst-prefix=b/',
        commits,
        '--',
        *(files or []),
    ]
    return subprocess.Popen(
        command,
        cwd=repo_root,
        stdout=subprocess.PIPE,
        text=True,
        encoding='utf-8',
        errors='replace',
    )


_NEW_FILE_MARKER = '+++ '
_NEW_LINE_MARKER = '+'
_DEV_NULL_PATH = '/dev/null'


def parse_unified_diff(
    stream: Iterable[str],
) -> dict[pathlib.PurePosixPath, list[str]]:
    """Parses unified diff stream into a mapping of file_path -> added lines."""
    added_lines_by_path = collections.defaultdict(list)
    # Paths on Windows still use POSIX separators.
    current_path: pathlib.PurePosixPath | None = None

    for raw_line in stream:
        line = raw_line.rstrip('\r\n')
        if line.startswith(_NEW_FILE_MARKER):
            new_path = (
                line.removeprefix(_NEW_FILE_MARKER).strip().removeprefix('b/')
            )
            if new_path == _DEV_NULL_PATH:
                # File was deleted.
                current_path = None
            else:
                current_path = pathlib.PurePosixPath(new_path)
        elif line.startswith(_NEW_LINE_MARKER) and current_path:
            added_lines_by_path[current_path].append(
                line.removeprefix(_NEW_LINE_MARKER)
            )

    return added_lines_by_path


def resolve_capabilities(
    repo_root: pathlib.Path,
    added_lines_by_path: dict[pathlib.PurePosixPath, list[str]],
) -> CapabilityGroup:
    dirs = {
        repo_root.joinpath(*path_from_root.parent.parts)
        for path_from_root in added_lines_by_path
    }
    with subprocess.Popen(
        ['dirmd', 'read', '-form', 'sparse', *sorted(dirs)],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        text=True,
    ) as dirmd_proc:
        assert dirmd_proc.stdout is not None
        dir_metadata = json.load(dirmd_proc.stdout)

    merged_group = CapabilityGroup()
    exclude_skills, exclude_rules = set(), set()
    for config in dir_metadata.get('dirs', {}).values():
        review_config = config.get('agenticReview', {})
        for raw_group in review_config.get('capabilityGroups', []):
            group = CapabilityGroup.from_dirmd_json(raw_group)
            if any(
                group.matches(path, added_lines)
                for path, added_lines in added_lines_by_path.items()
            ):
                merged_group.skills.update(group.skills)
                merged_group.rules.update(group.rules)

        exclude_skills.update(review_config.get('excludeSkills', []))
        exclude_rules.update(review_config.get('excludeRules', []))

    for skill in map(pathlib.PurePosixPath, exclude_skills):
        merged_group.skills.discard(skill)
    for rule in map(pathlib.PurePosixPath, exclude_rules):
        merged_group.rules.discard(rule)
    return merged_group


def _parse_frontmatter(content: str) -> dict[str, Any]:
    parts = content.split('---', 2)
    if len(parts) < 3:
        return {}
    try:
        data = yaml.safe_load(parts[1])
        if isinstance(data, dict):
            return data
    except yaml.YAMLError:
        pass
    return {}


def format_capabilities(
    repo_root: pathlib.Path,
    group: CapabilityGroup,
) -> str:
    """Formats review capabilities as human/agent-readable Markdown."""
    if not group.rules and not group.skills:
        return 'No review capabilities found.'

    buf = io.StringIO()
    print('# Review Capabilities\n', file=buf)

    if group.rules:
        print('## Rules\n', file=buf)
        for rule_path in sorted(group.rules):
            full_path = repo_root.joinpath(*rule_path.parts)
            print(f'### {_as_posix(rule_path)}\n', file=buf)
            try:
                content = full_path.read_text().strip()
                print(content, file=buf)
            except OSError as error:
                print(f'Error reading rule file: {error}', file=buf)
            print(file=buf)

    if group.skills:
        # Only show the frontmatter for skills so that the calling agent can
        # decide which `SKILL.md`s are worth reading in full.
        print('## Skills\n', file=buf)
        for skill_path in sorted(group.skills):
            frontmatter = {}
            try:
                content = repo_root.joinpath(*skill_path.parts).read_text()
                frontmatter = _parse_frontmatter(content)
            except OSError:
                pass

            name = frontmatter.get('name', skill_path.parent.name)
            print(f'### {name}', file=buf)
            print(f'- Path: {_as_posix(skill_path)}', file=buf)
            description = frontmatter.get('description')
            if description:
                print(f'- Description: {str(description).strip()}', file=buf)
            print(file=buf)

    return f'{buf.getvalue().strip()}\n'


def _parse_args(args: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        '--repo-root',
        type=pathlib.Path,
        help='Path to repository root (defaults to enclosing git checkout).',
    )
    parser.add_argument(
        '--base',
        default='origin/main',
        help='Base git ref/commit to diff against (default: %(default)s).',
    )
    parser.add_argument(
        '--file',
        action='append',
        dest='files',
        help=(
            'Specific changed file to evaluate (can be specified multiple'
            ' times).'
        ),
    )
    parser.add_argument(
        '--json',
        action='store_true',
        help='Output review capabilities in JSON format.',
    )
    return parser.parse_args(args)


def main() -> int:
    args = _parse_args()
    if args.repo_root:
        repo_root = args.repo_root.resolve()
    else:
        try:
            repo_root = find_repo_root()
        except NonGitWorkspaceError as error:
            print(f'Error: {error}', file=sys.stderr)
            return 1

    with git_diff(repo_root, base=args.base, files=args.files) as diff_proc:
        if diff_proc.stdout is None:
            return 1
        added_lines_by_path = parse_unified_diff(diff_proc.stdout)

    result = resolve_capabilities(repo_root, added_lines_by_path)
    if args.json:
        print(
            json.dumps(
                {
                    'rules': sorted(_as_posix(p) for p in result.rules),
                    'skills': sorted(_as_posix(p) for p in result.skills),
                },
                indent=2,
            )
        )
    else:
        print(format_capabilities(repo_root, result))
    return 0


if __name__ == '__main__':
    sys.exit(main())
