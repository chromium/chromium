// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'iban-list-entry' is an IBAN row to be shown on the settings
 * page.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../i18n_setup.js';
import '../settings_shared.css.js';
import './passwords_shared.css.js';
import './screen_reader_only.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './iban_list_entry.html.js';

export type DotsIbanMenuClickEvent = CustomEvent<{
  iban: chrome.autofillPrivate.IbanEntry,
  anchorElement: HTMLElement,
}>;

export type RemoteIbanMenuClickEvent = CustomEvent<{
  iban: chrome.autofillPrivate.IbanEntry,
  anchorElement: HTMLElement,
}>;

declare global {
  interface HTMLElementEventMap {
    'dots-iban-menu-click': DotsIbanMenuClickEvent;
    'remote-iban-menu-click': RemoteIbanMenuClickEvent;
  }
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

      showNewFopDisplayEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableNewFopDisplay');
        },
        readOnly: true,
      },
    };
  }

  declare iban: chrome.autofillPrivate.IbanEntry;

  declare private showNewFopDisplayEnabled_: boolean;

  get dotsMenu(): HTMLElement|null {
    return this.shadowRoot!.getElementById('ibanMenu');
  }

  /**
   * The 3-dot menu should be shown if the IBAN is a local IBAN.
   */
  private showDotsMenu_(): boolean {
    return !!this.iban.metadata!.isLocal;
  }

  /**
   * The Google Payments icon should be shown if the IBAN is a server IBAN.
   */
  private shouldShowGooglePaymentsIndicator_(): boolean {
    return !this.iban.metadata!.isLocal;
  }

  /**
   * This function returns a string that can be used in a srcset to scale
   * the provided `url` based on the user's screen resolution.
   */
  private getScaledSrcSet_(url: string): string {
    return `${url} 1x, ${url}@2x 2x`;
  }

  /**
   * Opens the IBAN action menu.
   */
  private onDotsMenuClick_() {
    this.dispatchEvent(new CustomEvent('dots-iban-menu-click', {
      bubbles: true,
      composed: true,
      detail: {
        iban: this.iban,
        anchorElement: this.dotsMenu,
      },
    }));
  }

  private onRemoteEditClick_() {
    this.dispatchEvent(new CustomEvent('remote-iban-menu-click', {
      bubbles: true,
      composed: true,
      detail: {
        iban: this.iban,
        anchorElement: this.dotsMenu,
      },
    }));
  }

  private getA11yIbanDescription_(iban: chrome.autofillPrivate.IbanEntry):
      string {
    // Strip all whitespace and get the pure last four digits of the value.
    const strippedSummaryLabel =
        iban.metadata ? iban.metadata.summaryLabel.replace(/\s/g, '') : '';
    const lastFourDigits = strippedSummaryLabel.substring(
        Math.max(0, strippedSummaryLabel.length - 4));

    return this.i18n('a11yIbanDescription', lastFourDigits);
  }

  private getIbanImageSrc_(): string {
    return this.showNewFopDisplayEnabled_ ?
        'chrome://settings/images/iban.svg' :
        'chrome://settings/images/iban_old.svg';
  }

  private getLabel_(iban: chrome.autofillPrivate.IbanEntry): string {
    if (this.showNewFopDisplayEnabled_ && iban.nickname) {
      return iban.nickname;
    }
    return iban.metadata!.summaryLabel;
  }

  private getSubLabel_(iban: chrome.autofillPrivate.IbanEntry): string {
    if (this.showNewFopDisplayEnabled_ && iban.nickname) {
      return iban.metadata!.summaryLabel;
    }
    return iban.nickname || '';
  }

  /**
   * @return the title for the More Actions button corresponding to the IBAN
   *     which is described by the nickname or last 4 digits of the IBAN's
   *     value.
   */
  private getMoreActionsTitle_(iban: chrome.autofillPrivate.IbanEntry): string {
    return this.i18n(
        'moreActionsForIban',
        iban.nickname || this.getA11yIbanDescription_(iban));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-iban-list-entry': SettingsIbanListEntryElement;
  }
}

customElements.define(
    SettingsIbanListEntryElement.is, SettingsIbanListEntryElement);
