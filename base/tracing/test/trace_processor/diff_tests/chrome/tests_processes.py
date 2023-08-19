# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ChromeProcesses(TestSuite):

  def test_chrome_processes(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_processes.sql');
        SELECT pid, name, process_type FROM chrome_process;
        """,
        out=Csv("""
        "pid","name","process_type"
        18250,"Renderer","Renderer"
        17547,"Browser","Browser"
        18277,"GPU Process","Gpu"
        17578,"Browser","Browser"
        """))

  def test_chrome_processes_android_systrace(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_android_systrace.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_processes.sql');
        SELECT pid, name, process_type FROM chrome_process;
        """,
        out=Path('chrome_processes_android_systrace.out'))

  def test_chrome_threads(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_processes.sql');
        SELECT tid, name, is_main_thread, canonical_name
        FROM chrome_thread
        ORDER BY tid, name;
        """,
        out=Path('chrome_threads.out'))

  def test_chrome_threads_android_systrace(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_android_systrace.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_processes.sql');
        SELECT tid, name, is_main_thread, canonical_name
        FROM chrome_thread
        ORDER BY tid, name;
        """,
        out=Path('chrome_threads_android_systrace.out'))

  def test_chrome_processes_type(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT pid, name, string_value AS chrome_process_type
        FROM
          process
        JOIN
          (SELECT * FROM args WHERE key = "chrome.process_type")
          chrome_process_args
          ON
            process.arg_set_id = chrome_process_args.arg_set_id
        ORDER BY pid;
        """,
        out=Csv("""
        "pid","name","chrome_process_type"
        17547,"Browser","Browser"
        17578,"Browser","Browser"
        18250,"Renderer","Renderer"
        18277,"GPU Process","Gpu"
        """))

  def test_chrome_processes_type_android_systrace(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_android_systrace.pftrace'),
        query="""
        SELECT pid, name, string_value AS chrome_process_type
        FROM
          process
        JOIN
          (SELECT * FROM args WHERE key = "chrome.process_type")
          chrome_process_args
          ON
            process.arg_set_id = chrome_process_args.arg_set_id
        ORDER BY pid;
        """,
        out=Path('chrome_processes_type_android_systrace.out'))

  def test_track_with_chrome_process(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          trusted_packet_sequence_id: 1
          incremental_state_cleared: true
          timestamp: 0
          track_descriptor {
            uuid: 10
            process {
              pid: 5
              process_name: "p5"
            }
            # Empty Chrome process. This is similar to a process descriptor
            # emitted by Chrome for a process with an unknown Chrome
            # process_type. This process should still receive a
            # "chrome_process_type" arg in the args table, but with a NULL
            # value.
            chrome_process {}
          }
        }
        """),
        query="""
        SELECT pid, name, string_value AS chrome_process_type
        FROM
          process
        JOIN
          (SELECT * FROM args WHERE key = "chrome.process_type")
          chrome_process_args
          ON
            process.arg_set_id = chrome_process_args.arg_set_id
        ORDER BY pid;
        """,
        out=Csv("""
        "pid","name","chrome_process_type"
        5,"p5","[NULL]"
        """))

  # Missing processes.
  def test_chrome_missing_processes_default_trace(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT upid, pid, reliable_from
        FROM
          experimental_missing_chrome_processes
        JOIN
          process
          USING(upid)
        ORDER BY upid;
        """,
        out=Csv("""
        "upid","pid","reliable_from"
        """))

  def test_chrome_missing_processes(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 1
          incremental_state_cleared: true
          trusted_packet_sequence_id: 1
          track_event {
            type: TYPE_INSTANT
            name: "ActiveProcesses"
            chrome_active_processes {
              pid: 10
              pid: 100
              pid: 1000
            }
          }
        }
        packet {
          timestamp: 1
          trusted_packet_sequence_id: 2
          track_descriptor {
            uuid: 1
            process {
              pid: 10
            }
            parent_uuid: 0
          }
        }
        packet {
          timestamp: 1000000000
          trusted_packet_sequence_id: 3
          track_descriptor {
            uuid: 2
            process {
              pid: 100
            }
            parent_uuid: 0
          }
        }
        """),
        query="""
        SELECT upid, pid, reliable_from
        FROM
          experimental_missing_chrome_processes
        JOIN
          process
          USING(upid)
        ORDER BY upid;
        """,
        out=Csv("""
        "upid","pid","reliable_from"
        2,100,1000000000
        3,1000,"[NULL]"
        """))

  def test_chrome_missing_processes_args(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 1
          incremental_state_cleared: true
          trusted_packet_sequence_id: 1
          track_event {
            type: TYPE_INSTANT
            name: "ActiveProcesses"
            chrome_active_processes {
              pid: 10
              pid: 100
              pid: 1000
            }
          }
        }
        packet {
          timestamp: 1
          trusted_packet_sequence_id: 2
          track_descriptor {
            uuid: 1
            process {
              pid: 10
            }
            parent_uuid: 0
          }
        }
        packet {
          timestamp: 1000000000
          trusted_packet_sequence_id: 3
          track_descriptor {
            uuid: 2
            process {
              pid: 100
            }
            parent_uuid: 0
          }
        }
        """),
        query="""
        SELECT arg_set_id, key, int_value
        FROM
          slice
        JOIN
          args
          USING(arg_set_id)
        ORDER BY arg_set_id, key;
        """,
        out=Csv("""
        "arg_set_id","key","int_value"
        2,"chrome_active_processes.pid[0]",10
        2,"chrome_active_processes.pid[1]",100
        2,"chrome_active_processes.pid[2]",1000
        """))

  def test_chrome_missing_processes_2(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 1
          incremental_state_cleared: true
          trusted_packet_sequence_id: 1
          track_event {
            type: TYPE_INSTANT
            name: "ActiveProcesses"
            [perfetto.protos.ChromeTrackEvent.active_processes]: {
              pid: 10
              pid: 100
              pid: 1000
            }
          }
        }
        packet {
          timestamp: 1
          trusted_packet_sequence_id: 2
          track_descriptor {
            uuid: 1
            process {
              pid: 10
            }
            parent_uuid: 0
          }
        }
        packet {
          timestamp: 1000000000
          trusted_packet_sequence_id: 3
          track_descriptor {
            uuid: 2
            process {
              pid: 100
            }
            parent_uuid: 0
          }
        }
        """),
        query="""
        SELECT upid, pid, reliable_from
        FROM
          experimental_missing_chrome_processes
        JOIN
          process
          USING(upid)
        ORDER BY upid;
        """,
        out=Csv("""
        "upid","pid","reliable_from"
        2,100,1000000000
        3,1000,"[NULL]"
        """))

  def test_chrome_missing_processes_extension_args(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 1
          incremental_state_cleared: true
          trusted_packet_sequence_id: 1
          track_event {
            type: TYPE_INSTANT
            name: "ActiveProcesses"
            [perfetto.protos.ChromeTrackEvent.active_processes]: {
              pid: 10
              pid: 100
              pid: 1000
            }
          }
        }
        packet {
          timestamp: 1
          trusted_packet_sequence_id: 2
          track_descriptor {
            uuid: 1
            process {
              pid: 10
            }
            parent_uuid: 0
          }
        }
        packet {
          timestamp: 1000000000
          trusted_packet_sequence_id: 3
          track_descriptor {
            uuid: 2
            process {
              pid: 100
            }
            parent_uuid: 0
          }
        }
        """),
        query="""
        SELECT arg_set_id, key, int_value
        FROM
          slice
        JOIN
          args
          USING(arg_set_id)
        ORDER BY arg_set_id, key;
        """,
        out=Csv("""
        "arg_set_id","key","int_value"
        2,"active_processes.pid[0]",10
        2,"active_processes.pid[1]",100
        2,"active_processes.pid[2]",1000
        """))
