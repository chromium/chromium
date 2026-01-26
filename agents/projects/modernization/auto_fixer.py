# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for AI-assisted auto-fixing of code errors."""

import enum
import logging
import subprocess

from agents.common import gemini_helpers
from agents.projects.modernization import utils

logger = logging.getLogger(__name__)


class FixStatus(enum.StrEnum):
    """Result of an auto-fix attempt."""
    SUCCESS = enum.auto()
    AGENT_FAILURE = enum.auto()
    VERIFICATION_FAILURE = enum.auto()


class AutoFixer:
    """Handles auto-fixing of code errors using Gemini."""

    def __init__(self, max_attempts: int = 3, verification_timeout: int = 300):
        assert max_attempts >= 1
        self.max_attempts = max_attempts
        self.verification_timeout = verification_timeout

    def _query_gemini(
        self,
        prompt: str,
        extra_args: list[str] | None = None,
        timeout: int = 300,
    ) -> subprocess.CompletedProcess:
        """Queries Gemini with the given prompt non-interactively.

        Args:
            prompt: The prompt to send to Gemini.
            extra_args: Optional list of additional arguments for the CLI.
            timeout: Timeout in seconds for the Gemini query.

        Returns:
            CompletedProcess object containing the execution result.
        """
        gemini_exe = gemini_helpers.get_gemini_executable()
        cmd = [gemini_exe]
        if extra_args:
            cmd.extend(extra_args)

        # Pass prompt via stdin to avoid ARG_MAX limits.
        return utils.run_command(
            cmd,
            input=prompt,
            capture_output=True,
            timeout=timeout,
        )

    def _verify_fix(
            self,
            verification_command: list[str]) -> subprocess.CompletedProcess:
        """Runs the verification command and returns the result.

        Args:
            verification_command: The command to run for verification.

        Returns:
            CompletedProcess object containing the execution result.
        """
        logger.info('Verifying fix with: %s', verification_command)
        return utils.run_command(
            verification_command,
            capture_output=True,
            timeout=self.verification_timeout,
        )

    def fix(
        self,
        error_content: str,
        verification_command: list[str] | None = None,
    ) -> FixStatus:
        """Uses Gemini agent to fix the provided error directly with retries.

        Args:
            error_content: The error message or output to be fixed.
            verification_command: Optional command (as a list of strings) to run
                to verify the fix.

        Returns:
            FixStatus indicating whether the fix succeeded, the agent failed,
            or the verification failed.
        """
        logger.info('Asking Gemini to fix the error...')

        last_error = error_content
        last_status = FixStatus.AGENT_FAILURE
        for attempt in range(self.max_attempts):
            logger.info('Starting fix attempt %d/%d', attempt + 1,
                        self.max_attempts)
            prompt = (f'The following error failed for my changes:\n\n'
                      f'{last_error}\n\n'
                      'Please fix the code to resolve these errors.')

            # Pass -y to authorize tool execution
            result = self._query_gemini(prompt, extra_args=['-y'])

            if result.returncode == 0:
                logger.info('Gemini agent finished execution.')
                if not verification_command:
                    return FixStatus.SUCCESS

                v_result = self._verify_fix(verification_command)
                if v_result.returncode == 0:
                    logger.info('Verification succeeded.')
                    return FixStatus.SUCCESS

                logger.warning(
                    'Verification failed after agent finished. '
                    'Output: %s', v_result.stdout)
                last_error = v_result.stdout
                last_status = FixStatus.VERIFICATION_FAILURE
            else:
                logger.warning('Gemini fix attempt failed. Output: %s',
                               result.stdout)
                last_error = result.stdout
                last_status = FixStatus.AGENT_FAILURE

        logger.error(
            'Gemini execution failed after %d attempts. Last status: %s',
            self.max_attempts, last_status)
        return last_status
