#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import DataPath
from python.generators.diff_tests.testing import Csv
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ChromeInteractions(TestSuite):
  def test_chrome_fcp_lcp_navigations(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_fcp_lcp_navigations.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.page_loads;

        SELECT
          navigation_id,
          navigation_start_ts,
          fcp,
          fcp_ts,
          lcp,
          lcp_ts,
          browser_upid
        FROM chrome_page_loads
        ORDER by navigation_start_ts;
        """,
        out=Csv("""
        "navigation_id","navigation_start_ts","fcp","fcp_ts","lcp","lcp_ts","browser_upid"
        6,687425601436243,950000000,687426551436243,950000000,687426551436243,1
        7,687427799068243,888000000,687428687068243,888000000,687428687068243,1
        8,687429970749243,1031000000,687431001749243,1132000000,687431102749243,1
        9,687432344113243,539000000,687432883113243,539000000,687432883113243,1
        10,687434796215243,475000000,687435271215243,475000000,687435271215243,1
        11,687435970742243,763000000,687436733742243,852000000,687436822742243,1
        13,687438343638243,1005000000,687439348638243,1005000000,687439348638243,1
        14,687440258111243,900000000,687441158111243,"[NULL]","[NULL]",1
        """))
