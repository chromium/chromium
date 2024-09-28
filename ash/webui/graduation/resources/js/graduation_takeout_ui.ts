// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../strings.m.js';

import {isRTL} from '//resources/js/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScreenSwitchEvents} from './graduation_app.js';
import {getTemplate} from './graduation_takeout_ui.html.js';

/**
 * The base URL of the banner shown in Takeout indicating that the user has
 * completed the final step of the flow.
 * May be suffixed by a year, for example: "-2024.png".
 */
const TAKEOUT_COMPLETED_BANNER_BASE_URL: string =
    'https://www.gstatic.com/ac/takeout/migration/migration-banner';

export class GraduationTakeoutUi extends PolymerElement {
  static get is() {
    return 'graduation-takeout-ui' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      webviewLoading: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether the webview content has indicated that the user has completed
       * the Takeout flow.
       */
      takeoutFlowCompleted: {
        type: Boolean,
        value: false,
      },
    };
  }

  webviewLoading: boolean;
  takeoutFlowCompleted: boolean;
  private webview: chrome.webviewTag.WebView;

  override ready() {
    super.ready();
    const webviewUrl = loadTimeData.getString('webviewUrl');
    this.webview =
        this.shadowRoot!.querySelector<chrome.webviewTag.WebView>('webview')!;

    this.webview.addEventListener('contentload', () => {
      this.webviewLoading = false;
    });

    this.webview.addEventListener('loadabort', () => {
      this.webviewLoading = false;
      this.dispatchEvent(new CustomEvent(ScreenSwitchEvents.SHOW_ERROR, {
        bubbles: true,
        composed: true,
      }));
    });

    /**
     * The done button is made visible when the image shown at the end of the
     * Takeout flow is displayed to the user.
     */
    this.webview.request.onCompleted.addListener((details: any) => {
      if (details.statusCode === 200 &&
          details.url.startsWith(TAKEOUT_COMPLETED_BANNER_BASE_URL)) {
        this.takeoutFlowCompleted = true;
      }
    }, {urls: ['<all_urls>']});

    this.webview.src = webviewUrl.toString();
  }

  setWebviewForTest(webview: chrome.webviewTag.WebView) {
    this.webview = webview;
  }

  private getBackButtonIcon_(): string {
    return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
  }

  private onBackClicked_(): void {
    this.dispatchEvent(new CustomEvent(ScreenSwitchEvents.SHOW_WELCOME, {
      bubbles: true,
      composed: true,
    }));
  }

  private onDoneClicked_(): void {
    window.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GraduationTakeoutUi.is]: GraduationTakeoutUi;
  }
}

customElements.define(GraduationTakeoutUi.is, GraduationTakeoutUi);
