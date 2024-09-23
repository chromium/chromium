// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';

import {Accelerator, AcceleratorCategory, AcceleratorId, AcceleratorSource, AcceleratorSubcategory, LayoutInfo, LayoutStyle, MetaKey, MojoAcceleratorConfig, MojoAcceleratorInfo, MojoLayoutInfo, StandardAcceleratorInfo, TextAcceleratorInfo} from './shortcut_types.js';
import {getAcceleratorId, getSourceAndActionFromAcceleratorId, isStandardAcceleratorInfo, isTextAcceleratorInfo} from './shortcut_utils.js';

// Convert from Mojo types to the app types.
function createSanitizedAccelInfo(info: MojoAcceleratorInfo):
    StandardAcceleratorInfo {
  assert(isStandardAcceleratorInfo(info));
  const {acceleratorLocked, locked, state, type, layoutProperties} = info;
  const sanitizedAccelerator: Accelerator = {
    keyCode: layoutProperties.standardAccelerator.accelerator.keyCode,
    modifiers: layoutProperties.standardAccelerator.accelerator.modifiers,
    keyState: layoutProperties.standardAccelerator.accelerator.keyState,
  };
  const originalAccelerator =
      layoutProperties.standardAccelerator?.originalAccelerator;
  let sanitizedOriginalAccelerator: Accelerator|undefined = undefined;
  if (originalAccelerator) {
    sanitizedOriginalAccelerator = {
      keyCode: originalAccelerator.keyCode,
      modifiers: originalAccelerator.modifiers,
      keyState: layoutProperties.standardAccelerator.accelerator.keyState,
    };
  }

  return {
    acceleratorLocked,
    locked,
    state,
    type,
    layoutProperties: {
      standardAccelerator: {
        accelerator: sanitizedAccelerator,
        keyDisplay: mojoString16ToString(
            layoutProperties.standardAccelerator.keyDisplay),
        originalAccelerator: sanitizedOriginalAccelerator,
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

  // An enum including Search, Launcher and LauncherRefresh to display the
  // keyboard 'meta' key with correct icon.
  private metaKey: MetaKey = MetaKey.kSearch;

  // Determine if a search result row is currently focused. This ensures the
  // focused row stays highlighted as the search result, despite mouse hover
  // actions.
  private searchResultRowFocused: boolean = false;

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

  isStandardAccelerator(style: number|string): boolean {
    return style === LayoutStyle.kDefault;
  }

  isStandardAcceleratorById(id: AcceleratorId): boolean {
    return this.standardAcceleratorLookup.has(id);
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

  getAcceleratorSubcategory(source: number|string, action: number|string):
      AcceleratorSubcategory {
    return this.layoutInfoProvider.getAcceleratorSubcategory(source, action);
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
    this.textAcceleratorLookup.clear();

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

  setMetaKeyToDisplay(metaKey: MetaKey): void {
    this.metaKey = metaKey;
  }

  getMetaKeyToDisplay(): MetaKey {
    return this.metaKey;
  }

  setSearchResultRowFocused(searchResultRowFocused: boolean): void {
    this.searchResultRowFocused = searchResultRowFocused;
  }

  getSearchResultRowFocused(): boolean {
    return this.searchResultRowFocused;
  }

  isSubcategoryLocked(subcategory: AcceleratorSubcategory): boolean {
    const acceleratorIds =
        this.layoutInfoProvider.getAcceleratorIdsBySubcategory(subcategory);

    for (const acceleratorId of acceleratorIds) {
      // Skip TextAccelerators as they are always locked.
      if (!this.isStandardAcceleratorById(acceleratorId)) {
        continue;
      }
      const {source, action} =
          getSourceAndActionFromAcceleratorId(acceleratorId);
      const acceleratorInfos = this.getStandardAcceleratorInfos(source, action);

      for (const acceleratorInfo of acceleratorInfos) {
        // Return false early when accelerator is editable, which is when
        // acceleratorInfo is not locked and source is kAsh(Only ash
        // accelerator is editable).
        if (!acceleratorInfo.locked && source === AcceleratorSource.kAsh) {
          return false;
        }
      }
    }
    return true;
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
type AcceleratorSubcategoryLookupMap =
    Map<AcceleratorId, AcceleratorSubcategory>;
type AcceleratorIdsBySubcategoryLookupMap =
    Map<AcceleratorSubcategory, AcceleratorId[]>;

interface LayoutProviderInterface {
  getAcceleratorLayout(
      category: AcceleratorCategory,
      subCategory: AcceleratorSubcategory): LayoutInfo[];
  getSubcategories(category: AcceleratorCategory):
      Map<AcceleratorSubcategory, LayoutInfo[]>|undefined;
  getAcceleratorName(source: number|string, action: number|string):
      AcceleratorName;
  getAcceleratorSubcategory(source: number|string, action: number|string):
      AcceleratorSubcategory;
  getAcceleratorIdsBySubcategory(subcategory: AcceleratorSubcategory):
      AcceleratorId[];
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
  /**
   * A map with the string key formatted as `${source_id}-${action_id}` and
   * the value corresponding to the accelerator's subcategory.
   */
  private acceleratorSubcategoryLookup: AcceleratorSubcategoryLookupMap =
      new Map();
  /**
   * A map with the key "subcategory" and the value corresponding to the
   * accelerators under the subcategory.
   */
  private acceleratorIdsBySubcategoryLookup:
      AcceleratorIdsBySubcategoryLookupMap = new Map();

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

  getAcceleratorSubcategory(source: number|string, action: number|string):
      AcceleratorSubcategory {
    const uuid: AcceleratorId = getAcceleratorId(source, action);
    const acceleratorSubcategory = this.acceleratorSubcategoryLookup.get(uuid);
    // The value of 'acceleratorCategory' could possibly be '0' (representing
    // 'kGeneralControls'). So we should only assert that it's not 'undefined'.
    assert(acceleratorSubcategory !== undefined);
    return acceleratorSubcategory;
  }

  getAcceleratorIdsBySubcategory(subcategory: AcceleratorSubcategory):
      AcceleratorId[] {
    const acceleratorIds =
        this.acceleratorIdsBySubcategoryLookup.get(subcategory);
    assert(acceleratorIds);
    return acceleratorIds;
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

      const acceleratorId = getAcceleratorId(entry.source, entry.action);
      this.addEntryToAcceleratorNameLookup(
          acceleratorId, layoutInfo.description);
      this.addEntryToAcceleratorSubcategoryLookup(
          acceleratorId, entry.subCategory);
      this.addEntryToAcceleratorsBySubcategoryLookup(
          acceleratorId, entry.subCategory);
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

  private addEntryToAcceleratorNameLookup(uuid: string, description: string):
      void {
    this.acceleratorNameLookup.set(uuid, description);
  }

  private addEntryToAcceleratorSubcategoryLookup(
      uuid: string, subcategory: AcceleratorSubcategory): void {
    this.acceleratorSubcategoryLookup.set(uuid, subcategory);
  }

  private addEntryToAcceleratorsBySubcategoryLookup(
      uuid: string, subcategory: AcceleratorSubcategory): void {
    const acceleratorIds =
        this.acceleratorIdsBySubcategoryLookup.get(subcategory) || [];
    acceleratorIds.push(uuid);
    this.acceleratorIdsBySubcategoryLookup.set(subcategory, acceleratorIds);
  }

  resetLookupMaps(): void {
    this.acceleratorLayoutLookup.clear();
    this.acceleratorNameLookup.clear();
    this.acceleratorSubcategoryLookup.clear();
    this.acceleratorIdsBySubcategoryLookup.clear();
  }
}
