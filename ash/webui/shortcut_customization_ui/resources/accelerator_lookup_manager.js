// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {fakeActionNames} from './fake_data.js';
import {AcceleratorConfig, AcceleratorInfo, AcceleratorKeys, LayoutInfo, LayoutInfoList} from './shortcut_types.js';

/**
 * A singleton class that manages the fetched accelerators and layout
 * information from the backend service. All accelerator-related manipulation is
 * handled in this class.
 */
export class AcceleratorLookupManager {
  constructor() {
    /**
     * A map with the key set to a concatenated string of the accelerator's
     * '{source} - {action_id}', this concatenation uniquely identifies one
     * accelerator. The value is an array of AcceleratorInfo's associated to one
     * accelerator. This map serves as a way to quickly look up all
     * AcceleratorInfos for one accelerator.
     * @type {!Map<string, !Array<!AcceleratorInfo>>}
     * @private
     */
    this.acceleratorLookup_ = new Map();

    /**
     * A multi-layered map container. The top-most layer is a map with the key
     * as the accelerator's category (e.g. ChromeOS, Browser). The value of the
     * top-most map is another map in which the key is the accelerator's
     * subcategory (e.g. Window Management, Virtual Desk) and the value is an
     * Array of LayoutInfo. This map serves as a way to find all LayoutInfo's of
     * a given subsection of accelerators, where each LayoutInfo corresponds to
     * one AcceleratorRow.
     * @type {!Map<number, !Map<number, !Array<!LayoutInfo>>>}
     * @private
     */
    this.acceleratorLayoutLookup_ = new Map();

    /**
     * A map with the string key formatted as `${source_id}-${action_id}` and
     * the value as the string corresponding to the accelerator's name.
     * @type {!Map<string, string>}
     * @private
     */
    this.acceleratorNameLookup_ = new Map();

    /**
     * A map with the key as a stringified version of AcceleratorKey and the
     * value as the unique string identifier `${source_id}-${action_id}`. Note
     * that Javascript Maps uses the SameValueZero algorithm to compare keys,
     * meaning objects are compared by their references instead of their
     * intrinsic values, therefore this uses a stringified version of
     * AcceleratorKey as the key instead of the object itself. This is used to
     * perform a reverse lookup to detect if a given shortcut is already
     * bound to an accelerator.
     * @type {!Map<string, string>}
     */
    this.reverseAcceleratorLookup_ = new Map();
  }

  /**
   * @param {number} source
   * @param {number} action
   * @return {!Array<!AcceleratorInfo>}
   */
  getAccelerators(source, action) {
    const uuid = `${source}-${action}`;
    return this.acceleratorLookup_.get(uuid);
  }

  /**
   * @param {number} category
   * @param {number} sub_category
   * @return {!Array<!LayoutInfo>}
   */
  getAcceleratorLayout(category, sub_category) {
    return this.acceleratorLayoutLookup_.get(category).get(sub_category);
  }

  /**
   * @param {number} category
   * @return {!Map<number, !Array<!LayoutInfo>>}
   */
  getSubcategories(category) {
    return this.acceleratorLayoutLookup_.get(category);
  }

  /**
   * @param {number} source
   * @param {number} action
   * @return {string}
   */
  getAcceleratorName(source, action) {
    const uuid = `${source}-${action}`;
    return this.acceleratorNameLookup_.get(uuid);
  }

  /**
   * @param {string} keys
   * @return {string|undefined} Returns the uuid of an accelerator if the
   * accelerator exists. Otherwise returns `undefined`.
   */
  getAcceleratorFromKeys(keys) {
    return this.reverseAcceleratorLookup_.get(keys);
  }

  /** @param {!AcceleratorConfig} acceleratorConfig */
  setAcceleratorLookup(acceleratorConfig) {
    for (const [source, accelInfoMap] of acceleratorConfig.entries()) {
      for (const [actionId, accelInfos] of accelInfoMap.entries()) {
        const id = `${source}-${actionId}`;
        if (!this.acceleratorLookup_.has(id)) {
          this.acceleratorLookup_.set(id, []);
        }
        accelInfos.forEach((info) => {
          this.acceleratorLookup_.get(id).push(info);
          const accelKeys = info.accelerator;
          this.reverseAcceleratorLookup_.set(JSON.stringify(accelKeys), id);
        });
      }
    }
  }

  /** @param {!LayoutInfoList} layoutInfoList */
  setAcceleratorLayoutLookup(layoutInfoList) {
    for (const entry of layoutInfoList) {
      if (!this.acceleratorLayoutLookup_.has(entry.category)) {
        this.acceleratorLayoutLookup_.set(entry.category, new Map());
      }

      let subcatMap = this.acceleratorLayoutLookup_.get(entry.category);
      if (!subcatMap.has(entry.sub_category)) {
        subcatMap.set(entry.sub_category, []);
      }
      this.acceleratorLayoutLookup_.get(entry.category)
          .get(entry.sub_category)
          .push(entry);

      // Add the entry to the AcceleratorNameLookup.
      const uuid = `${entry.source}-${entry.action}`;
      // TODO(jimmyxgong): Use real name lookup instead of using fake_data.js.
      this.acceleratorNameLookup_.set(
          uuid, fakeActionNames.get(entry.description));
    }
  }

  reset() {
    this.acceleratorLookup_.clear();
    this.acceleratorLayoutLookup_.clear();
  }
}

addSingletonGetter(AcceleratorLookupManager);