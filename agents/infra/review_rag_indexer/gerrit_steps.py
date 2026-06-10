# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Handles interacting with Gerrit to retrieve CL metadata."""

import concurrent.futures
import functools
import json
import logging
import threading

import requests
from urllib3 import util

from common_types import ClInfo, CommonArgs

GERRIT_MAGIC_PREFIX = ")]}'"
REQUEST_TIMEOUT_SECONDS = 30
MAX_RETRIES = 2
BACKOFF_FACTOR_SECONDS = 1.0


class _SessionManager:
    """Manages all requests.Sessions for a ThreadPoolExecutor."""

    def __init__(self):
        self._lock = threading.Lock()
        self._sessions = {}

    def register_session_for_current_thread(self) -> None:
        thread_name = threading.current_thread()
        with self._lock:
            assert thread_name not in self._sessions
            s = requests.Session()
            retry = util.Retry(
                total=MAX_RETRIES,
                backoff_factor=BACKOFF_FACTOR_SECONDS,
                allowed_methods={'GET'},
                status_forcelist={500, 502, 503, 504},
            )
            s.mount('https://',
                    requests.adapters.HTTPAdapter(max_retries=retry))
            self._sessions[thread_name] = s

    def get_session_for_current_thread(self) -> requests.Session:
        thread_name = threading.current_thread()
        with self._lock:
            assert thread_name in self._sessions
            return self._sessions[thread_name]


def _fetch_hashtags_for_cl(project: str, session_manager: _SessionManager,
                           cl_info: ClInfo) -> bool:
    """Fetches hashtags for a single CL and updates it in place.

    Retries up to 2 additional times on network failure with exponential
    backoff.

    Args:
        project: The Git-on-Borg project name.
        session_manager: The _SessionManager storing per-thread Sessions for
            the current executor.
        cl_info: The ClInfo object to update.

    Returns:
        True if hashtags were successfully retrieved, False if all attempts
        failed.

    Raises:
        ValueError: If the response from Gerrit is not a JSON list.
    """
    gerrit_host = f'{project}-review.googlesource.com'
    url = f'https://{gerrit_host}/changes/{cl_info.cl_number}/hashtags'

    session = session_manager.get_session_for_current_thread()

    try:
        logging.debug('Fetching hashtags for CL %d from %s', cl_info.cl_number,
                      url)
        response = session.get(url, timeout=REQUEST_TIMEOUT_SECONDS)
        response.raise_for_status()

        text = response.text
        text = response.text.removeprefix(GERRIT_MAGIC_PREFIX).lstrip()

        hashtags = json.loads(text)
        if not isinstance(hashtags, list):
            raise ValueError(
                f'Expected list of hashtags for CL {cl_info.cl_number}, '
                f'got {type(hashtags)}')

        cl_info.hashtags.update(str(h) for h in hashtags)
        logging.debug('Found hashtags for CL %d: %s', cl_info.cl_number,
                      cl_info.hashtags)
        return True
    except requests.exceptions.RequestException as e:
        logging.warning('Failed to fetch hashtags for CL %d: %s',
                        cl_info.cl_number, e)
        return False


def retrieve_hashtags(common_args: CommonArgs, cl_infos: list[ClInfo]) -> None:
    """Retrieves hashtags for all given CLs in parallel.

    Args:
        common_args: The CommonArgs for the run.
        cl_infos: The list of ClInfo objects to update.

    Raises:
        RuntimeError: If the failure rate of hashtag retrieval exceeds 1%.
    """
    if not cl_infos:
        return

    logging.info('Retrieving hashtags for %d CLs...', len(cl_infos))

    manager = _SessionManager()
    with concurrent.futures.ThreadPoolExecutor(
            max_workers=common_args.num_network_workers,
            initializer=manager.register_session_for_current_thread
    ) as executor:
        func = functools.partial(_fetch_hashtags_for_cl, common_args.project,
                                 manager)
        results = list(executor.map(func, cl_infos))

    failures = results.count(False)
    if failures > 0:
        failure_rate = failures / len(cl_infos)
        logging.warning('%d/%d CLs failed to retrieve hashtags (%.1f%%)',
                        failures, len(cl_infos), failure_rate * 100)
        if failure_rate > 0.01:
            raise RuntimeError(
                f'Hashtag retrieval failure rate ({failure_rate:.1%}) '
                f'exceeded threshold (1.0%). Aborting.')
    else:
        logging.info('Successfully retrieved hashtags for all CLs.')
