// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

import {PageHandler} from './floc_internals.mojom-webui.js';

/**
 * Creates a single row of the status entries.
 * @param {string} name The status identifier name.
 * @param {string} value The text content that describes the status.
 * @return {!Node}
 */
function createRow(name, value) {
  const row = $('result-template').content.cloneNode(true);
  row.querySelectorAll('span')[0].textContent = name + ': ';
  row.querySelectorAll('span')[1].textContent = value;
  return row;
}

/**
 * Converts a time period in seconds into a user-readable string.
 * @param {number} totalMicroseconds The total microseconds in time.
 * @return {string}
 */
function formatTimeDuration(totalMicroseconds) {
  if (totalMicroseconds > Number.MAX_SAFE_INTEGER) {
    return '+inf';
  }

  let totalSeconds = Math.round(totalMicroseconds / 1000000);

  const days = Math.round(totalSeconds / 3600 / 24);
  totalSeconds %= 3600 * 24;
  const hours = Math.round(totalSeconds / 3600);
  totalSeconds %= 3600;
  const minutes = Math.round(totalSeconds / 60);
  const seconds = Math.round(totalSeconds % 60);
  return days + 'd-' + hours + 'h-' + minutes + 'm-' + seconds + 's';
}

/**
 * Converts a point of time into a user-readable string.
 * @param {number} microsecondsSinceWindowsEpoch The total microseconds since
 *     Windows FILETIME epoch.
 * @return {string}
 */
function formatTime(microsecondsSinceWindowsEpoch) {
  if (microsecondsSinceWindowsEpoch === 0) {
    return 'N/A';
  }

  const unixWindowsOffsetMicroseconds = 11644473600000000;
  const msSinceUnixEpoch =
      (microsecondsSinceWindowsEpoch - unixWindowsOffsetMicroseconds) / 1000;

  return new Date(msSinceUnixEpoch).toLocaleString();
}

document.addEventListener('DOMContentLoaded', function() {
  PageHandler.getRemote().getFlocStatus().then((response) => {
    const status = response.status;

    const statusDiv = $('floc-status-div');
    if (status.id) {
      statusDiv.appendChild(createRow('id', status.id));
      statusDiv.appendChild(createRow('version', status.version));
    } else {
      statusDiv.appendChild(createRow('id', 'N/A'));
      statusDiv.appendChild(createRow('version', 'N/A'));
    }
    statusDiv.appendChild(createRow(
        'last compute time',
        formatTime(Number(status.computeTime.internalValue))));

    const featuresDiv = $('floc-features-div');
    featuresDiv.appendChild(createRow(
        'FlocPagesWithAdResourcesDefaultIncludedInFlocComputation',
        status.featurePagesWithAdResourcesDefaultIncludedInFlocComputation
            .toString()));
    featuresDiv.appendChild(createRow(
        'InterestCohortAPIOriginTrial',
        status.featureInterestCohortApiOriginTrial.toString()));
    featuresDiv.appendChild(createRow(
        'InterestCohortFeaturePolicy',
        status.featureInterestCohortFeaturePolicy.toString()));

    const paramsDiv = $('floc-params-div');
    paramsDiv.appendChild(createRow(
        'FlocIdScheduledUpdateInterval',
        formatTimeDuration(
            Number(status.featureParamScheduledUpdateInterval.microseconds))));
    paramsDiv.appendChild(createRow(
        'FlocIdMinimumHistoryDomainSizeRequired',
        status.featureParamMinimumHistoryDomainSizeRequired.toString()));
    paramsDiv.appendChild(createRow(
        'FlocIdFinchConfigVersion',
        status.featureParamFinchConfigVersion.toString()));
  });
});
