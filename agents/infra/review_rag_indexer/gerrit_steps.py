# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Handles interacting with Gerrit to retrieve CL metadata."""

import collections
import concurrent.futures
import functools
import json
import logging
import pathlib
import posixpath
import sys
import threading
import urllib.parse

import requests
from urllib3 import util

# depot_tools is DEPSed in at //third_party/depot_tools.
_DEPOT_TOOLS_DIR = (
    pathlib.Path(__file__).resolve().parents[3] / 'third_party' / 'depot_tools'
)
if _DEPOT_TOOLS_DIR.exists():
    _DEPOT_TOOLS_DIR_STR = str(_DEPOT_TOOLS_DIR)
    if _DEPOT_TOOLS_DIR_STR not in sys.path:
        sys.path.append(_DEPOT_TOOLS_DIR_STR)
else:
    logging.warning(
        'depot_tools not found at %s, gerrit_util import may fail',
        _DEPOT_TOOLS_DIR,
    )

import gerrit_util  # pylint: disable=import-error

from common_types import ClInfo, CommentThread, CommonArgs

GERRIT_MAGIC_PREFIX = ")]}'"
REQUEST_TIMEOUT_SECONDS = 30
MAX_RETRIES = 2
BACKOFF_FACTOR_SECONDS = 1.0


class GerritUtilHttpConnAdapter:
    """Adapter to extract auth headers from gerrit_util."""

    def __init__(self, host: str, uri: str):
        self.req_host = host
        self.req_uri = uri
        self.req_headers = {}
        self.proxy_info = None

    def has_header(self, header: str) -> bool:
        return header in self.req_headers

    def get_full_url(self) -> str:
        return self.req_uri

    def get_header(self, header: str, default: str = None) -> str:
        return self.req_headers.get(header, default)

    def add_unredirected_header(self, header: str, value: str):
        self.req_headers[header] = value

    @property
    def unverifiable(self) -> bool:
        return False

    @property
    def origin_req_host(self) -> str:
        return self.req_host

    @property
    def type(self) -> str:
        return urllib.parse.urlparse(self.req_uri).scheme

    @property
    def host(self) -> str:
        return self.req_host


class _SessionManager:
    """Manages all requests.Sessions for a ThreadPoolExecutor."""

    def __init__(self, gerrit_host: str):
        self._lock = threading.Lock()
        self._sessions = {}
        self._gerrit_host = gerrit_host

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
            s.mount(
                'https://', requests.adapters.HTTPAdapter(max_retries=retry)
            )
            self._configure_session_auth(s)
            self._sessions[thread_name] = s

    def _configure_session_auth(self, session: requests.Session) -> None:
        """Configures a Session to have authentication headers applied.

        Args:
            session: The requests.Session object to add authentication to.
        """
        gerrit_adapter = GerritUtilHttpConnAdapter(
            self._gerrit_host, f'https://{self._gerrit_host}/a/'
        )

        try:
            # pylint: disable=protected-access
            authenticator = gerrit_util._Authenticator.get()
            # pylint: enable=protected-access
            authenticator.authenticate(gerrit_adapter)
        except Exception as e:
            raise RuntimeError(
                f'Failed to authenticate for {self._gerrit_host}: {e}'
            ) from e

        session.headers.update(gerrit_adapter.req_headers)

        # Apply proxy if set for SSO.
        if gerrit_adapter.proxy_info:
            proxy_url = (
                f'http://{gerrit_adapter.proxy_info.proxy_host.decode()}'
                f':{gerrit_adapter.proxy_info.proxy_port}'
            )
            session.proxies = {
                'http': proxy_url,
                'https': proxy_url,
            }
            logging.debug('Using SSO proxy: %s', proxy_url)

        # Store the base URL (potentially rewritten by SSO).
        session.gerrit_base_url = gerrit_adapter.req_uri.rstrip('/')

    def get_session_for_current_thread(self) -> requests.Session:
        thread_name = threading.current_thread()
        with self._lock:
            assert thread_name in self._sessions
            return self._sessions[thread_name]


def _fetch_hashtags_for_cl(
    session_manager: _SessionManager, cl_info: ClInfo
) -> bool:
    """Fetches hashtags for a single CL and updates it in place.

    Retries up to 2 additional times on network failure with exponential
    backoff.

    Args:
        session_manager: The _SessionManager storing per-thread Sessions for
            the current executor.
        cl_info: The ClInfo object to update.

    Returns:
        True if hashtags were successfully retrieved, False if all attempts
        failed.

    Raises:
        ValueError: If the response from Gerrit is not a JSON list.
    """
    session = session_manager.get_session_for_current_thread()
    url = posixpath.join(
        session.gerrit_base_url, 'changes', str(cl_info.cl_number), 'hashtags'
    )

    try:
        logging.debug(
            'Fetching hashtags for CL %d from %s', cl_info.cl_number, url
        )
        response = session.get(url, timeout=REQUEST_TIMEOUT_SECONDS)
        response.raise_for_status()

        text = response.text
        text = response.text.removeprefix(GERRIT_MAGIC_PREFIX).lstrip()

        hashtags = json.loads(text)
        if not isinstance(hashtags, list):
            raise ValueError(
                f'Expected list of hashtags for CL {cl_info.cl_number}, '
                f'got {type(hashtags)}'
            )

        cl_info.hashtags.update(str(h) for h in hashtags)
        logging.debug(
            'Found hashtags for CL %d: %s', cl_info.cl_number, cl_info.hashtags
        )
        return True
    except requests.exceptions.RequestException as e:
        logging.warning(
            'Failed to fetch hashtags for CL %d: %s', cl_info.cl_number, e
        )
        return False


def retrieve_hashtags(common_args: CommonArgs, cl_infos: list[ClInfo]) -> None:
    """Retrieves hashtags for all given CLs in parallel.

    Args:
        common_args: The CommonArgs for the run.
        cl_infos: The list of ClInfo objects to update.

    Raises:
        RuntimeError: If the failure rate of hashtag retrieval exceeds 1%.
    """
    logging.info('Retrieving hashtags for %d CLs...', len(cl_infos))

    manager = _SessionManager(f'{common_args.project}-review.googlesource.com')
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=common_args.num_network_workers,
        initializer=manager.register_session_for_current_thread,
    ) as executor:
        func = functools.partial(_fetch_hashtags_for_cl, manager)
        results = list(executor.map(func, cl_infos))

    failures = results.count(False)
    if failures > 0:
        failure_rate = failures / len(cl_infos)
        logging.warning(
            '%d/%d CLs failed to retrieve hashtags (%.1f%%)',
            failures,
            len(cl_infos),
            failure_rate * 100,
        )
        if failure_rate > 0.01:
            raise RuntimeError(
                f'Hashtag retrieval failure rate ({failure_rate:.1%}) '
                f'exceeded threshold (1.0%). Aborting.'
            )
    else:
        logging.info('Successfully retrieved hashtags for all CLs.')


def _traverse_comment_thread(
    node: dict, replies: dict[str, list[dict]], thread_comments: list[dict]
) -> None:
    """Helper to recursively traverse a comment thread (DFS).

    Args:
        node: The current comment being processed.
        replies: A map from parent IDs to comments that reply to that parent.
        thread_comments: The replies that have been added to this thread so
            far. Will be modified in place.
    """
    thread_comments.append(node)
    for reply in replies.get(node['id'], []):
        _traverse_comment_thread(reply, replies, thread_comments)


def _reconstruct_threads_for_file(
    file_path: str, comments: list[dict]
) -> list[CommentThread]:
    """Reconstructs comment threads for a single file.

    Omits threads that contain only a single comment, as they are likely
    unhelpful (e.g. "LGTM").

    Args:
        file_path: The path of the file the comments are on.
        comments: A list of comment dicts from Gerrit.

    Returns:
        A list of CommentThread objects.
    """
    known_ids = {c['id']: c for c in comments}
    replies = collections.defaultdict(list)
    roots = []

    for c in comments:
        parent_id = c.get('in_reply_to')
        if parent_id and parent_id in known_ids:
            replies[parent_id].append(c)
        else:
            roots.append(c)

    # Sort roots and replies by updated time to ensure chronological order.
    # Gerrit timestamp format "YYYY-MM-DD HH:MM:SS.SSSSSSSSS" is
    # lexicographically sortable.
    roots.sort(key=lambda c: c.get('updated', ''))
    for children in replies.values():
        children.sort(key=lambda c: c.get('updated', ''))

    thread_dataclasses = []

    for root in roots:
        thread_comments = []
        _traverse_comment_thread(root, replies, thread_comments)

        if len(thread_comments) < 2:
            continue

        markdown_parts = []
        for i, c in enumerate(thread_comments, start=1):
            author_name = c.get('author', {}).get('name', 'Unknown')
            header = f'# Comment {i} ({author_name})'
            message = c.get('message', '').strip()
            markdown_parts.append(f"{header}\n\n{message}")

        thread_markdown = '\n\n'.join(markdown_parts)

        thread_dataclasses.append(
            CommentThread(
                file_path=file_path,
                patch_set=root.get('patch_set', 1),
                thread_markdown=thread_markdown,
            )
        )

    return thread_dataclasses


def _fetch_comments_for_cl(
    session_manager: _SessionManager, cl_info: ClInfo
) -> bool:
    """Fetches/reconstructs comments for a single CL and updates in place.

    Args:
        session_manager: The _SessionManager storing per-thread Sessions.
        cl_info: The ClInfo object to fetch comments for and update.

    Returns:
        True if comments were successfully retrieved (even if there were none),
        False if the retrieval failed.

    Raises:
        ValueError: If the response from Gerrit is not a JSON dict.
    """
    session = session_manager.get_session_for_current_thread()
    url = posixpath.join(
        session.gerrit_base_url, 'changes', str(cl_info.cl_number), 'comments'
    )

    try:
        logging.debug(
            'Fetching comments for CL %d from %s', cl_info.cl_number, url
        )
        response = session.get(url, timeout=REQUEST_TIMEOUT_SECONDS)
        response.raise_for_status()

        text = response.text.removeprefix(GERRIT_MAGIC_PREFIX).lstrip()
        comments_map = json.loads(text)

        if not isinstance(comments_map, dict):
            raise ValueError(
                f'Expected dict of comments for CL {cl_info.cl_number}, '
                f'got {type(comments_map)}'
            )

        threads = []
        for file_path, comments in comments_map.items():
            threads.extend(_reconstruct_threads_for_file(file_path, comments))

        cl_info.comments = threads
        return True
    except requests.exceptions.RequestException as e:
        logging.warning(
            'Failed to fetch comments for CL %d: %s', cl_info.cl_number, e
        )
        return False


def retrieve_comments(common_args: CommonArgs, cl_infos: list[ClInfo]) -> None:
    """Retrieves and reconstructs comments for all given CLs in parallel.

    Updates the `comments` field of each `ClInfo` object in place.

    Args:
        common_args: The CommonArgs for the run.
        cl_infos: The list of ClInfo objects to update.

    Raises:
        RuntimeError: If the failure rate of comment retrieval exceeds 1%.
    """
    logging.info('Retrieving comments for %d CLs...', len(cl_infos))

    manager = _SessionManager(f'{common_args.project}-review.googlesource.com')
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=common_args.num_network_workers,
        initializer=manager.register_session_for_current_thread,
    ) as executor:
        func = functools.partial(_fetch_comments_for_cl, manager)
        results = list(executor.map(func, cl_infos))

    failures = results.count(False)
    if failures > 0:
        failure_rate = failures / len(cl_infos)
        logging.warning(
            '%d/%d CLs failed to retrieve comments (%.1f%%)',
            failures,
            len(cl_infos),
            failure_rate * 100,
        )
        if failure_rate > 0.01:
            raise RuntimeError(
                f'Comment retrieval failure rate ({failure_rate:.1%}) '
                f'exceeded threshold (1.0%). Aborting.'
            )
    else:
        logging.info('Successfully retrieved comments for all CLs.')
