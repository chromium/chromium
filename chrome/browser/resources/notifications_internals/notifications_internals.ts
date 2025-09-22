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
  getRequiredElement('esb-notification')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.scheduleNotification('esb');
      });
  getRequiredElement('quick-delete-notification')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.scheduleNotification('quick_delete');
      });
  getRequiredElement('google-lens-notification')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.scheduleNotification('google_lens');
      });
  getRequiredElement('bottom-omnibox-notification')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.scheduleNotification('bottom_omnibox');
      });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = PageHandler.getRemote();

  setupEventListeners();
});
