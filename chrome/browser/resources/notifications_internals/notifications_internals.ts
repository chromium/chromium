// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {PageHandlerRemote} from './notifications_internals.mojom-webui.js';
import {PageHandler} from './notifications_internals.mojom-webui.js';

/**
 * Reference to the backend.
 */
let pageHandler: PageHandlerRemote|null = null;

/**
 * Hook up buttons to event listeners.
 */
function setupEventListeners() {
  getRequiredElement('schedule-notification')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.scheduleNotification();
      });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = PageHandler.getRemote();

  setupEventListeners();
});
