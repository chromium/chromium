// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Overview Tracing UI root element.
 */

cr.define('cr.ArcOverviewTracing', function() {
  return {
    /**
     * Initializes internal structures.
     */
    initialize() {
      const maxTime = $('arc-overview-tracing-max-time');
      maxTime.addEventListener('change', function(event) {
        let value = parseFloat(maxTime.value);
        if (Number.isNaN(value) || value < 1) {
          console.error('invalid maxTime:', maxTime.value);
          value = 1;
          maxTime.value = '1';
        }
        chrome.send('setMaxTime', [value]);
      }, false);
      chrome.send('setMaxTime', [parseFloat(maxTime.value)]);
      initializeOverviewUi();
    },

    setStatus: setStatus,

    setModel(model) {
      addModel(model);
    },
  };
});

/**
 * Initializes UI.
 */
window.onload = function() {
  cr.ArcOverviewTracing.initialize();
};
