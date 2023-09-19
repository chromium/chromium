// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './loading_page.html.js';
import {AppState} from './scanning_app_types.js';

/**
 * @fileoverview
 * 'loading-page' is shown while searching for available scanners.
 */

const LoadingPageElementBase = I18nMixin(PolymerElement);

class LoadingPageElement extends LoadingPageElementBase {
  static get is() {
    return 'loading-page' as const;
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

      isDarkModeEnabled: {
        type: Boolean,
      },

      isJellyEnabled: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isJellyEnabledForScanningApp');
        },
      },

      noScannersAvailable: {
        type: Boolean,
        value: false,
      },
    };
  }

  appState: AppState;
  private isDarkModeEnabled: boolean;
  private isJellyEnabled: boolean;
  private noScannersAvailable: boolean;

  /**
   * Determines correct SVG for "no scanners" based on dark mode.
   */
  private getNoScannersSvgSrc(): string {
    return this.isDarkModeEnabled ? 'svg/no_scanners_dark.svg' :
                                    'svg/no_scanners.svg';
  }

  /**
   * Determines correct SVG for "scanners loading" based on dark mode.
   */
  private getScannersLoadingSvgSrc(): string {
    return this.isDarkModeEnabled ? 'svg/scanners_loading_dark.svg' :
                                    'svg/scanners_loading.svg';
  }

  private appStateChanged(): void {
    this.noScannersAvailable = this.appState === AppState.NO_SCANNERS;
  }

  private onRetryClick(): void {
    this.dispatchEvent(
        new CustomEvent('retry-click', {bubbles: true, composed: true}));
  }

  private onLearnMoreClick(): void {
    this.dispatchEvent(
        new CustomEvent('learn-more-click', {bubbles: true, composed: true}));
  }

  setIsJellyEnabledForTesting(enabled: boolean): void {
    this.isJellyEnabled = enabled;
  }
}

declare global {
  interface HTMLElementEventMap {
    'learn-more-click': CustomEvent<void>;
    'retry-click': CustomEvent<void>;
  }

  interface HTMLElementTagNameMap {
    [LoadingPageElement.is]: LoadingPageElement;
  }
}


customElements.define(LoadingPageElement.is, LoadingPageElement);
