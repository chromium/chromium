// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_subsection.js';
import '../css/shortcut_customization_shared.css.js';
import './shortcut_input.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {AcceleratorSubsectionElement} from './accelerator_subsection.js';
import {AcceleratorCategory, AcceleratorSubcategory} from './shortcut_types';
import {getTemplate} from './shortcuts_page.html.js';

/**
 * @fileoverview
 * 'shortcuts-page' is a generic page that is capable of rendering the
 * shortcuts for a specific category.
 *
 * TODO(jimmyxgong): Implement this skeleton element.
 */
export class ShortcutsPageElement extends PolymerElement {
  static get is(): string {
    return 'shortcuts-page';
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Implicit property from NavigationSelector. Contains one Number field,
       * |category|, that holds the category type of this shortcut page.
       */
      initialData: {
        type: Object,
      },

      subcategories: {
        type: Array,
        value: [],
      },
    };
  }

  initialData: {category: AcceleratorCategory};
  private subcategories: AcceleratorSubcategory[];
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

  override connectedCallback(): void {
    super.connectedCallback();
    this.updateAccelerators();
  }

  updateAccelerators(): void {
    const subcatMap =
        this.lookupManager.getSubcategories(this.initialData.category);
    if (subcatMap === undefined) {
      return;
    }

    const subcategories: number[] = [];
    for (const key of subcatMap.keys()) {
      subcategories.push(key);
    }
    this.subcategories = subcategories;
  }

  private getAllSubsections(): NodeListOf<AcceleratorSubsectionElement> {
    const subsections =
        this.shadowRoot!.querySelectorAll('accelerator-subsection');
    assert(subsections);
    return subsections;
  }

  updateSubsections(): void {
    for (const subsection of this.getAllSubsections()) {
      subsection.updateSubsection();
    }
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

customElements.define(ShortcutsPageElement.is, ShortcutsPageElement);
