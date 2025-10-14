# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module containing shared constants for agents testing."""

import pathlib

CHROMIUM_SRC = pathlib.Path(__file__).resolve().parents[2]
GEMINI_SANDBOX_IMAGE_URL = (
    'us-docker.pkg.dev/gemini-code-dev/gemini-cli/sandbox')
GEMINI_CLI_TOKEN_USAGE = 'gemini_cli_token_usage'
