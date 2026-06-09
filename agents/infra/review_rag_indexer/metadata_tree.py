# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Metadata tree structure and utilities for DIR_METADATA files."""

from collections.abc import Iterable
import copy
import logging
import pathlib
import subprocess
from typing import Any

from git_utils import read_files_at_revision, revision_exists
import text_protos


class MetadataNode:
    """A node in the metadata tree representing a directory."""

    def __init__(self, name: str):
        self.name: str = name
        self.children: dict[str, MetadataNode] = {}
        self.metadata: dict[str, Any] | None = None

    def get_or_create_child(self, name: str) -> 'MetadataNode':
        """Gets the child node with the given name.

        If it does not exists, a new child is created.

        Args:
            name: The name of the child to get.

        Returns:
            The MetadataNode for `name`.
        """
        if name not in self.children:
            self.children[name] = MetadataNode(name)
        return self.children[name]


class MetadataTree:
    """A tree representing DIR_METADATA files and their locations."""

    def __init__(self):
        self.root = MetadataNode('')

    def insert(self, path: str, metadata: dict[str, Any]) -> None:
        """Insert `metadata` in the tree at `path`.

        If any components of `path` do not exist in the tree yet, they will
        be created.

        Args:
            path: The filepath to insert data for.
            metadata: The metadata content to insert.
        """
        parent = pathlib.PurePosixPath(path).parent
        parts = parent.parts
        node = self.root
        for part in parts:
            if part in ('/', '.'):
                continue
            node = node.get_or_create_child(part)
        node.metadata = metadata

    def get_metadata(self, path: str) -> dict[str, Any] | None:
        """Get the closest metadata in the tree for `path`.

        Args:
            path: The filepath to get data for.

        Returns:
            The closest metadata content in the tree for `path`. If metadata
            exists for `path`, it will be returned. Otherwise, the metadata for
            `path`'s closest parent with valid metadata will be returned.
        """
        parts = pathlib.PurePosixPath(path).parts
        node = self.root
        best_metadata = self.root.metadata
        for part in parts:
            if part == '/':
                continue
            if part in node.children:
                node = node.children[part]
                if node.metadata:
                    best_metadata = node.metadata
            else:
                break
        return best_metadata


def _deep_merge(dict1: dict, dict2: dict) -> dict:
    """Merges dict2 into dict1 recursively.

    Args:
        dict1: The dict to merge data into. Will be modified in place.
        dict2: The dict whose data will be merged into `dict1`

    Returns:
        A reference to `dict1`.
    """
    for key, val in dict2.items():
        if key in dict1:
            if isinstance(dict1[key], dict) and isinstance(val, dict):
                _deep_merge(dict1[key], val)
            elif isinstance(dict1[key], list) and isinstance(val, list):
                dict1[key].extend(val)
            else:
                dict1[key] = val
        else:
            dict1[key] = val
    return dict1


def resolve_metadata(path: str, parsed_files: dict[str, Any],
                     resolved_cache: dict[str, Any]) -> dict[str, Any]:
    """Resolves metadata for a path, applying mixins recursively.

    Args:
        path: The filepath to resolve metadata for.
        parsed_files: A map from DIR_METADATA-relevant path names to their
            file content.
        resolved_cache: A map from DIR_METADATA-relevant path names to their
            resolved metadata. Will be modified in place if any new metadata
            is resolved.

    Returns:
        A dict containing the resolved metadata for `path`.
    """
    if path in resolved_cache:
        return resolved_cache[path]
    parsed = parsed_files.get(path, {})
    resolved = {}
    mixins = parsed.get('mixins', [])
    if isinstance(mixins, str):
        mixins = [mixins]
    for mixin in mixins:
        if mixin.startswith('//'):
            mixin_path = mixin[2:]
            mixin_resolved = resolve_metadata(mixin_path, parsed_files,
                                              resolved_cache)
            _deep_merge(resolved, copy.deepcopy(mixin_resolved))
    local_copy = copy.deepcopy(parsed)
    local_copy.pop('mixins', None)
    _deep_merge(resolved, local_copy)
    resolved_cache[path] = resolved
    return resolved


def parse_and_get_mixins(content: str) -> tuple[dict[str, Any], list[str]]:
    """Parses DIR_METADATA content and returns the parsed dict and mixin paths.

    Args:
        content: The text content of a DIR_METADATA or mixin file.

    Returns:
        A tuple of (parsed_dict, mixin_paths).
    """
    parsed = text_protos.parse_text_proto(content)
    mixins = parsed.get('mixins', [])
    if isinstance(mixins, str):
        mixins = [mixins]
    mixin_paths = []
    for mixin in mixins:
        if mixin.startswith('//'):
            mixin_paths.append(mixin[2:])
    return parsed, mixin_paths


def load_mixins_recursive(
    revision: str,
    pending_paths: Iterable[str],
    parsed_files: dict[str, Any],
) -> None:
    """Recursively loads mixins for the given paths at a revision.

    Args:
        revision: The git revision to read files from.
        pending_paths: The initial iterable of paths to load mixins for.
        parsed_files: A dict mapping file paths to their parsed content. Will
            be updated in place.
    """
    pending = set(pending_paths)
    while pending:
        contents = read_files_at_revision(revision, pending)
        new_pending = set()
        for path, content in contents.items():
            if content is None:
                continue
            parsed, mixin_paths = parse_and_get_mixins(content)
            parsed_files[path] = parsed
            for mixin_path in mixin_paths:
                if (mixin_path not in parsed_files
                        and mixin_path not in pending):
                    new_pending.add(mixin_path)
        pending = new_pending


def build_metadata_tree(
    dir_metadata_paths: set[str],
    parsed_files: dict[str, Any],
) -> tuple[MetadataTree, dict[str, Any]]:
    """Resolves metadata and builds a MetadataTree.

    Args:
        dir_metadata_paths: A set of paths to DIR_METADATA files.
        parsed_files: A dict mapping file paths to their parsed content.

    Returns:
        A new MetadataTree instance filled with the information for the paths
        in `dir_metadata_paths`.
    """
    resolved_cache = {}
    for path in dir_metadata_paths:
        resolve_metadata(path, parsed_files, resolved_cache)

    tree = MetadataTree()
    for path in dir_metadata_paths:
        tree.insert(path, resolved_cache[path])

    return tree


def initialize_metadata_tree(
        first_rev: str) -> tuple[MetadataTree, dict[str, Any], set[str]]:
    """Initializes the metadata tree to the state before `first_rev`.

    If a revision is not available before `first_rev` (i.e. `first_rev` is the
    first commit in the repo), the tree will be initialized to the state of
    `first_rev`.

    Args:
        first_rev: The git revision to use for initialization.

    Returns:
        A tuple (metadata_tree, parsed_files, dir_metadata_paths).
        `metadata_tree` is a MetadataTree instance filled with the metadata
        from `first_rev`~1 or `first_rev` depending on which commits exist.
        `parsed_files` is map from DIR_METADATA-relevant filepaths to file
        content parsed for them at the relevant revision. `dir_metadata_paths`
        is the set of keys for `parsed_files` that are actual DIR_METADATA
        files (as oppposed to other files such as mixins).
    """
    init_rev = f"{first_rev}~1"
    if not revision_exists(init_rev):
        logging.warning('Failed to verify %s, using %s for initialization',
                        init_rev, first_rev)
        init_rev = first_rev

    cmd = ['git', 'ls-tree', '-r', '--name-only', init_rev]
    logging.info('Running: %s', ' '.join(cmd))
    output = subprocess.check_output(cmd, encoding='utf-8')

    dir_metadata_paths = {
        line.strip()
        for line in output.splitlines()
        if line.strip().endswith('DIR_METADATA')
    }

    parsed_files = {}
    load_mixins_recursive(init_rev, dir_metadata_paths, parsed_files)

    tree = build_metadata_tree(dir_metadata_paths, parsed_files)

    return tree, parsed_files, dir_metadata_paths
