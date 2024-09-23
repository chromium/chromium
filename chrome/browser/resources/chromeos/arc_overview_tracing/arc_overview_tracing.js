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

      // Note autocomplete for this input element must be *off* for this to
      // do the right thing. Otherwise, the last-entered value may be restored
      // automatically sometime after window.onload completes. I could not find
      // the right event to intercept to capture the restored value. A vanilla
      // HTML page I threw together did not have this problem, and the load
      // event fired after the value was restored.
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
