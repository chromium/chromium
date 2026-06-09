# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Handles the processing of local git data for Review RAG index generation."""

import datetime
import logging
import re
import subprocess
import sys
from typing import Any, NamedTuple

import git_utils
from common_types import ClInfo, CommonArgs
from metadata_tree import (MetadataTree, initialize_metadata_tree,
                           parse_and_get_mixins, load_mixins_recursive,
                           build_metadata_tree)


class _RevisionAndChangedFiles(NamedTuple):
    revision: str
    changed_files: list[str]


def process_local_git_data(common_args: CommonArgs) -> list[ClInfo]:
    """Process local git data into a format suitable for index creation.

    If `common_args.clobber`, then all commits in `common_args.window` will be
    processed. Otherwise, all commits since `common_args.previous_run.revision`
    will be processed.

    Args:
        common_args: The CommonArgs object filled with information for this
            run.

    Returns:
        A list of common_types.ClInfo objects in chronological order for all
        commits in the relevant time window.
    """
    commits = _get_commits_to_process(common_args)
    if not commits:
        logging.info('No new commits to process.')
        sys.exit(0)

    logging.info('Found %d commits to process.', len(commits))
    first_revision = commits[0].revision

    initial_tree, parsed_files, dir_metadata_paths = initialize_metadata_tree(
        first_revision)

    cl_objects = _process_commits(commits, initial_tree, parsed_files,
                                  dir_metadata_paths)

    logging.info('Successfully processed %d CLs', len(cl_objects))
    return cl_objects


def _extract_cl_info(revision: str) -> ClInfo:
    """Extracts CL information for a given revision.

    Args:
        revision: The git revision to get information about.

    Returns:
        A ClInfo containing extracted CL information with an empty
        MetadataTree.
    """
    cmd = ['git', 'show', '-s', '--format=%ct%n%B', revision]
    output = subprocess.check_output(cmd, encoding='utf-8')
    lines = output.splitlines()
    if not lines:
        raise ValueError(f'git show output is empty for {revision}')
    try:
        timestamp = int(lines[0])
    except ValueError as e:
        raise ValueError(f'Failed to parse timestamp from git show output for '
                         f'{revision}: {lines[0]}') from e
    commit_time = datetime.datetime.fromtimestamp(timestamp,
                                                  tz=datetime.timezone.utc)
    body = '\n'.join(lines[1:])
    cl_numbers = re.findall(r'^Reviewed-on:\s*https://\S+/\+/(\d+)', body,
                            re.MULTILINE)
    if not cl_numbers:
        raise ValueError(
            f'Reviewed-on URL not found in commit description for {revision}')
    cl_number = int(cl_numbers[-1])
    cp_match = re.search(r'^Cr-Commit-Position:\s*[^@]+@\{#(\d+)\}', body,
                         re.MULTILINE)
    if not cp_match:
        raise ValueError(
            f'Cr-Commit-Position not found in commit description for {revision}'
        )
    commit_position = int(cp_match.group(1))
    return ClInfo(
        revision=revision,
        cl_number=cl_number,
        commit_time=commit_time,
        commit_position=commit_position,
        description=body,
        dir_metadata=MetadataTree(),
    )


def get_commit_position(revision: str) -> int:
    """Gets the commit position of a revision.

    Args:
        revision: The git revision to get the commit position for.

    Returns:
        The Chromium commit position of `revision`.
    """
    info = _extract_cl_info(revision)
    return info.commit_position


def _parse_git_log_output(output: str) -> list[_RevisionAndChangedFiles]:
    """Helper to parse git log output to associate file changes with commits.

    This function is only expected to work with log output when it was
    collected with the `--format=%H` and `--name-only` flags.

    Args:
        output: The output of a `git log` run.

    Returns:
        A list of _RevisionAndChangedFiles objects. The returned list is
        ordered in the same order that the commits appeared in `output`.
    """
    commits = []
    current_commit = None
    current_files = []
    for line in output.splitlines():
        line = line.strip()
        if not line:
            continue
        if re.match(r'^[0-9a-f]{40}$', line):
            if current_commit:
                commits.append(
                    _RevisionAndChangedFiles(revision=current_commit,
                                             changed_files=current_files))
            current_commit = line
            current_files = []
        else:
            if current_commit:
                current_files.append(line)
            else:
                logging.warning('File line before any commit: %s', line)
    if current_commit:
        commits.append(
            _RevisionAndChangedFiles(revision=current_commit,
                                     changed_files=current_files))
    return commits


def _get_commits_to_process(
        common_args: CommonArgs) -> list[_RevisionAndChangedFiles]:
    """Gets a list of commits from local git history to process.

    The window of returned commits matches whatever window was determined in
    `common_args` when processing the previous run's manifest.

    Args:
        common_args: The CommonArgs object filled with information for this
            run.

    Returns:
        A list of _RevisionAndChangedFiles objects. The returned list is
        ordered in the same order that the commits appeared in `output`.
    """
    if common_args.clobber:
        since_time = common_args.window_base - common_args.window
        since_str = since_time.isoformat()
        cmd = [
            'git', 'log', '--format=%H', '--name-only', '--reverse',
            f'--since={since_str}', common_args.head_git_revision
        ]
    else:
        last_rev = common_args.previous_run.revision
        cmd = [
            'git', 'log', '--format=%H', '--name-only', '--reverse',
            f'{last_rev}..{common_args.head_git_revision}'
        ]
    logging.info('Running git log: %s', ' '.join(cmd))
    # TODO(b/517156708): Stream output from the subprocess and process it
    # as it becomes available instead of capturing a monolithic string.
    output = subprocess.check_output(cmd, encoding='utf-8')
    return _parse_git_log_output(output)


def _process_commits(
    commits: list[_RevisionAndChangedFiles],
    initial_tree: MetadataTree,
    parsed_files: dict[str, Any],
    dir_metadata_paths: set[str],
) -> list[ClInfo]:
    """Process the given commits into ClInfo objects.

    Args:
        commits: A list of revisions and the files changed in them. Should be
            the return value of _get_commits_to_process().
        initial_tree: A metadata_tree.MetadataTree initialized with information
            from the first revision.
        parsed_files: A map from DIR_METADATA-relevant path names to their
            file content. Should have been created at the same time as
            `initial_tree`. Will be modified in-place as DIR_METADATA changes
            are found.
        dir_metadata_paths: A set of all paths to actual DIR_METADATA files.
            Should have been created at the same time as `initial_tree`. Will
            be modified in-place as DIR_METADATA file additions/removals are
            found.

    Returns:
        A list of common_types.ClInfo objects in chronological order for all
        commits in the relevant time window.
    """
    cl_objects = []

    current_tree = initial_tree

    # TODO(b/517156708): Handle CL info extraction using async or threads since
    # it will be I/O bound.
    for commit in commits:
        cl_info = _extract_cl_info(commit.revision)

        metadata_changes = []
        for f in commit.changed_files:
            if f.endswith('DIR_METADATA') or f in parsed_files:
                metadata_changes.append(f)

        if metadata_changes:
            logging.debug('Commit %s changed metadata files: %s',
                          commit.revision, metadata_changes)
            current_tree = _create_updated_metadata_tree(
                commit.revision, metadata_changes, parsed_files,
                dir_metadata_paths)

        cl_info.dir_metadata = current_tree
        cl_objects.append(cl_info)

    return cl_objects


def _create_updated_metadata_tree(
    revision: str,
    metadata_changes: list[str],
    parsed_files: dict[str, Any],
    dir_metadata_paths: set[str],
) -> MetadataTree:
    """Creates a new metadata tree by applying changes from a revision.

    Args:
        revision: The revision that contains the metadata changes to apply.
        metadata_changes: A list of DIR_METADATA or metadata-relevant files
            that were changed in `revision`.
        parsed_files: A dict mapping DIR_METADATA/metadata-relevant file paths
            to file content. Will be updated in place with new content for
            `revision`.
        dir_metadata_paths: The set of paths that are actually `DIR_METADATA`
            files in `parsed_files`.

    Returns:
        A new metadata_tree.MetadataTree object filled with the updated
        metadata information from `revision`.
    """
    contents = git_utils.read_files_at_revision(revision, metadata_changes)
    new_mixins = set()

    for f, content in contents.items():
        if content is None:
            parsed_files.pop(f, None)
            dir_metadata_paths.discard(f)
        else:
            if f.endswith('DIR_METADATA'):
                dir_metadata_paths.add(f)

            parsed, mixin_paths = parse_and_get_mixins(content)
            parsed_files[f] = parsed
            for mixin_path in mixin_paths:
                if mixin_path not in parsed_files:
                    new_mixins.add(mixin_path)

    load_mixins_recursive(revision, new_mixins, parsed_files)

    new_tree = build_metadata_tree(dir_metadata_paths, parsed_files)
    return new_tree
