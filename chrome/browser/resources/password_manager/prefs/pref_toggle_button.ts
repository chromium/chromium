// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `pref-toggle-button` is a toggle that controls a supplied preference.
 */
import '//resources/cr_elements/cr_actionable_row_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './pref_mixin.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefMixin} from './pref_mixin.js';
import {getTemplate} from './pref_toggle_button.html.js';

const PrefToggleButtonElementBase = PrefMixin(PolymerElement);

export class PrefToggleButtonElement extends PrefToggleButtonElementBase {
  static get is() {
    return 'pref-toggle-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The main label. */
      label: {
        type: String,
        value: '',
      },

      /** Additional (optional) sub-label. */
      subLabel: {
        type: String,
        value: '',
      },

      /** Whether the control is checked. */
      checked: {
        type: Boolean,
        value: false,
        notify: true,
        reflectToAttribute: true,
      },
    };
  }

  static get observers() {
    return ['prefValueChanged_(pref.value)'];
  }

  label: string;
  subLabel: string;
  checked: boolean;

  override ready() {
    super.ready();

    this.addEventListener('click', this.onHostClick_);
  }

  private onChange_(e: CustomEvent<boolean>) {
    this.checked = e.detail;
    this.updatePrefValue_();
  }

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   */
  private onHostClick_(e: Event) {
    e.stopPropagation();

    this.checked = !this.checked;
    this.updatePrefValue_();
  }

  private prefValueChanged_(prefValue: boolean) {
    this.checked = prefValue;
  }

  /** Update the pref to the current |checked| value. */
  private updatePrefValue_() {
    this.setPrefValue(this.checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pref-toggle-button': PrefToggleButtonElement;
  }
}

customElements.define(PrefToggleButtonElement.is, PrefToggleButtonElement);
