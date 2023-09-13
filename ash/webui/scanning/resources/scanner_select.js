// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Scanner} from './scanning.mojom-webui.js';
import {ScannerArr, ScannerInfo} from './scanning_app_types.js';
import {alphabeticalCompare, getScannerDisplayName, tokenToString} from './scanning_app_util.js';

/**
 * @fileoverview
 * 'scanner-select' displays the connected scanners in a dropdown.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ScannerSelectElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ScannerSelectElement extends ScannerSelectElementBase {
  static get is() {
    return 'scanner-select';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {boolean} */
      disabled: Boolean,

      /** @type {!ScannerArr} */
      scanners: {
        type: Array,
        value: () => [],
      },

      /** @type {string} */
      selectedScannerId: {
        type: String,
        notify: true,
      },

      /** @type {!Map<string, !ScannerInfo>} */
      scannerInfoMap: Object,

      /** @type {string} */
      lastUsedScannerId: String,
    };
  }

  static get observers() {
    return ['onScannersChange_(scanners.*)'];
  }

  /**
   * @param {!Scanner} scanner
   * @return {string}
   * @private
   */
  getScannerDisplayName_(scanner) {
    return getScannerDisplayName(scanner);
  }

  /**
   * Converts an unguessable token to a string so it can be used as the value of
   * an option.
   * @param {!Scanner} scanner
   * @return {string}
   * @private
   */
  getTokenAsString_(scanner) {
    return tokenToString(scanner.id);
  }

  /**
   * Sorts the scanners and sets the selected scanner when the scanners array
   * changes.
   * @private
   */
  onScannersChange_() {
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
        this.shadowRoot.querySelector('#scannerSelect').selectedIndex =
            this.getSelectedScannerIndex_();
      });
    }
  }

  /**
   * @return {number}
   * @private
   */
  getSelectedScannerIndex_() {
    const selectedScannerToken =
        this.scannerInfoMap.get(this.selectedScannerId).token;
    return this.scanners.findIndex(
        scanner => scanner.id === selectedScannerToken);
  }
}

customElements.define(ScannerSelectElement.is, ScannerSelectElement);
