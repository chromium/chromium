# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Request approval for Git worktree creation."""

import json
import re
import sys

event = json.load(sys.stdin)
arguments = event["toolArgs"]
if isinstance(arguments, dict):
    arguments = arguments.get("command", "")
if not isinstance(arguments, str):
    arguments = json.dumps(arguments)

if re.search(
    r"\bgit(?:\.exe)?\b[^\r\n;&|]*?\bworktree[ \t]+add\b",
    arguments,
    re.IGNORECASE,
):
    print(
        json.dumps(
            {
                "permissionDecision": "ask",
                "permissionDecisionReason": (
                    "This command may create a Git worktree, which can take "
                    "a long time and use substantial disk space for Chromium. "
                    "Allow it to run?"
                ),
            }
        )
    )
else:
    print("{}")
