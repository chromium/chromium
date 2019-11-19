// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require cr.js
// require cr/event_target.js
// require cr/util.js

/**
 * Bridge between the browser and the page.
 * In this file:
 *   * define EventTargets to receive message from the browser,
 *   * dispatch browser messages to EventTarget,
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
   * Callback entry point from Browser.
   * Messages are Dispatched as Event to:
   *   * onAvailableSpaceUpdated,
   *   * onGlobalInfoUpdated,
   *   * onPerHostInfoUpdated,
   *   * onPerOriginInfoUpdated,
   *   * onStatisticsUpdated.
   * @param {string} message Message label. Possible Values are:
   *   * 'AvailableSpaceUpdated',
   *   * 'GlobalInfoUpdated',
   *   * 'PerHostInfoUpdated',
   *   * 'PerOriginInfoUpdated',
   *   * 'StatisticsUpdated'.
   * @param {Object} detail Message specific additional data.
   */
  function messageHandler(message, detail) {
    let target = null;
    switch (message) {
      case 'AvailableSpaceUpdated':
        target = cr.quota.onAvailableSpaceUpdated;
        break;
      case 'GlobalInfoUpdated':
        target = cr.quota.onGlobalInfoUpdated;
        break;
      case 'PerHostInfoUpdated':
        target = cr.quota.onPerHostInfoUpdated;
        break;
      case 'PerOriginInfoUpdated':
        target = cr.quota.onPerOriginInfoUpdated;
        break;
      case 'StatisticsUpdated':
        target = cr.quota.onStatisticsUpdated;
        break;
      default:
        console.error('Unknown Message');
        break;
    }
    if (target) {
      const event = document.createEvent('CustomEvent');
      event.initCustomEvent('update', false, false, detail);
      target.dispatchEvent(event);
    }
  }

  return {
    onAvailableSpaceUpdated: new cr.EventTarget(),
    onGlobalInfoUpdated: new cr.EventTarget(),
    onPerHostInfoUpdated: new cr.EventTarget(),
    onPerOriginInfoUpdated: new cr.EventTarget(),
    onStatisticsUpdated: new cr.EventTarget(),

    requestInfo: requestInfo,
    messageHandler: messageHandler
  };
});
