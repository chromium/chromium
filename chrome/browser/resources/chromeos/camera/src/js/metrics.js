// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for metrics.
 */
cca.metrics = cca.metrics || {};

/**
 * Event builder for basic metrics.
 * @type {?analytics.EventBuilder}
 * @private
 */
cca.metrics.base_ = null;

var analytics = window['analytics'] || {};

/**
 * Fixes analytics.EventBuilder's dimension() method.
 * @param {number} i
 * @param {string} v
 * @return {!analytics.EventBuilder}
 */
analytics.EventBuilder.prototype.dimen = function(i, v) {
  return this.dimension({index: i, value: v});
};

/**
 * Promise for Google Analytics tracker.
 * @type {Promise<analytics.Tracker>}
 * @suppress {checkTypes}
 * @private
 */
cca.metrics.ga_ = (function() {
  const id = 'UA-134822711-1';
  const service = analytics.getService('chrome-camera-app');

  var getConfig = () =>
      new Promise((resolve) => service.getConfig().addCallback(resolve));
  var checkEnabled = () => {
    return new Promise((resolve) => {
      try {
        chrome.metricsPrivate.getIsCrashReportingEnabled(resolve);
      } catch (e) {
        resolve(false);  // Disable reporting by default.
      }
    });
  };
  var initBuilder = () => {
    return new Promise((resolve) => {
      try {
        chrome.chromeosInfoPrivate.get(['board'],
            (values) => resolve(values['board']));
      } catch (e) {
        resolve('');
      }
    }).then((board) => {
      var boardName = /^(x86-)?(\w*)/.exec(board)[0];
      var match = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/);
      var osVer = match ? match[1] : '';
      cca.metrics.base_ = analytics.EventBuilder.builder()
          .dimen(1, boardName).dimen(2, osVer);
    });
  };

  return Promise.all([getConfig(), checkEnabled(), initBuilder()])
      .then(([config, enabled]) => {
        config.setTrackingPermitted(enabled);
        return service.getTracker(id);
      });
})();

/**
 * Returns event builder for the metrics type: launch.
 * @param {boolean} ackMigrate Whether acknowledged to migrate during launch.
 * @return {!analytics.EventBuilder}
 * @private
 */
cca.metrics.launchType_ = function(ackMigrate) {
  return cca.metrics.base_.category('launch').action('start')
      .label(ackMigrate ? 'ack-migrate' : '');
};

/**
 * Types of intent result dimension.
 * @enum {string}
 */
cca.metrics.IntentResultType = {
  NOT_INTENT: '',
  CANCELED: 'canceled',
  CONFIRMED: 'confirmed',
};

/**
 * Returns event builder for the metrics type: capture.
 * @param {?string} facingMode Camera facing-mode of the capture.
 * @param {number} length Length of 1 minute buckets for captured video.
 * @param {!{width: number, height: number}} resolution Capture resolution.
 * @param {!cca.metrics.IntentResultType} intentResult
 * @return {!analytics.EventBuilder}
 * @private
 */
cca.metrics.captureType_ = function(
    facingMode, length, {width, height}, intentResult) {
  var condState = (states, cond = undefined, strict = undefined) => {
    // Return the first existing state among the given states only if there is
    // no gate condition or the condition is met.
    const prerequisite = !cond || cca.state.get(cond);
    if (strict && !prerequisite) {
      return '';
    }
    return prerequisite && states.find((state) => cca.state.get(state)) ||
        'n/a';
  };

  return cca.metrics.base_.category('capture')
      .action(/^(\w*)/.exec(condState(
          ['video-mode', 'photo-mode', 'square-mode', 'portrait-mode']))[0])
      .label(facingMode || '(not set)')
      .dimen(3, condState(['sound']))
      .dimen(4, condState(['mirror']))
      .dimen(5, condState(['_3x3', '_4x4', 'golden'], 'grid'))
      .dimen(6, condState(['_3sec', '_10sec'], 'timer'))
      .dimen(7, condState(['mic'], 'video-mode', true))
      .dimen(8, condState(['max-wnd']))
      .dimen(9, condState(['tall']))
      .dimen(10, `${width}x${height}`)
      .dimen(11, condState(['_30fps', '_60fps'], 'video-mode', true))
      .dimen(12, intentResult)
      .value(length || 0);
};

/**
 * Metrics types.
 * @enum {function(...): !analytics.EventBuilder}
 */
cca.metrics.Type = {
  LAUNCH: cca.metrics.launchType_,
  CAPTURE: cca.metrics.captureType_,
};

/**
 * Logs the given metrics.
 * @param {!cca.metrics.Type} type Metrics type.
 * @param {...*} args Optional rest parameters for logging metrics.
 */
cca.metrics.log = function(type, ...args) {
  cca.metrics.ga_.then((tracker) => tracker.send(type(...args)));
};
