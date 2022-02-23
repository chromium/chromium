// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/** @type {boolean} */
let initialized = false;

function initialize() {
  $('slow-disable').addEventListener('click', () => disableTracing());
  $('slow-enable').addEventListener('click', () => enableTracing());
  addWebUIListener('tracing-pref-changed', tracingPrefChanged);
  initialized = true;
}

function disableTracing() {
  chrome.send('disableTracing');
}

function enableTracing() {
  chrome.send('enableTracing');
}

/** @param {boolean} enabled */
function tracingPrefChanged(enabled) {
  $('slow-disable').hidden = !enabled;
  $('slow-enable').hidden = enabled;
}

document.addEventListener('DOMContentLoaded', () => {
  initialize();
  chrome.send('loadComplete');
});
