// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Reference to the backend.
 * @type {feedInternals.mojom.PageHandlerRemote}
 */
let pageHandler = null;

(function() {

/**
 * Get and display general properties.
 */
function updatePageWithProperties() {
  pageHandler.getGeneralProperties().then(response => {
    /** @type {!feedInternals.mojom.Properties} */
    const properties = response.properties;
    $('is-feed-enabled').textContent = properties.isFeedEnabled;
    $('is-feed-visible').textContent = properties.isFeedVisible;
    $('is-feed-allowed').textContent = properties.isFeedAllowed;
    $('is-prefetching-enabled').textContent = properties.isPrefetchingEnabled;
    $('feed-fetch-url').textContent = properties.feedFetchUrl.url;
  });
}

/**
 * Get and display user classifier properties.
 */
function updatePageWithUserClass() {
  pageHandler.getUserClassifierProperties().then(response => {
    /** @type {!feedInternals.mojom.UserClassifier} */
    const properties = response.properties;
    $('user-class-description').textContent = properties.userClassDescription;
    $('avg-hours-between-views').textContent = properties.avgHoursBetweenViews;
    $('avg-hours-between-uses').textContent = properties.avgHoursBetweenUses;
  });
}

/**
 * Get and display last fetch data.
 */
function updatePageWithLastFetchProperties() {
  pageHandler.getLastFetchProperties().then(response => {
    /** @type {!feedInternals.mojom.LastFetchProperties} */
    const properties = response.properties;
    $('last-fetch-status').textContent = properties.lastFetchStatus;
    $('last-fetch-trigger').textContent = properties.lastFetchTrigger;
    $('last-fetch-time').textContent = toDateString(properties.lastFetchTime);
    $('refresh-suppress-time').textContent =
        toDateString(properties.refreshSuppressTime);
    $('last-fetch-bless-nonce').textContent = properties.lastBlessNonce;
  });
}

/**
 * Get and display last known content.
 */
function updatePageWithCurrentContent() {
  pageHandler.getCurrentContent().then(response => {
    const before = $('current-content');
    const after = before.cloneNode(false);

    /** @type {!Array<feedInternals.mojom.Suggestion>} */
    const suggestions = response.suggestions;

    for (const suggestion of suggestions) {
      // Create new content item from template.
      const item = document.importNode($('suggestion-template').content, true);

      // Populate template with text metadata.
      item.querySelector('.title').textContent = suggestion.title;
      item.querySelector('.publisher').textContent = suggestion.publisherName;

      // Populate template with link metadata.
      setLinkNode(item.querySelector('a.url'), suggestion.url.url);
      setLinkNode(item.querySelector('a.image'), suggestion.imageUrl.url);
      setLinkNode(item.querySelector('a.favicon'), suggestion.faviconUrl.url);

      after.appendChild(item);
    }

    before.replaceWith(after);
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
 * Convert time to string for display.
 *
 * @param {feedInternals.mojom.Time|undefined} time
 * @return {string}
 */
function toDateString(time) {
  return time == null ? '' : new Date(time.msSinceEpoch).toLocaleString();
}

/**
 * Update last fetch properties and current content following a Feed refresh.
 */
function updateAfterRefresh() {
  // TODO(crbug.com/939907): Listen for Feed update events rather than waiting
  // an arbitrary period of time.
  setTimeout(updatePageWithLastFetchProperties, 1000);
  setTimeout(updatePageWithCurrentContent, 1000);
}

/**
 * Hook up buttons to event listeners.
 */
function setupEventListeners() {
  $('clear-user-classification').addEventListener('click', function() {
    pageHandler.clearUserClassifierProperties();
    updatePageWithUserClass();
  });

  $('clear-cached-data').addEventListener('click', function() {
    pageHandler.clearCachedDataAndRefreshFeed();
    updateAfterRefresh();
  });

  $('refresh-feed').addEventListener('click', function() {
    pageHandler.refreshFeed();
    updateAfterRefresh();
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
    pageHandler.overrideFeedHost($('feed-host-override').value);
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = feedInternals.mojom.PageHandler.getRemote();

  updatePageWithProperties();
  updatePageWithUserClass();
  updatePageWithLastFetchProperties();
  updatePageWithCurrentContent();

  setupEventListeners();
});
})();
