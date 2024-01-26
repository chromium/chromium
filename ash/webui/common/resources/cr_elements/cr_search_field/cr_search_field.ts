// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-search-field' is a simple implementation of a polymer component that
 * uses CrSearchFieldMixin.
 *
 * Forked from
 * ui/webui/resources/cr_elements/cr_search_field/cr_search_field.ts
 */

import '../cr_icon_button/cr_icon_button.js';
import '../cr_input/cr_input.js';
import '../cr_input/cr_input_style.css.js';
import '../icons.html.js';
import '../cr_shared_style.css.js';
import '../cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrInputElement} from '../cr_input/cr_input.js';

import {getTemplate} from './cr_search_field.html.js';
import {CrSearchFieldMixin} from './cr_search_field_mixin.js';

const CrSearchFieldElementBase = CrSearchFieldMixin(PolymerElement);

export interface CrSearchFieldElement {
  $: {
    clearSearch: HTMLElement,
    searchInput: CrInputElement,
  };
}

export class CrSearchFieldElement extends CrSearchFieldElementBase {
  static get is() {
    return 'cr-search-field';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      autofocus: {
        type: Boolean,
        value: false,
      },
    };
  }

  override autofocus: boolean;

  override getSearchInput(): CrInputElement {
    return this.$.searchInput;
  }

  private onTapClear_() {
    this.setValue('');
    setTimeout(() => {
      this.$.searchInput.focus();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-search-field': CrSearchFieldElement;
  }
}

customElements.define(CrSearchFieldElement.is, CrSearchFieldElement);
