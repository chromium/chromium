// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {fakeActionNames} from './fake_data.js';
import {AcceleratorConfig, AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, AcceleratorType, LayoutInfo, LayoutInfoList} from './shortcut_types.js';

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
          this.acceleratorLookup_.get(id).push(
              /** @type {!AcceleratorInfo} */(Object.assign({}, info)));
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

      const subcatMap = this.acceleratorLayoutLookup_.get(entry.category);
      if (!subcatMap.has(entry.sub_category)) {
        subcatMap.set(entry.sub_category, []);
      }
      this.acceleratorLayoutLookup_.get(entry.category)
          .get(entry.sub_category)
          .push(/** @type {!LayoutInfo} */(Object.assign({}, entry)));

      // Add the entry to the AcceleratorNameLookup.
      const uuid = `${entry.source}-${entry.action}`;
      // TODO(jimmyxgong): Use real name lookup instead of using fake_data.js.
      this.acceleratorNameLookup_.set(
          uuid, fakeActionNames.get(entry.description));
    }
  }

  /**
   * @param {AcceleratorSource} source
   * @param {number} action
   * @param {AcceleratorKeys} oldAccelerator
   * @param {AcceleratorKeys} newAccelerator
   */
  replaceAccelerator(source, action, oldAccelerator, newAccelerator) {
    const foundIdx =
        this.getAcceleratorInfoIndex_(source, action, oldAccelerator);

    if (foundIdx === -1) {
      // Should only be able to call this function with a valid
      // |oldAccelerator|.
      assertNotReached();
    }

    if (JSON.stringify(oldAccelerator) === JSON.stringify(newAccelerator)) {
      // Attempted to replace with the same accelerator.
      return;
    }

    // Check to see if there is a pre-existing accelerator to remove or disable
    // first.
    this.maybeRemoveOrDisableAccelerator_(newAccelerator);

    const accelInfos = this.getAccelerators(source, action);
    const currentAccelerator = accelInfos[foundIdx];

    // Handle the edge case in which the user is attempting to replace an
    // existing accelerator with a disabled default accelerator.
    if (this.maybeReenableDefaultAccelerator(accelInfos, newAccelerator)) {
      // User replaced a non-default accelerator with a default accelerator.
      // Remove the non-default accelerator.
      accelInfos.splice(foundIdx, 1);
    } else {
      // If the old accelerator is a default accelerator, disable it and add a
      // new accelerator.
      if (currentAccelerator.type === AcceleratorType.kDefault) {
        // The default accelerator should be disabled.
        currentAccelerator.state = AcceleratorState.kDisabledByUser;

        this.addAccelerator(source, action, newAccelerator);
      } else {
        // Update the old accelerator with the new one.
        currentAccelerator.accelerator = newAccelerator;
      }
    }

    // Update the reverse look up maps.
    this.reverseAcceleratorLookup_
        .set(JSON.stringify(newAccelerator), `${source}-${action}`);
    this.reverseAcceleratorLookup_.delete(JSON.stringify(oldAccelerator));
  }

  /**
   * @param {AcceleratorSource} source
   * @param {number} action
   * @param {AcceleratorKeys} newAccelerator
   */
  addAccelerator(source, action, newAccelerator) {
    // Check to see if there is a pre-existing accelerator to remove first.
    this.maybeRemoveOrDisableAccelerator_(newAccelerator);

    // Get the matching accelerator and add the new accelerator to its
    // container.
    const accelInfos = this.getAccelerators(source, action);

    // Handle edge case in which the user attempts to add a disabled default
    // accelerator.
    const addedDefault =
        this.maybeReenableDefaultAccelerator(accelInfos, newAccelerator);

    if (!addedDefault) {
      // No matching default accelerator, add the new accelerator directly.
      const newAccelInfo = /** @type {!AcceleratorInfo} */ ({
        accelerator: newAccelerator,
        type: AcceleratorType.kUserDefined,
        state: AcceleratorState.kEnabled,
        locked: false,
      });
      accelInfos.push(newAccelInfo);
    }

    // Update the reverse look up maps.
    this.reverseAcceleratorLookup_.set(
        JSON.stringify(newAccelerator), `${source}-${action}`);
  }

  /**
   * @param {!AcceleratorSource} source
   * @param {number} action
   * @param {AcceleratorKeys} keys
   */
  removeAccelerator(source, action, keys) {
    const foundAccel = this.getAcceleratorInfoFromKeys_(source, action, keys);

    // Can only remove an existing accelerator.
    assert(foundAccel != null);

    // Remove from reverse lookup.
    this.reverseAcceleratorLookup_.delete(JSON.stringify(keys));

    // Default accelerators are only disabled, not removed.
    if (foundAccel.type === AcceleratorType.kDefault) {
      foundAccel.state = AcceleratorState.kDisabledByUser;
      return;
    }

    if (foundAccel.locked) {
      // Not possible to remove a locked accelerator manually.
      assertNotReached();
    }

    const accelInfos = this.getAccelerators(source, action);
    const foundIdx = this.getAcceleratorInfoIndex_(source, action, keys);
    // Remove accelerator from main map.
    accelInfos.splice(foundIdx, 1);
  }

  /**
   * @param {!Array<!AcceleratorInfo>} accelInfos
   * @param {!AcceleratorKeys} accelerator
   * @return {boolean} returns true if `accelerator` is a default accelerator
   * and has been re-enabled.
   */
  maybeReenableDefaultAccelerator(accelInfos, accelerator) {
    // Check if `accelerator` matches a default accelerator.
    const defaultIdx = accelInfos.findIndex(accel => {
      return accel.type === AcceleratorType.kDefault &&
          JSON.stringify(accel.accelerator) === JSON.stringify(accelerator);
    });

    if (defaultIdx === -1) {
      return false;
    }

    // Re-enable the default accelerator.
    accelInfos[defaultIdx].state = AcceleratorState.kEnabled;

    return true;
  }

  /**
   * @param {!AcceleratorSource} source
   * @param {number} action
   * @param {!AcceleratorKeys} keys
   * @return {boolean}
   */
  isAcceleratorLocked(source, action, keys) {
    const accel = this.getAcceleratorInfoFromKeys_(source, action, keys);
    assert(accel);

    return accel.locked;
  }

  /**
   * Called to either remove or disable (if locked) an accelerator.
   * @param {!AcceleratorKeys} accelKeys
   * @private
   */
  maybeRemoveOrDisableAccelerator_(accelKeys) {
    const uuid = this.getAcceleratorFromKeys(JSON.stringify(accelKeys));
    if (uuid === undefined) {
      // Not replacing a pre-existing accelerator.
      return;
    }

    // Split '{source}-{action}` into [source][action].
    const uuidSplit = uuid.split('-');
    const source = /** @type {AcceleratorSource}*/ (parseInt(uuidSplit[0], 10));
    const action = parseInt(uuidSplit[1], 10);
    const accelInfos = this.getAccelerators(source, action);
    const foundIdx = this.getAcceleratorInfoIndex_(source, action, accelKeys);

    const accelerator = accelInfos[foundIdx];
    assert(accelerator);

    // Cannot remove a locked accelerator.
    if (accelInfos[foundIdx].locked) {
      return;
    }

    // Default accelerators are only disabled, not removed.
    if (accelerator.type === AcceleratorType.kDefault) {
      accelerator.state = AcceleratorState.kDisabledByUser;
      return;
    }

    // Otherwise, remove the accelerator.
    accelInfos.splice(foundIdx, 1);
  }

  /**
   * @param {AcceleratorSource} source
   * @param {number} action
   * @param {AcceleratorKeys} acceleratorKeys
   * @return {number} the index of the AcceleratorInfo with the matching
   * |acceleratorKeys| in |acceleratorLookup|. Returns -1 if no match can be
   * found.
   * @private
   */
  getAcceleratorInfoIndex_(source, action, acceleratorKeys) {
    // Stingify the Object so that it compared to other objects.
    const accelKey = JSON.stringify(acceleratorKeys);
    const accelInfos = this.getAccelerators(source, action);
    for (let i = 0; i < accelInfos.length; ++i) {
      const accelCompare = JSON.stringify(accelInfos[i].accelerator);
      if (accelKey === accelCompare) {
        return i;
      }
    }
    return -1;
  }

  /**
   * @param {!AcceleratorSource} source
   * @param {number} action
   * @param {!AcceleratorKeys} keys
   * @return {?AcceleratorInfo}
   */
  getAcceleratorInfoFromKeys_(source, action, keys) {
    const foundIdx = this.getAcceleratorInfoIndex_(source, action, keys);

    if (foundIdx === -1) {
      return null;
    }

    const accelInfos = this.getAccelerators(source, action);
    return accelInfos[foundIdx];
  }

  reset() {
    this.acceleratorLookup_.clear();
    this.acceleratorNameLookup_.clear();
    this.acceleratorLayoutLookup_.clear();
    this.reverseAcceleratorLookup_.clear();
  }
}

addSingletonGetter(AcceleratorLookupManager);
