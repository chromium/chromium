#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for create_string_rc.py."""

import os
import sys
import tempfile
import unittest

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
  sys.path.insert(0, _THIS_DIR)

import create_string_rc  # noqa: E402


def _write_grd(path, messages):
  lines = [
    '<?xml version="1.0" encoding="UTF-8"?>',
    '<grit latest_public_release="0" current_release="1">',
    '  <translations />',
    '  <release seq="1">',
    '    <messages>',
  ]
  for name, text in messages.items():
    lines.append(f'      <message name="{name}">{text}</message>')
  lines.extend(['    </messages>', '  </release>', '</grit>'])
  with open(path, 'w', encoding='utf-8', newline='') as f:
    f.write('\n'.join(lines))


def _make_files_in_dir(
  tmp_dir,
  grd_specs,
  string_ids,
  mode_specific_strings=None,
  brand=None,
):
  inputs = []
  for idx, messages in enumerate(grd_specs):
    grd_path = os.path.join(tmp_dir, f'test_{idx}.grd')
    _write_grd(grd_path, messages)
    inputs.append((grd_path, 'translations'))
  header_file = os.path.join(tmp_dir, 'out.h')
  rc_file = os.path.join(tmp_dir, 'out.rc')
  create_string_rc.StringRcMaker(
    # Pass a single-use iterator matching main()'s zip(...) call so the
    # missing-ID error path verifies that inputs are materialized up front.
    inputs=iter(inputs),
    expected_xtb_input_files=[],
    header_file=header_file,
    rc_file=rc_file,
    brand=brand,
    first_resource_id=1600,
    string_ids_to_extract=string_ids,
    mode_specific_strings=mode_specific_strings or {},
  ).MakeFiles()
  with open(header_file, 'r', encoding='utf-8') as f:
    header_text = f.read()
  with open(rc_file, 'r', encoding='utf-16') as f:
    rc_text = f.read()
  return header_text, rc_text


def _make_files(grd_specs, string_ids, mode_specific_strings=None, brand=None):
  with tempfile.TemporaryDirectory() as tmp_dir:
    return _make_files_in_dir(
      tmp_dir,
      grd_specs,
      string_ids,
      mode_specific_strings=mode_specific_strings,
      brand=brand,
    )


class CreateStringRcTest(unittest.TestCase):
  def test_extracts_requested_strings_across_multiple_grds(self):
    header, rc = _make_files(
      [
        {'IDS_FIRST': 'First', 'IDS_UNUSED': 'Unused'},
        {'IDS_SECOND': 'Second'},
      ],
      string_ids=['IDS_FIRST', 'IDS_SECOND'],
    )
    self.assertIn('IDS_FIRST_EN_US', header)
    self.assertIn('IDS_SECOND_EN_US', header)
    self.assertNotIn('IDS_UNUSED', header)
    self.assertIn('IDS_FIRST_EN_US "First"', rc)
    self.assertIn('IDS_SECOND_EN_US "Second"', rc)

  def test_fails_when_requested_string_id_is_missing(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      with self.assertRaises(RuntimeError) as ctx:
        _make_files_in_dir(
          tmp_dir,
          [{'IDS_PRESENT': 'Present'}],
          string_ids=['IDS_PRESENT', 'IDS_MISSING_B', 'IDS_MISSING_A'],
        )
      message = str(ctx.exception)
      self.assertIn('test_0.grd', message)
      self.assertIn('IDS_MISSING_A\n  IDS_MISSING_B', message)
      self.assertNotIn('IDS_PRESENT', message)
      self.assertFalse(os.path.exists(os.path.join(tmp_dir, 'out.h')))
      self.assertFalse(os.path.exists(os.path.join(tmp_dir, 'out.rc')))

  def test_active_brand_mode_specific_strings(self):
    mode_strings = {
      'IDS_PRODUCT_NAME': {
        'chromium': ['IDS_PRODUCT_NAME'],
        'google_chrome': ['IDS_PRODUCT_NAME', 'IDS_SHORTCUT_NAME_BETA'],
      },
    }
    # Inactive brand's extra string is not required when building chromium.
    header, _ = _make_files(
      [{'IDS_PRODUCT_NAME': 'Chromium'}],
      string_ids=['IDS_PRODUCT_NAME'],
      mode_specific_strings=mode_strings,
      brand='chromium',
    )
    self.assertIn('IDS_PRODUCT_NAME_EN_US', header)

    # Active brand's mode-specific string is extracted when present in .grd.
    header, _ = _make_files(
      [
        {
          'IDS_PRODUCT_NAME': 'Chrome',
          'IDS_SHORTCUT_NAME_BETA': 'Chrome Beta',
        }
      ],
      string_ids=['IDS_PRODUCT_NAME'],
      mode_specific_strings=mode_strings,
      brand='google_chrome',
    )
    self.assertIn('IDS_SHORTCUT_NAME_BETA_EN_US', header)

    # Active brand's missing mode-specific string fails without writing outputs.
    with tempfile.TemporaryDirectory() as tmp_dir:
      with self.assertRaises(RuntimeError) as ctx:
        _make_files_in_dir(
          tmp_dir,
          [{'IDS_PRODUCT_NAME': 'Chrome'}],
          string_ids=['IDS_PRODUCT_NAME'],
          mode_specific_strings=mode_strings,
          brand='google_chrome',
        )
      message = str(ctx.exception)
      self.assertIn('test_0.grd', message)
      self.assertIn('IDS_SHORTCUT_NAME_BETA', message)
      self.assertFalse(os.path.exists(os.path.join(tmp_dir, 'out.h')))
      self.assertFalse(os.path.exists(os.path.join(tmp_dir, 'out.rc')))

  def test_extracts_all_strings_when_string_ids_empty(self):
    header, rc = _make_files(
      [{'IDS_ONE': 'One', 'IDS_TWO': 'Two'}],
      string_ids=[],
    )
    self.assertIn('IDS_ONE_EN_US', header)
    self.assertIn('IDS_TWO_EN_US', header)
    self.assertIn('IDS_ONE_EN_US "One"', rc)
    self.assertIn('IDS_TWO_EN_US "Two"', rc)


if __name__ == '__main__':
  unittest.main()
