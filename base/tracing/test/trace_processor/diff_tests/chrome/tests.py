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

  def test_loadline_2_stages(self):
    return DiffTestBlueprint(
        trace=DataPath('loadline2_phone_perfetto_trace.pb.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.loadline_2;

        SELECT * FROM chrome_loadline2_stages;
        """,
        out=Csv("""
        "page","story_start","start_request","end_request","renderer_ready","visual_mark","visual_raf","visual_presentation","interactive_mark","interactive_raf","interactive_presentation","story_finish"
        "amazon_product",92168959184844,92168964292547,92168971170844,92168972399547,92169083553844,92169188454844,92169184256844,92169685753844,92169695454844,92169733497844,92169878447844
        "cnn_article",92171089712844,92171099378547,92171106636844,92171107875858,92171191827844,92171329926844,92171248159844,92171413627844,92171450327844,92171481210844,92171558926844
        "wikipedia_article",92172730780844,92172739162547,92172746708844,92172747906244,92172801226844,92172843126844,92172846114844,92172826526844,92172877126844,92172879408844,92173054810844
        "globo_homepage",92174078990844,92174091306547,92174101735844,92174091680202,92174166369844,92174220769844,92174277596844,92174638068844,92174679269844,92174710386844,92175115291844
        "google_search_result",92176269047844,92176276990547,92176286094844,92176276922481,92176326400844,92176365000844,92176391496844,92176571700844,92176587400844,92176607896844,92176764223844
        """))

  def test_chrome_graphics_pipeline_surface_frame_steps(self):
    return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.graphics_pipeline;

        SELECT
          s.id,
          s.ts,
          s.dur,
          s.step,
          s.surface_frame_trace_id,
          t.tid
        FROM chrome_graphics_pipeline_surface_frame_steps s
        JOIN thread t ON s.utid = t.utid
        ORDER BY s.ts
        LIMIT 10;
        """,
        out=Csv("""
        "id","ts","dur","step","surface_frame_trace_id","tid"
        209,1292552020392633,142000,"STEP_ISSUE_BEGIN_FRAME",1407387768455321,6440
        210,1292552020907210,1264000,"STEP_RECEIVE_BEGIN_FRAME",1407387768455321,6476
        259,1292552026179210,1550000,"STEP_GENERATE_COMPOSITOR_FRAME",1407387768455321,6476
        264,1292552026586210,924000,"STEP_SUBMIT_COMPOSITOR_FRAME",1407387768455321,6476
        265,1292552027255633,791000,"STEP_RECEIVE_COMPOSITOR_FRAME",1407387768455321,6440
        268,1292552028200633,122000,"STEP_ISSUE_BEGIN_FRAME",4294967439,6440
        269,1292552028581257,1772000,"STEP_GENERATE_COMPOSITOR_FRAME",4294967439,6263
        276,1292552030185257,164000,"STEP_SUBMIT_COMPOSITOR_FRAME",4294967439,6263
        277,1292552030600633,195000,"STEP_RECEIVE_COMPOSITOR_FRAME",4294967439,6440
        302,1292552032277633,178000,"STEP_ISSUE_BEGIN_FRAME",1407387768455322,6440
        """))

  def test_chrome_graphics_pipeline_display_frame_steps(self):
    return DiffTestBlueprint(
      trace=DataPath('scroll_m131.pftrace'),
      query="""
      INCLUDE PERFETTO MODULE chrome.graphics_pipeline;

      SELECT
        id,
        ts,
        dur,
        step,
        display_trace_id,
        utid
      FROM chrome_graphics_pipeline_display_frame_steps
      ORDER BY ts
      LIMIT 10;
      """,
      out=Csv("""
      "id","ts","dur","step","display_trace_id","utid"
      279,1292552030930633,1263000,"STEP_DRAW_AND_SWAP",65565,6
      285,1292552031240633,143000,"STEP_SURFACE_AGGREGATION",65565,6
      299,1292552032042633,68000,"STEP_SEND_BUFFER_SWAP",65565,6
      319,1292552033751131,667000,"STEP_BUFFER_SWAP_POST_SUBMIT",65565,7
      337,1292552036240633,2033000,"STEP_DRAW_AND_SWAP",65566,6
      341,1292552036520633,873000,"STEP_SURFACE_AGGREGATION",65566,6
      359,1292552038113633,75000,"STEP_SEND_BUFFER_SWAP",65566,6
      376,1292552039773131,458000,"STEP_BUFFER_SWAP_POST_SUBMIT",65566,7
      394,1292552043191131,48000,"STEP_FINISH_BUFFER_SWAP",65565,7
      397,1292552043253633,75000,"STEP_SWAP_BUFFERS_ACK",65565,6
      """))

  def test_chrome_surface_frame_id_to_first_display_id(self):
    return DiffTestBlueprint(
      trace=DataPath('scroll_m131.pftrace'),
      query="""
      INCLUDE PERFETTO MODULE chrome.graphics_pipeline;

      SELECT
        surface_frame_trace_id,
        display_trace_id
      FROM chrome_surface_frame_id_to_first_display_id
      ORDER BY surface_frame_trace_id, display_trace_id
      LIMIT 10;
      """,
      out=Csv("""
      "surface_frame_trace_id","display_trace_id"
      4294967439,65565
      4294967440,65566
      4294967441,65567
      4294967442,65568
      4294967443,65569
      4294967449,65658
      4294967458,65716
      4294967459,65717
      4294967460,65718
      4294967461,65719
      """))

  def test_chrome_graphics_pipeline_inputs_to_surface_frames(self):
    return DiffTestBlueprint(
      trace=DataPath('scroll_m131.pftrace'),
      query="""
      INCLUDE PERFETTO MODULE chrome.graphics_pipeline;

      SELECT
        surface_frame_trace_id,
        latency_id
      FROM chrome_graphics_pipeline_inputs_to_surface_frames
      ORDER BY surface_frame_trace_id, latency_id
      LIMIT 10;
      """,
      out=Csv("""
      "surface_frame_trace_id","latency_id"
      1407387768455321,-2143831735395279174
      1407387768455321,-2143831735395279169
      1407387768455322,-2143831735395279191
      1407387768455323,-2143831735395279278
      1407387768455324,-2143831735395279270
      1407387768455325,-2143831735395279284
      1407387768455326,-2143831735395279244
      1407387768455327,-2143831735395279233
      1407387768455328,-2143831735395279258
      1407387768455329,-2143831735395279255
      """))
