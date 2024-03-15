// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_row.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './accelerator_subsection.html.js';
import {AcceleratorCategory, AcceleratorInfo, AcceleratorState, AcceleratorSubcategory, AcceleratorType, LayoutInfo} from './shortcut_types.js';
import {compareAcceleratorInfos, getSubcategoryNameStringId, isCustomizationAllowed} from './shortcut_utils.js';

/**
 * This interface is used to hold all the data needed by an
 * AcceleratorRowElement.
 */
interface AcceleratorRowData {
  acceleratorInfos: AcceleratorInfo[];
  layoutInfo: LayoutInfo;
}

export interface AcceleratorSubsectionElement {
  $: {
    list: DomRepeat,
  };
}

/**
 * @fileoverview
 * 'accelerator-subsection' is a wrapper component for a subsection of
 * shortcuts.
 */
const AcceleratorSubsectionElementBase = I18nMixin(PolymerElement);
export class AcceleratorSubsectionElement extends
    AcceleratorSubsectionElementBase {
  static get is(): string {
    return 'accelerator-subsection';
  }

  static get properties(): PolymerElementProperties {
    return {
      title: {
        type: String,
        value: '',
      },

      category: {
        type: Number,
        value: '',
      },

      subcategory: {
        type: Number,
        value: null,
        observer: AcceleratorSubsectionElement.prototype.onCategoryUpdated,
      },

      acceleratorContainer: {
        type: Array,
        value: [],
      },
    };
  }

  override title: string;
  category: AcceleratorCategory;
  subcategory: AcceleratorSubcategory;
  accelRowDataArray: AcceleratorRowData[];
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

  updateSubsection(): void {
    // Force the rendered list to reset, Polymer's dom-repeat does not perform
    // a deep check on objects so it won't detect changes to same size length
    // array of objects.
    this.set('acceleratorContainer', []);
    this.$.list.render();
    this.onCategoryUpdated();
  }

  protected onCategoryUpdated(): void {
    if (this.subcategory === null) {
      return;
    }

    // Fetch the layout infos based off of the subsection's category and
    // subcategory.
    const layoutInfos = this.lookupManager.getAcceleratorLayout(
        this.category, this.subcategory);

    this.title = this.i18n(getSubcategoryNameStringId(this.subcategory));

    // Use an atomic replacement instead of using Polymer's array manipulation
    // functions. Polymer's array manipulation functions batch all slices
    // updates as one which results in strange behaviors with updating
    // individual subsections. An atomic replacement makes ensures each
    // subsection's accelerators are kept distinct from each other.
    const tempAccelRowData: AcceleratorRowData[] = [];
    layoutInfos!.forEach((layoutInfo) => {
      if (this.lookupManager.isStandardAccelerator(layoutInfo.style)) {
        const acceleratorInfos =
            this.lookupManager
                .getStandardAcceleratorInfos(
                    layoutInfo.source, layoutInfo.action)
                .filter((accel) => {
                  // Hide accelerators that are default and disabled because the
                  // necessary keys aren't available on the keyboard.
                  return !(
                      accel.type === AcceleratorType.kDefault &&
                      (accel.state === AcceleratorState.kDisabledByUser ||
                       accel.state ===
                           AcceleratorState.kDisabledByUnavailableKeys));
                });
        // Do not hide empty accelerator rows if customization is enabled.
        if (!isCustomizationAllowed()) {
          if (acceleratorInfos.length === 0) {
            return;
          }
        }
        const accelRowData: AcceleratorRowData = {
          layoutInfo,
          acceleratorInfos,
        };
        tempAccelRowData.push(accelRowData);
      } else {
        tempAccelRowData.push({
          layoutInfo,
          acceleratorInfos: this.lookupManager.getTextAcceleratorInfos(
              layoutInfo.source, layoutInfo.action),
        });
      }
    });
    this.accelRowDataArray = tempAccelRowData;
  }

  // Sorts the accelerators so that they are displayed based off of a heuristic.
  protected getSortedAccelerators(accelerators: AcceleratorInfo[]):
      AcceleratorInfo[] {
    return accelerators.sort(compareAcceleratorInfos);
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  // Show lock icon next to subcategory if customization is enabled and the
  // category is locked.
  private shouldShowLockIcon(): boolean {
    if (!isCustomizationAllowed()) {
      return false;
    }
    return this.lookupManager.isSubcategoryLocked(this.subcategory);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-subsection': AcceleratorSubsectionElement;
  }
}

customElements.define(
    AcceleratorSubsectionElement.is, AcceleratorSubsectionElement);