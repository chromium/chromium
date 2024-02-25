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

  def test_speedometer(self):
    return DiffTestBlueprint(
        trace=DataPath('speedometer.perfetto_trace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.speedometer;

        SELECT
          iteration,
          ts,
          dur,
          total,
          format('%.1f', mean) AS mean,
          format('%.1f', geomean) AS geomean,
          format('%.1f', score) AS score,
          num_measurements
        FROM
          chrome_speedometer_iteration,
          (
            SELECT iteration, COUNT(*) AS num_measurements
            FROM chrome_speedometer_measure
            GROUP BY iteration
          )
        USING (iteration)
        ORDER BY iteration;
        """,
        out=Path('chrome_speedometer.out'))

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