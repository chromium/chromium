// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require cr.js

/**
 * Bridge between the browser and the page.
 * In this file:
 *   * define interface to request data to the browser.
 */

cr.define('cr.quota', function() {
  'use strict';

  /**
   * Post requestInfo message to Browser.
   */
  function requestInfo() {
    chrome.send('requestInfo');
  }

  /**
   * Post triggerStoragePressure message to Browser.
   */
  function triggerStoragePressure(origin) {
    chrome.send('triggerStoragePressure', [origin]);
  }



  return {
    requestInfo: requestInfo,
    triggerStoragePressure: triggerStoragePressure,
  };
});
