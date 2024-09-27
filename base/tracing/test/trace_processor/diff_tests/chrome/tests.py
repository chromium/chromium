#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ChromeStdlib(TestSuite):
  # Chrome tasks.
  def test_chrome_tasks(self):
    return DiffTestBlueprint(
        trace=DataPath(
            'chrome_page_load_all_categories_not_extended.pftrace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.tasks;

        SELECT full_name as name, task_type, count() AS count
        FROM chrome_tasks
        GROUP BY full_name, task_type
        HAVING count >= 5
        ORDER BY count DESC, name;
        """,
        out=Path('chrome_tasks.out'))

  def test_top_level_java_choreographer_slices_top_level_java_chrome_tasks(
      self):
    return DiffTestBlueprint(
        trace=DataPath('top_level_java_choreographer_slices'),
        query="""
        INCLUDE PERFETTO MODULE chrome.tasks;

        SELECT
          full_name,
          task_type
        FROM chrome_tasks
        WHERE category = "toplevel,Java"
        AND ts < 263904000000000
        GROUP BY full_name, task_type;
        """,
        out=Path(
            'top_level_java_choreographer_slices_top_level_java_chrome_tasks_test.out'
        ))

  # Chrome custom navigation event names
  def test_chrome_custom_navigation_tasks(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_custom_navigation_trace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.tasks;

        SELECT full_name, task_type, count() AS count
        FROM chrome_tasks
        WHERE full_name GLOB 'FrameHost::BeginNavigation*'
          OR full_name GLOB 'FrameHost::DidCommitProvisionalLoad*'
          OR full_name GLOB 'FrameHost::DidCommitSameDocumentNavigation*'
          OR full_name GLOB 'FrameHost::DidStopLoading*'
        GROUP BY full_name, task_type
        ORDER BY count DESC
        LIMIT 50;
        """,
        out=Csv("""
        "full_name","task_type","count"
        "FrameHost::BeginNavigation (SUBFRAME)","navigation_task",5
        "FrameHost::DidStopLoading (SUBFRAME)","navigation_task",3
        "FrameHost::BeginNavigation (PRIMARY_MAIN_FRAME)","navigation_task",1
        "FrameHost::DidCommitProvisionalLoad (SUBFRAME)","navigation_task",1
        """))

  # Chrome custom navigation event names
  def test_chrome_histograms(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_5672_histograms.pftrace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.histograms;

        SELECT
          name,
          count() as count
        FROM chrome_histograms
        GROUP BY name
        ORDER BY count DESC, name
        LIMIT 20;
        """,
        out=Csv("""
        "name","count"
        "Net.QuicSession.AsyncRead",19207
        "Net.QuicSession.NumQueuedPacketsBeforeWrite",19193
        "RendererScheduler.QueueingDuration.NormalPriority",9110
        "Net.OnTransferSizeUpdated.Experimental.OverridenBy",8525
        "Compositing.Renderer.AnimationUpdateOnMissingPropertyNode",3489
        "Net.QuicConnection.WritePacketStatus",3099
        "Net.QuicSession.PacketWriteTime.Synchronous",3082
        "Net.QuicSession.SendPacketSize.ForwardSecure",3012
        "Net.URLLoaderThrottleExecutionTime.WillStartRequest",1789
        "Net.URLLoaderThrottleExecutionTime.BeforeWillProcessResponse",1773
        "Net.URLLoaderThrottleExecutionTime.WillProcessResponse",1773
        "UMA.StackProfiler.SampleInOrder",1534
        "GPU.SharedImage.ContentConsumed",1037
        "Gpu.Rasterization.Raster.MSAASampleCountLog2",825
        "Scheduling.Renderer.DeadlineMode",637
        "Blink.CullRect.UpdateTime",622
        "Scheduling.Renderer.BeginImplFrameLatency2",591
        "Net.QuicSession.CoalesceStreamFrameStatus",551
        "API.StorageAccess.AllowedRequests2",541
        "Net.HttpResponseCode",541
        """))

  def test_speedometer_2_1_score(self):
    return DiffTestBlueprint(
        trace=DataPath('speedometer_21.perfetto_trace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.speedometer;

        SELECT format("%.1f", chrome_speedometer_score()) AS score
        """,
        out=Csv("""
        "score"
        "95.8"
        """))

  def test_speedometer_2_1_iteration(self):
    return DiffTestBlueprint(
        trace=DataPath('speedometer_21.perfetto_trace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.speedometer;

        SELECT
         ts,dur,name,iteration,
         format("%.1f", geomean) AS geomean,
         format("%.1f", score) AS score
           FROM chrome_speedometer_iteration
          ORDER BY iteration ASC
        """,
        out=Csv("""
        "ts","dur","name","iteration","geomean","score"
        693997310311984,7020297000,"iteration-1",1,"254.2","78.7"
        694004414619984,6308034000,"iteration-2",2,"224.4","89.1"
        694010770005984,5878289000,"iteration-3",3,"200.3","99.9"
        694016699502984,5934578000,"iteration-4",4,"201.2","99.4"
        694022683560984,5952163000,"iteration-5",5,"203.0","98.5"
        694028690570984,5966530000,"iteration-6",6,"204.3","97.9"
        694034719276984,5853043000,"iteration-7",7,"200.4","99.8"
        694040637173984,6087435000,"iteration-8",8,"203.0","98.5"
        694046772284984,6040820000,"iteration-9",9,"199.3","100.3"
        694052857814984,6063770000,"iteration-10",10,"208.0","96.2"
        """))

  def test_speedometer_2_1_renderer_main_utid(self):
    return DiffTestBlueprint(
        trace=DataPath('speedometer_21.perfetto_trace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.speedometer;

        SELECT chrome_speedometer_renderer_main_utid();
        """,
        out=Csv("""
        "chrome_speedometer_renderer_main_utid()"
        4
        """))

  def test_speedometer_3_score(self):
    return DiffTestBlueprint(
        trace=DataPath('speedometer_3.perfetto_trace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.speedometer;

        SELECT format("%.2f", chrome_speedometer_score()) AS score
        """,
        out=Csv("""
        "score"
        "9.32"
        """))

  def test_speedometer_3_iteration(self):
    return DiffTestBlueprint(
        trace=DataPath('speedometer_3.perfetto_trace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.speedometer;

        SELECT
         ts,dur,name,iteration,
         format("%.1f", geomean) AS geomean,
         format("%.1f", score) AS score
           FROM chrome_speedometer_iteration
          ORDER BY iteration ASC
        """,
        out=Csv("""
        "ts","dur","name","iteration","geomean","score"
        303831755756,29237440000,"iteration-0",0,"149.6","6.7"
        333069212756,5852045000,"iteration-1",1,"110.7","9.0"
        338921282756,5128440000,"iteration-2",2,"113.5","8.8"
        344049763756,4640412000,"iteration-3",3,"105.0","9.5"
        348690198756,4790109000,"iteration-4",4,"106.9","9.4"
        353480329756,5150878000,"iteration-5",5,"106.0","9.4"
        358631265756,4825246000,"iteration-6",6,"103.1","9.7"
        363456560756,4447621000,"iteration-7",7,"95.5","10.5"
        367904208756,4566333000,"iteration-8",8,"100.8","9.9"
        372470568756,4301553000,"iteration-9",9,"96.9","10.3"
        """))

  def test_speedometer_3_renderer_main_utid(self):
    return DiffTestBlueprint(
        trace=DataPath('speedometer_3.perfetto_trace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.speedometer;

        SELECT chrome_speedometer_renderer_main_utid();
        """,
        out=Csv("""
        "chrome_speedometer_renderer_main_utid()"
        2
        """))

  # CPU power ups
  def test_cpu_powerups(self):
    return DiffTestBlueprint(
        trace=DataPath('cpu_powerups_1.pb'),
        query="""
        INCLUDE PERFETTO MODULE chrome.cpu_powerups;
        SELECT * FROM chrome_cpu_power_first_toplevel_slice_after_powerup;
        """,
        out=Csv("""
        "slice_id","previous_power_state"
        424,2
        703,2
        708,2
        """))
