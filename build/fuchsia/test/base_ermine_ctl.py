# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Adds python interface to erminectl tools on workstation products."""

import logging
import subprocess
import time
from typing import List, Tuple


class BaseErmineCtl:
    """Compatible class for automating control of Ermine and its OOBE.

    Must be used after checking if the tool exists.

    Usage:
      ctl = base_ermine_ctl.BaseErmineCtl(some_target)
      if ctl.exists:
        ctl.take_to_shell()

        logging.info('In the shell')
      else:
        logging.info('Tool does not exist!')

    This is only necessary after a target reboot or provision (IE pave).
    """

    _OOBE_PASSWORD = 'workstation_test_password'
    _TOOL = 'erminectl'
    _OOBE_SUBTOOL = 'oobe'
    _MAX_STATE_TRANSITIONS = 5

    # Mapping between the current state and the next command to run
    # to move it to the next state.
    _STATE_TO_NEXT = {
        'SetPassword': ['set_password', _OOBE_PASSWORD],
        'Unknown': ['skip'],
        'Shell': [],
        'Login': ['login', _OOBE_PASSWORD],
    }
    _COMPLETE_STATE = 'Shell'

    _READY_TIMEOUT = 10
    _WAIT_ATTEMPTS = 10
    _WAIT_FOR_READY_SLEEP_SEC = 3

    def __init__(self):
        self._ermine_exists = False
        self._ermine_exists_check = False

    # pylint: disable=no-self-use
    # Overridable method to determine how command gets executed.
    def execute_command_async(self, args: List[str]) -> subprocess.Popen:
        """Executes command asynchronously, returning immediately."""
        raise NotImplementedError

    # pylint: enable=no-self-use

    @property
    def exists(self) -> bool:
        """Returns the existence of the tool.

        Checks whether the tool exists on and caches the result.

        Returns:
          True if the tool exists, False if not.
        """
        if not self._ermine_exists_check:
            self._ermine_exists = self._execute_tool(['--help'],
                                                     can_fail=True) == 0
            self._ermine_exists_check = True
            logging.debug('erminectl exists: %s',
                          ('true' if self._ermine_exists else 'false'))
        return self._ermine_exists

    @property
    def status(self) -> Tuple[int, str]:
        """Returns the status of ermine.

        Note that if the tool times out or does not exist, a non-zero code
        is returned.

        Returns:
          Tuple of (return code, status as string). -1 for timeout.
        Raises:
          AssertionError: if the tool does not exist.
        """
        assert self.exists, (f'Tool {self._TOOL} cannot have a status if'
                             ' it does not exist')
        # Executes base command, which returns status.
        proc = self._execute_tool_async([])
        try:
            proc.wait(timeout=self._READY_TIMEOUT)
        except subprocess.TimeoutExpired:
            logging.warning('Timed out waiting for status')
            return -1, 'Timeout'
        stdout, _ = proc.communicate()
        return proc.returncode, stdout.strip()

    @property
    def ready(self) -> bool:
        """Indicates if the tool is ready for regular use.

        Returns:
          False if not ready, and True if ready.
        Raises:
          AssertionError: if the tool does not exist.
        """
        assert self.exists, (f'Tool {self._TOOL} cannot be ready if'
                             ' it does not exist')
        return_code, _ = self.status
        return return_code == 0

    def _execute_tool_async(self, command: List[str]) -> subprocess.Popen:
        """Executes a sub-command asynchronously.

        Args:
          command: list of strings to compose the command. Forwards to the
            command runner.
        Returns:
          Popen of the subprocess.
        """
        full_command = [self._TOOL, self._OOBE_SUBTOOL]
        full_command.extend(command)

        # Returns immediately with Popen.
        return self.execute_command_async(full_command)

    def _execute_tool(self, command: List[str], can_fail: bool = False) -> int:
        """Executes a sub-command of the tool synchronously.
        Raises exception if non-zero returncode is given and |can_fail| = False.

        Args:
            command: list of strings to compose the command. Forwards to the
                command runner.
            can_fail: Whether or not the command can fail.
        Raises:
            RuntimeError: if non-zero returncode is returned and can_fail =
                False.
        Returns:
            Return code of command execution if |can_fail| is True.
        """
        proc = self._execute_tool_async(command)
        stdout, stderr = proc.communicate()
        if not can_fail and proc.returncode != 0:
            raise RuntimeError(f'Command {" ".join(command)} failed.'
                               f'\nSTDOUT: {stdout}\nSTDERR: {stderr}')
        return proc.returncode

    def wait_until_ready(self) -> None:
        """Waits until the tool is ready through sleep-poll.

        The tool may not be ready after a pave or restart.
        This checks the status and exits after its ready or Timeout.

        Raises:
          TimeoutError: if tool is not ready after certain amount of attempts.
          AssertionError: if tool does not exist.
        """
        assert self.exists, f'Tool {self._TOOL} must exist to use it.'
        for _ in range(self._WAIT_ATTEMPTS):
            if self.ready:
                return
            time.sleep(self._WAIT_FOR_READY_SLEEP_SEC)
        raise TimeoutError('Timed out waiting for a valid status to return')

    def take_to_shell(self) -> None:
        """Takes device to shell after waiting for tool to be ready.

        Examines the current state of the device after waiting for it to be
        ready. Once ready, goes through the states of logging in. This is:
        - CreatePassword -> Skip screen -> Shell
        - Login -> Shell
        - Shell

        Regardless of starting state, this will exit once the shell state is
        reached.

        Raises:
          NotImplementedError: if an unknown state is reached.
          RuntimeError: If number of state transitions exceeds the max number
            that is expected.
        """
        self.wait_until_ready()
        _, state = self.status
        max_states = self._MAX_STATE_TRANSITIONS
        while state != self._COMPLETE_STATE and max_states:
            max_states -= 1
            command = self._STATE_TO_NEXT.get(state)
            logging.debug('Ermine state is: %s', state)
            if command is None:
                raise NotImplementedError('Encountered invalid state: %s' %
                                          state)
            self._execute_tool(command)
            _, state = self.status

        if not max_states:
            raise RuntimeError('Did not transition to shell in %d attempts.'
                               ' Please file a bug.' %
                               self._MAX_STATE_TRANSITIONS)
