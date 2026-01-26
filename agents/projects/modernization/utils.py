# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common utilities for modernization agents."""

import dataclasses
import enum
import logging
import subprocess

logger = logging.getLogger(__name__)


class TaskType(enum.StrEnum):
    """Types of modernization tasks."""
    UNDEFINED = 'undefined'
    NULL_TO_NULLPTR = 'null_to_nullptr'


# Standard exit code for the 'timeout' command.
TIMEOUT_EXIT_CODE = 124


@dataclasses.dataclass
class Task:
    """Represents a modernization task."""
    task_id: str
    owners_directory: str
    files: list[str]
    task_type: TaskType
    cl_link: str | None = None
    local_branch: str | None = None

    def to_dict(self) -> dict:
        """Converts the task to a dictionary."""
        return {
            'task_id': self.task_id,
            'owners_directory': self.owners_directory,
            'files': self.files,
            'task_type': str(self.task_type),
            'cl_link': self.cl_link,
            'local_branch': self.local_branch,
        }

    @classmethod
    def from_dict(cls, data: dict) -> 'Task':
        """Creates a Task instance from a dictionary."""
        task_type_str = data.get('task_type', '')
        task_type = TaskType(
            task_type_str) if task_type_str else TaskType.UNDEFINED
        default_id = f'{task_type}:{data["owners_directory"]}'
        return cls(task_id=data.get('task_id', default_id),
                   owners_directory=data['owners_directory'],
                   files=data['files'],
                   task_type=task_type,
                   cl_link=data.get('cl_link'),
                   local_branch=data.get('local_branch'))


def run_command(command: list[str],
                capture_output: bool = False,
                **kwargs) -> subprocess.CompletedProcess:
    """Runs a shell command and returns a CompletedProcess.

    If capture_output is True, stdout and stderr are captured and merged into
    the stdout field of the returned CompletedProcess.
    """
    logger.info('Running: %s', ' '.join(command))
    stdout_arg = subprocess.PIPE if capture_output else None
    stderr_arg = subprocess.STDOUT if capture_output else None
    try:
        return subprocess.run(command,
                              stdout=stdout_arg,
                              stderr=stderr_arg,
                              text=True,
                              check=False,
                              **kwargs)
    except subprocess.TimeoutExpired as e:
        logger.error('Command timed out: %s', e)
        return subprocess.CompletedProcess(e.cmd,
                                           TIMEOUT_EXIT_CODE,
                                           stdout=e.stdout,
                                           stderr=e.stderr)
