#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wraps text to 72 characters per line for CL descriptions."""
import fileinput
import textwrap


def wrap_text(text, width=72):
    if not text.strip():
        return ""

    # Separate subject and body
    parts = text.split('\n', 1)
    subject = parts[0].strip()
    body = parts[1].strip() if len(parts) > 1 else ""

    # Wrap body lines
    wrapped_body_lines = []
    paragraphs = body.split('\n\n')
    for p in paragraphs:
        if p.strip():
            wrapped_p = textwrap.fill(p, width=width)
            wrapped_body_lines.append(wrapped_p)

    body_wrapped = '\n\n'.join(wrapped_body_lines)

    if body_wrapped:
        return f"{subject}\n\n{body_wrapped}"
    return subject


if __name__ == "__main__":
    input_text = "".join(fileinput.input())
    print(wrap_text(input_text))
