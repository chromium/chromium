// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_discard_exception_current_sites_entry.html.js';

export interface TabDiscardExceptionCurrentSitesEntryElement {
  $: {
    checkbox: CrCheckboxElement,
  };
}

export class TabDiscardExceptionCurrentSitesEntryElement extends
    PolymerElement {
  static get is() {
    return 'tab-discard-exception-current-sites-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      item: String,
    };
  }

  private item: string;

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('keydown', this.onKeyDown_);
    this.addEventListener('keyup', this.onKeyUp_);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.removeEventListener('keydown', this.onKeyDown_);
    this.removeEventListener('keyup', this.onKeyUp_);
  }

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
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-current-sites-entry':
        TabDiscardExceptionCurrentSitesEntryElement;
  }
}

customElements.define(
    TabDiscardExceptionCurrentSitesEntryElement.is,
    TabDiscardExceptionCurrentSitesEntryElement);
