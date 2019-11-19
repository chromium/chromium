// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('DomainReliabilityInternals', function() {
  'use strict';

  function initialize() {
    chrome.send('updateData');
  }

  function onDataUpdated(data) {
    jstProcess(new JsEvalContext(data), $('template'));
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    onDataUpdated: onDataUpdated,
  };
});

document.addEventListener(
    'DOMContentLoaded', DomainReliabilityInternals.initialize);
