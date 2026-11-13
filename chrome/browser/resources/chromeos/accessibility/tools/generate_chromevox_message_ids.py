#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a TypeScript union type of ChromeVox message IDs."""

import argparse
import gzip
import json
import os
import sys

_CHROMEVOX_PREFIX = 'chromevox_'

_FILE_TEMPLATE = (
  '// Copyright 2026 The Chromium Authors\n'
  '// Use of this source code is governed by a BSD-style license that can be\n'
  '// found in the LICENSE file.\n'
  '\n'
  '// GENERATED FROM %(input_file)s. DO NOT EDIT.\n'
  '\n'
  'type ChromeVoxResourceMessageId =\n'
  '%(union_members)s;\n'
)


def _read_json(path: str) -> dict:
  if path.endswith('.gz'):
    with gzip.open(path, 'rt', encoding='utf-8') as f:
      return json.load(f)
  with open(path, 'r', encoding='utf-8') as f:
    return json.load(f)


def extract_chromevox_message_ids(messages: dict) -> list[str]:
  """Extracts sorted lowercase IDs stripped of the `chromevox_` prefix."""
  ids = {
    key.lower()[len(_CHROMEVOX_PREFIX) :]
    for key in messages
    if key.lower().startswith(_CHROMEVOX_PREFIX)
    and len(key) > len(_CHROMEVOX_PREFIX)
  }
  return sorted(ids)


def generate_dts_content(input_label: str, message_ids: list[str]) -> str:
  """Generates the `.d.ts` file contents for the given message IDs."""
  union_members = '\n'.join(f"    | '{msg_id}'" for msg_id in message_ids)
  return _FILE_TEMPLATE % {
    'input_file': input_label,
    'union_members': union_members,
  }


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
    '-o',
    '--output_file',
    required=True,
    help='Path to the generated .d.ts file.',
  )
  parser.add_argument(
    'input_file',
    help='Path to _locales/en/messages.json(.gz).',
  )
  args = parser.parse_args(argv)

  messages = _read_json(args.input_file)
  message_ids = extract_chromevox_message_ids(messages)
  if not message_ids:
    sys.stderr.write(
      f'Error: no {_CHROMEVOX_PREFIX}* messages found in {args.input_file}\n'
    )
    return 1

  out_dir = os.path.dirname(args.output_file)
  if out_dir:
    os.makedirs(out_dir, exist_ok=True)
  content = generate_dts_content(os.path.basename(args.input_file), message_ids)
  with open(args.output_file, 'w', encoding='utf-8', newline='') as out_file:
    out_file.write(content)
  return 0


if __name__ == '__main__':
  sys.exit(main())
