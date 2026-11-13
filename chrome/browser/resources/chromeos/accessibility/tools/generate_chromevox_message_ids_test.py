#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for generate_chromevox_message_ids.py."""

import gzip
import json
import os
import tempfile
import unittest

import generate_chromevox_message_ids


class GenerateChromeVoxMessageIdsTest(unittest.TestCase):
  def test_extract_and_generate_from_gzip_json(self):
    sample_messages = {
      'CHROMEVOX_OPTIONS_AUDIO_duck': {'message': 'Duck audio'},
      'CHROMEVOX_ACCESS_KEY': {'message': 'Access key $1'},
      'CHROMEVOX_': {'message': 'Bare prefix should be ignored'},
      'DICTATION_HELP': {'message': 'Help'},
      'LOCALE': {'message': 'en'},
    }
    with tempfile.TemporaryDirectory() as tmp_dir:
      in_path = os.path.join(tmp_dir, 'messages.json.gz')
      out_path = os.path.join(tmp_dir, 'chromevox_message_ids.d.ts')
      with gzip.open(in_path, 'wt', encoding='utf-8') as f:
        json.dump(sample_messages, f)

      rc = generate_chromevox_message_ids.main(['-o', out_path, in_path])
      self.assertEqual(rc, 0)
      with open(out_path, 'r', encoding='utf-8') as f:
        dts = f.read()

      self.assertIn(
        'type ChromeVoxResourceMessageId =\n'
        "    | 'access_key'\n"
        "    | 'options_audio_duck';\n",
        dts,
      )
      self.assertNotIn("''", dts)
      self.assertNotIn('dictation_help', dts)
      self.assertNotIn("'locale'", dts)

  def test_fails_when_no_chromevox_messages(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      in_path = os.path.join(tmp_dir, 'messages.json')
      out_path = os.path.join(tmp_dir, 'chromevox_message_ids.d.ts')
      with open(in_path, 'w', encoding='utf-8', newline='') as f:
        json.dump({'DICTATION_HELP': {'message': 'Help'}}, f)

      rc = generate_chromevox_message_ids.main(['-o', out_path, in_path])
      self.assertEqual(rc, 1)
      self.assertFalse(os.path.exists(out_path))


if __name__ == '__main__':
  unittest.main()
