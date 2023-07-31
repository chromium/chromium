#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access


import unittest

from pylib.local.machine import local_machine_junit_test_run


class LocalMachineJunitTestRunTests(unittest.TestCase):
  def setUp(self):
    local_machine_junit_test_run._MAX_TESTS_PER_JOB = 150

  def testGroupTestsForShardWithSdk(self):
    test_list = []
    results = local_machine_junit_test_run.GroupTestsForShard(test_list)
    self.assertListEqual(results, [[]])

    # All the same SDK and classes. Should come back as one job.
    t1 = 'a.b#c[28]'
    t2 = 'a.b#d[28]'
    t3 = 'a.b#e[28]'
    test_list = [t1, t2, t3]
    results = local_machine_junit_test_run.GroupTestsForShard(test_list)
    ans = [[t1.replace('#', '.'), t2.replace('#', '.'), t3.replace('#', '.')]]
    for idx, _ in enumerate(ans):
      self.assertCountEqual(ans[idx], results[idx])

    # Tests same class, but different sdks.
    # Should come back as 3 jobs as they're different sdks.
    t1 = 'a.b#c[28]'
    t2 = 'a.b#d[27]'
    t3 = 'a.b#e[26]'
    test_list = [t1, t2, t3]
    results = local_machine_junit_test_run.GroupTestsForShard(test_list)
    ans = [[t1.replace('#', '.')], [t2.replace('#', '.')],
           [t3.replace('#', '.')]]
    self.assertCountEqual(results, ans)

    # Tests having different tests and sdks.
    # Should come back as 3 jobs.
    t1 = 'a.1#c[28]'
    t2 = 'a.2#d[27]'
    t3 = 'a.3#e[26]'
    t4 = 'a.4#e[26]'
    test_list = [t1, t2, t3, t4]
    results = local_machine_junit_test_run.GroupTestsForShard(test_list)

    ans = [[t2.replace('#', '.')], [t3.replace('#', '.'),
                                    t4.replace('#', '.')],
           [t1.replace('#', '.')]]
    self.assertCountEqual(ans, results)

    # Tests multiple tests of same sdk split across multiple jobs.
    t0 = 'a.b#c[28]'
    t1 = 'foo.bar#d[27]'
    t2 = 'alice.bob#e[26]'
    t3 = 'a.l#c[28]'
    t4 = 'z.x#c[28]'
    t5 = 'z.y#c[28]'
    t6 = 'z.z#c[28]'
    test_list = [t0, t1, t2, t3, t4, t5, t6]
    results = local_machine_junit_test_run.GroupTestsForShard(test_list)
    results = sorted(results)
    t_ans = [x.replace('#', '.') for x in test_list]
    ans = [[t_ans[0], t_ans[3], t_ans[4], t_ans[5], t_ans[6]], [t_ans[2]],
           [t_ans[1]]]
    self.assertCountEqual(ans, results)

    # Tests having a class without an sdk
    t0 = 'cow.moo#chicken'
    t1 = 'a.b#c[28]'
    t2 = 'foo.bar#d[27]'
    t3 = 'alice.bob#e[26]'
    t4 = 'a.l#c[28]'
    t5 = 'z.x#c[28]'
    t6 = 'z.y#c[28]'
    t7 = 'z.moo#c[28]'
    test_list = [t0, t1, t2, t3, t4, t5, t6, t7]
    results = local_machine_junit_test_run.GroupTestsForShard(test_list)
    t_ans = [x.replace('#', '.') for x in test_list]

    self.assertEqual(len(results), 4)
    ans = [[t_ans[0]], [t_ans[1], t_ans[4], t_ans[7], t_ans[5], t_ans[6]],
           [t_ans[2]], [t_ans[3]]]
    self.assertCountEqual(ans, results)

  def testGroupTestsForShardWithSDK_ClassesPerJob(self):
    # Tests grouping tests when classes_per_job is exceeded.
    # All tests are from same class so should be in a single job.
    local_machine_junit_test_run._MAX_TESTS_PER_JOB = 3
    t0 = 'plane.b17#bomb[28]'
    t1 = 'plane.b17#gunner[28]'
    t2 = 'plane.b17#pilot[28]'
    t3 = 'plane.b17#copilot[28]'
    t4 = 'plane.b17#radio[28]'
    test_list = [t0, t1, t2, t3, t4]
    results = local_machine_junit_test_run.GroupTestsForShard(test_list)
    t_ans = [x.replace('#', '.') for x in test_list]
    ans = [t_ans[0], t_ans[1], t_ans[2], t_ans[3], t_ans[4]]
    found_ans = False
    for r in results:
      if len(r) > 0:
        self.assertCountEqual(r, ans)
        found_ans = True
    self.assertTrue(found_ans)

    # Tests grouping tests when classes_per_job is exceeded and classes are
    # different.
    t0 = 'plane.b17#bomb[28]'
    t1 = 'plane.b17#gunner[28]'
    t2 = 'plane.b17#pilot[28]'
    t3 = 'plane.b24_liberator#copilot[28]'
    t4 = 'plane.b24_liberator#radio[28]'
    t5 = 'plane.b25_mitchel#doolittle[28]'
    t6 = 'plane.b26_marauder#radio[28]'
    t7 = 'plane.b36_peacemaker#nuclear[28]'
    t8 = 'plane.b52_stratofortress#nuclear[30]'
    test_list = [t0, t1, t2, t3, t4, t5, t6, t7, t8]
    results = local_machine_junit_test_run.GroupTestsForShard(test_list)
    t_ans = [x.replace('#', '.') for x in test_list]
    checked_b17 = False
    checked_b52 = False
    for r in results:
      if t_ans[0] in r:
        self.assertTrue(t_ans[1] in r)
        self.assertTrue(t_ans[2] in r)
        checked_b17 = True
      if t_ans[8] in r:
        self.assertEqual(1, len(r))
        checked_b52 = True
        continue
      # Every job should have at least 1 test. Max any job could have is 5
      # if b17 and b24 are paired together as there is no need for any job
      # to have 3 classes with 3 shards for the 5 sdk28 classes.
      self.assertTrue(len(r) >= 1 and len(r) <= 5)

    self.assertTrue(all([checked_b17, checked_b52, len(results) == 4]))


if __name__ == '__main__':
  unittest.main()
