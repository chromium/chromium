// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './theme_snapshot.html.js';

export enum CustomizeThemeType {
  CLASSIC_CHROME = 'classicChrome',
  CUSTOM_THEME = 'customTheme',
  UPLOADED_IMAGE = 'uploadedImage',
}

/** Element used to display a snapshot of the NTP. */
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

      themeType_: {
        type: String,
        computed: 'computeThemeType_(theme_)',
      },
    };
  }

  private theme_: Theme|undefined = undefined;
  private setThemeListenerId_: number|null = null;
  private themeType_: CustomizeThemeType|null = null;

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
          if (this.theme_) {
            this.updateStyles({
              '--customize-chrome-color-background-color':
                  skColorToRgba(this.theme_.backgroundColor),
            });
          }
        });
    this.pageHandler_.updateTheme();
  }


  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setThemeListenerId_);
    this.callbackRouter_.removeListener(this.setThemeListenerId_);
  }

  private computeThemeType_(): CustomizeThemeType|null {
    if (!this.theme_) {
      return null;
    }
    if (!this.theme_.backgroundImage) {
      return CustomizeThemeType.CLASSIC_CHROME;
    } else if (!this.theme_.backgroundImage.isUploadedImage) {
      return CustomizeThemeType.CUSTOM_THEME;
    } else {
      return CustomizeThemeType.UPLOADED_IMAGE;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-theme-snapshot': ThemeSnapshotElement;
  }
}

customElements.define(ThemeSnapshotElement.is, ThemeSnapshotElement);
