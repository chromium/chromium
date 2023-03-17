// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {mojoString16ToString} from './mojo_utils.js';
import {Accelerator, AcceleratorCategory, AcceleratorId, AcceleratorInfo, AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, LayoutInfo, MojoAcceleratorConfig, MojoAcceleratorInfo, MojoLayoutInfo, StandardAcceleratorInfo, TextAcceleratorInfo} from './shortcut_types.js';
import {areAcceleratorsEqual, getAccelerator, getAcceleratorId, isStandardAcceleratorInfo, isTextAcceleratorInfo} from './shortcut_utils.js';

// Convert from Mojo types to the app types.
function createSanitizedAccelInfo(info: MojoAcceleratorInfo):
    StandardAcceleratorInfo {
  assert(isStandardAcceleratorInfo(info));
  const {locked, state, type, layoutProperties} = info;
  const sanitizedAccelerator: Accelerator = {
    keyCode: layoutProperties.standardAccelerator.accelerator.keyCode,
    modifiers: layoutProperties.standardAccelerator.accelerator.modifiers,
  };
  return {
    locked,
    state,
    type,
    layoutProperties: {
      standardAccelerator: {
        accelerator: sanitizedAccelerator,
        keyDisplay: mojoString16ToString(
            layoutProperties.standardAccelerator.keyDisplay),
      },
    },
  };
}

/** The name of an {@link Accelerator}, e.g. "Snap Window Left". */
type AcceleratorName = string;
/**
 * The key used to lookup {@link AcceleratorId}s from a
 * {@link ReverseAcceleratorLookupMap}.
 * See getKeyForLookup() in this file for the implementation details.
 */
type AcceleratorLookupKey = string;
type StandardAcceleratorLookupMap =
    Map<AcceleratorId, StandardAcceleratorInfo[]>;
type TextAcceleratorLookupMap = Map<AcceleratorId, TextAcceleratorInfo[]>;

type ReverseAcceleratorLookupMap = Map<AcceleratorLookupKey, AcceleratorId>;

/**
 * A singleton class that manages the fetched accelerators and layout
 * information from the backend service. All accelerator-related manipulation is
 * handled in this class.
 */
export class AcceleratorLookupManager {
  private layoutInfoProvider = new LayoutInfoProvider();
  /**
   * A map with the key set to a concatenated string of the accelerator's
   * '{source}-{action_id}', this concatenation uniquely identifies one
   * accelerator. The value is an array of StandardAcceleratorInfo's
   * associated to one accelerator. This map serves as a way to quickly look up
   * all StandardAcceleratorInfos for one accelerator.
   */
  private standardAcceleratorLookup: StandardAcceleratorLookupMap = new Map();

  /**
   * A map with the key set to a concatenated string of the accelerator's
   * '{source}-{action_id}', this concatenation uniquely identifies one
   * accelerator. The value is a TextAcceleratorInfo associated to one
   * accelerator.
   */
  private textAcceleratorLookup: TextAcceleratorLookupMap = new Map();


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
  private reverseAcceleratorLookup: ReverseAcceleratorLookupMap = new Map();

  // Determine whether the keyboard has a launcher button or a search button. It
  // is used to display the 'meta' key with correct icon.
  private hasLauncherButton: boolean = false;

  /**
   * Used to generate the keys for the ReverseAcceleratorLookupMap.
   */
  private getKeyForLookup(accelerator: Accelerator): AcceleratorLookupKey {
    return JSON.stringify(
        {keyCode: accelerator.keyCode, modifiers: accelerator.modifiers});
  }

  getStandardAcceleratorInfos(source: number|string, action: number|string):
      StandardAcceleratorInfo[] {
    const uuid: AcceleratorId = getAcceleratorId(source, action);
    const acceleratorInfos = this.standardAcceleratorLookup.get(uuid);
    assert(acceleratorInfos);
    return acceleratorInfos;
  }

  getTextAcceleratorInfos(source: number|string, action: number|string):
      TextAcceleratorInfo[] {
    const uuid: AcceleratorId = getAcceleratorId(source, action);
    const acceleratorInfos = this.textAcceleratorLookup.get(uuid);
    assert(acceleratorInfos);
    return acceleratorInfos;
  }

  isStandardAccelerator(source: number|string, action: number|string): boolean {
    return this.standardAcceleratorLookup.has(getAcceleratorId(source, action));
  }

  getAcceleratorLayout(
      category: AcceleratorCategory,
      subCategory: AcceleratorSubcategory): LayoutInfo[] {
    return this.layoutInfoProvider.getAcceleratorLayout(category, subCategory);
  }

  getSubcategories(category: AcceleratorCategory):
      Map<AcceleratorSubcategory, LayoutInfo[]>|undefined {
    return this.layoutInfoProvider.getSubcategories(category);
  }

  getAcceleratorName(source: number|string, action: number|string):
      AcceleratorName {
    return this.layoutInfoProvider.getAcceleratorName(source, action);
  }

  /**
   * Returns the uuid of an accelerator if the
   * accelerator exists. Otherwise returns `undefined`.
   */
  getAcceleratorIdFromReverseLookup(accelerator: Accelerator): AcceleratorId
      |undefined {
    return this.reverseAcceleratorLookup.get(this.getKeyForLookup(accelerator));
  }

  initializeLookupIdForStandardAccelerator(source: string, actionId: string):
      void {
    const id = getAcceleratorId(source, actionId);
    if (!this.standardAcceleratorLookup.has(id)) {
      this.standardAcceleratorLookup.set(id, []);
    }
  }

  initializeLookupIdForTextAccelerator(source: string, actionId: string): void {
    const id = getAcceleratorId(source, actionId);
    if (!this.textAcceleratorLookup.has(id)) {
      this.textAcceleratorLookup.set(id, []);
    }
  }

  setAcceleratorLookup(acceleratorConfig: MojoAcceleratorConfig): void {
    // Reset the lookup maps every time we update the accelerator mappings.
    this.reverseAcceleratorLookup.clear();
    this.standardAcceleratorLookup.clear();

    for (const [source, accelInfoMap] of Object.entries(acceleratorConfig)) {
      // When calling Object.entries on an object with optional enum keys,
      // TypeScript considers the values to be possibly undefined.
      // This guard lets us use this value later as if it were not undefined.
      if (!accelInfoMap) {
        continue;
      }
      for (const [actionId, accelInfos] of Object.entries(accelInfoMap)) {
        accelInfos.forEach((info: MojoAcceleratorInfo) => {
          if (isTextAcceleratorInfo(info)) {
            this.initializeLookupIdForTextAccelerator(source, actionId);
            this.getTextAcceleratorInfos(source, actionId).push({...info});
          } else {
            assert(isStandardAcceleratorInfo(info));
            this.initializeLookupIdForStandardAccelerator(source, actionId);
            const sanitizedAccelInfo = createSanitizedAccelInfo(info);
            this.reverseAcceleratorLookup.set(
                this.getKeyForLookup(sanitizedAccelInfo.layoutProperties
                                         .standardAccelerator.accelerator),
                getAcceleratorId(source, actionId));
            this.getStandardAcceleratorInfos(source, actionId)
                .push({...sanitizedAccelInfo});
          }
        });
      }
    }
  }

  setAcceleratorLayoutLookup(layoutInfoList: MojoLayoutInfo[]): void {
    this.layoutInfoProvider.initializeLayoutInfo(layoutInfoList);
  }

  setHasLauncherButton(hasLauncherButton: boolean): void {
    this.hasLauncherButton = hasLauncherButton;
  }

  getHasLauncherButton(): boolean {
    return this.hasLauncherButton;
  }

  replaceAccelerator(
      source: AcceleratorSource, action: number, oldAccelerator: Accelerator,
      newAccelInfo: StandardAcceleratorInfo): void {
    const foundIdx =
        this.getAcceleratorInfoIndex(source, action, oldAccelerator);

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
    this.maybeRemoveOrDisableAccelerator(getAccelerator(newAccelInfo));

    const accelInfos = this.getStandardAcceleratorInfos(source, action);
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
    this.reverseAcceleratorLookup.set(
        this.getKeyForLookup(getAccelerator(newAccelInfo)),
        getAcceleratorId(source, action));
    this.reverseAcceleratorLookup.delete(this.getKeyForLookup(oldAccelerator));
  }

  addAccelerator(
      source: AcceleratorSource, action: number,
      newAccelInfo: StandardAcceleratorInfo): void {
    // Check to see if there is a pre-existing accelerator to remove first.
    this.maybeRemoveOrDisableAccelerator(getAccelerator(newAccelInfo));

    // Get the matching accelerator and add the new accelerator to its
    // container.
    const accelInfos = this.getStandardAcceleratorInfos(source, action);

    // Handle edge case in which the user attempts to add a disabled default
    // accelerator.
    const addedDefault = this.maybeReenableDefaultAccelerator(
        accelInfos, getAccelerator(newAccelInfo));

    if (!addedDefault) {
      // No matching default accelerator, add the new accelerator directly.
      accelInfos.push(newAccelInfo);
    }

    // Update the reverse look up maps.
    this.reverseAcceleratorLookup.set(
        this.getKeyForLookup(getAccelerator(newAccelInfo)),
        getAcceleratorId(source, action));
  }

  removeAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): void {
    const foundAccel =
        this.getAcceleratorInfoFromAccelerator(source, action, accelerator);

    // Can only remove an existing accelerator.
    assert(foundAccel != null);

    // Remove from reverse lookup.
    this.reverseAcceleratorLookup.delete(this.getKeyForLookup(accelerator));

    // Default accelerators are only disabled, not removed.
    if (foundAccel!.type === AcceleratorType.kDefault) {
      foundAccel!.state = AcceleratorState.kDisabledByUser;
      return;
    }

    if (foundAccel!.locked) {
      // Not possible to remove a locked accelerator manually.
      assertNotReached();
    }

    const accelInfos = this.getStandardAcceleratorInfos(source, action);
    const foundIdx = this.getAcceleratorInfoIndex(source, action, accelerator);
    // Remove accelerator from main map.
    accelInfos.splice(foundIdx, 1);
  }

  /**
   * Returns true if `accelerator` is a default accelerator
   * and has been re-enabled.
   */
  maybeReenableDefaultAccelerator(
      accelInfos: StandardAcceleratorInfo[],
      accelerator: Accelerator): boolean {
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
        this.getAcceleratorInfoFromAccelerator(source, action, accelerator);
    assert(accel);

    return accel.locked;
  }

  /**
   * Called to either remove or disable (if locked) an accelerator.
   */
  private maybeRemoveOrDisableAccelerator(accelerator: Accelerator): void {
    const uuid = this.getAcceleratorIdFromReverseLookup(accelerator);
    if (uuid === undefined) {
      // Not replacing a pre-existing accelerator.
      return;
    }

    // Split '{source}-{action}` into [source][action].
    const uuidSplit = uuid.split('-');
    const source: AcceleratorSource = parseInt(uuidSplit[0], 10);
    const action = parseInt(uuidSplit[1], 10);
    const accelInfos = this.getStandardAcceleratorInfos(source, action);
    const foundIdx = this.getAcceleratorInfoIndex(source, action, accelerator);

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
  private getAcceleratorInfoIndex(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): number {
    const accelInfos = this.getStandardAcceleratorInfos(source, action);
    for (let i = 0; i < accelInfos.length; ++i) {
      if (areAcceleratorsEqual(accelerator, getAccelerator(accelInfos[i]))) {
        return i;
      }
    }
    return -1;
  }

  private getAcceleratorInfoFromAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): AcceleratorInfo|null {
    const foundIdx = this.getAcceleratorInfoIndex(source, action, accelerator);

    if (foundIdx === -1) {
      return null;
    }

    const accelInfos = this.getStandardAcceleratorInfos(source, action);
    return accelInfos[foundIdx];
  }

  reset(): void {
    this.standardAcceleratorLookup.clear();
    this.textAcceleratorLookup.clear();
    this.layoutInfoProvider.resetLookupMaps();
    this.reverseAcceleratorLookup.clear();
  }


  static getInstance(): AcceleratorLookupManager {
    return managerInstance ||
        (managerInstance = new AcceleratorLookupManager());
  }

  static setInstance(obj: AcceleratorLookupManager): void {
    managerInstance = obj;
  }
}

let managerInstance: AcceleratorLookupManager|null = null;


function createSanitizedLayoutInfo(entry: MojoLayoutInfo): LayoutInfo {
  return {...entry, description: mojoString16ToString(entry.description)};
}

type AcceleratorLayoutLookupMap =
    Map<AcceleratorCategory, Map<AcceleratorSubcategory, LayoutInfo[]>>;
type AcceleratorNameLookupMap = Map<AcceleratorId, AcceleratorName>;

interface LayoutProviderInterface {
  getAcceleratorLayout(
      category: AcceleratorCategory,
      subCategory: AcceleratorSubcategory): LayoutInfo[];
  getSubcategories(category: AcceleratorCategory):
      Map<AcceleratorSubcategory, LayoutInfo[]>|undefined;
  getAcceleratorName(source: number|string, action: number|string):
      AcceleratorName;
  initializeLayoutInfo(layoutInfoList: MojoLayoutInfo[]): void;
  resetLookupMaps(): void;
}

// Responsible for initializing/maintaining layout information for
// accelerators.
class LayoutInfoProvider implements LayoutProviderInterface {
  /**
   * A multi-layered map container. The top-most layer is a map with the key
   * as the accelerator's category (e.g. Tabs & Windows, Page & Web Browser).
   * The value of the top-most map is another map in which the key is the
   * accelerator's subcategory (e.g. System Controls, System Apps) and the value
   * is an Array of LayoutInfo. This map serves as a way to find all
   * LayoutInfo's of a given subsection of accelerators, where each LayoutInfo
   * corresponds to one AcceleratorRow.
   */
  private acceleratorLayoutLookup: AcceleratorLayoutLookupMap = new Map();
  /**
   * A map with the string key formatted as `${source_id}-${action_id}` and
   * the value as the string corresponding to the accelerator's name.
   */
  private acceleratorNameLookup: AcceleratorNameLookupMap = new Map();

  getAcceleratorLayout(
      category: AcceleratorCategory,
      subCategory: AcceleratorSubcategory): LayoutInfo[] {
    const categoryMap = this.acceleratorLayoutLookup.get(category);
    assert(categoryMap);
    const subCategoryMap = categoryMap.get(subCategory);
    assert(subCategoryMap);
    return subCategoryMap;
  }

  getSubcategories(category: AcceleratorCategory):
      Map<AcceleratorSubcategory, LayoutInfo[]>|undefined {
    return this.acceleratorLayoutLookup.get(category);
  }

  getAcceleratorName(source: number|string, action: number|string):
      AcceleratorName {
    const uuid: AcceleratorId = getAcceleratorId(source, action);
    const acceleratorName = this.acceleratorNameLookup.get(uuid);
    assert(acceleratorName);
    return acceleratorName;
  }

  initializeLayoutInfo(layoutInfoList: MojoLayoutInfo[]): void {
    this.initializeCategoryMaps(layoutInfoList);
    for (const entry of layoutInfoList) {
      // The Accelerator layout table doesn't currently contain any
      // developer/debug accelerators. Once they are added, we need to
      // check if they should be shown or not. This assert is to ensure that
      // this case is handled once developer/debug accelerators are added.
      assert(
          entry.category !== AcceleratorCategory.kDebug &&
          entry.category !== AcceleratorCategory.kDeveloper);
      const layoutInfo = createSanitizedLayoutInfo(entry);
      this.getAcceleratorLayout(entry.category, entry.subCategory)
          .push(layoutInfo);
      this.addEntrytoAcceleratorNameLookup(
          getAcceleratorId(entry.source, entry.action), layoutInfo.description);
    }
  }

  initializeCategoryMaps(layoutInfoList: MojoLayoutInfo[]): void {
    for (const entry of layoutInfoList) {
      if (!this.acceleratorLayoutLookup.has(entry.category)) {
        this.acceleratorLayoutLookup.set(entry.category, new Map());
      }

      const subcatMap = this.acceleratorLayoutLookup.get(entry.category);
      if (!subcatMap!.has(entry.subCategory)) {
        subcatMap!.set(entry.subCategory, []);
      }
    }
  }

  private addEntrytoAcceleratorNameLookup(uuid: string, description: string):
      void {
    this.acceleratorNameLookup.set(uuid, description);
  }

  resetLookupMaps(): void {
    this.acceleratorLayoutLookup.clear();
    this.acceleratorNameLookup.clear();
  }
}
