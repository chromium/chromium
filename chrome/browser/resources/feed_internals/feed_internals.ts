// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import type {TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import type {LastFetchProperties, PageHandlerRemote} from './feed_internals.mojom-webui.js';
import {PageHandler} from './feed_internals.mojom-webui.js';

/**
 * Reference to the backend.
 */
let pageHandler: PageHandlerRemote|null = null;

/**
 * Get and display general properties.
 */
function updatePageWithProperties() {
  assert(pageHandler);
  pageHandler.getGeneralProperties().then(response => {
    const properties = response.properties;
    getRequiredElement('is-feed-enabled').textContent =
        String(properties.isFeedEnabled);
    getRequiredElement('is-feed-visible').textContent =
        String(properties.isFeedVisible);
    getRequiredElement('is-feed-allowed').textContent =
        String(properties.isFeedAllowed);
    getRequiredElement('is-prefetching-enabled').textContent =
        String(properties.isPrefetchingEnabled);
    getRequiredElement('load-stream-status').textContent =
        properties.loadStreamStatus;
    getRequiredElement('feed-fetch-url').textContent = properties.feedFetchUrl;
    getRequiredElement('feed-actions-url').textContent =
        properties.feedActionsUrl;
  });
}

/**
 * Get and display last fetch data.
 */
function updatePageWithLastFetchProperties() {
  assert(pageHandler);
  pageHandler.getLastFetchProperties().then(response => {
    const properties: LastFetchProperties = response.properties;
    getRequiredElement('last-fetch-status').textContent =
        String(properties.lastFetchStatus);
    getRequiredElement('last-fetch-trigger').textContent =
        properties.lastFetchTrigger;
    getRequiredElement('last-fetch-time').textContent =
        toDateString(properties.lastFetchTime);
    getRequiredElement('refresh-suppress-time').textContent =
        toDateString(properties.refreshSuppressTime);
    getRequiredElement('last-fetch-bless-nonce').textContent =
        properties.lastBlessNonce;
    getRequiredElement('last-action-upload-status').textContent =
        String(properties.lastActionUploadStatus);
    getRequiredElement('last-action-upload-time').textContent =
        toDateString(properties.lastActionUploadTime);
  });
}

/**
 * Convert timeSinceEpoch to string for display.
 */
function toDateString(timeSinceEpoch: TimeDelta): string {
  const microseconds = Number(timeSinceEpoch.microseconds);
  return microseconds === 0 ? '' :
                              new Date(microseconds / 1000).toLocaleString();
}

/**
 * Hook up buttons to event listeners.
 */
function setupEventListeners() {
  getRequiredElement('refresh-for-you').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.refreshForYouFeed();
  });

  getRequiredElement('dump-feed-process-scope')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.getFeedProcessScopeDump().then(response => {
          getRequiredElement('feed-process-scope-dump').textContent =
              response.dump;
          getRequiredElement<HTMLDetailsElement>('feed-process-scope-details')
              .open = true;
        });
      });

  getRequiredElement('load-feed-histograms')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.getFeedHistograms().then(response => {
          getRequiredElement('feed-histograms-log').textContent = response.log;
          getRequiredElement<HTMLDetailsElement>('feed-histograms-details')
              .open = true;
        });
      });

  getRequiredElement('feed-host-override-apply')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.overrideFeedHost(
            getRequiredElement<HTMLInputElement>('feed-host-override').value,
        );
      });

  getRequiredElement('discover-api-override-apply')
      .addEventListener('click', function() {
        assert(pageHandler);
        pageHandler.overrideDiscoverApiEndpoint(
            getRequiredElement<HTMLInputElement>('discover-api-override').value,
        );
      });

  getRequiredElement('feed-stream-data-override')
      .addEventListener('click', function() {
        assert(pageHandler);
        const file =
            getRequiredElement<HTMLInputElement>('feed-stream-data-file')
                .files![0];
        if (file && typeof pageHandler.overrideFeedStreamData === 'function') {
          const reader = new FileReader();
          reader.readAsArrayBuffer(file);
          reader.onload = function(e) {
            assert(pageHandler);
            const typedArray = new Uint8Array(e.target!.result as ArrayBuffer);
            pageHandler.overrideFeedStreamData([...typedArray]);
          };
        }
      });
}

function updatePage() {
  updatePageWithProperties();
  updatePageWithLastFetchProperties();
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = PageHandler.getRemote();

  setInterval(updatePage, 2000);
  updatePage();

  setupEventListeners();
});
