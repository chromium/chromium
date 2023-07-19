# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ChromeRailModes(TestSuite):

  def test_combined_rail_modes(self):
    return DiffTestBlueprint(
        trace=Path('combined_rail_modes.py'),
        query="""
        SELECT RUN_METRIC('chrome/rail_modes.sql');
        SELECT * FROM combined_overall_rail_slices;
        """,
        out=Csv("""
        "id","ts","dur","rail_mode"
        1,0,10000,"response"
        2,10000,25000,"animation"
        3,35000,10000,"background"
        """))

  def test_cpu_time_by_combined_rail_mode(self):
    return DiffTestBlueprint(
        trace=Path('cpu_time_by_combined_rail_mode.py'),
        query="""
        SELECT RUN_METRIC('chrome/cpu_time_by_rail_mode.sql');
        SELECT * FROM cpu_time_by_rail_mode;
        """,
        out=Csv("""
        "id","ts","dur","rail_mode","cpu_dur"
        1,0,10000,"response",26000
        2,10000,20000,"animation",20000
        3,30000,5000,"background",8000
        4,35000,10000,"animation",21000
        5,45000,10000,"background",1000
        """))

  def test_actual_power_by_combined_rail_mode(self):
    return DiffTestBlueprint(
        trace=Path('actual_power_by_combined_rail_mode.py'),
        query="""
        SELECT RUN_METRIC('chrome/actual_power_by_rail_mode.sql');
        SELECT * FROM real_power_by_rail_mode;
        """,
        out=Csv("""
        "id","ts","dur","rail_mode","subsystem","joules","drain_w"
        1,0,10000000,"response","cellular",0.000000,0.000000
        1,0,10000000,"response","cpu_little",0.000140,0.014000
        2,10000000,20000000,"animation","cellular",0.000350,0.017500
        2,10000000,20000000,"animation","cpu_little",0.000140,0.007000
        3,30000000,5000000,"background","cellular",0.000018,0.003500
        3,30000000,5000000,"background","cpu_little",0.000007,0.001400
        4,35000000,10000000,"animation","cellular",0.000021,0.002100
        4,35000000,10000000,"animation","cpu_little",0.000070,0.007000
        5,45000000,10000000,"background","cellular",0.000003,0.000350
        5,45000000,10000000,"background","cpu_little",0.000070,0.007000
        """))

  def test_estimated_power_by_combined_rail_mode(self):
    return DiffTestBlueprint(
        trace=Path('estimated_power_by_combined_rail_mode.py'),
        query="""
        SELECT RUN_METRIC('chrome/estimated_power_by_rail_mode.sql');
        SELECT * FROM power_by_rail_mode;
        """,
        out=Csv("""
        "id","ts","dur","rail_mode","mas","ma"
        1,0,10000000,"response",0.554275,55.427500
        2,10000000,20000000,"animation",0.284850,14.242500
        3,30000000,5000000,"background",0.076233,15.246667
        4,35000000,10000000,"animation",0.536850,53.685000
        5,45000000,10000000,"background",0.071580,7.158000
        """))

  def test_modified_rail_modes(self):
    return DiffTestBlueprint(
        trace=Path('modified_rail_modes.py'),
        query="""
        SELECT RUN_METRIC('chrome/rail_modes.sql');
        SELECT * FROM modified_rail_slices;
        """,
        out=Csv("""
        "id","ts","dur","mode"
        2,0,1000000000,"response"
        3,1000000000,1950000000,"foreground_idle"
        4,2950000000,333333324,"animation"
        5,3283333324,216666676,"foreground_idle"
        6,3500000000,1000000000,"background"
        """))

  def test_modified_rail_modes_no_vsyncs(self):
    return DiffTestBlueprint(
        trace=Path('modified_rail_modes_no_vsyncs.py'),
        query="""
        SELECT RUN_METRIC('chrome/rail_modes.sql');
        SELECT * FROM modified_rail_slices;
        """,
        out=Csv("""
        "id","ts","dur","mode"
        2,0,1000000000,"response"
        3,1000000000,2500000000,"foreground_idle"
        4,3500000000,1000000000,"background"
        """))

  def test_modified_rail_modes_with_input(self):
    return DiffTestBlueprint(
        trace=Path('modified_rail_modes_with_input.py'),
        query="""
        SELECT RUN_METRIC('chrome/rail_modes.sql');
        SELECT * FROM modified_rail_slices;
        """,
        out=Csv("""
        "id","ts","dur","mode"
        2,0,1000000000,"response"
        3,1000000000,1950000000,"foreground_idle"
        4,2950000000,50000000,"animation"
        5,3000000000,66666674,"response"
        6,3066666674,216666650,"animation"
        7,3283333324,216666676,"foreground_idle"
        8,3500000000,1000000000,"background"
        """))

  def test_modified_rail_modes_long(self):
    return DiffTestBlueprint(
        trace=Path('modified_rail_modes_long.py'),
        query="""
        SELECT RUN_METRIC('chrome/rail_modes.sql');
        SELECT * FROM modified_rail_slices;
        """,
        out=Csv("""
        "id","ts","dur","mode"
        2,0,1000000000,"response"
        3,1000000000,1,"background"
        """))

  def test_modified_rail_modes_extra_long(self):
    return DiffTestBlueprint(
        trace=Path('modified_rail_modes_extra_long.py'),
        query="""
        SELECT RUN_METRIC('chrome/rail_modes.sql');
        SELECT * FROM modified_rail_slices;
        """,
        out=Csv("""
        "id","ts","dur","mode"
        """))
