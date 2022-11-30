/* Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * JavaScript hooks into the native WebUI handler.
 */
const WebUI = {
  getLogMessages: function() {
    chrome.send('getLogMessages');
  },

  clearLogs: function() {
    chrome.send('clearLogBuffer');
  },

  forceDeviceSync: function() {
    chrome.send('forceDeviceSync');
  },

  forceEnrollment: function() {
    chrome.send('forceEnrollment');
  },

  generateChallenge: function() {
    chrome.send('generateChallenge');
  },

  getAssertion: function() {
    chrome.send('getAssertion');
  },

  getLocalState: function() {
    chrome.send('getLocalState');
  },

  onWebContentsInitialized: function() {
    chrome.send('onWebContentsInitialized');
  },
};

