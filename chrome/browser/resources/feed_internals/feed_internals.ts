// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {$} from 'chrome://resources/js/util.js';
import {TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {FeedOrder, LastFetchProperties, PageHandler, PageHandlerRemote} from './feed_internals.mojom-webui.js';

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
    $('is-feed-enabled').textContent = String(properties.isFeedEnabled);
    $('is-feed-visible').textContent = String(properties.isFeedVisible);
    $('is-feed-allowed').textContent = String(properties.isFeedAllowed);
    $('is-prefetching-enabled').textContent =
        String(properties.isPrefetchingEnabled);
    $('load-stream-status').textContent = properties.loadStreamStatus;
    $('feed-fetch-url').textContent = properties.feedFetchUrl.url;
    $('feed-actions-url').textContent = properties.feedActionsUrl.url;
    ($('enable-webfeed-follow-intro-debug') as HTMLInputElement).checked =
        properties.isWebFeedFollowIntroDebugEnabled;
    ($('enable-webfeed-follow-intro-debug') as HTMLInputElement).disabled =
        false;
    ($('use-feed-query-requests') as HTMLInputElement).checked =
        properties.useFeedQueryRequests;

    switch (properties.followingFeedOrder) {
      case FeedOrder.kUnspecified:
        ($('following-feed-order-unset') as HTMLInputElement).checked = true;
        break;
      case FeedOrder.kGrouped:
        ($('following-feed-order-grouped') as HTMLInputElement).checked = true;
        break;
      case FeedOrder.kReverseChron:
        ($('following-feed-order-reverse-chron') as HTMLInputElement).checked =
            true;
        break;
    }
    ($('following-feed-order-grouped') as HTMLInputElement).disabled = false;
    ($('following-feed-order-reverse-chron') as HTMLInputElement).disabled =
        false;
    ($('following-feed-order-unset') as HTMLInputElement).disabled = false;
  });
}

/**
 * Get and display last fetch data.
 */
function updatePageWithLastFetchProperties() {
  assert(pageHandler);
  pageHandler.getLastFetchProperties().then(response => {
    const properties: LastFetchProperties = response.properties;
    $('last-fetch-status').textContent = String(properties.lastFetchStatus);
    $('last-fetch-trigger').textContent = properties.lastFetchTrigger;
    $('last-fetch-time').textContent = toDateString(properties.lastFetchTime);
    $('refresh-suppress-time').textContent =
        toDateString(properties.refreshSuppressTime);
    $('last-fetch-bless-nonce').textContent = properties.lastBlessNonce;
    $('last-action-upload-status').textContent =
        String(properties.lastActionUploadStatus);
    $('last-action-upload-time').textContent =
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
  $('refresh-for-you').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.refreshForYouFeed();
  });

  $('refresh-following').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.refreshFollowingFeed();
  });

  $('refresh-webfeed-suggestions').addEventListener('click', () => {
    assert(pageHandler);
    pageHandler.refreshWebFeedSuggestions();
  });

  $('dump-feed-process-scope').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.getFeedProcessScopeDump().then(response => {
      $('feed-process-scope-dump').textContent = response.dump;
      ($('feed-process-scope-details') as HTMLDetailsElement).open = true;
    });
  });

  $('load-feed-histograms').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.getFeedHistograms().then(response => {
      $('feed-histograms-log').textContent = response.log;
      ($('feed-histograms-details') as HTMLDetailsElement).open = true;
    });
  });

  $('feed-host-override-apply').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.overrideFeedHost(
        {url: ($('feed-host-override') as HTMLInputElement).value});
  });

  $('discover-api-override-apply').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.overrideDiscoverApiEndpoint(
        {url: ($('discover-api-override') as HTMLInputElement).value});
  });

  $('feed-stream-data-override').addEventListener('click', function() {
    assert(pageHandler);
    const file = ($('feed-stream-data-file') as HTMLInputElement).files![0];
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

  $('enable-webfeed-follow-intro-debug').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.setWebFeedFollowIntroDebugEnabled(
        ($('enable-webfeed-follow-intro-debug') as HTMLInputElement).checked);
    ($('enable-webfeed-follow-intro-debug') as HTMLInputElement).disabled =
        true;
  });

  $('use-feed-query-requests').addEventListener('click', function() {
    assert(pageHandler);
    pageHandler.setUseFeedQueryRequests(
        ($('use-feed-query-requests') as HTMLInputElement).checked);
  });

  const orderRadioClickListener = function(order: FeedOrder) {
    assert(pageHandler);
    ($('following-feed-order-grouped') as HTMLInputElement).disabled = true;
    ($('following-feed-order-reverse-chron') as HTMLInputElement).disabled =
        true;
    ($('following-feed-order-unset') as HTMLInputElement).disabled = true;
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
