// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_row.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {fakeSubCategories} from './fake_data.js';
import {AcceleratorInfo, AcceleratorState, AcceleratorType} from './shortcut_types.js';

/**
 * @fileoverview
 * 'accelerator-subsection' is a wrapper component for a subsection of
 * shortcuts.
 */
export class AcceleratorSubsectionElement extends PolymerElement {
  static get is() {
    return 'accelerator-subsection';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
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
        observer: 'onCategoryUpdated_',
      },

      /**
       * TODO(jimmyxgong): Fetch the shortcuts and it accelerators with the
       * mojom::source_id and mojom::subsection_id. This serves as a
       * temporary way to populate a subsection.
       * @type {Array<!Object<string, number, number,
       *     Array<!AcceleratorInfo>>>}
       */
      acceleratorContainer: {
        type: Array,
        value: [],
      }
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!AcceleratorLookupManager} */
    this.lookupManager_ = AcceleratorLookupManager.getInstance();
  }

  updateSubsection() {
    // Force the rendered list to reset, Polymer's dom-repeat does not perform
    // a deep check on objects so it won't detect changes to same size length
    // array of objects.
    this.set('acceleratorContainer', []);
    this.$.list.render();
    this.onCategoryUpdated_();
  }

  /** @protected */
  onCategoryUpdated_() {
    if (this.subcategory === null) {
      return;
    }

    // Fetch the layout infos based off of the subsection's category and
    // subcategory.
    const layoutInfos = this.lookupManager_.getAcceleratorLayout(
        this.category, this.subcategory);
    assert(!!layoutInfos);

    // TODO(jimmyxgong): Fetch real string for title once available.
    this.title = fakeSubCategories.get(this.subcategory);

    // Use an atomic replacement instead of using Polymer's array manipulation
    // functions. Polymer's array manipulation functions batch all slices
    // updates as one which results in strange behaviors with updating
    // individual subsections. An atomic replacement makes ensures each
    // subsection's accelerators are kept distinct from each other.
    const tempAccelContainer = [];
    layoutInfos.forEach((value) => {
      const acceleratorInfos =
          this.lookupManager_.getAccelerators(value.source, value.action)
              .filter((accel) => {
                // Hide accelerators that are default and disabled.
                return !(accel.type === AcceleratorType.kDefault &&
                    accel.state === AcceleratorState.kDisabledByUser);
              });
      const accel =
          /**@type {!Object<string, number, number, Array<!AcceleratorInfo>>}*/
          ({
            description: this.lookupManager_.getAcceleratorName(
                value.source, value.action),
            action: value.action,
            source: value.source,
            acceleratorInfos: acceleratorInfos,
          });
      tempAccelContainer.push(accel);
    });
    this.acceleratorContainer = tempAccelContainer;
  }
}

customElements.define(AcceleratorSubsectionElement.is,
                      AcceleratorSubsectionElement);