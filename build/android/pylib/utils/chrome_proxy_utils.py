# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for setting up and tear down WPR and TsProxy service."""

from py_utils import ts_proxy_server
from py_utils import webpagereplay_go_server

from devil.android import forwarder

PROXY_HOST_IP = '127.0.0.1'
# From Catapult/WebPageReplay document.
IGNORE_CERT_ERROR_SPKI_LIST = 'PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I='
PROXY_SERVER = 'socks5://localhost'
DEFAULT_DEVICE_PORT = 1080
DEFAULT_ROUND_TRIP_LATENCY_MS = 100
DEFAULT_DOWNLOAD_BANDWIDTH_KBPS = 72000
DEFAULT_UPLOAD_BANDWIDTH_KBPS = 72000


class WPRServer(object):
  """Utils to set up a webpagereplay_go_server instance."""

  def __init__(self):
    self._archive_path = None
    self._host_http_port = 0
    self._host_https_port = 0
    self._record_mode = False
    self._server = None

  def StartServer(self, wpr_archive_path):
    """Starts a webpagereplay_go_server instance."""
    if wpr_archive_path == self._archive_path and self._server:
      # Reuse existing webpagereplay_go_server instance.
      return

    if self._server:
      self.StopServer()

    replay_options = []
    if self._record_mode:
      replay_options.append('--record')

    ports = {}
    if not self._server:
      self._server = webpagereplay_go_server.ReplayServer(
          wpr_archive_path,
          PROXY_HOST_IP,
          http_port=self._host_http_port,
          https_port=self._host_https_port,
          replay_options=replay_options)
      self._archive_path = wpr_archive_path
      ports = self._server.StartServer()

    self._host_http_port = ports['http']
    self._host_https_port = ports['https']

  def StopServer(self):
    """Stops the webpagereplay_go_server instance and resets archive."""
    self._server.StopServer()
    self._server = None
    self._host_http_port = 0
    self._host_https_port = 0

  @staticmethod
  def SetServerBinaryPath(go_binary_path):
    """Sets the go_binary_path for webpagereplay_go_server.ReplayServer."""
    webpagereplay_go_server.ReplayServer.SetGoBinaryPath(go_binary_path)

  @property
  def record_mode(self):
    return self._record_mode

  @record_mode.setter
  def record_mode(self, value):
    self._record_mode = value

  @property
  def http_port(self):
    return self._host_http_port

  @property
  def https_port(self):
    return self._host_https_port

  @property
  def archive_path(self):
    return self._archive_path


class ChromeProxySession(object):
  """Utils to help set up a Chrome Proxy."""

  def __init__(self, device_proxy_port=DEFAULT_DEVICE_PORT):
    self._device_proxy_port = device_proxy_port
    self._ts_proxy_server = ts_proxy_server.TsProxyServer(PROXY_HOST_IP)
    self._wpr_server = WPRServer()

  @property
  def wpr_record_mode(self):
    """Returns whether this proxy session was running in record mode."""
    return self._wpr_server.record_mode

  @wpr_record_mode.setter
  def wpr_record_mode(self, value):
    self._wpr_server.record_mode = value

  @property
  def wpr_replay_mode(self):
    """Returns whether this proxy session was running in replay mode."""
    return not self._wpr_server.record_mode

  @property
  def wpr_archive_path(self):
    """Returns the wpr archive file path used in this proxy session."""
    return self._wpr_server.archive_path

  @property
  def device_proxy_port(self):
    return self._device_proxy_port

  def GetFlags(self):
    """Gets the chrome command line flags to be needed by ChromeProxySession."""
    extra_flags = []

    extra_flags.append('--ignore-certificate-errors-spki-list=%s' %
                       IGNORE_CERT_ERROR_SPKI_LIST)
    extra_flags.append('--proxy-server=%s:%s' %
                       (PROXY_SERVER, self._device_proxy_port))
    return extra_flags

  @staticmethod
  def SetWPRServerBinary(go_binary_path):
    """Sets the WPR server go_binary_path."""
    WPRServer.SetServerBinaryPath(go_binary_path)

  def Start(self, device, wpr_archive_path):
    """Starts the wpr_server as well as the ts_proxy server and setups env.

    Args:
      device: A DeviceUtils instance.
      wpr_archive_path: A abs path to the wpr archive file.

    """
    self._wpr_server.StartServer(wpr_archive_path)
    self._ts_proxy_server.StartServer()

    # Maps device port to host port
    forwarder.Forwarder.Map(
        [(self._device_proxy_port, self._ts_proxy_server.port)], device)
    # Maps tsProxy port to wpr http/https ports
    self._ts_proxy_server.UpdateOutboundPorts(
        http_port=self._wpr_server.http_port,
        https_port=self._wpr_server.https_port)
    self._ts_proxy_server.UpdateTrafficSettings(
        round_trip_latency_ms=DEFAULT_ROUND_TRIP_LATENCY_MS,
        download_bandwidth_kbps=DEFAULT_DOWNLOAD_BANDWIDTH_KBPS,
        upload_bandwidth_kbps=DEFAULT_UPLOAD_BANDWIDTH_KBPS)

  def Stop(self, device):
    """Stops the wpr_server, and ts_proxy server and tears down env.

    Note that Stop does not reset wpr_record_mode, wpr_replay_mode,
    wpr_archive_path property.

    Args:
      device: A DeviceUtils instance.
    """
    self._wpr_server.StopServer()
    self._ts_proxy_server.StopServer()
    forwarder.Forwarder.UnmapDevicePort(self._device_proxy_port, device)
