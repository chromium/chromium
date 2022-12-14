// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {mojoString16ToString} from './mojo_utils.js';
import {Accelerator, AcceleratorCategory, AcceleratorId, AcceleratorInfo, AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, DefaultAcceleratorInfo, LayoutInfo, MojoAcceleratorConfig, MojoAcceleratorInfo, MojoLayoutInfo, TextAcceleratorInfo} from './shortcut_types.js';
import {areAcceleratorsEqual, getAccelerator, getAcceleratorId, isDefaultAcceleratorInfo, isTextAcceleratorInfo} from './shortcut_utils.js';

/** The name of an {@link Accelerator}, e.g. "Snap Window Left". */
type AcceleratorName = string;
/**
 * The key used to lookup {@link AcceleratorId}s from a
 * {@link ReverseAcceleratorLookupMap}.
 * See getKeyForLookup() in this file for the implementation details.
 */
type AcceleratorLookupKey = string;
type AcceleratorLookupMap = Map<AcceleratorId, DefaultAcceleratorInfo[]>;
type AcceleratorLayoutLookupMap =
    Map<AcceleratorCategory, Map<AcceleratorSubcategory, LayoutInfo[]>>;
type AcceleratorNameLookupMap = Map<AcceleratorId, AcceleratorName>;
type ReverseAcceleratorLookupMap = Map<AcceleratorLookupKey, AcceleratorId>;

/**
 * A singleton class that manages the fetched accelerators and layout
 * information from the backend service. All accelerator-related manipulation is
 * handled in this class.
 */
export class AcceleratorLookupManager {
  /**
   * A map with the key set to a concatenated string of the accelerator's
   * '{source}-{action_id}', this concatenation uniquely identifies one
   * accelerator. The value is an array of AcceleratorInfo's associated to one
   * accelerator. This map serves as a way to quickly look up all
   * AcceleratorInfos for one accelerator.
   */
  private acceleratorLookup_: AcceleratorLookupMap = new Map();
  /**
   * A multi-layered map container. The top-most layer is a map with the key
   * as the accelerator's category (e.g. Tabs & Windows, Page & Web Browser).
   * The value of the top-most map is another map in which the key is the
   * accelerator's subcategory (e.g. System Controls, System Apps) and the value
   * is an Array of LayoutInfo. This map serves as a way to find all
   * LayoutInfo's of a given subsection of accelerators, where each LayoutInfo
   * corresponds to one AcceleratorRow.
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

  /**
   * Used to generate the keys for the ReverseAcceleratorLookupMap.
   */
  private getKeyForLookup(accelerator: Accelerator): AcceleratorLookupKey {
    return JSON.stringify(
        {keyCode: accelerator.keyCode, modifiers: accelerator.modifiers});
  }

  getAcceleratorInfos(source: number|string, action: number|string):
      DefaultAcceleratorInfo[] {
    const uuid: AcceleratorId = getAcceleratorId(source, action);
    const acceleratorInfos = this.acceleratorLookup_.get(uuid);
    assert(acceleratorInfos);
    return acceleratorInfos;
  }

  getAcceleratorLayout(
      category: AcceleratorCategory,
      subCategory: AcceleratorSubcategory): LayoutInfo[] {
    const categoryMap = this.acceleratorLayoutLookup_.get(category);
    assert(categoryMap);
    const subCategoryMap = categoryMap.get(subCategory);
    assert(subCategoryMap);
    return subCategoryMap;
  }

  getSubcategories(category: AcceleratorCategory):
      Map<AcceleratorSubcategory, LayoutInfo[]>|undefined {
    return this.acceleratorLayoutLookup_.get(category);
  }

  getAcceleratorName(source: number|string, action: number|string):
      AcceleratorName {
    const uuid: AcceleratorId = getAcceleratorId(source, action);
    const acceleratorName = this.acceleratorNameLookup_.get(uuid);
    assert(acceleratorName);
    return acceleratorName;
  }

  /**
   * Returns the uuid of an accelerator if the
   * accelerator exists. Otherwise returns `undefined`.
   */
  getAcceleratorIdFromReverseLookup(accelerator: Accelerator): AcceleratorId
      |undefined {
    return this.reverseAcceleratorLookup_.get(
        this.getKeyForLookup(accelerator));
  }

  setAcceleratorLookup(acceleratorConfig: MojoAcceleratorConfig) {
    for (const [source, accelInfoMap] of Object.entries(acceleratorConfig)) {
      // When calling Object.entries on an object with optional enum keys,
      // TypeScript considers the values to be possibly undefined.
      // This guard lets us use this value later as if it were not undefined.
      if (!accelInfoMap) {
        continue;
      }
      for (const [actionId, accelInfos] of Object.entries(accelInfoMap)) {
        const id = getAcceleratorId(source, actionId);
        if (!this.acceleratorLookup_.has(id)) {
          this.acceleratorLookup_.set(id, []);
        }
        accelInfos.forEach((info: MojoAcceleratorInfo) => {
          // Convert from Mojo types to the app types.
          let sanitizedAccelInfo: DefaultAcceleratorInfo|TextAcceleratorInfo;
          if (isTextAcceleratorInfo(info)) {
            sanitizedAccelInfo = {
              layoutProperties: {
                textAccelerator: {
                  textAccelerator: [],
                },
              },
              locked: info.locked,
              state: info.state,
              type: info.type,
            };
          } else {
            assert(isDefaultAcceleratorInfo(info));
            const sanitizedAccelerator: Accelerator = {
              keyCode:
                  info.layoutProperties.defaultAccelerator.accelerator.keyCode,
              modifiers: info.layoutProperties.defaultAccelerator.accelerator
                             .modifiers,
            };
            sanitizedAccelInfo = {
              layoutProperties: {
                defaultAccelerator: {
                  accelerator: sanitizedAccelerator,
                  keyDisplay: mojoString16ToString(
                      info.layoutProperties.defaultAccelerator.keyDisplay),
                },
              },
              locked: info.locked,
              state: info.state,
              type: info.type,
            };
            this.reverseAcceleratorLookup_.set(
                this.getKeyForLookup(
                    info.layoutProperties.defaultAccelerator.accelerator),
                id);
          }
          this.getAcceleratorInfos(source, actionId)
              .push(Object.assign(
                  {}, sanitizedAccelInfo as DefaultAcceleratorInfo));
        });
      }
    }
  }

  setAcceleratorLayoutLookup(layoutInfoList: MojoLayoutInfo[]) {
    for (const entry of layoutInfoList) {
      // This check is necessary because the layout info list may contain
      // references to accelerators that are not always present
      // (e.g. developer or debug mode accelerators).
      const acceleratorExists = this.acceleratorLookup_.has(
          getAcceleratorId(entry.source, entry.action));
      if (!acceleratorExists) {
        continue;
      }

      if (!this.acceleratorLayoutLookup_.has(entry.category)) {
        this.acceleratorLayoutLookup_.set(entry.category, new Map());
      }

      const subcatMap = this.acceleratorLayoutLookup_.get(entry.category);
      if (!subcatMap!.has(entry.subCategory)) {
        subcatMap!.set(entry.subCategory, []);
      }

      const sanitizedLayoutInfo: LayoutInfo = {
        source: entry.source,
        style: entry.style,
        description: mojoString16ToString(entry.description),
        category: entry.category,
        subCategory: entry.subCategory,
        action: entry.action,
      };

      this.getAcceleratorLayout(entry.category, entry.subCategory)
          .push(sanitizedLayoutInfo);

      // Add the entry to the AcceleratorNameLookup.
      const uuid = getAcceleratorId(entry.source, entry.action);
      this.acceleratorNameLookup_.set(uuid, sanitizedLayoutInfo.description);
    }
  }

  replaceAccelerator(
      source: AcceleratorSource, action: number, oldAccelerator: Accelerator,
      newAccelInfo: DefaultAcceleratorInfo) {
    const foundIdx =
        this.getAcceleratorInfoIndex_(source, action, oldAccelerator);

    if (foundIdx === -1) {
      // Should only be able to call this function with a valid
      // |oldAccelerator|.
      assertNotReached();
    }

    if (areAcceleratorsEqual(oldAccelerator, getAccelerator(newAccelInfo))) {
      // Attempted to replace with the same accelerator.
      return;
    }

    // Check to see if there is a pre-existing accelerator to remove or disable
    // first.
    this.maybeRemoveOrDisableAccelerator_(getAccelerator(newAccelInfo));

    const accelInfos = this.getAcceleratorInfos(source, action);
    const currentAccelerator = accelInfos[foundIdx];

    // Handle the edge case in which the user is attempting to replace an
    // existing accelerator with a disabled default accelerator.
    if (this.maybeReenableDefaultAccelerator(
            accelInfos, getAccelerator(newAccelInfo))) {
      // User replaced a non-default accelerator with a default accelerator.
      // Remove the non-default accelerator.
      accelInfos.splice(foundIdx, 1);
    } else {
      // If the old accelerator is a default accelerator, disable it and add a
      // new accelerator.
      if (currentAccelerator.type === AcceleratorType.kDefault) {
        // The default accelerator should be disabled.
        currentAccelerator.state = AcceleratorState.kDisabledByUser;

        this.addAccelerator(source, action, newAccelInfo);
      } else {
        // Replace previous AcceleratorInfo with the new one.
        accelInfos[foundIdx] = newAccelInfo;
      }
    }

    // Update the reverse look up maps.
    this.reverseAcceleratorLookup_.set(
        this.getKeyForLookup(getAccelerator(newAccelInfo)),
        getAcceleratorId(source, action));
    this.reverseAcceleratorLookup_.delete(this.getKeyForLookup(oldAccelerator));
  }

  addAccelerator(
      source: AcceleratorSource, action: number,
      newAccelInfo: DefaultAcceleratorInfo) {
    // Check to see if there is a pre-existing accelerator to remove first.
    this.maybeRemoveOrDisableAccelerator_(getAccelerator(newAccelInfo));

    // Get the matching accelerator and add the new accelerator to its
    // container.
    const accelInfos = this.getAcceleratorInfos(source, action);

    // Handle edge case in which the user attempts to add a disabled default
    // accelerator.
    const addedDefault = this.maybeReenableDefaultAccelerator(
        accelInfos, getAccelerator(newAccelInfo));

    if (!addedDefault) {
      // No matching default accelerator, add the new accelerator directly.
      accelInfos.push(newAccelInfo);
    }

    // Update the reverse look up maps.
    this.reverseAcceleratorLookup_.set(
        this.getKeyForLookup(getAccelerator(newAccelInfo)),
        getAcceleratorId(source, action));
  }

  removeAccelerator(
      source: AcceleratorSource, action: number, accelerator: Accelerator) {
    const foundAccel =
        this.getAcceleratorInfoFromAccelerator_(source, action, accelerator);

    // Can only remove an existing accelerator.
    assert(foundAccel != null);

    // Remove from reverse lookup.
    this.reverseAcceleratorLookup_.delete(this.getKeyForLookup(accelerator));

    // Default accelerators are only disabled, not removed.
    if (foundAccel!.type === AcceleratorType.kDefault) {
      foundAccel!.state = AcceleratorState.kDisabledByUser;
      return;
    }

    if (foundAccel!.locked) {
      // Not possible to remove a locked accelerator manually.
      assertNotReached();
    }

    const accelInfos = this.getAcceleratorInfos(source, action);
    const foundIdx = this.getAcceleratorInfoIndex_(source, action, accelerator);
    // Remove accelerator from main map.
    accelInfos.splice(foundIdx, 1);
  }

  /**
   * Returns true if `accelerator` is a default accelerator
   * and has been re-enabled.
   */
  maybeReenableDefaultAccelerator(
      accelInfos: DefaultAcceleratorInfo[], accelerator: Accelerator): boolean {
    // Check if `accelerator` matches a default accelerator.
    const defaultIdx = accelInfos.findIndex(accelInfo => {
      return accelInfo.type === AcceleratorType.kDefault &&
          areAcceleratorsEqual(getAccelerator(accelInfo), accelerator);
    });

    if (defaultIdx === -1) {
      return false;
    }

    // Re-enable the default accelerator.
    accelInfos[defaultIdx].state = AcceleratorState.kEnabled;

    return true;
  }

  isAcceleratorLocked(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): boolean {
    const accel =
        this.getAcceleratorInfoFromAccelerator_(source, action, accelerator);
    assert(accel);

    return accel.locked;
  }

  /**
   * Called to either remove or disable (if locked) an accelerator.
   */
  private maybeRemoveOrDisableAccelerator_(accelerator: Accelerator) {
    const uuid = this.getAcceleratorIdFromReverseLookup(accelerator);
    if (uuid === undefined) {
      // Not replacing a pre-existing accelerator.
      return;
    }

    // Split '{source}-{action}` into [source][action].
    const uuidSplit = uuid.split('-');
    const source: AcceleratorSource = parseInt(uuidSplit[0], 10);
    const action = parseInt(uuidSplit[1], 10);
    const accelInfos = this.getAcceleratorInfos(source, action);
    const foundIdx = this.getAcceleratorInfoIndex_(source, action, accelerator);

    const foundAccel = accelInfos[foundIdx];
    assert(foundAccel);

    // Cannot remove a locked accelerator.
    if (accelInfos[foundIdx].locked) {
      return;
    }

    // Default accelerators are only disabled, not removed.
    if (foundAccel.type === AcceleratorType.kDefault) {
      foundAccel.state = AcceleratorState.kDisabledByUser;
      return;
    }

    // Otherwise, remove the accelerator.
    accelInfos.splice(foundIdx, 1);
  }

  /**
   * The index of the AcceleratorInfo with the matching
   * |accelerator| in |acceleratorLookup|. Returns -1 if no match can be
   * found.
   */
  private getAcceleratorInfoIndex_(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): number {
    const accelInfos = this.getAcceleratorInfos(source, action);
    for (let i = 0; i < accelInfos.length; ++i) {
      if (areAcceleratorsEqual(accelerator, getAccelerator(accelInfos[i]))) {
        return i;
      }
    }
    return -1;
  }

  private getAcceleratorInfoFromAccelerator_(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): AcceleratorInfo|null {
    const foundIdx = this.getAcceleratorInfoIndex_(source, action, accelerator);

    if (foundIdx === -1) {
      return null;
    }

    const accelInfos = this.getAcceleratorInfos(source, action);
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
