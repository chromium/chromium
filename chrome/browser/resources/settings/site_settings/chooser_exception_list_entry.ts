// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'chooser-exception-list-entry' shows a single chooser exception for a given
 * chooser type.
 */
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared.css.js';
import './site_list_entry.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './chooser_exception_list_entry.html.js';
import type {ChooserException} from './site_settings_prefs_browser_proxy.js';

export interface ChooserExceptionListEntryElement {
  $: {
    listContainer: HTMLElement,
  };
}

export class ChooserExceptionListEntryElement extends PolymerElement {
  static get is() {
    return 'chooser-exception-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Chooser exception object to display in the widget.
       */
      exception: Object,

      lastFocused_: Object,
    };
  }

  exception: ChooserException;
  private lastFocused_: HTMLElement|null;
}

declare global {
  interface HTMLElementTagNameMap {
    'chooser-exception-list-entry': ChooserExceptionListEntryElement;
  }
}

customElements.define(
    ChooserExceptionListEntryElement.is, ChooserExceptionListEntryElement);
