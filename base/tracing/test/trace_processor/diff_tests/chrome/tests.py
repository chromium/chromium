# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class Chrome(TestSuite):
  # Tests related to Chrome's use of Perfetto. Chrome histogram hashes
  def test_chrome_histogram_hashes(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          trusted_packet_sequence_id: 1
          timestamp: 0
          incremental_state_cleared: true
          track_event {
            categories: "cat1"
            type: 3
            name_iid: 1
            chrome_histogram_sample {
              name_hash: 10
              sample: 100
            }
          }
        }
        packet {
          trusted_packet_sequence_id: 1
          timestamp: 0
          incremental_state_cleared: true
          track_event {
            categories: "cat2"
            type: 3
            name_iid: 2
            chrome_histogram_sample {
              name_hash: 20
            }
          }
        }
        """),
        query=Metric('chrome_histogram_hashes'),
        out=TextProto(r"""
        [perfetto.protos.chrome_histogram_hashes]: {
          hash: 10
          hash: 20
        }
        """))

  # Chrome user events
  def test_chrome_user_event_hashes(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          trusted_packet_sequence_id: 1
          timestamp: 0
          incremental_state_cleared: true
          track_event {
            categories: "cat1"
            type: 3
            name_iid: 1
            chrome_user_event {
              action_hash: 10
            }
          }
        }
        packet {
          trusted_packet_sequence_id: 1
          timestamp: 0
          incremental_state_cleared: true
          track_event {
            categories: "cat2"
            type: 3
            name_iid: 2
            chrome_user_event {
              action_hash: 20
            }
          }
        }
        """),
        query=Metric('chrome_user_event_hashes'),
        out=TextProto(r"""
        [perfetto.protos.chrome_user_event_hashes]: {
          action_hash: 10
          action_hash: 20
        }
        """))

  # Chrome performance mark
  def test_chrome_performance_mark_hashes(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          trusted_packet_sequence_id: 1
          timestamp: 0
          incremental_state_cleared: true
          track_event {
            categories: "cat1"
            type: 3
            name: "name1"
            [perfetto.protos.ChromeTrackEvent.chrome_hashed_performance_mark] {
              site_hash: 10
              mark_hash: 100
            }
          }
        }
        packet {
          trusted_packet_sequence_id: 1
          timestamp: 0
          incremental_state_cleared: true
          track_event {
            categories: "cat2"
            type: 3
            name: "name2"
            [perfetto.protos.ChromeTrackEvent.chrome_hashed_performance_mark] {
              site_hash: 20
              mark_hash: 200
            }
          }
        }
        """),
        query=Metric('chrome_performance_mark_hashes'),
        out=TextProto(r"""
        [perfetto.protos.chrome_performance_mark_hashes]: {
          site_hash: 10
          site_hash: 20
          mark_hash: 100
          mark_hash: 200
        }
        """))

  # Chrome reliable range
  def test_chrome_reliable_range(self):
    return DiffTestBlueprint(
        trace=Path('chrome_reliable_range.textproto'),
        query=Path('chrome_reliable_range_test.sql'),
        out=Csv("""
        "start","reason","debug_limiting_upid","debug_limiting_utid"
        12,"First slice for utid=2","[NULL]",2
        """))

  def test_chrome_reliable_range_cropping(self):
    return DiffTestBlueprint(
        trace=Path('chrome_reliable_range_cropping.textproto'),
        query=Path('chrome_reliable_range_test.sql'),
        out=Csv("""
        "start","reason","debug_limiting_upid","debug_limiting_utid"
        10000,"Range of interest packet","[NULL]",2
        """))

  def test_chrome_reliable_range_missing_processes(self):
    return DiffTestBlueprint(
        trace=Path('chrome_reliable_range_missing_processes.textproto'),
        query=Path('chrome_reliable_range_test.sql'),
        out=Csv("""
        "start","reason","debug_limiting_upid","debug_limiting_utid"
        1011,"Missing process data for upid=2",2,1
        """))

  def test_chrome_reliable_range_missing_browser_main(self):
    return DiffTestBlueprint(
        trace=Path('chrome_reliable_range_missing_browser_main.textproto'),
        query=Path('chrome_reliable_range_test.sql'),
        out=Csv("""
        "start","reason","debug_limiting_upid","debug_limiting_utid"
        1011,"Missing main thread for upid=1",1,1
        """))

  def test_chrome_reliable_range_missing_gpu_main(self):
    return DiffTestBlueprint(
        trace=Path('chrome_reliable_range_missing_gpu_main.textproto'),
        query=Path('chrome_reliable_range_test.sql'),
        out=Csv("""
        "start","reason","debug_limiting_upid","debug_limiting_utid"
        1011,"Missing main thread for upid=1",1,1
        """))

  def test_chrome_reliable_range_missing_renderer_main(self):
    return DiffTestBlueprint(
        trace=Path('chrome_reliable_range_missing_renderer_main.textproto'),
        query=Path('chrome_reliable_range_test.sql'),
        out=Csv("""
        "start","reason","debug_limiting_upid","debug_limiting_utid"
        1011,"Missing main thread for upid=1",1,1
        """))

  def test_chrome_reliable_range_non_chrome_process(self):
    return DiffTestBlueprint(
        # We need a trace with a large number of non-chrome slices, so that the
        # reliable range is affected by their filtering.
        trace=DataPath('example_android_trace_30s.pb'),
        query=Path('chrome_reliable_range_test.sql'),
        out=Csv("""
        "start","reason","debug_limiting_upid","debug_limiting_utid"
        0,"[NULL]","[NULL]","[NULL]"
        """))

  # Chrome slices
  def test_chrome_slice_names(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          trusted_packet_sequence_id: 1
          timestamp: 1000
          track_event {
            categories: "cat"
            name: "Looper.Dispatch: class1"
            type: 3
          }
        }
        packet {
          trusted_packet_sequence_id: 1
          timestamp: 2000
          track_event {
            categories: "cat"
            name: "name2"
            type: 3
          }
        }
        packet {
          chrome_metadata {
            chrome_version_code: 123
          }
        }
        """),
        query=Metric('chrome_slice_names'),
        out=TextProto(r"""
        [perfetto.protos.chrome_slice_names]: {
          chrome_version_code: 123
          slice_name: "Looper.Dispatch: class1"
          slice_name: "name2"
        }
        """))

  # Chrome tasks.
  def test_chrome_tasks(self):
    return DiffTestBlueprint(
        trace=DataPath(
            'chrome_page_load_all_categories_not_extended.pftrace.gz'),
        query="""
        SELECT IMPORT('chrome.tasks');

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
        SELECT IMPORT('chrome.tasks');

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

  # Chrome stack samples.
  def test_chrome_stack_samples_for_task(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_stack_traces_symbolized_trace.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_stack_samples_for_task.sql',
            'target_duration_ms', '0.000001',
            'thread_name', '"CrBrowserMain"',
            'task_name', '"sendTouchEvent"');

        SELECT
          sample.description,
          sample.ts,
          sample.depth
        FROM chrome_stack_samples_for_task sample
        JOIN (
            SELECT
              ts,
              dur
            FROM slice
            WHERE ts = 696373965001470
        ) test_slice
        ON sample.ts >= test_slice.ts
          AND sample.ts <= test_slice.ts + test_slice.dur
        ORDER BY sample.ts, sample.depth;
        """,
        out=Path('chrome_stack_samples_for_task_test.out'))

  # Log messages.
  def test_chrome_log_message(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 0
          incremental_state_cleared: true
          trusted_packet_sequence_id: 1
          track_descriptor {
            uuid: 12345
            thread {
              pid: 123
              tid: 345
            }
            parent_uuid: 0
            chrome_thread {
              thread_type: THREAD_POOL_FG_WORKER
            }
          }
        }

        packet {
          trusted_packet_sequence_id: 1
          timestamp: 10
          track_event {
            track_uuid: 12345
            categories: "cat1"
            type: TYPE_INSTANT
            name: "slice1"
            log_message {
                body_iid: 1
                source_location_iid: 3
            }
          }
          interned_data {
            log_message_body {
                iid: 1
                body: "log message"
            }
            source_locations {
                iid: 3
                function_name: "func"
                file_name: "foo.cc"
                line_number: 123
            }
          }
        }
        """),
        query="""
        SELECT utid, tag, msg, prio FROM android_logs;
        """,
        # If the log_message_body doesn't have any priority, a default 4 (i.e.
        # INFO) is assumed (otherwise the UI will not show the message).
        out=Csv("""
        "utid","tag","msg","prio"
        1,"foo.cc:123","log message",4
        """))

  def test_chrome_log_message_priority(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 0
          incremental_state_cleared: true
          trusted_packet_sequence_id: 1
          track_descriptor {
            uuid: 12345
            thread {
              pid: 123
              tid: 345
            }
            parent_uuid: 0
            chrome_thread {
              thread_type: THREAD_POOL_FG_WORKER
            }
          }
        }

        packet {
          trusted_packet_sequence_id: 1
          timestamp: 10
          track_event {
            track_uuid: 12345
            categories: "cat1"
            type: TYPE_INSTANT
            name: "slice1"
            log_message {
                body_iid: 1
                source_location_iid: 3
                prio: PRIO_WARN
            }
          }
          interned_data {
            log_message_body {
                iid: 1
                body: "log message"
            }
            source_locations {
                iid: 3
                function_name: "func"
                file_name: "foo.cc"
                line_number: 123
            }
          }
        }
        """),
        query="""
        SELECT utid, tag, msg, prio FROM android_logs;
        """,
        out=Csv("""
        "utid","tag","msg","prio"
        1,"foo.cc:123","log message",5
        """))

  def test_chrome_log_message_args(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 0
          incremental_state_cleared: true
          trusted_packet_sequence_id: 1
          track_descriptor {
            uuid: 12345
            thread {
              pid: 123
              tid: 345
            }
            parent_uuid: 0
            chrome_thread {
              thread_type: THREAD_POOL_FG_WORKER
            }
          }
        }

        packet {
          trusted_packet_sequence_id: 1
          timestamp: 10
          track_event {
            track_uuid: 12345
            categories: "cat1"
            type: TYPE_INSTANT
            name: "slice1"
            log_message {
                body_iid: 1
                source_location_iid: 3
            }
          }
          interned_data {
            log_message_body {
                iid: 1
                body: "log message"
            }
            source_locations {
                iid: 3
                function_name: "func"
                file_name: "foo.cc"
                line_number: 123
            }
          }
        }
        """),
        query=Path('chrome_log_message_args_test.sql'),
        out=Csv("""
        "log_message","function_name","file_name","line_number"
        "log message","func","foo.cc",123
        """))

  # Chrome custom navigation event names
  def test_chrome_custom_navigation_tasks(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_custom_navigation_trace.gz'),
        query="""
        SELECT IMPORT('chrome.tasks');

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
        SELECT IMPORT('chrome.histograms');

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

  # Trace proto content
  def test_proto_content(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query=Path('proto_content_test.sql'),
        out=Path('proto_content.out'))

  def test_speedometer(self):
    return DiffTestBlueprint(
        trace=DataPath('speedometer.perfetto_trace.gz'),
        query=Path('chrome_speedometer_test.sql'),
        out=Path('chrome_speedometer.out'))

  # TODO(mayzner): Uncomment when it works
  # def test_proto_content_path(self):
  #   return DiffTestBlueprint(
  #       trace=DataPath('chrome_scroll_without_vsync.pftrace'),
  #       query=Path('proto_content_path_test.sql'),
  #       out=Csv("""
  #       "total_size","field_type","field_name","parent_id","event_category",
  #                                                          "event_name"
  #       137426,"TracePacket","[NULL]","[NULL]","[NULL]","[NULL]"
  #       59475,"TrackEvent","#track_event",415,"[NULL]","[NULL]"
  #       37903,"TrackEvent","#track_event",17,"[NULL]","[NULL]"
  #       35904,"int32","#trusted_uid",17,"[NULL]","[NULL]"
  #       35705,"TracePacket","[NULL]","[NULL]","input,benchmark",
  #                                             "LatencyInfo.Flow"
  #       29403,"TracePacket","[NULL]","[NULL]","cc,input","[NULL]"
  #       24703,"ChromeLatencyInfo","#chrome_latency_info",18,"[NULL]","[NULL]"
  #       22620,"uint64","#time_us",26,"[NULL]","[NULL]"
  #       18711,"TrackEvent","#track_event",1467,"[NULL]","[NULL]"
  #       15606,"uint64","#timestamp",17,"[NULL]","[NULL]"
  #       """))
