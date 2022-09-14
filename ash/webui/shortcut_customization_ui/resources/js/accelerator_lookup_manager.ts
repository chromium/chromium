// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {fakeActionNames} from './fake_data.js';
import {AcceleratorConfig, AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, AcceleratorType, LayoutInfo, LayoutInfoList} from './shortcut_types.js';

type AcceleratorLookupMap = Map<string, AcceleratorInfo[]>;
type AcceleratorLayoutLookupMap = Map<number, Map<number, LayoutInfo[]>>;
type AcceleratorNameLookupMap = Map<string, string>;
type ReverseAcceleratorLookupMap = Map<string, string>;

/**
 * A singleton class that manages the fetched accelerators and layout
 * information from the backend service. All accelerator-related manipulation is
 * handled in this class.
 */
export class AcceleratorLookupManager {
  /**
   * A map with the key set to a concatenated string of the accelerator's
   * '{source} - {action_id}', this concatenation uniquely identifies one
   * accelerator. The value is an array of AcceleratorInfo's associated to one
   * accelerator. This map serves as a way to quickly look up all
   * AcceleratorInfos for one accelerator.
   */
  private acceleratorLookup_: AcceleratorLookupMap = new Map();
  /**
   * A multi-layered map container. The top-most layer is a map with the key
   * as the accelerator's category (e.g. ChromeOS, Browser). The value of the
   * top-most map is another map in which the key is the accelerator's
   * subcategory (e.g. Window Management, Virtual Desk) and the value is an
   * Array of LayoutInfo. This map serves as a way to find all LayoutInfo's of
   * a given subsection of accelerators, where each LayoutInfo corresponds to
   * one AcceleratorRow.
   */
  private acceleratorLayoutLookup_: AcceleratorLayoutLookupMap = new Map();
  /**
   * A map with the string key formatted as `${source_id}-${action_id}` and
   * the value as the string corresponding to the accelerator's name.
   */
  private acceleratorNameLookup_: AcceleratorNameLookupMap = new Map();
  /**
   * A map with the key as a stringified version of AcceleratorKey and the
   * value as the unique string identifier `${source_id}-${action_id}`. Note
   * that Javascript Maps uses the SameValueZero algorithm to compare keys,
   * meaning objects are compared by their references instead of their
   * intrinsic values, therefore this uses a stringified version of
   * AcceleratorKey as the key instead of the object itself. This is used to
   * perform a reverse lookup to detect if a given shortcut is already
   * bound to an accelerator.
   */
  private reverseAcceleratorLookup_: ReverseAcceleratorLookupMap = new Map();

  getAccelerators(source: number, action: number): AcceleratorInfo[] {
    const uuid = `${source}-${action}`;
    const accelerator = this.acceleratorLookup_.get(uuid);
    assert(accelerator);
    return accelerator;
  }

  getAcceleratorLayout(category: number, subCategory: number): LayoutInfo[] {
    const categoryMap = this.acceleratorLayoutLookup_.get(category);
    assert(categoryMap);
    const subCategoryMap = categoryMap.get(subCategory);
    assert(subCategoryMap);
    return subCategoryMap;
  }

  getSubcategories(category: number): Map<number, LayoutInfo[]>|undefined {
    return this.acceleratorLayoutLookup_.get(category);
  }

  getAcceleratorName(source: number, action: number): string {
    const uuid = `${source}-${action}`;
    const acceleratorName = this.acceleratorNameLookup_.get(uuid);
    assert(acceleratorName);
    return acceleratorName;
  }

  /**
   * Returns the uuid of an accelerator if the
   * accelerator exists. Otherwise returns `undefined`.
   */
  getAcceleratorFromKeys(keys: string): string|undefined {
    return this.reverseAcceleratorLookup_.get(keys);
  }

  setAcceleratorLookup(acceleratorConfig: AcceleratorConfig) {
    for (const [source, accelInfoMap] of acceleratorConfig.entries()) {
      for (const [actionId, accelInfos] of accelInfoMap.entries()) {
        const id = `${source}-${actionId}`;
        if (!this.acceleratorLookup_.has(id)) {
          this.acceleratorLookup_.set(id, []);
        }
        accelInfos.forEach((info: AcceleratorInfo) => {
          this.getAccelerators(source, actionId).push(Object.assign({}, info));
          const accelKeys = info.accelerator;
          this.reverseAcceleratorLookup_.set(JSON.stringify(accelKeys), id);
        });
      }
    }
  }

  setAcceleratorLayoutLookup(layoutInfoList: LayoutInfoList) {
    for (const entry of layoutInfoList) {
      if (!this.acceleratorLayoutLookup_.has(entry.category)) {
        this.acceleratorLayoutLookup_.set(entry.category, new Map());
      }

      const subcatMap = this.acceleratorLayoutLookup_.get(entry.category);
      if (!subcatMap!.has(entry.sub_category)) {
        subcatMap!.set(entry.sub_category, []);
      }

      this.getAcceleratorLayout(entry.category, entry.sub_category)
          .push(Object.assign({}, entry));

      // Add the entry to the AcceleratorNameLookup.
      const uuid = `${entry.source}-${entry.action}`;
      // TODO(jimmyxgong): Use real name lookup instead of using fake_data.js.
      this.acceleratorNameLookup_.set(
          uuid, fakeActionNames.get(entry.description) as string);
    }
  }

  replaceAccelerator(
      source: AcceleratorSource, action: number,
      oldAccelerator: AcceleratorKeys, newAccelerator: AcceleratorKeys) {
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
      if (currentAccelerator.type === AcceleratorType.DEFAULT) {
        // The default accelerator should be disabled.
        currentAccelerator.state = AcceleratorState.DISABLED_BY_USER;

        this.addAccelerator(source, action, newAccelerator);
      } else {
        // Update the old accelerator with the new one.
        currentAccelerator.accelerator = newAccelerator;
      }
    }

    // Update the reverse look up maps.
    this.reverseAcceleratorLookup_.set(
        JSON.stringify(newAccelerator), `${source}-${action}`);
    this.reverseAcceleratorLookup_.delete(JSON.stringify(oldAccelerator));
  }

  addAccelerator(
      source: AcceleratorSource, action: number,
      newAccelerator: AcceleratorKeys) {
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
      const newAccelInfo: AcceleratorInfo = {
        accelerator: newAccelerator,
        type: AcceleratorType.USER_DEFINED,
        state: AcceleratorState.ENABLED,
        locked: false,
      };
      accelInfos.push(newAccelInfo);
    }

    // Update the reverse look up maps.
    this.reverseAcceleratorLookup_.set(
        JSON.stringify(newAccelerator), `${source}-${action}`);
  }

  removeAccelerator(
      source: AcceleratorSource, action: number, keys: AcceleratorKeys) {
    const foundAccel = this.getAcceleratorInfoFromKeys_(source, action, keys);

    // Can only remove an existing accelerator.
    assert(foundAccel != null);

    // Remove from reverse lookup.
    this.reverseAcceleratorLookup_.delete(JSON.stringify(keys));

    // Default accelerators are only disabled, not removed.
    if (foundAccel!.type === AcceleratorType.DEFAULT) {
      foundAccel!.state = AcceleratorState.DISABLED_BY_USER;
      return;
    }

    if (foundAccel!.locked) {
      // Not possible to remove a locked accelerator manually.
      assertNotReached();
    }

    const accelInfos = this.getAccelerators(source, action);
    const foundIdx = this.getAcceleratorInfoIndex_(source, action, keys);
    // Remove accelerator from main map.
    accelInfos.splice(foundIdx, 1);
  }

  /**
   * Returns true if `accelerator` is a default accelerator
   * and has been re-enabled.
   */
  maybeReenableDefaultAccelerator(
      accelInfos: AcceleratorInfo[], accelerator: AcceleratorKeys): boolean {
    // Check if `accelerator` matches a default accelerator.
    const defaultIdx = accelInfos.findIndex(accel => {
      return accel.type === AcceleratorType.DEFAULT &&
          JSON.stringify(accel.accelerator) === JSON.stringify(accelerator);
    });

    if (defaultIdx === -1) {
      return false;
    }

    // Re-enable the default accelerator.
    accelInfos[defaultIdx].state = AcceleratorState.ENABLED;

    return true;
  }

  isAcceleratorLocked(
      source: AcceleratorSource, action: number,
      keys: AcceleratorKeys): boolean {
    const accel = this.getAcceleratorInfoFromKeys_(source, action, keys);
    assert(accel);

    return accel.locked;
  }

  /**
   * Called to either remove or disable (if locked) an accelerator.
   */
  private maybeRemoveOrDisableAccelerator_(accelKeys: AcceleratorKeys) {
    const uuid = this.getAcceleratorFromKeys(JSON.stringify(accelKeys));
    if (uuid === undefined) {
      // Not replacing a pre-existing accelerator.
      return;
    }

    // Split '{source}-{action}` into [source][action].
    const uuidSplit = uuid.split('-');
    const source: AcceleratorSource = parseInt(uuidSplit[0], 10);
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
    if (accelerator.type === AcceleratorType.DEFAULT) {
      accelerator.state = AcceleratorState.DISABLED_BY_USER;
      return;
    }

    // Otherwise, remove the accelerator.
    accelInfos.splice(foundIdx, 1);
  }

  /**
   * The index of the AcceleratorInfo with the matching
   * |acceleratorKeys| in |acceleratorLookup|. Returns -1 if no match can be
   * found.
   */
  private getAcceleratorInfoIndex_(
      source: AcceleratorSource, action: number,
      acceleratorKeys: AcceleratorKeys): number {
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

  private getAcceleratorInfoFromKeys_(
      source: AcceleratorSource, action: number,
      keys: AcceleratorKeys): AcceleratorInfo|null {
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


  static getInstance(): AcceleratorLookupManager {
    return managerInstance ||
        (managerInstance = new AcceleratorLookupManager());
  }

  static setInstance(obj: AcceleratorLookupManager) {
    managerInstance = obj;
  }
}

let managerInstance: AcceleratorLookupManager|null = null;
