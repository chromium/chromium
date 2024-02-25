// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './scanner_select.html.js';
import {Scanner} from './scanning.mojom-webui.js';
import {ScannerInfo} from './scanning_app_types.js';
import {alphabeticalCompare, getScannerDisplayName, tokenToString} from './scanning_app_util.js';

/**
 * @fileoverview
 * 'scanner-select' displays the connected scanners in a dropdown.
 */

const ScannerSelectElementBase =
    I18nMixin(PolymerElement) as {new (): PolymerElement & I18nMixinInterface};

export class ScannerSelectElement extends ScannerSelectElementBase {
  static get is() {
    return 'scanner-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,

      scanners: {
        type: Array,
        value: () => [],
      },

      selectedScannerId: {
        type: String,
        notify: true,
      },

      scannerInfoMap: Object,

      lastUsedScannerId: String,
    };
  }

  static get observers() {
    return ['scannersChanged(scanners.*)'];
  }

  disabled: boolean;
  scanners: Scanner[];
  selectedScannerId: string;
  scannerInfoMap: Map<string, ScannerInfo>;
  lastUsedScannerId: string;

  private getScannerDisplayName(scanner: Scanner): string {
    return getScannerDisplayName(scanner);
  }

  /**
   * Converts an unguessable token to a string so it can be used as the value of
   * an option.
   */
  private getTokenAsString(scanner: Scanner): string {
    return tokenToString(scanner.id);
  }

  /**
   * Sorts the scanners and sets the selected scanner when the scanners array
   * changes.
   */
  private scannersChanged(): void {
    if (this.scanners.length > 1) {
      this.scanners.sort((a, b) => {
        return alphabeticalCompare(
            getScannerDisplayName(a), getScannerDisplayName(b));
      });
    }

    // Either select the last used scanner or default to the first scanner in
    // the dropdown.
    if (this.scanners.length > 0) {
      if (!this.lastUsedScannerId) {
        this.selectedScannerId = tokenToString(this.scanners[0].id);
        return;
      }

      this.selectedScannerId = this.lastUsedScannerId;

      // After the dropdown renders with the scanner options, set the selected
      // scanner.
      afterNextRender(this, () => {
        strictQuery('#scannerSelect', this.shadowRoot, HTMLSelectElement)
            .selectedIndex = this.getSelectedScannerIndex();
      });
    }
  }

  private getSelectedScannerIndex(): number {
    const selectedScannerToken =
        this.scannerInfoMap.get(this.selectedScannerId)!.token;
    return this.scanners.findIndex(
        (scanner: Scanner) => scanner.id === selectedScannerToken);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ScannerSelectElement.is]: ScannerSelectElement;
  }
}

customElements.define(ScannerSelectElement.is, ScannerSelectElement);
