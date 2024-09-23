// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './database_tab.html.js';
import {boolToString, durationToString, getOrCreateSiteDataProvider, secondsToString} from './discards.js';
import type {SiteDataDatabaseSize, SiteDataEntry, SiteDataFeature, SiteDataProviderRemote} from './site_data.mojom-webui.js';
import {SortedTableMixin} from './sorted_table_mixin.js';

/**
 * Compares two db rows by their origin.
 * @param a The first value being compared.
 * @param b The second value being compared.
 * @return A negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
function compareRowsByOrigin(a: SiteDataEntry, b: SiteDataEntry): number {
  return a.origin.localeCompare(b.origin);
}

/**
 * Compares two db rows by their dirty bit.
 * @param a The first value being compared.
 * @param b The second value being compared.
 * @return A negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
function compareRowsByIsDirty(a: SiteDataEntry, b: SiteDataEntry): number {
  return (a.isDirty ? 1 : 0) - (b.isDirty ? 1 : 0);
}

/**
 * Compares two db rows by their last load time.
 * @param a The first value being compared.
 * @param b The second value being compared.
 * @return A negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
function compareRowsByLastLoaded(a: SiteDataEntry, b: SiteDataEntry): number {
  return a.value!.lastLoaded - b.value!.lastLoaded;
}

/**
 * Compares two db rows by their CPU usage.
 * @param a The first value being compared.
 * @param b The second value being compared.
 * @return A negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
function compareRowsByCpuUsage(a: SiteDataEntry, b: SiteDataEntry): number {
  const keyA =
      a.value!.loadTimeEstimates ? a.value!.loadTimeEstimates.avgCpuUsageUs : 0;
  const keyB =
      b.value!.loadTimeEstimates ? b.value!.loadTimeEstimates.avgCpuUsageUs : 0;
  return keyA - keyB;
}

/**
 * Compares two db rows by their memory usage.
 * @param a The first value being compared.
 * @param b The second value being compared.
 * @return A negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
function compareRowsByMemoryUsage(a: SiteDataEntry, b: SiteDataEntry): number {
  const keyA = a.value!.loadTimeEstimates ?
      a.value!.loadTimeEstimates.avgFootprintKb :
      0;
  const keyB = b.value!.loadTimeEstimates ?
      b.value!.loadTimeEstimates.avgFootprintKb :
      0;
  return keyA - keyB;
}

/**
 * Compares two db rows by their load duration.
 * @param a The first value being compared.
 * @param b The second value being compared.
 * @return A negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
function compareRowsByLoadDuration(a: SiteDataEntry, b: SiteDataEntry): number {
  const keyA = a.value!.loadTimeEstimates ?
      a.value!.loadTimeEstimates.avgLoadDurationUs :
      0;
  const keyB = b.value!.loadTimeEstimates ?
      b.value!.loadTimeEstimates.avgLoadDurationUs :
      0;
  return keyA - keyB;
}

/**
 * @param sortKey The sort key to get a function for.
 * @return {function(SiteDataEntry, SiteDataEntry): number}
 *     A comparison function that compares two tab infos, returns
 *     negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
function getSortFunctionForKey(sortKey: string): (
    a: SiteDataEntry, b: SiteDataEntry) => number {
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
 * @param time A time in microseconds.
 * @return A friendly, human readable string representing the input
 *    time with units.
 */
function microsecondsToString(time: number): string {
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
 * @param value A memory amount in kilobytes.
 * @return A friendly, human readable string representing the input
 *    time with units.
 */
function kilobytesToString(value: number): string {
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
 * @param item The item to retrieve a load time estimate for.
 * @param propertyName Name of the load time estimate to retrieve.
 * @return The requested load time estimate or 'N/A' if unavailable.
 */
function formatLoadTimeEstimate(
    item: SiteDataEntry, propertyName: string): string {
  if (!item.value || !item.value.loadTimeEstimates) {
    return 'N/A';
  }

  const value =
      (item.value.loadTimeEstimates as unknown as
       {[key: string]: number})[propertyName];
  if (propertyName.endsWith('Us')) {
    return microsecondsToString(value);
  } else if (propertyName.endsWith('Kb')) {
    return kilobytesToString(value);
  }
  return value.toString();
}

interface DatabaseTabElement {
  $: {
    addOriginInput: CrInputElement,
  };
}

const DatabaseTabElementBase = SortedTableMixin(PolymerElement);
class DatabaseTabElement extends DatabaseTabElementBase {
  static get is() {
    return 'database-tab';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of database rows.
       */
      rows_: Array,

      /**
       * The database size response.
       */
      size_: {
        type: Object,
        value: {numRows: -1, onDiskSizeKb: -1},
      },

      /**
       * An origin that can be added to requestedOrigins_ by onAddOriginClick_.
       */
      newOrigin_: String,
    };
  }

  private rows_: SiteDataEntry[]|null;
  private size_: SiteDataDatabaseSize;
  private newOrigin_: string;

  private updateTableTimer_: number = 0;
  private updateSizesTimer_: number = 0;
  private requestedOrigins_: {[key: string]: boolean} = {};
  private siteDataProvider_: SiteDataProviderRemote|null = null;

  override connectedCallback() {
    this.setSortKey('origin');
    this.requestedOrigins_ = {};
    this.siteDataProvider_ = getOrCreateSiteDataProvider();

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
  }

  override disconnectedCallback() {
    // Clear the update timers to avoid memory leaks.
    clearInterval(this.updateTableTimer_);
    this.updateTableTimer_ = 0;
    clearInterval(this.updateSizesTimer_);
    this.updateSizesTimer_ = 0;
  }

  /**
   * Issues a request for the data and renders on response.
   */
  private updateDbRows_() {
    this.siteDataProvider_!
        .getSiteDataArray(Object.keys(this.requestedOrigins_))
        .then(response => {
          // Bail if the SiteData database is turned off.
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
  }

  /**
   * Adds the current new origin to requested origins and starts an update.
   */
  private addNewOrigin_() {
    this.requestedOrigins_[this.newOrigin_] = true;
    this.newOrigin_ = '';
    this.updateDbRows_();
  }

  /**
   * An on-click handler that adds the current new origin to requested
   * origins.
   */
  private onAddOriginClick_() {
    this.addNewOrigin_();

    // Set the focus back to the input field for convenience.
    this.$.addOriginInput.focus();
  }

  /**
   * A key-down handler that adds the current new origin to requested origins.
   */
  private onOriginKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && this.isValidOrigin_(this.newOrigin_)) {
      this.addNewOrigin_();
      e.stopPropagation();
    }
  }

  /** Issues a request for the database sizes and renders on response. */
  private updateDbSizes_() {
    this.siteDataProvider_!.getSiteDataDatabaseSize().then(response => {
      // Bail if the SiteData database is turned off.
      if (!response.dbSize) {
        return;
      }
      this.size_ = response.dbSize;
    });
  }

  /**
   * Returns a sort function to compare tab infos based on the provided sort
   * key and a boolean reverse flag.
   * @param sortKey The sort key for the  returned function.
   * @param sortReverse True if sorting is reversed.
   * @return A comparison function that compares two tab infos, returns
   *     negative number if a < b, 0 if a === b, and a positive
   *     number if a > b.
   */
  private computeSortFunction_(sortKey: string, sortReverse: boolean):
      (a: SiteDataEntry, b: SiteDataEntry) => number {
    // Polymer 2 may invoke multi-property observers before all properties
    // are defined.
    if (!sortKey) {
      return (_a, _b) => 0;
    }

    const sortFunction = getSortFunctionForKey(sortKey);
    return (a, b) => {
      const comp = sortFunction(a, b);
      return sortReverse ? -comp : comp;
    };
  }

  /**
   * @param origin A potentially valid origin string.
   * @return Whether the origin is valid.
   */
  private isValidOrigin_(origin: string): boolean {
    const re = /(https?|ftp):\/\/[a-z+.]/;

    return re.test(origin);
  }

  /**
   * @param origin A potentially valid origin string.
   * @return Whether the origin is valid or empty.
   */
  private isEmptyOrValidOrigin_(origin: string): boolean {
    return !origin || this.isValidOrigin_(origin);
  }

  /**
   * @param value The value to convert.
   * @return A display string representing value.
   */
  private boolToString_(value: boolean): string {
    return boolToString(value);
  }

  /**
   * @param time Time in seconds since epoch.
   * @return A user-friendly string explaining how long ago time
   *     occurred.
   */
  private lastUseToString_(time: number): string {
    const nowSecondsFromEpoch = Math.round(Date.now() / 1000);
    return durationToString(nowSecondsFromEpoch - time);
  }

  /**
   * @param feature The feature in question.
   * @return A human-readable string representing the feature.
   */
  private featureToString_(feature: SiteDataFeature|null): string {
    if (!feature) {
      return 'N/A';
    }

    if (feature.useTimestamp) {
      const nowSecondsFromEpoch = Math.round(Date.now() / 1000);
      return 'Used ' +
          durationToString(
                 Number(BigInt(nowSecondsFromEpoch) - feature.useTimestamp));
    }

    if (feature.observationDuration) {
      return secondsToString(Number(feature.observationDuration));
    }

    return 'N/A';
  }

  /**
   * @param item The item to retrieve a load time estimate for.
   * @param propertyName Name of the load time estimate to retrieve.
   * @return The requested load time estimate or 'N/A' if
   *     unavailable.
   */
  private getLoadTimeEstimate_(item: SiteDataEntry, propertyName: string):
      string {
    return formatLoadTimeEstimate(item, propertyName);
  }

  /**
   * @param value A value in units of kilobytes, or -1 indicating not
   *     available.
   * @return A human readable string representing value.
   */
  private kilobytesToString_(value: number): string {
    return value === -1 ? 'N/A' : kilobytesToString(value);
  }

  /**
   * @param value A numeric value or -1, indicating not available.
   * @return A human readable string representing value.
   */
  private optionalIntegerToString_(value: number): string {
    return value === -1 ? 'N/A' : value.toString();
  }
}

customElements.define(DatabaseTabElement.is, DatabaseTabElement);
