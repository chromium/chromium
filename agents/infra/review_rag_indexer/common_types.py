# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple types for the Review RAG indexer that are used in multiple places."""

import dataclasses
import datetime
from metadata_tree import MetadataTree


@dataclasses.dataclass
class PreviousRunInfo:
    """Information extracted from the previous run's manifest."""
    # The git revision that was used as HEAD in the previous run.
    revision: str
    # The time that the previous run was started at. Equivalent to the
    # `window_base` value of that run.
    start_time: datetime.datetime


@dataclasses.dataclass
class CommonArgs:
    """Arguments that are expected to be passed around frequently."""
    # The Git-on-Borg project hosting the repo that is being operated on.
    project: str
    # The git repo within `project` that is being operated on.
    repo: str
    # The window over which the index is being created.
    window: datetime.timedelta
    # The timestamp that `window` was calculated over.
    window_base: datetime.datetime
    # Whether we are running in dryrun mode.
    dryrun: bool
    # Information about the previous index creation run, if available.
    previous_run: PreviousRunInfo | None
    # A git revision to treat as HEAD, ignoring commits after it.
    head_git_revision: str = 'HEAD'

    @property
    def clobber(self) -> bool:
        """Create a fresh index rather than building on a previous one."""
        return self.previous_run is None


@dataclasses.dataclass
class ClInfo:
    """Information representing a changelist/commit."""
    # The git revision of the CL.
    revision: str
    # The Gerrit CL number.
    cl_number: int
    # The time the CL landed in the UTC timezone.
    commit_time: datetime.datetime
    # The Chromium commit position extracted from the CL description.
    commit_position: int
    # The full CL description.
    description: str
    # The DIR_METADATA state at the time the CL landed.
    dir_metadata: MetadataTree
