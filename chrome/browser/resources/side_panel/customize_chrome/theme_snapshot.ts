// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './theme_snapshot.html.js';

/** Element used to display a snapshot of a NTP custom background. */
export class ThemeSnapshotElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-theme-snapshot';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      theme_: Object,
    };
  }

  private theme_: Theme;
  private setThemeListenerId_: number|null = null;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener((theme: Theme) => {
          this.theme_ = theme;
        });
    this.pageHandler_.updateTheme();
  }


  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setThemeListenerId_);
    this.callbackRouter_.removeListener(this.setThemeListenerId_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-theme-snapshot': ThemeSnapshotElement;
  }
}

customElements.define(ThemeSnapshotElement.is, ThemeSnapshotElement);
