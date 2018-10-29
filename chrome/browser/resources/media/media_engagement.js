// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated with media engagement details.
var pageIsPopulatedResolver = new PromiseResolver();
function whenPageIsPopulatedForTest() {
  return pageIsPopulatedResolver.promise;
}

(function() {

var uiHandler = null;
var info = null;
var engagementTableBody = null;
var sortReverse = true;
var sortKey = 'totalScore';
var configTableBody = null;
var showNoPlaybacks = false;

/**
 * Creates a single row in the engagement table.
 * @param {!MediaEngagementScoreDetails} rowInfo The info to create the row.
 * @return {!HTMLElement}
 */
function createRow(rowInfo) {
  var template = $('datarow');
  var td = template.content.querySelectorAll('td');
  td[0].textContent = rowInfo.origin.url;
  td[1].textContent = rowInfo.visits;
  td[2].textContent = rowInfo.mediaPlaybacks;
  td[3].textContent = rowInfo.audioContextPlaybacks;
  td[4].textContent = rowInfo.mediaElementPlaybacks;
  td[5].textContent = rowInfo.audiblePlaybacks;
  td[6].textContent = rowInfo.significantPlaybacks;
  td[7].textContent = rowInfo.lastMediaPlaybackTime ?
      new Date(rowInfo.lastMediaPlaybackTime).toISOString() :
      '';
  td[8].textContent = rowInfo.isHigh ? 'Yes' : 'No';
  td[9].textContent = rowInfo.highScoreChanges;
  td[10].textContent = rowInfo.totalScore ? rowInfo.totalScore.toFixed(2) : '0';
  td[11].getElementsByClassName('engagement-bar')[0].style.width =
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
 * @param {number|url.mojom.Url} The first object to compare.
 * @param {number|url.mojom.Url} The second object to compare.
 * @return {number} A negative number if |a| should be ordered before
 *     |b|, a positive number otherwise.
 */
function compareTableItem(sortKey, a, b) {
  var val1 = a[sortKey];
  var val2 = b[sortKey];

  // Compare the hosts of the origin ignoring schemes.
  if (sortKey == 'origin')
    return new URL(val1.url).host > new URL(val2.url).host ? 1 : -1;

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
  var template = $('configrow');
  var td = template.content.querySelectorAll('td');
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
  uiHandler.getMediaEngagementScoreDetails().then(response => {
    info = response.info;
    renderTable();
    pageIsPopulatedResolver.resolve();
  });

  // Populate config settings.
  uiHandler.getMediaEngagementConfig().then(response => {
    renderConfigTable(response.config);
  });
}

document.addEventListener('DOMContentLoaded', function() {
  uiHandler = new media.mojom.MediaEngagementScoreDetailsProviderPtr;
  Mojo.bindInterface(
      media.mojom.MediaEngagementScoreDetailsProvider.name,
      mojo.makeRequest(uiHandler).handle);
  updateEngagementTable();

  engagementTableBody = $('engagement-table-body');
  configTableBody = $('config-table-body');

  // Set table header sort handlers.
  var engagementTableHeader = $('engagement-table-header');
  var headers = engagementTableHeader.children;
  for (var i = 0; i < headers.length; i++) {
    headers[i].addEventListener('click', (e) => {
      var newSortKey = e.target.getAttribute('sort-key');
      if (sortKey == newSortKey) {
        sortReverse = !sortReverse;
      } else {
        sortKey = newSortKey;
        sortReverse = false;
      }
      var oldSortColumn = document.querySelector('.sort-column');
      oldSortColumn.classList.remove('sort-column');
      e.target.classList.add('sort-column');
      if (sortReverse)
        e.target.setAttribute('sort-reverse', '');
      else
        e.target.removeAttribute('sort-reverse');
      renderTable();
    });
  }

  // Add handler to 'copy all to clipboard' button
  var copyAllToClipboardButton = $('copy-all-to-clipboard');
  copyAllToClipboardButton.addEventListener('click', (e) => {
    // Make sure nothing is selected
    window.getSelection().removeAllRanges();

    document.execCommand('selectAll');
    document.execCommand('copy');

    // And deselect everything at the end.
    window.getSelection().removeAllRanges();
  });

  // Add handler to 'show no playbacks' checkbox
  var showNoPlaybacksCheckbox = $('show-no-playbacks');
  showNoPlaybacksCheckbox.addEventListener('change', (e) => {
    showNoPlaybacks = e.target.checked;
    renderTable();
  });

});

})();
