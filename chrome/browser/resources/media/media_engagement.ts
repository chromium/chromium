// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import type {MediaEngagementConfig, MediaEngagementScoreDetails, MediaEngagementScoreDetailsProviderRemote} from './media_engagement_score_details.mojom-webui.js';
import {MediaEngagementScoreDetailsProvider} from './media_engagement_score_details.mojom-webui.js';

// Allow a function to be provided by tests, which will be called when
// the page has been populated with media engagement details.
const pageIsPopulatedResolver = new PromiseResolver<void>();
function whenPageIsPopulatedForTest(): Promise<void> {
  return pageIsPopulatedResolver.promise;
}
Object.assign(window, {whenPageIsPopulatedForTest});

let detailsProvider: MediaEngagementScoreDetailsProviderRemote|null = null;
let info: MediaEngagementScoreDetails[]|null = null;
let engagementTableBody: HTMLElement|null = null;
let sortReverse: boolean = true;
let sortKey: string = 'totalScore';
let configTableBody: HTMLElement|null = null;
let showNoPlaybacks: boolean = false;

/**
 * Creates a single row in the engagement table.
 */
function createRow(rowInfo: MediaEngagementScoreDetails): DocumentFragment {
  const template = document.querySelector<HTMLTemplateElement>('#datarow');
  assert(template);
  const td = template.content.querySelectorAll('td');

  td[0]!.textContent = rowInfo.origin.scheme + '://' + rowInfo.origin.host;
  if (rowInfo.origin.scheme === 'http' && rowInfo.origin.port !== 80) {
    td[0]!.textContent += ':' + rowInfo.origin.port;
  } else if (rowInfo.origin.scheme === 'https' && rowInfo.origin.port !== 443) {
    td[0]!.textContent += ':' + rowInfo.origin.port;
  }

  td[1]!.textContent = rowInfo.visits.toString();
  td[2]!.textContent = rowInfo.mediaPlaybacks.toString();
  td[3]!.textContent = rowInfo.lastMediaPlaybackTime ?
      new Date(rowInfo.lastMediaPlaybackTime).toISOString() :
      '';
  td[4]!.textContent = rowInfo.isHigh ? 'Yes' : 'No';
  td[5]!.textContent = rowInfo.totalScore ? rowInfo.totalScore.toFixed(2) : '0';
  td[6]!.querySelectorAll<HTMLElement>('.engagement-bar')[0]!.style.width =
      (rowInfo.totalScore * 50) + 'px';
  return document.importNode(template.content, true);
}

/**
 * Remove all rows from the engagement table.
 */
function clearTable() {
  assert(engagementTableBody);
  engagementTableBody.innerHTML =
      window.trustedTypes ? window.trustedTypes.emptyHTML : '';
}

/**
 * Sort the engagement info based on |sortKey| and |sortReverse|.
 */
function sortInfo() {
  assert(info);
  info.sort((a, b) => {
    return (sortReverse ? -1 : 1) * compareTableItem(sortKey, a, b);
  });
}

/**
 * Compares two MediaEngagementScoreDetails objects based on |sortKey|.
 * @param sortKey The name of the property to sort by.
 * @param a The first object to compare.
 * @param b The second object to compare.
 * @return A negative number if |a| should be ordered before
 *     |b|, a positive number otherwise.
 */
function compareTableItem(
    sortKey: string, a: MediaEngagementScoreDetails,
    b: MediaEngagementScoreDetails): number {
  // Compare the hosts of the origin ignoring schemes.
  if (sortKey === 'origin') {
    return a.origin.host > b.origin.host ? 1 : -1;
  }

  if (sortKey === 'visits' || sortKey === 'mediaPlaybacks' ||
      sortKey === 'lastMediaPlaybackTime' || sortKey === 'totalScore' ||
      sortKey === 'audiblePlaybacks' || sortKey === 'significantPlaybacks' ||
      sortKey === 'highScoreChanges' || sortKey === 'mediaElementPlaybacks' ||
      sortKey === 'audioContextPlaybacks' || sortKey === 'isHigh') {
    const val1 = (a as {[key: string]: any})[sortKey];
    const val2 = (b as {[key: string]: any})[sortKey];

    return (val1 as number) - (val2 as number);
  }

  assertNotReached('Unsupported sort key: ' + sortKey);
}

/**
 * Creates a single row in the config table.
 * @param name The name of the config setting.
 * @param value The value of the config setting.
 */
function createConfigRow(name: string, value: number|string): DocumentFragment {
  const template = document.querySelector<HTMLTemplateElement>('#configrow');
  assert(template);
  const td = template.content.querySelectorAll('td');
  td[0]!.textContent = name;
  td[1]!.textContent = value.toString();
  return document.importNode(template.content, true);
}

/**
 * Regenerates the config table.
 * @param config The config of the MEI service.
 */

function renderConfigTable(config: MediaEngagementConfig) {
  assert(configTableBody);
  configTableBody.innerHTML =
      window.trustedTypes ? (window.trustedTypes.emptyHTML) : '';

  configTableBody.appendChild(
      createConfigRow('Min Sessions', config.scoreMinVisits));
  configTableBody.appendChild(
      createConfigRow('Lower Threshold', config.highScoreLowerThreshold));
  configTableBody.appendChild(
      createConfigRow('Upper Threshold', config.highScoreUpperThreshold));

  configTableBody.appendChild(createConfigRow(
      'Record MEI data', formatFeatureFlag(config.featureRecordData)));
  configTableBody.appendChild(createConfigRow(
      'Bypass autoplay based on MEI',
      formatFeatureFlag(config.featureBypassAutoplay)));
  configTableBody.appendChild(createConfigRow(
      'Preload MEI data', formatFeatureFlag(config.featurePreloadData)));
  configTableBody.appendChild(createConfigRow(
      'MEI for HTTPS only', formatFeatureFlag(config.featureHttpsOnly)));
  configTableBody.appendChild(createConfigRow(
      'Autoplay disable settings',
      formatFeatureFlag(config.featureAutoplayDisableSettings)));
  configTableBody.appendChild(createConfigRow(
      'Unified autoplay (preference)',
      formatFeatureFlag(config.prefDisableUnifiedAutoplay)));
  configTableBody.appendChild(createConfigRow(
      'Custom autoplay policy',
      formatFeatureFlag(config.hasCustomAutoplayPolicy)));
  configTableBody.appendChild(
      createConfigRow('Autoplay Policy', config.autoplayPolicy));
  configTableBody.appendChild(createConfigRow(
      'Preload version',
      config.preloadVersion ? config.preloadVersion : 'Not Available'));
}

/**
 * Converts a boolean into a string value.
 */
function formatFeatureFlag(value: boolean): string {
  return value ? 'Enabled' : 'Disabled';
}

/**
 * Regenerates the engagement table from |info|.
 */
function renderTable() {
  clearTable();
  sortInfo();
  assert(info);
  assert(engagementTableBody);
  info.filter(rowInfo => (showNoPlaybacks || rowInfo.mediaPlaybacks > 0))
      .forEach(rowInfo => engagementTableBody!.appendChild(createRow(rowInfo)));
}

/**
 * Retrieve media engagement info and render the engagement table.
 */
function updateEngagementTable() {
  assert(detailsProvider);
  // Populate engagement table.
  detailsProvider.getMediaEngagementScoreDetails().then(response => {
    info = response.info;
    renderTable();
    pageIsPopulatedResolver.resolve();
  });

  // Populate config settings.
  detailsProvider.getMediaEngagementConfig().then(response => {
    renderConfigTable(response.config);
  });
}

document.addEventListener('DOMContentLoaded', function() {
  detailsProvider = MediaEngagementScoreDetailsProvider.getRemote();
  updateEngagementTable();

  engagementTableBody =
      document.querySelector<HTMLElement>('#engagement-table-body');
  configTableBody = document.querySelector<HTMLElement>('#config-table-body');

  // Set table header sort handlers.
  const engagementTableHeader =
      document.querySelector<HTMLElement>('#engagement-table-header');
  assert(engagementTableHeader);
  const headers = engagementTableHeader.children;
  for (let i = 0; i < headers.length; i++) {
    headers[i]!.addEventListener('click', (e) => {
      const target = e.target as HTMLElement;
      const newSortKey = target.getAttribute('sort-key');
      if (sortKey === newSortKey) {
        sortReverse = !sortReverse;
      } else {
        assert(newSortKey);
        sortKey = newSortKey;
        sortReverse = false;
      }
      const oldSortColumn = document.querySelector<HTMLElement>('.sort-column');
      assert(oldSortColumn);
      oldSortColumn.classList.remove('sort-column');
      target.classList.add('sort-column');
      if (sortReverse) {
        target.setAttribute('sort-reverse', '');
      } else {
        target.removeAttribute('sort-reverse');
      }
      renderTable();
    });
  }

  // Add handler to 'copy all to clipboard' button
  const copyAllToClipboardButton =
      document.querySelector<HTMLElement>('#copy-all-to-clipboard');
  assert(copyAllToClipboardButton);
  copyAllToClipboardButton.addEventListener('click', () => {
    // Make sure nothing is selected
    window.getSelection()!.removeAllRanges();

    document.execCommand('selectAll');
    document.execCommand('copy');

    // And deselect everything at the end.
    window.getSelection()!.removeAllRanges();
  });

  // Add handler to 'show no playbacks' checkbox
  const showNoPlaybacksCheckbox =
      document.querySelector<HTMLElement>('#show-no-playbacks');
  assert(showNoPlaybacksCheckbox);
  showNoPlaybacksCheckbox.addEventListener('change', (e) => {
    showNoPlaybacks = (e.target as HTMLInputElement).checked;
    renderTable();
  });
});
