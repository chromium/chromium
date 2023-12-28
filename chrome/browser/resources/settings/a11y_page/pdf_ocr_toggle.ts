// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-pdf-ocr-toggle' is a toggle component for PDF OCR. It appears on
 * the accessibility subpage (chrome://settings/accessibility) on Windows,
 * macOS, and Linux.
 */

import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {AccessibilityBrowserProxy, AccessibilityBrowserProxyImpl, ScreenAiInstallStatus} from './a11y_browser_proxy.js';
import {getTemplate} from './pdf_ocr_toggle.html.js';

const SettingsPdfOcrToggleBaseElement =
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export interface SettingsPdfOcrToggleElement {
  $: {
    toggle: SettingsToggleButtonElement,
  };
}

export class SettingsPdfOcrToggleElement extends
    SettingsPdfOcrToggleBaseElement {
  static get is() {
    return 'settings-pdf-ocr-toggle';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * `pdfOcrProgress_` stores the downloading progress in percentage of
       * the ScreenAI library, which ranges from 0.0 to 100.0.
       */
      pdfOcrProgress_: Number,

      /**
       * `pdfOcrStatus_` stores the ScreenAI library install state.
       */
      pdfOcrStatus_: Number,
    };
  }

  private browserProxy_: AccessibilityBrowserProxy =
      AccessibilityBrowserProxyImpl.getInstance();

  private pdfOcrProgress_: number;
  private pdfOcrStatus_: ScreenAiInstallStatus;

  override connectedCallback() {
    super.connectedCallback();

    assert(loadTimeData.getBoolean('pdfOcrEnabled'));

    const updatePdfOcrState = (pdfOcrState: ScreenAiInstallStatus) => {
      this.pdfOcrStatus_ = pdfOcrState;
    };
    this.browserProxy_.getScreenAiInstallState().then(updatePdfOcrState);
    this.addWebUiListener('pdf-ocr-state-changed', updatePdfOcrState);
    this.addWebUiListener(
        'pdf-ocr-downloading-progress-changed', (progress: number) => {
          this.pdfOcrProgress_ = progress;
        });
  }

  private getPdfOcrToggleSublabel_(): string {
    switch (this.pdfOcrStatus_) {
      case ScreenAiInstallStatus.DOWNLOADING:
        return this.pdfOcrProgress_ > 0 && this.pdfOcrProgress_ < 100 ?
            this.i18n('pdfOcrDownloadProgressLabel', this.pdfOcrProgress_) :
            this.i18n('pdfOcrDownloadingLabel');
      case ScreenAiInstallStatus.FAILED:
        return this.i18n('pdfOcrDownloadErrorLabel');
      case ScreenAiInstallStatus.DOWNLOADED:
        return this.i18n('pdfOcrDownloadCompleteLabel');
      case ScreenAiInstallStatus.READY:  // fallthrough
      case ScreenAiInstallStatus.NOT_DOWNLOADED:
        // No subtitle update, so show a generic subtitle describing PDF OCR.
        return this.i18n('pdfOcrSubtitle');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-pdf-ocr-toggle': SettingsPdfOcrToggleElement;
  }
}

customElements.define(
    SettingsPdfOcrToggleElement.is, SettingsPdfOcrToggleElement);
