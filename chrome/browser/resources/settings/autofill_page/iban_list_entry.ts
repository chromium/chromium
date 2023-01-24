// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'iban-list-entry' is an IBAN row to be shown on the settings
 * page.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../i18n_setup.js';
import '../settings_shared.css.js';
import './passwords_shared.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './iban_list_entry.html.js';

export type DotsIbanMenuClickEvent = CustomEvent<{
  iban: chrome.autofillPrivate.IbanEntry,
  anchorElement: HTMLElement,
}>;

declare global {
  interface HTMLElementEventMap {
    'dots-iban-menu-click': DotsIbanMenuClickEvent;
  }
}

export interface SettingsIbanListEntryElement {
  $: {
    ibanMenu: CrButtonElement,
  };
}

const SettingsIbanListEntryElementBase = I18nMixin(PolymerElement);

export class SettingsIbanListEntryElement extends
    SettingsIbanListEntryElementBase {
  static get is() {
    return 'settings-iban-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** A saved IBAN. */
      iban: Object,
    };
  }

  iban: chrome.autofillPrivate.IbanEntry;

  /**
   * Opens the IBAN action menu.
   */
  private onDotsMenuClick_() {
    this.dispatchEvent(new CustomEvent('dots-iban-menu-click', {
      bubbles: true,
      composed: true,
      detail: {
        iban: this.iban,
        anchorElement: this.$.ibanMenu,
      },
    }));
  }

  private onRemoteEditClick_() {
    this.dispatchEvent(new CustomEvent('remote-iban-menu-click', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * @return the title for the More Actions button corresponding to the IBAN
   *     which is described by the nickname or last 4 digits of the IBAN's
   *     value.
   */
  private getMoreActionsTitle_(iban: chrome.autofillPrivate.IbanEntry): string {
    if (iban.nickname) {
      return this.i18n('moreActionsForIban', iban.nickname);
    }

    // Strip all whitespace and get the pure last four digits of the value.
    const strippedSummaryLabel =
        iban.metadata ? iban.metadata!.summaryLabel.replace(/\s/g, '') : '';
    const lastFourDigits = strippedSummaryLabel.substring(
        Math.max(0, strippedSummaryLabel.length - 4));

    return this.i18n(
        'moreActionsForIban',
        this.i18n('moreActionsForIbanDescription', lastFourDigits));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-iban-list-entry': SettingsIbanListEntryElement;
  }
}

customElements.define(
    SettingsIbanListEntryElement.is, SettingsIbanListEntryElement);
