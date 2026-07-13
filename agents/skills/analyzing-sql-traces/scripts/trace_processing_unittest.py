# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for trace analyzer and comparator tools and library."""

import os
import sys
import unittest
from unittest.mock import MagicMock, patch
import pandas as pd

# Add current directory to path to allow direct imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import trace_analyzer_lib
import trace_comparator


class TestTraceProcessing(unittest.TestCase):

    def test_format_path(self):
        path = ('Renderer (google.com)', 'CrRendererMain', 'RootSlice',
                'ChildSlice')
        formatted = trace_comparator.format_path(path)
        self.assertEqual(
            formatted,
            "[Renderer (google.com)::CrRendererMain] RootSlice > ChildSlice")

    @patch('trace_analyzer_lib.TraceProcessor')
    def test_get_process_labels_webui_toolbar(self, mock_tp_class):
        mock_tp = mock_tp_class.return_value

        # Mock process query
        df_proc = pd.DataFrame([{
            'upid': 1,
            'name': 'Browser'
        }, {
            'upid': 2,
            'name': 'Renderer'
        }, {
            'upid': 3,
            'name': 'GPU Process'
        }])

        # Mock args query
        df_args = pd.DataFrame([{
            'upid':
            2,
            'display_value':
            'chrome://webui-toolbar.top-chrome/'
        }, {
            'upid':
            2,
            'display_value':
            'chrome://resources/images/icon_clear.svg'
        }])

        def mock_query(sql):
            mock_result = MagicMock()
            if "FROM process" in sql:
                mock_result.as_pandas_dataframe.return_value = df_proc
            elif "FROM args" in sql:
                mock_result.as_pandas_dataframe.return_value = df_args
            else:
                mock_result.as_pandas_dataframe.return_value = pd.DataFrame()
            return mock_result

        mock_tp.query.side_effect = mock_query

        session = trace_analyzer_lib.TraceSession("dummy.pb")
        labels = session.get_process_labels()
        self.assertEqual(labels[1], 'Browser')
        self.assertEqual(labels[2], 'Renderer (WebUI Toolbar)')
        self.assertEqual(labels[3], 'GPU Process')

    @patch('trace_analyzer_lib.TraceProcessor')
    def test_get_process_labels_google_web(self, mock_tp_class):
        mock_tp = mock_tp_class.return_value

        # Mock process query
        df_proc = pd.DataFrame([{'upid': 1, 'name': 'Renderer'}])

        # Mock args query
        df_args = pd.DataFrame([{
            'upid':
            1,
            'display_value':
            'https://www.google.com/async/hpba'
        }, {
            'upid':
            1,
            'display_value':
            'https://www.google.com/xjs/_/ss/k=xjs.hd.FSyKelbsZkg...'
        }])

        def mock_query(sql):
            mock_result = MagicMock()
            if "FROM process" in sql:
                mock_result.as_pandas_dataframe.return_value = df_proc
            elif "FROM args" in sql:
                mock_result.as_pandas_dataframe.return_value = df_args
            return mock_result

        mock_tp.query.side_effect = mock_query

        session = trace_analyzer_lib.TraceSession("dummy.pb")
        labels = session.get_process_labels()
        self.assertEqual(labels[1], 'Renderer (google.com)')

    @patch('trace_analyzer_lib.TraceProcessor')
    def test_get_process_labels_other_webui(self, mock_tp_class):
        mock_tp = mock_tp_class.return_value

        # Mock process query
        df_proc = pd.DataFrame([{'upid': 1, 'name': 'Renderer'}])

        # Mock args query
        df_args = pd.DataFrame([{
            'upid': 1,
            'display_value': 'chrome://new-tab-page/'
        }])

        def mock_query(sql):
            mock_result = MagicMock()
            if "FROM process" in sql:
                mock_result.as_pandas_dataframe.return_value = df_proc
            elif "FROM args" in sql:
                mock_result.as_pandas_dataframe.return_value = df_args
            return mock_result

        mock_tp.query.side_effect = mock_query

        session = trace_analyzer_lib.TraceSession("dummy.pb")
        labels = session.get_process_labels()
        self.assertEqual(labels[1], 'Renderer (chrome://new-tab-page)')


class TestTraceAnalyzerLib(unittest.TestCase):

    def test_slice_node_properties(self):
        node = trace_analyzer_lib.SliceNode(slice_id=1,
                                            name="Test",
                                            dur=1500000.0,
                                            ts=1000,
                                            depth=0)
        self.assertEqual(node.dur_ms, 1.5)
        self.assertEqual(node.self_ms, 1.5)

    def test_get_slice_signature(self):
        # Test no args
        sig = trace_analyzer_lib.get_slice_signature_raw("SimpleSlice", {})
        self.assertEqual(sig, "SimpleSlice")

        # Test RunTask formatting
        args_run_task = {
            "task.posted_from.file_name": "base/task.cc",
            "task.posted_from.function_name": "DoWork"
        }
        sig = trace_analyzer_lib.get_slice_signature_raw(
            "ThreadControllerImpl::RunTask", args_run_task)
        self.assertEqual(
            sig, "ThreadControllerImpl::RunTask ("
            "file_name: base/task.cc, function_name: DoWork)")

        # Test RunTask formatting with line number
        args_run_task_line = {
            "task.posted_from.file_name": "base/task.cc",
            "task.posted_from.function_name": "DoWork",
            "task.posted_from.line_number": "123"
        }
        sig = trace_analyzer_lib.get_slice_signature_raw(
            "ThreadControllerImpl::RunTask", args_run_task_line)
        self.assertEqual(
            sig, "ThreadControllerImpl::RunTask ("
            "file_name: base/task.cc, function_name: DoWork, line_number: 123)"
        )

        # Test IsSuitableForUrlInfo
        args_url_info = {
            "url_info.url": "https://google.com/",
            "site_instance.site_instance_id": "42"
        }
        sig = trace_analyzer_lib.get_slice_signature_raw(
            "IsSuitableForUrlInfo", args_url_info)
        self.assertEqual(
            sig, "IsSuitableForUrlInfo ("
            "url: https://google.com/, site_instance_id: 42)")

        # Test custom JSON expand config
        custom_config = {"CustomSlice": ["my_arg", "actual_key"]}
        args_custom = {"my_arg": "value1", "actual_key": "value2"}
        sig = trace_analyzer_lib.get_slice_signature_raw(
            "CustomSlice", args_custom, custom_config)
        self.assertEqual(sig,
                         "CustomSlice (my_arg: value1, actual_key: value2)")

        # Test user config merging with default config
        merge_config = {"DetermineSiteInstanceForURL": ["custom_arg"]}
        args_merge = {
            "url_info.url": "https://google.com/",
            "custom_arg": "custom_val"
        }
        sig = trace_analyzer_lib.get_slice_signature_raw(
            "DetermineSiteInstanceForURL", args_merge, merge_config)
        self.assertEqual(
            sig, "DetermineSiteInstanceForURL ("
            "url: https://google.com/, custom_arg: custom_val)")

    @patch('trace_analyzer_lib.TraceSession')
    def test_extract_slice_hierarchies(self, mock_session_class):
        session = mock_session_class.return_value

        # Mock target slice query DataFrame
        df_target = pd.DataFrame([{
            'id': 1,
            'name': 'TargetSlice',
            'dur': 10000000.0,
            'ts': 1000
        }])

        # Mock descendants query DataFrame
        df_desc = pd.DataFrame([{
            'id': 2,
            'name': 'ChildSlice',
            'dur': 4000000.0,
            'ts': 2000,
            'depth': 1,
            'parent_id': 1
        }])

        def mock_query(sql):
            if "descendant_slice" in sql:
                return df_desc
            else:
                return df_target

        session.query.side_effect = mock_query

        root_nodes = trace_analyzer_lib.extract_slice_hierarchies(
            session, "TargetSlice")
        self.assertEqual(len(root_nodes), 1)

        root = root_nodes[0]
        self.assertEqual(root.name, "TargetSlice")
        self.assertEqual(root.dur_ms, 10.0)
        self.assertEqual(root.self_ms, 6.0)  # 10.0 - 4.0

        self.assertEqual(len(root.children), 1)
        child = root.children[0]
        self.assertEqual(child.name, "ChildSlice")
        self.assertEqual(child.dur_ms, 4.0)
        self.assertEqual(child.self_ms, 4.0)

    def test_aggregate_trees(self):
        # Tree 1: Root(10ms, self 6ms) -> Child1(4ms, self 4ms)
        root1 = trace_analyzer_lib.SliceNode(1, "Root", 10000000.0, 1000)
        child1 = trace_analyzer_lib.SliceNode(2, "Child1", 4000000.0, 2000, 1,
                                              1)
        root1.children = [child1]
        root1.self_time = 6000000.0

        # Tree 2: Root(20ms, self 12ms) -> Child1(8ms, self 8ms)
        root2 = trace_analyzer_lib.SliceNode(3, "Root", 20000000.0, 5000)
        child2 = trace_analyzer_lib.SliceNode(4, "Child1", 8000000.0, 6000, 1,
                                              3)
        root2.children = [child2]
        root2.self_time = 12000000.0

        agg_root = trace_analyzer_lib.aggregate_trees([root1, root2])
        self.assertIsNotNone(agg_root)
        self.assertEqual(agg_root.name, "Root")
        self.assertEqual(agg_root.dur_ms, 30.0)  # 10 + 20
        self.assertEqual(agg_root.self_ms, 18.0)  # 6 + 12
        self.assertEqual(agg_root.count, 2)

        self.assertIn("Child1", agg_root.children)
        agg_child = agg_root.children["Child1"]
        self.assertEqual(agg_child.dur_ms, 12.0)  # 4 + 8
        self.assertEqual(agg_child.self_ms, 12.0)  # 4 + 8
        self.assertEqual(agg_child.count, 2)

    def test_aggregate_trees_with_args(self):
        # Tree 1: Root -> IsSuitableForUrlInfo(
        #     url: google.com, site_instance: 1)
        root1 = trace_analyzer_lib.SliceNode(1, "Root", 10000000.0, 1000)
        child1 = trace_analyzer_lib.SliceNode(2, "IsSuitableForUrlInfo",
                                              4000000.0, 2000, 1, 1)
        child1.args = {
            "url_info.url": "https://google.com",
            "site_instance_id": "1"
        }
        root1.children = [child1]
        root1.self_time = 6000000.0

        # Tree 2: Root -> IsSuitableForUrlInfo(
        #     url: youtube.com, site_instance: 2)
        root2 = trace_analyzer_lib.SliceNode(3, "Root", 20000000.0, 5000)
        child2 = trace_analyzer_lib.SliceNode(4, "IsSuitableForUrlInfo",
                                              8000000.0, 6000, 1, 3)
        child2.args = {
            "url_info.url": "https://youtube.com",
            "site_instance_id": "2"
        }
        root2.children = [child2]
        root2.self_time = 12000000.0

        agg_root = trace_analyzer_lib.aggregate_trees([root1, root2])
        self.assertIsNotNone(agg_root)
        self.assertEqual(agg_root.name, "Root")
        self.assertEqual(agg_root.dur_ms, 30.0)

        # They should NOT be merged because they have different arguments!
        sig1 = ("IsSuitableForUrlInfo (url: https://google.com, "
                "site_instance_id: 1)")
        sig2 = ("IsSuitableForUrlInfo (url: https://youtube.com, "
                "site_instance_id: 2)")
        self.assertIn(sig1, agg_root.children)
        self.assertIn(sig2, agg_root.children)
        self.assertEqual(agg_root.children[sig1].count, 1)
        self.assertEqual(agg_root.children[sig2].count, 1)

    def test_get_flat_metrics(self):
        # Tree: Root(10ms, self 6ms) -> Child1(4ms, self 4ms)
        root = trace_analyzer_lib.SliceNode(1, "Root", 10000000.0, 1000)
        child = trace_analyzer_lib.SliceNode(2, "Child1", 4000000.0, 2000, 1,
                                             1)
        root.children = [child]
        root.self_time = 6000000.0

        metrics = trace_analyzer_lib.get_flat_metrics([root])
        self.assertIn("Root", metrics)
        self.assertEqual(metrics["Root"]["count"], 1)
        self.assertEqual(metrics["Root"]["dur_ms"], 10.0)
        self.assertEqual(metrics["Root"]["self_ms"], 6.0)

        self.assertIn("Child1", metrics)
        self.assertEqual(metrics["Child1"]["count"], 1)
        self.assertEqual(metrics["Child1"]["dur_ms"], 4.0)
        self.assertEqual(metrics["Child1"]["self_ms"], 4.0)

    @patch('trace_analyzer_lib.TraceProcessor')
    def test_fetch_windowed_slices(self, mock_tp_class):
        mock_tp = mock_tp_class.return_value
        df_metric = pd.DataFrame([{'ts': 1000, 'dur': 5000}])
        df_slices = pd.DataFrame([{
            'id': 10,
            'name': 'SliceA',
            'ts': 1200,
            'dur': 2000,
            'parent_id': None,
            'thread_name': 'Thread1',
            'process_name': 'Browser',
            'upid': 1
        }])
        df_proc = pd.DataFrame([{'upid': 1, 'name': 'Browser'}])

        def mock_query(sql):
            mock_result = MagicMock()
            if "s.name = 'Metric'" in sql:
                mock_result.as_pandas_dataframe.return_value = df_metric
            elif "FROM process" in sql:
                mock_result.as_pandas_dataframe.return_value = df_proc
            elif "FROM slice s" in sql:
                mock_result.as_pandas_dataframe.return_value = df_slices
            else:
                mock_result.as_pandas_dataframe.return_value = pd.DataFrame()
            return mock_result

        mock_tp.query.side_effect = mock_query

        session = trace_analyzer_lib.TraceSession("dummy.pb")
        slices, m_dur = trace_analyzer_lib.fetch_windowed_slices(
            session, "Metric")

        self.assertEqual(m_dur, 0.005)  # 5000 ns / 1e6
        self.assertIn(10, slices)
        s = slices[10]
        self.assertEqual(s['name'], 'SliceA')
        self.assertEqual(s['o_dur'], 2000)  # completely overlapping
        self.assertEqual(s['self_o_dur'], 2000)

    @patch('trace_analyzer_lib.TraceProcessor')
    def test_fetch_windowed_slices_with_arg_filter(self, mock_tp_class):
        mock_tp = mock_tp_class.return_value
        df_metric = pd.DataFrame([{'ts': 1000, 'dur': 5000}])
        df_slices = pd.DataFrame([{
            'id': 10,
            'name': 'SliceA',
            'ts': 1200,
            'dur': 2000,
            'parent_id': None,
            'thread_name': 'Thread1',
            'process_name': 'Browser',
            'upid': 1
        }])
        df_proc = pd.DataFrame([{'upid': 1, 'name': 'Browser'}])

        queries_run = []

        def mock_query(sql):
            queries_run.append(sql)
            mock_result = MagicMock()
            if "WHERE s.name = 'Metric'" in sql:
                mock_result.as_pandas_dataframe.return_value = df_metric
            elif "FROM process" in sql:
                mock_result.as_pandas_dataframe.return_value = df_proc
            elif "FROM slice s" in sql:
                mock_result.as_pandas_dataframe.return_value = df_slices
            else:
                mock_result.as_pandas_dataframe.return_value = pd.DataFrame()
            return mock_result

        mock_tp.query.side_effect = mock_query

        session = trace_analyzer_lib.TraceSession("dummy.pb")
        _, m_dur = trace_analyzer_lib.fetch_windowed_slices(session,
                                                            "Metric",
                                                            arg_key="mykey",
                                                            arg_value="myval")

        self.assertEqual(m_dur, 0.005)
        # Verify that the query joining args table was executed
        metric_query = [
            q for q in queries_run if "WHERE s.name = 'Metric'" in q
        ]
        self.assertEqual(len(metric_query), 1)
        self.assertIn("a.key = 'mykey'", metric_query[0])
        self.assertIn("a.string_value LIKE 'myval'", metric_query[0])

    @patch('trace_analyzer_lib.TraceProcessor')
    def test_fetch_windowed_slices_with_boundary_filter(self, mock_tp_class):
        mock_tp = mock_tp_class.return_value
        df_metric = pd.DataFrame([{'ts': 1000, 'dur': 5000}])
        df_slices = pd.DataFrame([{
            'id': 10,
            'name': 'SliceA',
            'ts': 1200,
            'dur': 2000,
            'parent_id': None,
            'thread_name': 'Thread1',
            'process_name': 'Browser',
            'upid': 1
        }])
        df_proc = pd.DataFrame([{'upid': 1, 'name': 'Browser'}])

        queries_run = []

        def mock_query(sql):
            queries_run.append(sql)
            mock_result = MagicMock()
            if "WHERE s.name = 'Metric'" in sql:
                mock_result.as_pandas_dataframe.return_value = df_metric
            elif "FROM process" in sql:
                mock_result.as_pandas_dataframe.return_value = df_proc
            elif "FROM slice s" in sql:
                mock_result.as_pandas_dataframe.return_value = df_slices
            else:
                mock_result.as_pandas_dataframe.return_value = pd.DataFrame()
            return mock_result

        mock_tp.query.side_effect = mock_query

        session = trace_analyzer_lib.TraceSession("dummy.pb")
        _, m_dur = trace_analyzer_lib.fetch_windowed_slices(
            session,
            "Metric",
            boundary_target="BoundarySlice",
            boundary_arg_key="boundkey",
            boundary_arg_value="boundval")

        self.assertEqual(m_dur, 0.005)
        # Verify that the query contains boundary clause
        metric_query = [
            q for q in queries_run if "WHERE s.name = 'Metric'" in q
        ]
        self.assertEqual(len(metric_query), 1)
        self.assertIn("EXISTS", metric_query[0])
        self.assertIn("p.name = 'BoundarySlice'", metric_query[0])
        self.assertIn("JOIN args pa ON p.arg_set_id = pa.arg_set_id",
                      metric_query[0])
        self.assertIn("pa.key = 'boundkey'", metric_query[0])
        self.assertIn("pa.string_value LIKE 'boundval'", metric_query[0])

    @patch('trace_analyzer_lib.TraceProcessor')
    def test_fetch_windowed_slices_with_boundary_filter_no_args(
            self, mock_tp_class):
        mock_tp = mock_tp_class.return_value
        df_metric = pd.DataFrame([{'ts': 1000, 'dur': 5000}])
        df_slices = pd.DataFrame([{
            'id': 10,
            'name': 'SliceA',
            'ts': 1200,
            'dur': 2000,
            'parent_id': None,
            'thread_name': 'Thread1',
            'process_name': 'Browser',
            'upid': 1
        }])
        df_proc = pd.DataFrame([{'upid': 1, 'name': 'Browser'}])

        queries_run = []

        def mock_query(sql):
            queries_run.append(sql)
            mock_result = MagicMock()
            if "WHERE s.name = 'Metric'" in sql:
                mock_result.as_pandas_dataframe.return_value = df_metric
            elif "FROM process" in sql:
                mock_result.as_pandas_dataframe.return_value = df_proc
            elif "FROM slice s" in sql:
                mock_result.as_pandas_dataframe.return_value = df_slices
            else:
                mock_result.as_pandas_dataframe.return_value = pd.DataFrame()
            return mock_result

        mock_tp.query.side_effect = mock_query

        session = trace_analyzer_lib.TraceSession("dummy.pb")
        _, m_dur = trace_analyzer_lib.fetch_windowed_slices(
            session, "Metric", boundary_target="BoundarySlice")

        self.assertEqual(m_dur, 0.005)
        # Verify that the query contains boundary clause but no args join
        metric_query = [
            q for q in queries_run if "WHERE s.name = 'Metric'" in q
        ]
        self.assertEqual(len(metric_query), 1)
        self.assertIn("EXISTS", metric_query[0])
        self.assertIn("p.name = 'BoundarySlice'", metric_query[0])
        self.assertNotIn("JOIN args pa", metric_query[0])

    def test_build_paths_from_slices(self):
        slices = {
            1: {
                'id': 1,
                'name': 'Root',
                'parent_id': None,
                'thread_name': 'ThreadMain',
                'process_name': 'Browser',
                'o_dur': 5000000.0,
                'self_o_dur': 2000000.0,
                'children': [2]
            },
            2: {
                'id': 2,
                'name': 'Child',
                'parent_id': 1,
                'thread_name': 'ThreadMain',
                'process_name': 'Browser',
                'o_dur': 3000000.0,
                'self_o_dur': 3000000.0,
                'children': []
            }
        }
        paths = trace_analyzer_lib.build_paths_from_slices(slices)
        root_path = ('Browser', 'ThreadMain', 'Root')
        child_path = ('Browser', 'ThreadMain', 'Root', 'Child')

        self.assertIn(root_path, paths)
        self.assertEqual(paths[root_path]['total'], 5.0)
        self.assertEqual(paths[root_path]['self'], 2.0)
        self.assertEqual(paths[root_path]['count'], 1)

        self.assertIn(child_path, paths)
        self.assertEqual(paths[child_path]['total'], 3.0)
        self.assertEqual(paths[child_path]['self'], 3.0)
        self.assertEqual(paths[child_path]['count'], 1)

    def test_build_paths_from_slices_multiple_occurrences(self):
        slices = {
            1: {
                'id': 1,
                'name': 'Root',
                'parent_id': None,
                'thread_name': 'ThreadMain',
                'process_name': 'Browser',
                'o_dur': 1000000.0,
                'self_o_dur': 1000000.0,
                'children': []
            },
            2: {
                'id': 2,
                'name': 'Root',
                'parent_id': None,
                'thread_name': 'ThreadMain',
                'process_name': 'Browser',
                'o_dur': 2000000.0,
                'self_o_dur': 2000000.0,
                'children': []
            }
        }
        paths = trace_analyzer_lib.build_paths_from_slices(slices)
        root_path = ('Browser', 'ThreadMain', 'Root')

        self.assertIn(root_path, paths)
        self.assertEqual(paths[root_path]['total'], 3.0)
        self.assertEqual(paths[root_path]['self'], 3.0)
        self.assertEqual(paths[root_path]['count'], 2)


    def test_build_tree_from_paths(self):
        path_durs = {
            ('Browser', 'ThreadMain', 'Root'): 5.0,
            ('Browser', 'ThreadMain', 'Root', 'Child'): 3.0
        }
        trees = trace_analyzer_lib.build_tree_from_paths(path_durs)
        track_key = ('Browser', 'ThreadMain')
        self.assertIn(track_key, trees)
        roots = trees[track_key]
        self.assertEqual(len(roots), 1)
        root = roots[0]
        self.assertEqual(root['name'], 'Root')
        self.assertEqual(root['dur'], 5.0)
        self.assertEqual(len(root['children']), 1)
        self.assertEqual(root['children'][0]['name'], 'Child')
        self.assertEqual(root['children'][0]['dur'], 3.0)


import trace_analyzer
import trace_comparator


class TestCliParsing(unittest.TestCase):

    @patch('trace_analyzer_lib.extract_slice_hierarchies')
    @patch('trace_analyzer_lib.TraceSession')
    @patch('os.path.exists', return_value=True)
    def test_analyzer_passes_args_to_lib(self, _mock_exists,
                                         mock_session_class, mock_extract):
        mock_node = MagicMock()
        mock_node.dur_ms = 10.0
        mock_node.dur = 10000000.0
        mock_node.self_time = 5000000.0
        mock_node.self_ms = 5.0
        mock_node.name = 'SliceA'
        mock_node.children = []
        mock_extract.return_value = [mock_node]

        test_args = [
            'trace_analyzer.py', '--traces', 'dummy.pb', '--target', 'SliceA',
            '--mode', 'descendants', '--arg-key', 'mykey', '--arg-value',
            'myval', '--format', 'text'
        ]
        with patch('sys.argv', test_args):
            trace_analyzer.main()

        mock_extract.assert_called_once_with(mock_session_class.return_value,
                                             'SliceA',
                                             arg_key='mykey',
                                             arg_value='myval',
                                             aggregate=False,
                                             boundary_target=None,
                                             boundary_arg_key=None,
                                             boundary_arg_value=None)


    @patch('trace_analyzer_lib.extract_slice_hierarchies')
    @patch('trace_analyzer_lib.TraceSession')
    @patch('trace_comparator.write_output')
    def test_comparator_passes_args_to_lib(self, _mock_write,
                                           mock_session_class, mock_extract):
        mock_node = MagicMock()
        mock_node.dur_ms = 10.0
        mock_node.dur = 10000000.0
        mock_node.self_time = 5000000.0
        mock_node.self_ms = 5.0
        mock_node.name = 'SliceA'
        mock_node.children = []
        mock_extract.return_value = [mock_node]

        test_args = [
            'trace_comparator.py', '--control', 'ctrl.pb', '--experiment',
            'exp.pb', '--target', 'SliceA', '--mode', 'descendants',
            '--arg-key', 'mykey', '--arg-value', 'myval', '--format',
            'markdown', '--output', 'out.md'
        ]
        with patch('sys.argv', test_args):
            trace_comparator.main()

        self.assertEqual(mock_extract.call_count, 2)
        mock_extract.assert_any_call(mock_session_class.return_value,
                                     'SliceA',
                                     arg_key='mykey',
                                     arg_value='myval',
                                     aggregate=False,
                                     boundary_target=None,
                                     boundary_arg_key=None,
                                     boundary_arg_value=None)

    @patch('trace_analyzer_lib.extract_slice_hierarchies')
    @patch('trace_analyzer_lib.TraceSession')
    @patch('trace_comparator.write_output')
    def test_comparator_passes_boundary_args_to_lib(self, _mock_write,
                                                    mock_session_class,
                                                    mock_extract):
        mock_node = MagicMock()
        mock_node.dur_ms = 10.0
        mock_node.dur = 10000000.0
        mock_node.self_time = 5000000.0
        mock_node.self_ms = 5.0
        mock_node.name = 'SliceA'
        mock_node.children = []
        mock_extract.return_value = [mock_node]

        test_args = [
            'trace_comparator.py', '--control', 'ctrl.pb', '--experiment',
            'exp.pb', '--target', 'SliceA', '--mode', 'descendants',
            '--arg-key', 'mykey', '--arg-value', 'myval', '--boundary-target',
            'BoundA', '--boundary-arg-key', 'bkey', '--boundary-arg-value',
            'bval', '--format', 'markdown', '--output', 'out.md'
        ]
        with patch('sys.argv', test_args):
            trace_comparator.main()

        self.assertEqual(mock_extract.call_count, 2)
        mock_extract.assert_any_call(mock_session_class.return_value,
                                     'SliceA',
                                     arg_key='mykey',
                                     arg_value='myval',
                                     aggregate=False,
                                     boundary_target='BoundA',
                                     boundary_arg_key='bkey',
                                     boundary_arg_value='bval')

    @patch('trace_analyzer_lib.fetch_windowed_slices')
    @patch('trace_analyzer_lib.TraceSession')
    @patch('trace_comparator.write_output')
    def test_comparator_passes_boundary_args_to_lib_window(
            self, _mock_write, mock_session_class, mock_fetch):
        mock_fetch.return_value = ({
            1: {
                'id': 1,
                'name': 'Root',
                'parent_id': None,
                'thread_name': 'ThreadMain',
                'process_name': 'Browser',
                'o_dur': 5000000.0,
                'self_o_dur': 5000000.0,
                'children': []
            }
        }, 5.0)

        test_args = [
            'trace_comparator.py', '--control', 'ctrl.pb', '--experiment',
            'exp.pb', '--target', 'MetricA', '--mode', 'window', '--arg-key',
            'mykey', '--arg-value', 'myval', '--boundary-target', 'BoundA',
            '--boundary-arg-key', 'bkey', '--boundary-arg-value', 'bval',
            '--format', 'markdown', '--output', 'out.md'
        ]
        with patch('sys.argv', test_args):
            trace_comparator.main()

        self.assertEqual(mock_fetch.call_count, 2)
        mock_fetch.assert_any_call(mock_session_class.return_value,
                                   'MetricA',
                                   threads=None,
                                   processes=None,
                                   arg_key='mykey',
                                   arg_value='myval',
                                   boundary_target='BoundA',
                                   boundary_arg_key='bkey',
                                   boundary_arg_value='bval')


if __name__ == '__main__':
    unittest.main()
