// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './mojo_api.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {boolToString, durationToString, getOrCreateDetailsProvider, secondsToString} from './discards.js';
import {SortedTableBehavior} from './sorted_table_behavior.js';

/**
 * Compares two db rows by their origin.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} a The first value
 *     being compared.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} b The second value
 *     being compared.
 * @return {number} A negative number if a < b, 0 if a == b, and a positive
 *     number if a > b.
 */
function compareRowsByOrigin(a, b) {
  return a.origin.localeCompare(b.origin);
}

/**
 * Compares two db rows by their dirty bit.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} a The first value
 *     being compared.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} b The second value
 *     being compared.
 * @return {number} A negative number if a < b, 0 if a == b, and a positive
 *     number if a > b.
 */
function compareRowsByIsDirty(a, b) {
  return a.isDirty - b.isDirty;
}

/**
 * Compares two db rows by their last load time.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} a The first value
 *     being compared.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} b The second value
 *     being compared.
 * @return {number} A negative number if a < b, 0 if a == b, and a positive
 *     number if a > b.
 */
function compareRowsByLastLoaded(a, b) {
  return a.value.lastLoaded - a.value.lastLoaded;
}

/**
 * Compares two db rows by their CPU usage.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} a The first value
 *     being compared.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} b The second value
 *     being compared.
 * @return {number} A negative number if a < b, 0 if a == b, and a positive
 *     number if a > b.
 */
function compareRowsByCpuUsage(a, b) {
  const keyA =
      a.value.loadTimeEstimates ? a.value.loadTimeEstimates.avgCpuUsageUs : 0;
  const keyB =
      b.value.loadTimeEstimates ? b.value.loadTimeEstimates.avgCpuUsageUs : 0;
  return keyA - keyB;
}

/**
 * Compares two db rows by their memory usage.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} a The first value
 *     being compared.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} b The second value
 *     being compared.
 * @return {number} A negative number if a < b, 0 if a == b, and a positive
 *     number if a > b.
 */
function compareRowsByMemoryUsage(a, b) {
  const keyA =
      a.value.loadTimeEstimates ? a.value.loadTimeEstimates.avgFootprintKb : 0;
  const keyB =
      b.value.loadTimeEstimates ? b.value.loadTimeEstimates.avgFootprintKb : 0;
  return keyA - keyB;
}

/**
 * Compares two db rows by their load duration.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} a The first value
 *     being compared.
 * @param {discards.mojom.SiteCharacteristicsDatabaseEntry} b The second value
 *     being compared.
 * @return {number} A negative number if a < b, 0 if a == b, and a positive
 *     number if a > b.
 */
function compareRowsByLoadDuration(a, b) {
  const keyA = a.value.loadTimeEstimates ?
      a.value.loadTimeEstimates.avgLoadDurationUs :
      0;
  const keyB = b.value.loadTimeEstimates ?
      b.value.loadTimeEstimates.avgLoadDurationUs :
      0;
  return keyA - keyB;
}

/**
 * @param {string} sortKey The sort key to get a function for.
 * @return {function(discards.mojom.SiteCharacteristicsDatabaseEntry,
                     discards.mojom.SiteCharacteristicsDatabaseEntry): number}
 *     A comparison function that compares two tab infos, returns
 *     negative number if a < b, 0 if a == b, and a positive
 *     number if a > b.
 */
function getSortFunctionForKey(sortKey) {
  switch (sortKey) {
    case 'origin':
      return compareRowsByOrigin;
    case 'dirty':
      return compareRowsByIsDirty;
    case 'lastLoaded':
      return compareRowsByLastLoaded;
    case 'cpuUsage':
      return compareRowsByCpuUsage;
    case 'memoryUsage':
      return compareRowsByMemoryUsage;
    case 'loadDuration':
      return compareRowsByLoadDuration;
    default:
      assertNotReached('Unknown sortKey: ' + sortKey);
  }
}

/**
 * @param {number} time A time in microseconds.
 * @return {string} A friendly, human readable string representing the input
 *    time with units.
 */
function microsecondsToString(time) {
  if (time < 1000) {
    return time.toString() + ' µs';
  }
  time /= 1000;
  if (time < 1000) {
    return time.toFixed(2) + ' ms';
  }
  time /= 1000;
  return time.toFixed(2) + ' s';
}

/**
 * @param {number} value A memory amount in kilobytes.
 * @return {string} A friendly, human readable string representing the input
 *    time with units.
 */
function kilobytesToString(value) {
  if (value < 1000) {
    return value.toString() + ' KB';
  }
  value /= 1000;
  if (value < 1000) {
    return value.toFixed(1) + ' MB';
  }
  value /= 1000;
  return value.toFixed(1) + ' GB';
}

/**
 * @param {!Object} item The item to retrieve a load time estimate for.
 * @param {string} propertyName Name of the load time estimate to retrieve.
 * @return {string} The requested load time estimate or 'N/A' if unavailable.
 */
function formatLoadTimeEstimate(item, propertyName) {
  if (!item.value || !item.value.loadTimeEstimates) {
    return 'N/A';
  }

  const value = item.value.loadTimeEstimates[propertyName];
  if (propertyName.endsWith('Us')) {
    return microsecondsToString(value);
  } else if (propertyName.endsWith('Kb')) {
    return kilobytesToString(value);
  }
  return value.toString();
}


Polymer({
  is: 'database-tab',

  _template: html`{__html_template__}`,

  behaviors: [SortedTableBehavior],

  properties: {
    /**
     * List of database rows.
     * @private {?Array<!discards.mojom.SiteCharacteristicsDatabaseEntry>}
     */
    rows_: {
      type: Array,
    },

    /**
     * The database size response.
     * @private {!discards.mojom.SiteCharacteristicsDatabaseSize}
     */
    size_: {
      type: Object,
      value: {numRows: -1, onDiskSizeKb: -1},
    },

    /**
     * An origin that can be added to requestedOrigins_ by onAddOriginClick_.
     * @private {!string}
     */
    newOrigin_: {
      type: String,
    },
  },

  /** @private {number} */
  updateTableTimer_: 0,

  /** @private {number} */
  updateSizesTimer_: 0,

  /** @private {!Object} */
  requestedOrigins_: {},

  /** @private {?discards.mojom.DetailsProviderRemote} */
  discardsDetailsProvider_: null,

  /** @override */
  ready: function() {
    this.setSortKey('origin');
    this.requestedOrigins_ = {};
    this.discardsDetailsProvider_ = getOrCreateDetailsProvider();

    // Specifies the update interval of the table, in ms.
    const UPDATE_INTERVAL_MS = 1000;

    // Update immediately.
    this.updateDbRows_();
    this.updateDbSizes_();

    // Set an interval timer to update the database periodically.
    this.updateTableTimer_ =
        setInterval(this.updateDbRows_.bind(this), UPDATE_INTERVAL_MS);

    // Set another interval timer to update the database sizes, but much less
    // frequently, as this requires iterating the entire database.
    this.updateSizesTimer_ =
        setInterval(this.updateDbSizes_.bind(this), UPDATE_INTERVAL_MS * 30);
  },

  /** @override */
  detached: function() {
    // Clear the update timers to avoid memory leaks.
    clearInterval(this.updateTableTimer_);
    this.updateTableTimer_ = 0;
    clearInterval(this.updateSizesTimer_);
    this.updateSizesTimer_ = 0;
  },

  /**
   * Issues a request for the data and renders on response.
   * @private
   */
  updateDbRows_: function() {
    this.discardsDetailsProvider_
        .getSiteCharacteristicsDatabase(Object.keys(this.requestedOrigins_))
        .then(response => {
          // Bail if the SiteCharacteristicsDatabase is turned off.
          if (!response.result) {
            return;
          }

          // Add any new origins to the (monotonically increasing)
          // set of requested origins.
          const dbRows = response.result.dbRows;
          for (const dbRow of dbRows) {
            this.requestedOrigins_[dbRow.origin] = true;
          }
          this.rows_ = dbRows;
        });
  },

  /**
   * Adds the current new origin to requested origins and starts an update.
   * @private
   */
  addNewOrigin_: function() {
    this.requestedOrigins_[this.newOrigin_] = true;
    this.newOrigin_ = '';
    this.updateDbRows_();
  },

  /**
   * An on-click handler that adds the current new origin to requested
   * origins.
   * @private
   */
  onAddOriginClick_: function() {
    this.addNewOrigin_();

    // Set the focus back to the input field for convenience.
    this.$.addOriginInput.focus();
  },

  /**
   * A key-down handler that adds the current new origin to requested origins.
   * @private
   */
  onOriginKeydown_: function(e) {
    if (e.key === 'Enter' && this.isValidOrigin_(this.newOrigin_)) {
      this.addNewOrigin_();
      e.stopPropagation();
    }
  },

  /**
   * Issues a request for the database sizes and renders on response.
   * @private
   */
  updateDbSizes_: function() {
    this.discardsDetailsProvider_.getSiteCharacteristicsDatabaseSize().then(
        response => {
          // Bail if the SiteCharacteristicsDatabase is turned off.
          if (!response.dbSize) {
            return;
          }
          this.size_ = response.dbSize;
        });
  },

  /**
   * Returns a sort function to compare tab infos based on the provided sort
   * key and a boolean reverse flag.
   * @param {string} sortKey The sort key for the  returned function.
   * @param {boolean} sortReverse True if sorting is reversed.
   * @return {function({Object}, {Object}): number}
   *     A comparison function that compares two tab infos, returns
   *     negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   * @private
   */
  computeSortFunction_: function(sortKey, sortReverse) {
    // Polymer 2 may invoke multi-property observers before all properties
    // are defined.
    if (!sortKey) {
      return (a, b) => 0;
    }

    const sortFunction = getSortFunctionForKey(sortKey);
    return (a, b) => {
      const comp = sortFunction(a, b);
      return sortReverse ? -comp : comp;
    };
  },

  /**
   * @param {string} origin A potentially valid origin string.
   * @return {boolean} Whether the origin is valid.
   * @private
   */
  isValidOrigin_: function(origin) {
    const re = /(https?|ftp):\/\/[a-z+.]/;

    return re.test(origin);
  },

  /**
   * @param {string} origin A potentially valid origin string.
   * @return {boolean} Whether the origin is valid or empty.
   * @private
   */
  isEmptyOrValidOrigin_: function(origin) {
    return !origin || this.isValidOrigin_(origin);
  },

  /**
   * @param {boolean} value The value to convert.
   * @return {string} A display string representing value.
   * @private
   */
  boolToString_: function(value) {
    return boolToString(value);
  },

  /**
   * @param {number} time Time in seconds since epoch.
   *     in question.
   * @return {string} A user-friendly string explaining how long ago time
   *     occurred.
   * @private
   */
  lastUseToString_: function(time) {
    const nowSecondsFromEpoch = Math.round(Date.now() / 1000);
    return durationToString(nowSecondsFromEpoch - time);
  },

  /**
   * @param {?discards.mojom.SiteCharacteristicsFeature} feature The feature
   *     in question.
   * @return {string} A human-readable string representing the feature.
   * @private
   */
  featureToString_: function(feature) {
    if (!feature) {
      return 'N/A';
    }

    if (feature.useTimestamp) {
      const nowSecondsFromEpoch = Math.round(Date.now() / 1000);
      return 'Used ' +
          durationToString(nowSecondsFromEpoch - feature.useTimestamp);
    }

    if (feature.observationDuration) {
      return secondsToString(feature.observationDuration);
    }

    return 'N/A';
  },

  /**
   * @param {!Object} item The item to retrieve a load time estimate for.
   * @param {string} propertyName Name of the load time estimate to retrieve.
   * @return {string} The requested load time estimate or 'N/A' if
   *     unavailable.
   * @private
   */
  getLoadTimeEstimate_: function(item, propertyName) {
    return formatLoadTimeEstimate(item, propertyName);
  },

  /**
   * @param {number} value A value in units of kilobytes, or -1 indicating not
   *     available.
   * @return {string} A human readable string representing value.
   * @private
   */
  kilobytesToString_: function(value) {
    return value == -1 ? 'N/A' : kilobytesToString(value);
  },

  /**
   * @param {number} value A numeric value or -1, indicating not available.
   * @return {string} A human readable string representing value.
   * @private
   */
  optionalIntegerToString_: function(value) {
    return value == -1 ? 'N/A' : value.toString();
  },
});
