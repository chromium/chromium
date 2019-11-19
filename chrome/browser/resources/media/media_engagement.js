// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated with media engagement details.
const pageIsPopulatedResolver = new PromiseResolver();
function whenPageIsPopulatedForTest() {
  return pageIsPopulatedResolver.promise;
}

(function() {

let detailsProvider = null;
let info = null;
let engagementTableBody = null;
let sortReverse = true;
let sortKey = 'totalScore';
let configTableBody = null;
let showNoPlaybacks = false;

/**
 * Creates a single row in the engagement table.
 * @param {!MediaEngagementScoreDetails} rowInfo The info to create the row.
 * @return {!HTMLElement}
 */
function createRow(rowInfo) {
  const template = $('datarow');
  const td = template.content.querySelectorAll('td');

  td[0].textContent = rowInfo.origin.scheme + '://' + rowInfo.origin.host;
  if (rowInfo.origin.scheme == 'http' && rowInfo.origin.port != '80') {
    td[0].textContent += ':' + rowInfo.origin.port;
  } else if (rowInfo.origin.scheme == 'https' && rowInfo.origin.port != '443') {
    td[0].textContent += ':' + rowInfo.origin.port;
  }

  td[1].textContent = rowInfo.visits;
  td[2].textContent = rowInfo.mediaPlaybacks;
  td[3].textContent = rowInfo.lastMediaPlaybackTime ?
      new Date(rowInfo.lastMediaPlaybackTime).toISOString() :
      '';
  td[4].textContent = rowInfo.isHigh ? 'Yes' : 'No';
  td[5].textContent = rowInfo.totalScore ? rowInfo.totalScore.toFixed(2) : '0';
  td[6].getElementsByClassName('engagement-bar')[0].style.width =
      (rowInfo.totalScore * 50) + 'px';
  return document.importNode(template.content, true);
}

/**
 * Remove all rows from the engagement table.
 */
function clearTable() {
  engagementTableBody.innerHTML = '';
}

/**
 * Sort the engagement info based on |sortKey| and |sortReverse|.
 */
function sortInfo() {
  info.sort((a, b) => {
    return (sortReverse ? -1 : 1) * compareTableItem(sortKey, a, b);
  });
}

/**
 * Compares two MediaEngagementScoreDetails objects based on |sortKey|.
 * @param {string} sortKey The name of the property to sort by.
 * @param {number|url.mojom.Origin} The first object to compare.
 * @param {number|url.mojom.Origin} The second object to compare.
 * @return {number} A negative number if |a| should be ordered before
 *     |b|, a positive number otherwise.
 */
function compareTableItem(sortKey, a, b) {
  const val1 = a[sortKey];
  const val2 = b[sortKey];

  // Compare the hosts of the origin ignoring schemes.
  if (sortKey == 'origin') {
    return val1.host > val2.host ? 1 : -1;
  }

  if (sortKey == 'visits' || sortKey == 'mediaPlaybacks' ||
      sortKey == 'lastMediaPlaybackTime' || sortKey == 'totalScore' ||
      sortKey == 'audiblePlaybacks' || sortKey == 'significantPlaybacks' ||
      sortKey == 'highScoreChanges' || sortKey == 'mediaElementPlaybacks' ||
      sortKey == 'audioContextPlaybacks' || sortKey == 'isHigh') {
    return val1 - val2;
  }

  assertNotReached('Unsupported sort key: ' + sortKey);
  return 0;
}

/**
 * Creates a single row in the config table.
 * @param {string} name The name of the config setting.
 * @param {string} value The value of the config setting.
 * @return {!HTMLElement}
 */
function createConfigRow(name, value) {
  const template = $('configrow');
  const td = template.content.querySelectorAll('td');
  td[0].textContent = name;
  td[1].textContent = value;
  return document.importNode(template.content, true);
}

/**
 * Regenerates the config table.
 * @param {!MediaEngagementConfig} config The config of the MEI service.
 */

function renderConfigTable(config) {
  configTableBody.innerHTML = '';

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
      'Autoplay whitelist settings',
      formatFeatureFlag(config.featureAutoplayWhitelistSettings)));
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
 * @param {bool} value The value of the config setting.
 * @return {string}
 */
function formatFeatureFlag(value) {
  return value ? 'Enabled' : 'Disabled';
}

/**
 * Regenerates the engagement table from |info|.
 */
function renderTable() {
  clearTable();
  sortInfo();
  info.filter(rowInfo => (showNoPlaybacks || rowInfo.mediaPlaybacks > 0))
      .forEach(rowInfo => engagementTableBody.appendChild(createRow(rowInfo)));
}

/**
 * Retrieve media engagement info and render the engagement table.
 */
function updateEngagementTable() {
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
  detailsProvider = media.mojom.MediaEngagementScoreDetailsProvider.getRemote();
  updateEngagementTable();

  engagementTableBody = $('engagement-table-body');
  configTableBody = $('config-table-body');

  // Set table header sort handlers.
  const engagementTableHeader = $('engagement-table-header');
  const headers = engagementTableHeader.children;
  for (let i = 0; i < headers.length; i++) {
    headers[i].addEventListener('click', (e) => {
      const newSortKey = e.target.getAttribute('sort-key');
      if (sortKey == newSortKey) {
        sortReverse = !sortReverse;
      } else {
        sortKey = newSortKey;
        sortReverse = false;
      }
      const oldSortColumn = document.querySelector('.sort-column');
      oldSortColumn.classList.remove('sort-column');
      e.target.classList.add('sort-column');
      if (sortReverse) {
        e.target.setAttribute('sort-reverse', '');
      } else {
        e.target.removeAttribute('sort-reverse');
      }
      renderTable();
    });
  }

  // Add handler to 'copy all to clipboard' button
  const copyAllToClipboardButton = $('copy-all-to-clipboard');
  copyAllToClipboardButton.addEventListener('click', (e) => {
    // Make sure nothing is selected
    window.getSelection().removeAllRanges();

    document.execCommand('selectAll');
    document.execCommand('copy');

    // And deselect everything at the end.
    window.getSelection().removeAllRanges();
  });

  // Add handler to 'show no playbacks' checkbox
  const showNoPlaybacksCheckbox = $('show-no-playbacks');
  showNoPlaybacksCheckbox.addEventListener('change', (e) => {
    showNoPlaybacks = e.target.checked;
    renderTable();
  });
});
})();
