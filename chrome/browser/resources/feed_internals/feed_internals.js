// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {FeedOrder, LastFetchProperties, PageHandler, PageHandlerRemote, Properties} from './feed_internals.mojom-webui.js';

/**
 * Reference to the backend.
 * @type {PageHandlerRemote}
 */
let pageHandler = null;

/**
 * Get and display general properties.
 */
function updatePageWithProperties() {
  pageHandler.getGeneralProperties().then(response => {
    /** @type {!Properties} */
    const properties = response.properties;
    $('is-feed-enabled').textContent = properties.isFeedEnabled;
    $('is-feed-visible').textContent = properties.isFeedVisible;
    $('is-feed-allowed').textContent = properties.isFeedAllowed;
    $('is-prefetching-enabled').textContent = properties.isPrefetchingEnabled;
    $('load-stream-status').textContent = properties.loadStreamStatus;
    $('feed-fetch-url').textContent = properties.feedFetchUrl.url;
    $('feed-actions-url').textContent = properties.feedActionsUrl.url;
    $('enable-webfeed-follow-intro-debug').checked =
        properties.isWebFeedFollowIntroDebugEnabled;
    $('enable-webfeed-follow-intro-debug').disabled = false;
    $('use-feed-query-requests').checked = properties.useFeedQueryRequests;

    switch (properties.followingFeedOrder) {
      case FeedOrder.kUnspecified:
        $('following-feed-order-unset').checked = true;
        break;
      case FeedOrder.kGrouped:
        $('following-feed-order-grouped').checked = true;
        break;
      case FeedOrder.kReverseChron:
        $('following-feed-order-reverse-chron').checked = true;
        break;
    }
    $('following-feed-order-grouped').disabled = false;
    $('following-feed-order-reverse-chron').disabled = false;
    $('following-feed-order-unset').disabled = false;
  });
}

/**
 * Get and display last fetch data.
 */
function updatePageWithLastFetchProperties() {
  pageHandler.getLastFetchProperties().then(response => {
    /** @type {!LastFetchProperties} */
    const properties = response.properties;
    $('last-fetch-status').textContent = properties.lastFetchStatus;
    $('last-fetch-trigger').textContent = properties.lastFetchTrigger;
    $('last-fetch-time').textContent = toDateString(properties.lastFetchTime);
    $('refresh-suppress-time').textContent =
        toDateString(properties.refreshSuppressTime);
    $('last-fetch-bless-nonce').textContent = properties.lastBlessNonce;
    $('last-action-upload-status').textContent =
        properties.lastActionUploadStatus;
    $('last-action-upload-time').textContent =
        toDateString(properties.lastActionUploadTime);
  });
}

/**
 * Populate <a> node with hyperlinked URL.
 *
 * @param {Element} node
 * @param {string} url
 */
function setLinkNode(node, url) {
  node.textContent = url;
  node.href = url;
}

/**
 * Convert timeSinceEpoch to string for display.
 *
 * @param {TimeDelta} timeSinceEpoch
 * @return {string}
 */
function toDateString(timeSinceEpoch) {
  const microseconds = Number(timeSinceEpoch.microseconds);
  return microseconds === 0 ? '' :
                              new Date(microseconds / 1000).toLocaleString();
}

/**
 * Hook up buttons to event listeners.
 */
function setupEventListeners() {
  $('refresh-for-you').addEventListener('click', function() {
    pageHandler.refreshForYouFeed();
  });

  $('refresh-following').addEventListener('click', function() {
    pageHandler.refreshFollowingFeed();
  });

  $('refresh-webfeed-suggestions').addEventListener('click', () => {
    pageHandler.refreshWebFeedSuggestions();
  });

  $('dump-feed-process-scope').addEventListener('click', function() {
    pageHandler.getFeedProcessScopeDump().then(response => {
      $('feed-process-scope-dump').textContent = response.dump;
      $('feed-process-scope-details').open = true;
    });
  });

  $('load-feed-histograms').addEventListener('click', function() {
    pageHandler.getFeedHistograms().then(response => {
      $('feed-histograms-log').textContent = response.log;
      $('feed-histograms-details').open = true;
    });
  });

  $('feed-host-override-apply').addEventListener('click', function() {
    pageHandler.overrideFeedHost({url: $('feed-host-override').value});
  });

  $('discover-api-override-apply').addEventListener('click', function() {
    pageHandler.overrideFeedHost({url: $('discover-api-override').value});
  });

  $('feed-stream-data-override').addEventListener('click', function() {
    const file = $('feed-stream-data-file').files[0];
    if (file && typeof pageHandler.overrideFeedStreamData === 'function') {
      const reader = new FileReader();
      reader.readAsArrayBuffer(file);
      reader.onload = function(e) {
        const typedArray = new Uint8Array(e.target.result);
        pageHandler.overrideFeedStreamData([...typedArray]);
      };
    }
  });

  $('enable-webfeed-follow-intro-debug').addEventListener('click', function() {
    pageHandler.setWebFeedFollowIntroDebugEnabled(
        $('enable-webfeed-follow-intro-debug').checked);
    $('enable-webfeed-follow-intro-debug').disabled = true;
  });

  $('use-feed-query-requests').addEventListener('click', function() {
    pageHandler.setUseFeedQueryRequests($('use-feed-query-requests').checked);
  });

  const orderRadioClickListener = function(order) {
    $('following-feed-order-grouped').disabled = true;
    $('following-feed-order-reverse-chron').disabled = true;
    $('following-feed-order-unset').disabled = true;
    pageHandler.setFollowingFeedOrder(order);
  };
  $('following-feed-order-unset')
      .addEventListener(
          'click', () => orderRadioClickListener(FeedOrder.kUnspecified));
  $('following-feed-order-grouped')
      .addEventListener(
          'click', () => orderRadioClickListener(FeedOrder.kGrouped));
  $('following-feed-order-reverse-chron')
      .addEventListener(
          'click', () => orderRadioClickListener(FeedOrder.kReverseChron));
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
