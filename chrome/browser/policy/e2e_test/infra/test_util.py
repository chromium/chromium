# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains utility methods that can be used by python tests on Windows."""

import os
import time
import win32con
import win32gui
from selenium import webdriver
from selenium.webdriver.chrome.options import Options


def _window_enum_handler(hwnd, window_list):
  win_title = win32gui.GetWindowText(hwnd)
  if 'Google Chrome' in win_title or 'Chromium' in win_title:
    window_list.append(hwnd)


def _get_chrome_windows():
  """Gets the list of hwnd of Chrome windows."""
  window_list = []
  win32gui.EnumWindows(_window_enum_handler, window_list)
  return window_list


def shutdown_chrome():
  """Shutdown Chrome cleanly.

    Surprisingly there is no easy way in chromedriver to shutdown Chrome
    cleanly on Windows. So we have to use win32 API to do that: we find
    the chrome window first, then send WM_CLOSE message to it.
  """
  window_list = _get_chrome_windows()
  if not window_list:
    raise RuntimeError("Cannot find chrome windows")

  for win in window_list:
    win32gui.SendMessage(win, win32con.WM_CLOSE, 0, 0)

  # wait a little bit for chrome processes to end.
  # TODO: the right way is to wait until there are no chrome.exe processes.
  time.sleep(2)


def create_chrome_webdriver(chrome_options=None, incognito=False, prefs=None):
  """Configures and returns a Chrome WebDriver object."

  Args:
    chrome_options: The default ChromeOptions to use.
    incognito: Whether or not to launch Chrome in incognito mode.
    prefs: Profile preferences. None for defaults.
  """
  if chrome_options == None:
    chrome_options = Options()

  if incognito:
    chrome_options.add_argument('incognito')

  if prefs != None:
    chrome_options.add_experimental_option("prefs", prefs)

  os.environ["CHROME_LOG_FILE"] = r"c:\temp\chrome_log.txt"

  return webdriver.Chrome(
      executable_path=r"C:\temp\chromedriver.exe",
      service_args=["--verbose", r"--log-path=c:\temp\chromedriver.log"],
      chrome_options=chrome_options)
