// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './graduation_ui.html.js';

/**
 * The URL of the banner shown in Takeout indicating that the user has
 * completed the final step of the flow.
 */
const TAKEOUT_COMPLETED_BANNER_URL: string =
    'https://www.gstatic.com/ac/takeout/migration/migration-banner.png';

export interface GraduationUi {
  $: {
    webview: chrome.webviewTag.WebView,
  };
}
export class GraduationUi extends PolymerElement {
  static get is() {
    return 'graduation-ui' as const;
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

  override ready() {
    super.ready();
    const webviewUrl = loadTimeData.getString('webviewUrl');
    const webview =
        this.shadowRoot!.querySelector<chrome.webviewTag.WebView>('webview')!;

    webview.addEventListener('contentload', () => {
      this.webviewLoading = false;
    });

    webview.addEventListener('loadabort', () => {
      this.webviewLoading = false;
      /** TODO(b/357877855) Trigger error screen. */
    });

    /**
     * The flow is marked as completed when the image shown at the end of the
     * Takeout flow is displayed to the user.
     * TODO(b/361797263): Enable Done button when `takeoutFlowCompleted` is
     * true.
     */
    webview.request.onCompleted.addListener((details: any) => {
      if (details.statusCode === 200 &&
          details.url === TAKEOUT_COMPLETED_BANNER_URL) {
        this.takeoutFlowCompleted = true;
      }
    }, {urls: ['<all_urls>']});

    webview.src = webviewUrl.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GraduationUi.is]: GraduationUi;
  }
}

customElements.define(GraduationUi.is, GraduationUi);
