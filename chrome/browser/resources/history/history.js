// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Send the history query immediately. This allows the query to process during
// the initial page startup.
chrome.send('queryHistory', ['', RESULTS_PER_PAGE]);
chrome.send('getForeignSessions');

/** @type {Promise} */
let upgradePromise = null;

/**
 * Adding this on |window| since it is accessed by tests.
 * @type {boolean}
 */
window.resultsRendered = false;

/**
 * @return {!Promise} Resolves once the history-app has been fully upgraded.
 */
function waitForAppUpgrade() {
  if (!upgradePromise) {
    upgradePromise = new Promise(function(resolve, reject) {
      $('bundle').addEventListener('load', resolve);
    });
  }
  return upgradePromise;
}

// Chrome Callbacks-------------------------------------------------------------

/**
 * Our history system calls this function with results from searches.
 * @param {HistoryQuery} info An object containing information about the query.
 * @param {!Array<HistoryEntry>} results A list of results.
 */
function historyResult(info, results) {
  waitForAppUpgrade().then(function() {
    const app = /** @type {HistoryAppElement} */ ($('history-app'));
    app.historyResult(info, results);
    document.body.classList.remove('loading');

    if (!window.resultsRendered) {
      window.resultsRendered = true;
      app.onFirstRender();
    }
  });
}

/**
 * Called by the history backend after receiving results and after discovering
 * the existence of other forms of browsing history.
 * @param {boolean} includeOtherFormsOfBrowsingHistory Whether to include
 *     a sentence about the existence of other forms of browsing history.
 */
function showNotification(includeOtherFormsOfBrowsingHistory) {
  waitForAppUpgrade().then(function() {
    const app = /** @type {HistoryAppElement} */ ($('history-app'));
    app.set(
        'footerInfo.otherFormsOfHistory', includeOtherFormsOfBrowsingHistory);
  });
}

/**
 * Receives the synced history data. An empty list means that either there are
 * no foreign sessions, or tab sync is disabled for this profile.
 *
 * @param {!Array<!ForeignSession>} sessionList Array of objects describing the
 *     sessions from other devices.
 */
function setForeignSessions(sessionList) {
  waitForAppUpgrade().then(function() {
    /** @type {HistoryAppElement} */ ($('history-app'))
        .setForeignSessions(sessionList);
  });
}

/**
 * Called when the history is deleted by someone else.
 */
function historyDeleted() {
  waitForAppUpgrade().then(function() {
    /** @type {HistoryAppElement} */ ($('history-app')).historyDeleted();
  });
}

/**
 * Called by the history backend after user's sign in state changes.
 * @param {boolean} isUserSignedIn Whether user is signed in or not now.
 */
function updateSignInState(isUserSignedIn) {
  waitForAppUpgrade().then(function() {
    if ($('history-app')) {
      /** @type {HistoryAppElement} */ ($('history-app'))
          .updateSignInState(isUserSignedIn);
    }
  });
}
