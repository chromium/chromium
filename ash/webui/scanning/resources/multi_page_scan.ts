// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './scanning_fonts.css.js';
import './strings.m.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './multi_page_scan.html.js';
import {AppState} from './scanning_app_types.js';
import {ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/**
 * @fileoverview
 * 'multi-page-scan' shows the available actions for a multi-page scan.
 */

const MultiPageScanElementBase = I18nMixin(PolymerElement);

export class MultiPageScanElement extends MultiPageScanElementBase {
  static get is() {
    return 'multi-page-scan' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appState: {
        type: Number,
        observer: 'appStateChanged',
      },

      pageNumber: {
        type: Number,
        observer: 'pageNumberChanged',
      },

      scanButtonText: String,

      showCancelButton: {
        type: Boolean,
        value: false,
      },

      cancelButtonDisabled: {
        type: Boolean,
        value: false,
      },

      showCancelingText: {
        type: Boolean,
        value: false,
      },
    };
  }

  appState: AppState;
  pageNumber: number;
  private scanButtonText: string;
  private showCancelButton: boolean;
  private cancelButtonDisabled: boolean;
  private showCancelingText: boolean;

  private appStateChanged(): void {
    this.showCancelButton = this.appState === AppState.MULTI_PAGE_SCANNING ||
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.cancelButtonDisabled = this.appState === AppState.MULTI_PAGE_CANCELING;
    this.showCancelingText = this.appState === AppState.MULTI_PAGE_CANCELING;
  }

  private pageNumberChanged(): void {
    ScanningBrowserProxyImpl.getInstance()
        .getPluralString('scanButtonText', this.pageNumber + 1)
        .then(
            /* @type {string} */ (pluralString) => {
              this.scanButtonText = pluralString;
            });
  }

  private onScanClick(): void {
    this.dispatchEvent(
        new CustomEvent('scan-next-page', {bubbles: true, composed: true}));
  }

  private onSaveClick(): void {
    this.dispatchEvent(new CustomEvent(
        'complete-multi-page-scan', {bubbles: true, composed: true}));
  }

  private onCancelClick(): void {
    this.dispatchEvent(
        new CustomEvent('cancel-click', {bubbles: true, composed: true}));
  }

  private getProgressText(): string {
    return this.i18n('multiPageScanProgressText', this.pageNumber);
  }
}

declare global {
  interface HTMLElementEventMap {
    'cancel-click': CustomEvent<void>;
    'complete-multi-page-scan': CustomEvent<void>;
    'scan-next-page': CustomEvent<void>;
  }

  interface HTMLElementTagNameMap {
    [MultiPageScanElement.is]: MultiPageScanElement;
  }
}

customElements.define(MultiPageScanElement.is, MultiPageScanElement);
