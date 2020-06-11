// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated.
const mediaHistoryPageIsPopulatedResolver = new PromiseResolver();
function whenPageIsPopulatedForTest() {
  return mediaHistoryPageIsPopulatedResolver.promise;
}

(function() {

let store = null;
let statsTableBody = null;
let originsTable = null;
let playbacksTable = null;
let sessionsTable = null;
let delegate = null;

/**
 * Creates a single row in the stats table.
 * @param {string} name The name of the table.
 * @param {number} count The row count of the table.
 * @return {!Node}
 */
function createStatsRow(name, count) {
  const template = $('stats-row');
  const td = template.content.querySelectorAll('td');
  td[0].textContent = name;
  td[1].textContent = count;
  return document.importNode(template.content, true);
}

/** @implements {cr.ui.MediaDataTableDelegate} */
class MediaHistoryTableDelegate {
  /**
   * Formats a field to be displayed in the data table and inserts it into the
   * element.
   * @param {Element} td
   * @param {?Object} data
   * @param {string} key
   */
  insertDataField(td, data, key) {
    if (data === undefined || data === null) {
      return;
    }

    if (key === 'origin') {
      // Format a mojo origin.
      const {scheme, host, port} = data;
      td.textContent = new URL(`${scheme}://${host}:${port}`).origin;
    } else if (key === 'lastUpdatedTime') {
      // Format a JS timestamp.
      td.textContent = data ? new Date(data).toISOString() : '';
    } else if (
        key === 'cachedAudioVideoWatchtime' ||
        key === 'actualAudioVideoWatchtime' || key === 'watchtime' ||
        key === 'duration' || key === 'position') {
      // Format a mojo timedelta.
      const secs = (data.microseconds / 1000000);
      td.textContent =
          secs.toString().replace(/(\d)(?=(\d{3})+(?!\d))/g, '$1,');
    } else if (key === 'url') {
      // Format a mojo GURL.
      td.textContent = data.url;
    } else if (key === 'hasAudio' || key === 'hasVideo') {
      // Format a boolean.
      td.textContent = data ? 'Yes' : 'No';
    } else if (
        key === 'title' || key === 'artist' || key === 'album' ||
        key === 'sourceTitle') {
      // Format a mojo string16.
      td.textContent = decodeString16(
          /** @type {mojoBase.mojom.String16} */ (data));
    } else if (key === 'artwork') {
      // Format an array of mojo media images.
      data.forEach((image) => {
        const a = document.createElement('a');
        a.href = image.src.url;
        a.textContent = image.src.url;
        a.target = '_blank';
        td.appendChild(a);
        td.appendChild(document.createElement('br'));
      });
    } else {
      td.textContent = data;
    }
  }

  /**
   * Compares two objects based on |sortKey|.
   * @param {string} sortKey The name of the property to sort by.
   * @param {?Object} a The first object to compare.
   * @param {?Object} b The second object to compare.
   * @return {number} A negative number if |a| should be ordered
   *     before |b|, a positive number otherwise.
   */
  compareTableItem(sortKey, a, b) {
    const val1 = a[sortKey];
    const val2 = b[sortKey];

    // Compare the hosts of the origin ignoring schemes.
    if (sortKey === 'origin') {
      return val1.host > val2.host ? 1 : -1;
    }

    // Compare the url property.
    if (sortKey === 'url') {
      return val1.url > val2.url ? 1 : -1;
    }

    // Compare mojo_base.mojom.TimeDelta microseconds value.
    if (sortKey === 'cachedAudioVideoWatchtime' ||
        sortKey === 'actualAudioVideoWatchtime' || sortKey === 'watchtime' ||
        sortKey === 'duration' || sortKey === 'position') {
      return val1.microseconds - val2.microseconds;
    }

    if (sortKey.startsWith('metadata.')) {
      // Keys with a period denote nested objects.
      let nestedA = a;
      let nestedB = b;
      const expandedKey = sortKey.split('.');
      expandedKey.forEach((k) => {
        nestedA = nestedA[k];
        nestedB = nestedB[k];
      });

      return nestedA > nestedB ? 1 : -1;
    }

    if (sortKey === 'lastUpdatedTime') {
      return val1 - val2;
    }

    assertNotReached('Unsupported sort key: ' + sortKey);
    return 0;
  }
}

/**
 * Parses utf16 coded string.
 * @param {mojoBase.mojom.String16} arr
 * @return {string}
 */
function decodeString16(arr) {
  if (!arr) {
    return '';
  }

  return arr.data.map(ch => String.fromCodePoint(ch)).join('');
}

/**
 * Regenerates the stats table.
 * @param {!mediaHistory.mojom.MediaHistoryStats} stats The stats for the Media
 *     History store.
 */
function renderStatsTable(stats) {
  statsTableBody.innerHTML = trustedTypes.emptyHTML;

  Object.keys(stats.tableRowCounts).forEach((key) => {
    statsTableBody.appendChild(createStatsRow(key, stats.tableRowCounts[key]));
  });
}

/**
 * @param {!string} name The name of the tab to show.
 * @return {Promise}
 */
function showTab(name) {
  switch (name) {
    case 'stats':
      return store.getMediaHistoryStats().then(response => {
        renderStatsTable(response.stats);
      });
    case 'origins':
      return store.getMediaHistoryOriginRows().then(response => {
        originsTable.setData(response.rows);
      });
    case 'playbacks':
      return store.getMediaHistoryPlaybackRows().then(response => {
        playbacksTable.setData(response.rows);
      });
    case 'sessions':
      return store.getMediaHistoryPlaybackSessionRows().then(response => {
        sessionsTable.setData(response.rows);
      });
  }

  // Return an empty promise if there is no tab.
  return new Promise(() => {});
}

document.addEventListener('DOMContentLoaded', function() {
  store = mediaHistory.mojom.MediaHistoryStore.getRemote();

  statsTableBody = $('stats-table-body');

  delegate = new MediaHistoryTableDelegate();

  originsTable = new cr.ui.MediaDataTable($('origins-table'), delegate);
  playbacksTable = new cr.ui.MediaDataTable($('playbacks-table'), delegate);
  sessionsTable = new cr.ui.MediaDataTable($('sessions-table'), delegate);

  cr.ui.decorate('tabbox', cr.ui.TabBox);

  // Allow tabs to be navigated to by fragment. The fragment with be of the
  // format "#tab-<tab id>".
  window.onhashchange = function() {
    showTab(window.location.hash.substr(5));
  };

  // Default to the stats tab.
  if (!window.location.hash.substr(5)) {
    window.location.hash = 'tab-stats';
  } else {
    showTab(window.location.hash.substr(5))
        .then(mediaHistoryPageIsPopulatedResolver.resolve);
  }

  // When the tab updates, update the anchor.
  $('tabbox').addEventListener('selectedChange', function() {
    const tabbox = $('tabbox');
    const tabs = tabbox.querySelector('tabs').children;
    const selectedTab = tabs[tabbox.selectedIndex];
    window.location.hash = 'tab-' + selectedTab.id;
  }, true);

  // Add handler to 'copy all to clipboard' button.
  const copyAllToClipboardButtons =
      document.querySelectorAll('.copy-all-to-clipboard');

  copyAllToClipboardButtons.forEach((button) => {
    button.addEventListener('click', (e) => {
      // Make sure nothing is selected.
      window.getSelection().removeAllRanges();

      document.execCommand('selectAll');
      document.execCommand('copy');

      // And deselect everything at the end.
      window.getSelection().removeAllRanges();
    });
  });
});
})();
