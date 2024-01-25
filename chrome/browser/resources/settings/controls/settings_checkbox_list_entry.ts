// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-checkbox-list-entry' is a wrapper for cr-checkbox so
 * that it can have the correct accessibility behavior while inside an
 * iron-list. Because cr-checkbox passes its focus to its inner checkbox
 * element, screen readers are unable to infer a parent-child relationship
 * between the list element and the focused checkbox. As a result using the
 * roles listbox/option and annotating with aria-setsize/aria-posinset will not
 * work properly.
 *
 * To fix this 'settings-checkbox-list-entry' hijacks focus and prevents it from
 * going into the inner element, so that screenreaders will properly read
 * "(x of y)". This however changes the visuals so that when the element is
 * focused the entire row is highlighted instead of just the checkbox.
 *
 * Example usage:
 * <iron-list role="listbox" items="[[items]]">
 *   <template>
 *     <settings-checkbox-list-entry role="option"
 *         checked="[[isSelected_(item)]]"
 *         tabindex="[[tabIndex]]"
 *         aria-posinset$="[[addOneTo_(index)]]"
 *         aria-setsize$="[[items.length]]"
 *         on-change="toggleSelection_">
 *       [[item]]
 *     </settings-checkbox-list-entry>
 *   </template>
 * </iron-list>
 */
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_checkbox_list_entry.html.js';

export interface SettingsCheckboxListEntryElement {
  $: {
    checkbox: CrCheckboxElement,
  };
}

export class SettingsCheckboxListEntryElement extends PolymerElement {
  static get is() {
    return 'settings-checkbox-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Used to set the status of the checkbox when the entry is created,
      // as well as when the list item for the entry changes.
      checked: {
        type: Boolean,
        value: false,
        observer: 'onCheckedChanged_',
      },

      // Reflects to the tabindex attribute. When it is negative (non-focusable)
      // aria-hidden will be set to "true", so that it will be ignored by screen
      // readers. This is needed because iron-list recycles its entries, so when
      // focusing an entry, the screen reader can be confused by other entries'
      // aria-posinset and aria-setsize attributes if they aren't aria-hidden.
      tabindex: {
        type: Number,
        value: 0,
        observer: 'onTabIndexChanged_',
        reflectToAttribute: true,
      },
    };
  }

  checked: boolean;
  private posinset: number;
  private setsize: number;
  private tabindex: number;

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    this.addEventListener('keydown', this.onKeyDown_);
    this.addEventListener('keyup', this.onKeyUp_);
  }

  private onClick_() {
    this.$.checkbox.click();
  }

  // Handle key presses in the same way as cr-checkbox, because it no longer
  // receives focus.
  private onKeyDown_(e: KeyboardEvent) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    if (e.repeat) {
      return;
    }

    if (e.key === 'Enter') {
      this.$.checkbox.click();
    }
  }

  private onKeyUp_(e: KeyboardEvent) {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
    }

    if (e.key === ' ') {
      this.$.checkbox.click();
    }
  }

  private onCheckedChanged_() {
    this.setAttribute('aria-checked', String(this.$.checkbox.checked));
  }

  private onTabIndexChanged_() {
    this.setAttribute('aria-hidden', this.tabindex >= 0 ? 'false' : 'true');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-checkbox-list-entry': SettingsCheckboxListEntryElement;
  }
}

customElements.define(
    SettingsCheckboxListEntryElement.is, SettingsCheckboxListEntryElement);
