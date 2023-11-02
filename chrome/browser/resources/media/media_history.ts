// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

import {MediaDataTable, MediaDataTableDelegate} from './media_data_table.js';
import {MediaHistoryStats, MediaHistoryStore, MediaHistoryStoreRemote} from './media_history_store.mojom-webui.js';

// Allow a function to be provided by tests, which will be called when
// the page has been populated.
const mediaHistoryPageIsPopulatedResolver = new PromiseResolver<void>();
function whenPageIsPopulatedForTest(): Promise<void> {
  return mediaHistoryPageIsPopulatedResolver.promise;
}
Object.assign(window, {whenPageIsPopulatedForTest});

let store: MediaHistoryStoreRemote|null = null;
let statsTableBody: HTMLElement|null = null;
let originsTable: MediaDataTable|null = null;
let playbacksTable: MediaDataTable|null = null;
let sessionsTable: MediaDataTable|null = null;
let delegate: MediaDataTableDelegate|null = null;

/**
 * Creates a single row in the stats table.
 * @param name The name of the table.
 * @param count The row count of the table.
 */
function createStatsRow(name: string, count: number): Node {
  const template = document.querySelector<HTMLTemplateElement>('#stats-row');
  assert(template);
  const td = template.content.querySelectorAll('td');
  td[0]!.textContent = name;
  td[1]!.textContent = count.toString();
  return document.importNode(template.content, true);
}

class MediaHistoryTableDelegate implements MediaDataTableDelegate {
  /**
   * Formats a field to be displayed in the data table and inserts it into the
   * element.
   */
  insertDataField(td: HTMLElement, data: unknown, key: string) {
    if (data === undefined || data === null) {
      return;
    }

    if (key === 'origin') {
      // Format a mojo origin.
      const {scheme, host, port} =
          data as {scheme: string, host: string, port: number};
      td.textContent = new URL(`${scheme}://${host}:${port}`).origin;
    } else if (key === 'lastUpdatedTime') {
      // Format a JS timestamp.
      td.textContent = data ? new Date(data as number).toISOString() : '';
    } else if (
        key === 'cachedAudioVideoWatchtime' ||
        key === 'actualAudioVideoWatchtime' || key === 'watchtime' ||
        key === 'duration' || key === 'position') {
      // Format a mojo timedelta.
      const secs = ((data as {microseconds: number}).microseconds / 1000000);
      td.textContent =
          secs.toString().replace(/(\d)(?=(\d{3})+(?!\d))/g, '$1,');
    } else if (key === 'url') {
      // Format a mojo GURL.
      td.textContent = (data as {url: string}).url;
    } else if (key === 'hasAudio' || key === 'hasVideo') {
      // Format a boolean.
      td.textContent = data as boolean ? 'Yes' : 'No';
    } else if (
        key === 'title' || key === 'artist' || key === 'album' ||
        key === 'sourceTitle') {
      // Format a mojo string16.
      td.textContent = decodeString16(data as String16);
    } else if (key === 'artwork') {
      // Format an array of mojo media images.
      (data as Array<{src: {url: string}}>).forEach((image) => {
        const a = document.createElement('a');
        a.href = image.src.url;
        a.textContent = image.src.url;
        a.target = '_blank';
        td.appendChild(a);
        td.appendChild(document.createElement('br'));
      });
    } else {
      td.textContent = data as string;
    }
  }

  compareTableItem(
      sortKey: string, a: {[key: string]: any},
      b: {[key: string]: any}): number {
    const val1 = a[sortKey];
    const val2 = b[sortKey];

    // Compare the hosts of the origin ignoring schemes.
    if (sortKey === 'origin') {
      return (val1 as {host: string}).host > (val2 as {host: string}).host ? 1 :
                                                                             -1;
    }

    // Compare the url property.
    if (sortKey === 'url') {
      return (val1 as {url: string}).url > (val2 as {url: string}).url ? 1 : -1;
    }

    // Compare TimeDelta microseconds value.
    if (sortKey === 'cachedAudioVideoWatchtime' ||
        sortKey === 'actualAudioVideoWatchtime' || sortKey === 'watchtime' ||
        sortKey === 'duration' || sortKey === 'position') {
      return (val1 as {microseconds: number}).microseconds -
          (val2 as {microseconds: number}).microseconds;
    }

    if (sortKey.startsWith('metadata.')) {
      // Keys with a period denote nested objects.
      let nestedA = a;
      let nestedB = b;
      const expandedKey = sortKey.split('.');
      expandedKey.forEach((k) => {
        nestedA = nestedA[k] as {[key: string]: any};
        nestedB = nestedB[k] as {[key: string]: any};
      });

      return (nestedA as unknown as number | string) >
              (nestedB as unknown as number | string) ?
          1 :
          -1;
    }

    if (sortKey === 'lastUpdatedTime') {
      return (val1 as number) - (val2 as number);
    }

    assertNotReached('Unsupported sort key: ' + sortKey);
  }
}

/**
 * Parses utf16 coded string.
 */
function decodeString16(arr: String16): string {
  if (!arr) {
    return '';
  }

  return arr.data.map(ch => String.fromCodePoint(ch)).join('');
}

/**
 * Regenerates the stats table.
 * @param stats The stats for the Media
 *     History store.
 */
function renderStatsTable(stats: MediaHistoryStats) {
  assert(statsTableBody);
  (statsTableBody.innerHTML as TrustedHTML | string) =
      window.trustedTypes ? window.trustedTypes.emptyHTML : '';

  Object.keys(stats.tableRowCounts).forEach((key) => {
    statsTableBody!.appendChild(createStatsRow(key, stats.tableRowCounts[key]));
  });
}

/**
 * @param name The name of the tab to show.
 */
function showTab(name: string): Promise<void> {
  assert(store);
  switch (name) {
    case 'stats':
      return store.getMediaHistoryStats().then(response => {
        renderStatsTable(response.stats);
        setSelectedTab(name);
      });
    case 'origins':
      return store.getMediaHistoryOriginRows().then(response => {
        assert(originsTable);
        originsTable.setData(response.rows);
        setSelectedTab(name);
      });
    case 'playbacks':
      return store.getMediaHistoryPlaybackRows().then(response => {
        assert(playbacksTable);
        playbacksTable.setData(response.rows);
        setSelectedTab(name);
      });
    case 'sessions':
      return store.getMediaHistoryPlaybackSessionRows().then(response => {
        assert(sessionsTable);
        sessionsTable.setData(response.rows);
        setSelectedTab(name);
      });
  }

  // Return an empty promise if there is no tab.
  return new Promise(() => {});
}

function setSelectedTab(id: string) {
  const tabbox = document.querySelector('cr-tab-box');
  assert(tabbox);
  const index =
      Array.from(tabbox.querySelectorAll<HTMLElement>('div[slot=\'tab\']'))
          .findIndex(tab => tab.id === id);
  tabbox.setAttribute('selected-index', `${index}`);
}

document.addEventListener('DOMContentLoaded', function() {
  store = MediaHistoryStore.getRemote();

  statsTableBody = document.querySelector<HTMLElement>('#stats-table-body');

  delegate = new MediaHistoryTableDelegate();

  originsTable = new MediaDataTable(
      document.querySelector<HTMLElement>('#origins-table')!, delegate);
  playbacksTable = new MediaDataTable(
      document.querySelector<HTMLElement>('#playbacks-table')!, delegate);
  sessionsTable = new MediaDataTable(
      document.querySelector<HTMLElement>('#sessions-table')!, delegate);

  // Allow tabs to be navigated to by fragment. The fragment with be of the
  // format "#tab-<tab id>".
  window.onhashchange = function() {
    showTab(window.location.hash.substr(5));
  };

  const tabBox = document.querySelector('cr-tab-box');
  assert(tabBox);

  // Default to the stats tab.
  if (!window.location.hash.substr(5)) {
    window.location.hash = 'tab-stats';
    // Show the tab box.
    tabBox.style.display = 'block';
  } else {
    showTab(window.location.hash.substr(5)).then(() => {
      // Show the tab box.
      tabBox.style.display = 'block';
      mediaHistoryPageIsPopulatedResolver.resolve();
    });
  }

  // When the tab updates, update the anchor.
  tabBox.addEventListener('selected-index-change', function(e) {
    const tabbox = document.querySelector('cr-tab-box');
    assert(tabbox);
    const tabs = tabbox.querySelectorAll<HTMLElement>('div[slot=\'tab\']');
    const selectedTab = tabs[(e as CustomEvent<number>).detail];
    window.location.hash = 'tab-' + selectedTab.id;
  }, true);

  // Add handler to 'copy all to clipboard' button.
  const copyAllToClipboardButtons =
      document.querySelectorAll<HTMLElement>('.copy-all-to-clipboard');

  copyAllToClipboardButtons.forEach((button) => {
    button.addEventListener('click', () => {
      // Make sure nothing is selected.
      window.getSelection()!.removeAllRanges();

      document.execCommand('selectAll');
      document.execCommand('copy');

      // And deselect everything at the end.
      window.getSelection()!.removeAllRanges();
    });
  });
});
