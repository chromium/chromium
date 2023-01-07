// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../settings_shared.css.js';

import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {FocusRowMixin} from 'chrome://resources/js/focus_row_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';

import {getTemplate} from './tab_discard_exception_entry.html.js';

export interface TabDiscardExceptionEntryElement {
  $: {
    button: CrIconButtonElement,
  };
}

const TabDiscardExceptionEntryElementBase =
    FocusRowMixin(BaseMixin(PolymerElement));

export class TabDiscardExceptionEntryElement extends
    TabDiscardExceptionEntryElementBase {
  static get is() {
    return 'tab-discard-exception-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      site: String,
    };
  }

  site: string;

  private onMenuClick_(e: Event) {
    this.fire('menu-click', {target: e.target as HTMLElement, site: this.site});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-entry': TabDiscardExceptionEntryElement;
  }
}

customElements.define(
    TabDiscardExceptionEntryElement.is, TabDiscardExceptionEntryElement);