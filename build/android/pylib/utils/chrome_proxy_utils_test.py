#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for chrome_proxy_utils."""

#pylint: disable=protected-access

import os
import unittest

from pylib.utils import chrome_proxy_utils

from devil.android import forwarder
from devil.android import device_utils
from devil.android.sdk import adb_wrapper
from py_utils import ts_proxy_server
from py_utils import webpagereplay_go_server

import mock  # pylint: disable=import-error


def _DeviceUtilsMock(test_serial, is_ready=True):
  """Returns a DeviceUtils instance based on given serial."""
  adb = mock.Mock(spec=adb_wrapper.AdbWrapper)
  adb.__str__ = mock.Mock(return_value=test_serial)
  adb.GetDeviceSerial.return_value = test_serial
  adb.is_ready = is_ready
  return device_utils.DeviceUtils(adb)


class ChromeProxySessionTest(unittest.TestCase):
  """Unittest for ChromeProxySession."""

  #pylint: disable=no-self-use

  @mock.patch.object(forwarder.Forwarder, 'Map')
  @mock.patch.object(chrome_proxy_utils.WPRServer, 'StartServer')
  @mock.patch.object(ts_proxy_server.TsProxyServer, 'StartServer')
  @mock.patch.object(ts_proxy_server.TsProxyServer, 'UpdateOutboundPorts')
  @mock.patch.object(ts_proxy_server.TsProxyServer, 'UpdateTrafficSettings')
  @mock.patch('py_utils.ts_proxy_server.TsProxyServer.port',
              new_callable=mock.PropertyMock)
  def test_Start(self, port_mock, traffic_setting_mock, outboundport_mock,
                 start_server_mock, wpr_mock, forwarder_mock):
    chrome_proxy = chrome_proxy_utils.ChromeProxySession(4)
    chrome_proxy._wpr_server._host_http_port = 1
    chrome_proxy._wpr_server._host_https_port = 2
    port_mock.return_value = 3
    device = _DeviceUtilsMock('01234')
    chrome_proxy.Start(device, 'abc')

    forwarder_mock.assert_called_once_with([(4, 3)], device)
    wpr_mock.assert_called_once_with('abc')
    start_server_mock.assert_called_once()
    outboundport_mock.assert_called_once_with(http_port=1, https_port=2)
    traffic_setting_mock.assert_called_once_with(download_bandwidth_kbps=72000,
                                                 round_trip_latency_ms=100,
                                                 upload_bandwidth_kbps=72000)
    port_mock.assert_called_once()

  @mock.patch.object(forwarder.Forwarder, 'UnmapDevicePort')
  @mock.patch.object(chrome_proxy_utils.WPRServer, 'StopServer')
  @mock.patch.object(ts_proxy_server.TsProxyServer, 'StopServer')
  def test_Stop(self, ts_proxy_mock, wpr_mock, forwarder_mock):
    chrome_proxy = chrome_proxy_utils.ChromeProxySession(4)
    device = _DeviceUtilsMock('01234')
    chrome_proxy.wpr_record_mode = True
    chrome_proxy._wpr_server._archive_path = 'abc'
    chrome_proxy.Stop(device)

    forwarder_mock.assert_called_once_with(4, device)
    wpr_mock.assert_called_once_with()
    ts_proxy_mock.assert_called_once_with()

  #pylint: enable=no-self-use

  @mock.patch.object(forwarder.Forwarder, 'UnmapDevicePort')
  @mock.patch.object(webpagereplay_go_server.ReplayServer, 'StopServer')
  @mock.patch.object(ts_proxy_server.TsProxyServer, 'StopServer')
  def test_Stop_WithProperties(self, ts_proxy_mock, wpr_mock, forwarder_mock):
    chrome_proxy = chrome_proxy_utils.ChromeProxySession(4)
    chrome_proxy._wpr_server._server = webpagereplay_go_server.ReplayServer(
        os.path.abspath(__file__), chrome_proxy_utils.PROXY_HOST_IP, 0, 0, [])
    chrome_proxy._wpr_server._archive_path = os.path.abspath(__file__)
    device = _DeviceUtilsMock('01234')
    chrome_proxy.wpr_record_mode = True
    chrome_proxy.Stop(device)

    forwarder_mock.assert_called_once_with(4, device)
    wpr_mock.assert_called_once_with()
    ts_proxy_mock.assert_called_once_with()
    self.assertFalse(chrome_proxy.wpr_replay_mode)
    self.assertEqual(chrome_proxy.wpr_archive_path, os.path.abspath(__file__))

  def test_SetWPRRecordMode(self):
    chrome_proxy = chrome_proxy_utils.ChromeProxySession(4)
    chrome_proxy.wpr_record_mode = True
    self.assertTrue(chrome_proxy._wpr_server.record_mode)
    self.assertTrue(chrome_proxy.wpr_record_mode)
    self.assertFalse(chrome_proxy.wpr_replay_mode)

    chrome_proxy.wpr_record_mode = False
    self.assertFalse(chrome_proxy._wpr_server.record_mode)
    self.assertFalse(chrome_proxy.wpr_record_mode)
    self.assertTrue(chrome_proxy.wpr_replay_mode)

  def test_SetWPRArchivePath(self):
    chrome_proxy = chrome_proxy_utils.ChromeProxySession(4)
    chrome_proxy._wpr_server._archive_path = 'abc'
    self.assertEqual(chrome_proxy.wpr_archive_path, 'abc')

  def test_UseDefaultDeviceProxyPort(self):
    chrome_proxy = chrome_proxy_utils.ChromeProxySession()
    expected_flags = [
        '--ignore-certificate-errors-spki-list='
        'PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=',
        '--proxy-server=socks5://localhost:1080'
    ]
    self.assertEqual(chrome_proxy.device_proxy_port, 1080)
    self.assertListEqual(chrome_proxy.GetFlags(), expected_flags)

  def test_UseNewDeviceProxyPort(self):
    chrome_proxy = chrome_proxy_utils.ChromeProxySession(1)
    expected_flags = [
        '--ignore-certificate-errors-spki-list='
        'PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=',
        '--proxy-server=socks5://localhost:1'
    ]
    self.assertEqual(chrome_proxy.device_proxy_port, 1)
    self.assertListEqual(chrome_proxy.GetFlags(), expected_flags)


class WPRServerTest(unittest.TestCase):
  @mock.patch('py_utils.webpagereplay_go_server.ReplayServer')
  def test_StartSever_fresh_replaymode(self, wpr_mock):
    wpr_server = chrome_proxy_utils.WPRServer()
    wpr_archive_file = os.path.abspath(__file__)
    wpr_server.StartServer(wpr_archive_file)

    wpr_mock.assert_called_once_with(wpr_archive_file,
                                     '127.0.0.1',
                                     http_port=0,
                                     https_port=0,
                                     replay_options=[])

    self.assertEqual(wpr_server._archive_path, wpr_archive_file)
    self.assertTrue(wpr_server._server)

  @mock.patch('py_utils.webpagereplay_go_server.ReplayServer')
  def test_StartSever_fresh_recordmode(self, wpr_mock):
    wpr_server = chrome_proxy_utils.WPRServer()
    wpr_server.record_mode = True
    wpr_server.StartServer(os.path.abspath(__file__))
    wpr_archive_file = os.path.abspath(__file__)

    wpr_mock.assert_called_once_with(wpr_archive_file,
                                     '127.0.0.1',
                                     http_port=0,
                                     https_port=0,
                                     replay_options=['--record'])

    self.assertEqual(wpr_server._archive_path, os.path.abspath(__file__))
    self.assertTrue(wpr_server._server)

  #pylint: disable=no-self-use

  @mock.patch.object(webpagereplay_go_server.ReplayServer, 'StartServer')
  def test_StartSever_recordmode(self, start_server_mock):
    wpr_server = chrome_proxy_utils.WPRServer()
    start_server_mock.return_value = {'http': 1, 'https': 2}
    wpr_server.StartServer(os.path.abspath(__file__))

    start_server_mock.assert_called_once()
    self.assertEqual(wpr_server._host_http_port, 1)
    self.assertEqual(wpr_server._host_https_port, 2)
    self.assertEqual(wpr_server._archive_path, os.path.abspath(__file__))
    self.assertTrue(wpr_server._server)

  @mock.patch.object(webpagereplay_go_server.ReplayServer, 'StartServer')
  def test_StartSever_reuseServer(self, start_server_mock):
    wpr_server = chrome_proxy_utils.WPRServer()
    wpr_server._server = webpagereplay_go_server.ReplayServer(
        os.path.abspath(__file__),
        chrome_proxy_utils.PROXY_HOST_IP,
        http_port=0,
        https_port=0,
        replay_options=[])
    wpr_server._archive_path = os.path.abspath(__file__)
    wpr_server.StartServer(os.path.abspath(__file__))
    start_server_mock.assert_not_called()

  @mock.patch.object(webpagereplay_go_server.ReplayServer, 'StartServer')
  @mock.patch.object(webpagereplay_go_server.ReplayServer, 'StopServer')
  def test_StartSever_notReuseServer(self, stop_server_mock, start_server_mock):
    wpr_server = chrome_proxy_utils.WPRServer()
    wpr_server._server = webpagereplay_go_server.ReplayServer(
        os.path.abspath(__file__),
        chrome_proxy_utils.PROXY_HOST_IP,
        http_port=0,
        https_port=0,
        replay_options=[])
    wpr_server._archive_path = ''
    wpr_server.StartServer(os.path.abspath(__file__))
    start_server_mock.assert_called_once()
    stop_server_mock.assert_called_once()

  #pylint: enable=no-self-use

  @mock.patch.object(webpagereplay_go_server.ReplayServer, 'StopServer')
  def test_StopServer(self, stop_server_mock):
    wpr_server = chrome_proxy_utils.WPRServer()
    wpr_server._server = webpagereplay_go_server.ReplayServer(
        os.path.abspath(__file__),
        chrome_proxy_utils.PROXY_HOST_IP,
        http_port=0,
        https_port=0,
        replay_options=[])
    wpr_server.StopServer()
    stop_server_mock.assert_called_once()
    self.assertFalse(wpr_server._server)
    self.assertFalse(wpr_server._archive_path)
    self.assertFalse(wpr_server.http_port)
    self.assertFalse(wpr_server.https_port)

  def test_SetWPRRecordMode(self):
    wpr_server = chrome_proxy_utils.WPRServer()
    wpr_server.record_mode = True
    self.assertTrue(wpr_server.record_mode)
    wpr_server.record_mode = False
    self.assertFalse(wpr_server.record_mode)


if __name__ == '__main__':
  unittest.main(verbosity=2)
