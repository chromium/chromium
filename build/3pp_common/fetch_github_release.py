# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import hashlib
import json
import os
import pathlib
import re
import sys
from typing import Dict, List
import urllib.request

import scripthash


def _fetch_json(url):
    return json.load(urllib.request.urlopen(url))


def _find_valid_urls(release, artifact_regex):
    urls = [x['browser_download_url'] for x in release['assets']]
    if artifact_regex:
        urls = [x for x in urls if re.search(artifact_regex, x)]
    return urls


def _latest(
    api_url, install_scripts=None, artifact_regex=None, include_deps_hash=True
):
    # Make the version change every time this file changes. Omitted for
    # ${platform}-pinned packages, which need identical versions per platform.
    file_hash = (
        scripthash.compute(extra_paths=install_scripts)
        if include_deps_hash
        else None
    )

    releases: List[Dict] = _fetch_json(f'{api_url}/releases')
    for release in releases:
        # Skip pre-releases; they may publish per-platform assets at different
        # times, causing platforms to resolve to different versions.
        if release.get('prerelease'):
            continue
        tag_name = release['tag_name']
        urls = _find_valid_urls(release, artifact_regex)
        if len(urls) == 1:
            print(f'{tag_name}.{file_hash}' if include_deps_hash else tag_name)
            return
        print(
            f'Bad urls={urls} for tag_name={tag_name}, skipping.',
            file=sys.stderr,
        )


def _get_url(
    api_url,
    artifact_filename=None,
    artifact_extension=None,
    artifact_regex=None,
    include_deps_hash=True,
):
    version = os.environ['_3PP_VERSION']
    if include_deps_hash:
        # Split off our md5 hash.
        version = version.rsplit('.', 1)[0]
    json_dict = _fetch_json(f'{api_url}/releases/tags/{version}')
    urls = _find_valid_urls(json_dict, artifact_regex)

    if len(urls) != 1:
        raise Exception('len(urls) != 1, urls: \n' + '\n'.join(urls))

    partial_manifest = {
        'url': urls,
        'ext': artifact_extension or '',
    }
    if artifact_filename:
        partial_manifest['name'] = [artifact_filename]

    print(json.dumps(partial_manifest))


def main(
    *,
    project,
    artifact_filename=None,
    artifact_extension=None,
    artifact_regex=None,
    install_scripts=None,
    include_deps_hash=True,
):
    """The fetch.py script for a 3pp module.

    Args:
      project: GitHub username for the repo. e.g. "google/protobuf".
      artifact_filename: The name for the downloaded file. Required when not
          setting "unpack_archive: true" in 3pp.pb.
      artifact_extension: File extension of file being downloaded. Required when
          setting "unpack_archive: true" in 3pp.pb.
      artifact_regex: A regex to use to identify the desired artifact from the
          list of artifacts on the release.
      install_scripts: List of script to add to the md5 of the version. The main
          module and this module are always included.
      include_deps_hash: Whether to append the per-build script hash to the
          version. Set False for ${platform}-pinned packages so the CIPD
          version is identical across platforms.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('latest', 'get_url'))
    args = parser.parse_args()

    api_url = f'https://api.github.com/repos/{project}'
    if args.action == 'latest':
        _latest(
            api_url,
            install_scripts=install_scripts,
            artifact_regex=artifact_regex,
            include_deps_hash=include_deps_hash,
        )
    else:
        _get_url(
            api_url,
            artifact_filename=artifact_filename,
            artifact_extension=artifact_extension,
            artifact_regex=artifact_regex,
            include_deps_hash=include_deps_hash,
        )
