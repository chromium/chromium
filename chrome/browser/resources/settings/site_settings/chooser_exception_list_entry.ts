// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'chooser-exception-list-entry' shows a single chooser exception for a given
 * chooser type.
 */
import './site_list_entry.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './chooser_exception_list_entry.css.js';
import {getHtml} from './chooser_exception_list_entry.html.js';
import {ChooserType} from './constants.js';
import type {SiteListEntryElement} from './site_list_entry.js';
import type {ChooserException} from './site_settings_browser_proxy.js';

export interface ChooserExceptionListEntryElement {
  $: {
    listContainer: HTMLElement,
  };
}

export class ChooserExceptionListEntryElement extends CrLitElement {
  static get is() {
    return 'chooser-exception-list-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Chooser exception object to display in the widget.
       */
      exception: {type: Object},
    };
  }

  accessor exception: ChooserException = {
    chooserType: ChooserType.NONE,
    displayName: '',
    object: {},
    sites: [],
  };

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // TODO(crbug.com/579564317): Delete this and use
    // settings-focus-list-container instead.
    // Restore focus to the last element if the previous last element was
    // deleted while it was focused.
    if (changedProperties.has('exception')) {
      const previousException = changedProperties.get('exception');
      if (previousException &&
          this.exception.sites.length < previousException.sites.length) {
        const focused =
            this.shadowRoot.querySelector('site-list-entry:focus-within');
        if (!focused) {
          const toFocus = this.shadowRoot.querySelector<SiteListEntryElement>(
              'site-list-entry:last-of-type');
          toFocus?.getFocusRow().getFirstFocusable('reset')?.focus();
        }
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chooser-exception-list-entry': ChooserExceptionListEntryElement;
  }
}

customElements.define(
    ChooserExceptionListEntryElement.is, ChooserExceptionListEntryElement);
