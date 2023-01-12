#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import qemu_target
import shutil
import subprocess
import tempfile
import time
import unittest

TEST_PAYLOAD = "Let's get this payload across the finish line!"

tmpdir = tempfile.mkdtemp()

# Register the target with the context manager so that it always gets
# torn down on process exit. Otherwise there might be lingering QEMU instances
# if Python crashes or is interrupted.
with qemu_target.QemuTarget(tmpdir, 'x64') as target:
  class TestQemuTarget(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
      target.Start()

    @classmethod
    def tearDownClass(cls):
      target.Shutdown()
      shutil.rmtree(tmpdir)

    def testRunCommand(self):
      self.assertEqual(0, target.RunCommand(['true']))
      self.assertEqual(1, target.RunCommand(['false']))

    def testRunCommandPiped(self):
      proc = target.RunCommandPiped(['cat'],
                                    stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE)
      proc.stdin.write(TEST_PAYLOAD)
      proc.stdin.flush()
      proc.stdin.close()
      self.assertEqual(TEST_PAYLOAD, proc.stdout.readline())
      proc.kill()


  if __name__ == '__main__':
    unittest.main()
