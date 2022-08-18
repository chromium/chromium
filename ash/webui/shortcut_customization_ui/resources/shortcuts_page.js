// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_subsection.js';
import './shortcut_customization_shared.css.js';
import './shortcut_input.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './shortcuts_page.html.js';

/**
 * @fileoverview
 * 'shortcuts-page' is a generic page that is capable of rendering the
 * shortcuts for a specific category.
 *
 * TODO(jimmyxgong): Implement this skeleton element.
 */
export class ShortcutsPageElement extends PolymerElement {
  static get is() {
    return 'shortcuts-page';
  }

  static get properties() {
    return {
      /**
       * Implicit property from NavigationSelector. Contains one Number field,
       * |category|, that holds the category type of this shortcut page.
       */
      initialData: {
        type: Object,
        value: () => {},
      },

      /**
       * @type {!Array<number>}
       * @private
       */
      subcategories_: {
        type: Array,
        value: [],
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!AcceleratorLookupManager} */
    this.lookupManager_ = AcceleratorLookupManager.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.updateAccelerators();
  }

  updateAccelerators() {
    const subcatMap =
        this.lookupManager_.getSubcategories(this.initialData.category);
    if (subcatMap === undefined) {
      return;
    }

    const subcategories = [];
    for (const key of subcatMap.keys()) {
      subcategories.push(key);
    }
    this.subcategories_ = subcategories;
  }

  updateSubsections() {
    const subsections =
        this.shadowRoot.querySelectorAll('accelerator-subsection');
    for (const subsection of subsections) {
      subsection.updateSubsection();
    }
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(ShortcutsPageElement.is, ShortcutsPageElement);
