// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {AcceleratorConfig, AcceleratorInfo, LayoutInfo, LayoutInfoList} from './shortcut_types.js';

/**
 * A singleton class that manages the fetched accelerators and layout
 * information from the backend service. All accelerator-related manipulation is
 * handled in this class.
 */
export class AcceleratorLookupManager {
  constructor() {
    /**
     * A map with the key set to a concatenated string of the accelerator's
     * '{souce} - {action_id}', this concatenation uniquely identifies one
     * accelerator. The value is a Set of AcceleratorInfo's associated to one
     * accelerator. This map serves as a way to quickly look up all
     * AcceleratorInfos for one accelerator.
     * @type {!Map<string, !Set<!AcceleratorInfo>>}
     * @private
     */
    this.acceleratorLookup_ = new Map();

    /**
     * A multi-layered map container. The top-most layer is a map with the key
     * as the accelerator's category (e.g. ChromeOS, Browser). The value of the
     * top-most map is another map in which the key is the accelerator's
     * subcategory (e.g. Window Management, Virtual Desk) and the value is a
     * Set of LayoutInfo. This map serves as a way to find all LayoutInfo's of
     * a given subsection of accelerators, where each LayoutInfo corresponds to
     * one AcceleratorRow.
     * @type {!Map<number, !Map<number, !Set<!LayoutInfo>>>}
     * @private
     */
    this.acceleratorLayoutLookup_ = new Map();
  }

  /** @return {!Map<string, !Set<!AcceleratorInfo>>} */
  get acceleratorLookup() {
    return this.acceleratorLookup_;
  }

  /** @type {!Map<number, !Map<number, !Set<!LayoutInfo>>>} */
  get acceleratorLayoutLookup() {
    return this.acceleratorLayoutLookup_;
  }

  /** @param {!AcceleratorConfig} acceleratorConfig */
  setAcceleratorLookup(acceleratorConfig) {
    for (const [source, accelInfoMap] of acceleratorConfig.entries()) {
      for (const [actionId, accelInfos] of accelInfoMap.entries()) {
        const id = `${source}-${actionId}`;
        if (!this.acceleratorLookup_.has(id)) {
          this.acceleratorLookup_.set(id, new Set());
        }
        accelInfos.forEach((info) => {
          this.acceleratorLookup_.get(id).add(info);
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
        subcatMap.set(entry.sub_category, new Set());
      }
      this.acceleratorLayoutLookup_.get(entry.category)
          .get(entry.sub_category)
          .add(entry);
    }
  }
}

addSingletonGetter(AcceleratorLookupManager);