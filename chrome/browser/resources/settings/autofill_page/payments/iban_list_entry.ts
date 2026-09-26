// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'iban-list-entry' is an IBAN row to be shown on the settings
 * page.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../../i18n_setup.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './iban_list_entry.css.js';
import {getHtml} from './iban_list_entry.html.js';

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

/**
 * This function returns a string that can be used in a srcset to scale
 * the provided `url` based on the user's screen resolution.
 */
function getScaledSrcSet(url: string): string {
  return `${url} 1x, ${url}@2x 2x`;
}

const SettingsIbanListEntryElementBase = I18nMixinLit(CrLitElement);

export class SettingsIbanListEntryElement extends
    SettingsIbanListEntryElementBase {
  static get is() {
    return 'settings-iban-list-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** A saved IBAN. */
      iban: {type: Object},

      autofillEnableWalletBrandingEnabled_: {type: Boolean},

      autofillEnableGradientGoogleLogosEnabled_: {type: Boolean},
    };
  }

  accessor iban: chrome.autofillPrivate.IbanEntry = {
    metadata: {
      isLocal: false,
      summaryLabel: '',
    },
  };

  protected accessor autofillEnableWalletBrandingEnabled_: boolean =
      loadTimeData.getBoolean('autofillEnableWalletBranding');

  protected accessor autofillEnableGradientGoogleLogosEnabled_: boolean =
      loadTimeData.getBoolean('autofillEnableGradientGoogleLogos');

  get dotsMenu(): HTMLElement|null {
    return this.shadowRoot.getElementById('ibanMenu');
  }

  /**
   * The 3-dot menu should be shown if the IBAN is a local IBAN.
   */
  protected showDotsMenu_(): boolean {
    return !!this.iban.metadata!.isLocal;
  }

  protected shouldShowOutlinkWithWalletBranding_(): boolean {
    return !this.showDotsMenu_() && this.autofillEnableWalletBrandingEnabled_;
  }

  protected shouldShowOutlinkWithoutWalletBranding_(): boolean {
    return !this.showDotsMenu_() && !this.autofillEnableWalletBrandingEnabled_;
  }

  /**
   * The Google Payments icon should be shown if the IBAN is a server IBAN.
   */
  protected shouldShowGooglePaymentsIndicator_(): boolean {
    return !this.iban.metadata!.isLocal;
  }

  /**
   * Opens the IBAN action menu.
   */
  protected onDotsMenuClick_() {
    this.fire('dots-iban-menu-click', {
      iban: this.iban,
      anchorElement: this.dotsMenu,
    });
  }

  protected onRemoteEditClick_() {
    this.fire('remote-iban-menu-click', {
      iban: this.iban,
      anchorElement: this.dotsMenu,
    });
  }

  protected getA11yIbanDescription_(): string {
    // Strip all whitespace and get the pure last four digits of the value.
    const strippedSummaryLabel = this.iban.metadata ?
        this.iban.metadata.summaryLabel.replace(/\s/g, '') :
        '';
    const lastFourDigits = strippedSummaryLabel.substring(
        Math.max(0, strippedSummaryLabel.length - 4));

    return this.i18n('a11yIbanDescription', lastFourDigits);
  }

  protected getLabel_(): string {
    return this.iban.nickname || this.iban.metadata!.summaryLabel;
  }

  protected getSubLabel_(): string {
    return this.iban.nickname ? this.iban.metadata!.summaryLabel : '';
  }

  /**
   * @return the title for the More Actions button corresponding to the IBAN
   *     which is described by the nickname or last 4 digits of the IBAN's
   *     value.
   */
  protected getMoreActionsTitle_(): string {
    return this.i18n(
        'moreActionsForIban',
        this.iban.nickname || this.getA11yIbanDescription_());
  }

  protected getGooglePayLightModeLogoSrcSet_(): string {
    const logoId = this.autofillEnableGradientGoogleLogosEnabled_ ?
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_SMALL' :
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_SMALL';
    return getScaledSrcSet(logoId);
  }

  protected getGooglePayDarkModeLogoSrcSet_(): string {
    const logoId = this.autofillEnableGradientGoogleLogosEnabled_ ?
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_DARK_SMALL' :
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_DARK_SMALL';
    return getScaledSrcSet(logoId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-iban-list-entry': SettingsIbanListEntryElement;
  }
}

customElements.define(
    SettingsIbanListEntryElement.is, SettingsIbanListEntryElement);
