#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import os
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir)))
from util import md5_check


def _WriteZipFile(path, entries):
  with zipfile.ZipFile(path, 'w') as zip_file:
    for subpath, data in entries:
      zip_file.writestr(subpath, data)


class TestMd5Check(unittest.TestCase):
  def setUp(self):
    self.called = False
    self.changes = None

  def testCallAndRecordIfStale(self):
    input_strings = ['string1', 'string2']
    input_file1 = tempfile.NamedTemporaryFile(suffix='.txt')
    input_file2 = tempfile.NamedTemporaryFile(suffix='.zip')
    file1_contents = b'input file 1'
    input_file1.write(file1_contents)
    input_file1.flush()
    # Test out empty zip file to start.
    _WriteZipFile(input_file2.name, [])
    input_files = [input_file1.name, input_file2.name]
    zip_paths = [input_file2.name]

    record_path = tempfile.NamedTemporaryFile(suffix='.stamp')

    def CheckCallAndRecord(should_call,
                           message,
                           force=False,
                           outputs_specified=False,
                           outputs_missing=False,
                           expected_changes=None,
                           added_or_modified_only=None,
                           track_subentries=False,
                           output_newer_than_record=False):
      output_paths = None
      if outputs_specified:
        output_file1 = tempfile.NamedTemporaryFile()
        if outputs_missing:
          output_file1.close()  # Gets deleted on close().
        output_paths = [output_file1.name]
      if output_newer_than_record:
        output_mtime = os.path.getmtime(output_file1.name)
        os.utime(record_path.name, (output_mtime - 1, output_mtime - 1))
      else:
        # touch the record file so it doesn't look like it's older that
        # the output we've just created
        os.utime(record_path.name, None)

      self.called = False
      self.changes = None
      if expected_changes or added_or_modified_only is not None:
        def MarkCalled(changes):
          self.called = True
          self.changes = changes
      else:
        def MarkCalled():
          self.called = True

      md5_check.CallAndRecordIfStale(
          MarkCalled,
          record_path=record_path.name,
          input_paths=input_files,
          input_strings=input_strings,
          output_paths=output_paths,
          force=force,
          pass_changes=(expected_changes or added_or_modified_only) is not None,
          track_subpaths_allowlist=zip_paths if track_subentries else None)
      self.assertEqual(should_call, self.called, message)
      if expected_changes:
        description = self.changes.DescribeDifference()
        self.assertTrue(fnmatch.fnmatch(description, expected_changes),
                        'Expected %s to match %s' % (
                        repr(description), repr(expected_changes)))
      if should_call and added_or_modified_only is not None:
        self.assertEqual(added_or_modified_only,
                         self.changes.AddedOrModifiedOnly())

    CheckCallAndRecord(True, 'should call when record doesn\'t exist',
                       expected_changes='Previous stamp file not found.',
                       added_or_modified_only=False)
    CheckCallAndRecord(False, 'should not call when nothing changed')
    input_files = input_files[::-1]
    CheckCallAndRecord(False, 'reordering of inputs shouldn\'t trigger call')

    CheckCallAndRecord(False, 'should not call when nothing changed #2',
                       outputs_specified=True, outputs_missing=False)
    CheckCallAndRecord(True, 'should call when output missing',
                       outputs_specified=True, outputs_missing=True,
                       expected_changes='Outputs do not exist:*',
                       added_or_modified_only=False)
    CheckCallAndRecord(True,
                       'should call when output is newer than record',
                       expected_changes='Outputs newer than stamp file:*',
                       outputs_specified=True,
                       outputs_missing=False,
                       added_or_modified_only=False,
                       output_newer_than_record=True)
    CheckCallAndRecord(True, force=True, message='should call when forced',
                       expected_changes='force=True',
                       added_or_modified_only=False)

    input_file1.write(b'some more input')
    input_file1.flush()
    CheckCallAndRecord(True, 'changed input file should trigger call',
                       expected_changes='*Modified: %s' % input_file1.name,
                       added_or_modified_only=True)

    input_files = input_files[:1]
    CheckCallAndRecord(True, 'removing file should trigger call',
                       expected_changes='*Removed: %s' % input_file1.name,
                       added_or_modified_only=False)

    input_files.append(input_file1.name)
    CheckCallAndRecord(True, 'added input file should trigger call',
                       expected_changes='*Added: %s' % input_file1.name,
                       added_or_modified_only=True)

    input_strings[0] = input_strings[0] + ' a bit longer'
    CheckCallAndRecord(True, 'changed input string should trigger call',
                       expected_changes='*Input strings changed*',
                       added_or_modified_only=False)

    input_strings = input_strings[::-1]
    CheckCallAndRecord(True, 'reordering of string inputs should trigger call',
                       expected_changes='*Input strings changed*')

    input_strings = input_strings[:1]
    CheckCallAndRecord(True, 'removing a string should trigger call')

    input_strings.append('a brand new string')
    CheckCallAndRecord(
        True,
        'added input string should trigger call',
        added_or_modified_only=False)

    _WriteZipFile(input_file2.name, [('path/1.txt', '1')])
    CheckCallAndRecord(
        True,
        'added subpath should trigger call',
        expected_changes='*Modified: %s*Subpath added: %s' % (input_file2.name,
                                                              'path/1.txt'),
        added_or_modified_only=True,
        track_subentries=True)
    _WriteZipFile(input_file2.name, [('path/1.txt', '2')])
    CheckCallAndRecord(
        True,
        'changed subpath should trigger call',
        expected_changes='*Modified: %s*Subpath modified: %s' %
        (input_file2.name, 'path/1.txt'),
        added_or_modified_only=True,
        track_subentries=True)

    _WriteZipFile(input_file2.name, [])
    CheckCallAndRecord(True, 'removed subpath should trigger call',
                       expected_changes='*Modified: %s*Subpath removed: %s' % (
                                        input_file2.name, 'path/1.txt'),
                       added_or_modified_only=False)


if __name__ == '__main__':
  unittest.main()
