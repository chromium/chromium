#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Accesses the storage of a component. """

import json
import os
import sys

from typing import List, Optional

from common import run_ffx_command


class ComponentStorage:
    """ Manages the storage of a component on a target.

        Unlike other OSes, the |remote_path| is impacted by the |capacity|,
        e.g. if a file needs to be copied to the /cache which is provided by the
        capability cache, the |remote_path| should be / or . instead of /cache.

        Unless otherwise mentioned, functions assert on any failures.
    """

    def __init__(self,
                 component: str,
                 target: str = os.getenv('FUCHSIA_NODENAME')):
        self._instance_id = json.loads(
            run_ffx_command(cmd=('--machine', 'json', 'component', 'show',
                                 component),
                            target_id=target,
                            capture_output=True).stdout.strip())["instance_id"]
        self._target = target

    def push(self,
             local_path: str,
             remote_path: str,
             capability: Optional[str] = None) -> None:
        """ Copies a file from |local_path| to |remote_path| on the target. """

        cmd = ['component', 'storage']
        if capability:
            cmd.extend(['--capability', capability])
        cmd.extend(
            ['copy', local_path, self._instance_id + '::' + remote_path])
        run_ffx_command(cmd=cmd, target_id=self._target)

    def pull(self,
             remote_path: str,
             local_path: str,
             capability: Optional[str] = None) -> None:
        """ Copies a file from |remote_path| to |local_path| from the target.
        """

        cmd = ['component', 'storage']
        if capability:
            cmd.extend(['--capability', capability])
        cmd.extend(
            ['copy', self._instance_id + '::' + remote_path, local_path])
        run_ffx_command(cmd=cmd, target_id=self._target)

    def delete(self,
               remote_path: str,
               capability: Optional[str] = None) -> None:
        """ Deletes a file from |remote_path| on the target. """

        cmd = ['component', 'storage']
        if capability:
            cmd.extend(['--capability', capability])
        cmd.extend(['delete', self._instance_id + '::' + remote_path])
        run_ffx_command(cmd=cmd, target_id=self._target)

    def list(self,
             remote_path: str,
             capability: Optional[str] = None) -> List[str]:
        """ Lists files in |remote_path| on the target. """

        cmd = ['component', 'storage']
        if capability:
            cmd.extend(['--capability', capability])
        cmd.extend(['list', self._instance_id + '::' + remote_path])
        return run_ffx_command(
            cmd=cmd, target_id=self._target,
            capture_output=True).stdout.strip().splitlines()

    def list_to_stdout(self,
                       remote_path: str,
                       capability: Optional[str] = None) -> None:
        """ Lists files in |remote_path| on the target and prints out to stdout.
        """
        for file in self.list(remote_path, capability):
            print(file)


if __name__ == "__main__":
    args = sys.argv
    getattr(ComponentStorage(args[1], args[2]), args[3])(*args[4:])
