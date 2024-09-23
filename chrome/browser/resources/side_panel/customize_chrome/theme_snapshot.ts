// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';

import {assert} from 'chrome://resources/js/assert.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getCss} from './theme_snapshot.css.js';
import {getHtml} from './theme_snapshot.html.js';

export enum CustomizeThemeType {
  CLASSIC_CHROME = 'classicChrome',
  CUSTOM_THEME = 'customTheme',
  UPLOADED_IMAGE = 'uploadedImage',
}

/** Element used to display a snapshot of the NTP. */
export class ThemeSnapshotElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-theme-snapshot';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      theme_: {type: Object},
      themeType_: {type: String},
    };
  }

  protected theme_: Theme|null = null;
  protected themeType_: CustomizeThemeType|null = null;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private setThemeListenerId_: number|null = null;

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
            this.style.setProperty(
                '--customize-chrome-color-background-color',
                skColorToRgba(this.theme_.backgroundColor));
          }
        });
    this.pageHandler_.updateTheme();
  }


  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setThemeListenerId_);
    this.callbackRouter_.removeListener(this.setThemeListenerId_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('theme_')) {
      this.themeType_ = this.computeThemeType_();
    }
  }

  private computeThemeType_(): CustomizeThemeType|null {
    if (this.theme_) {
      if (!this.theme_.backgroundImage) {
        return CustomizeThemeType.CLASSIC_CHROME;
      }

      if (this.theme_.backgroundImage.isUploadedImage) {
        return CustomizeThemeType.UPLOADED_IMAGE;
      }

      if (this.theme_.backgroundImage.snapshotUrl?.url) {
        return CustomizeThemeType.CUSTOM_THEME;
      }
    }
    return null;
  }

  protected onThemeSnapshotClick_() {
    if (this.theme_ && this.theme_.backgroundManagedByPolicy) {
      return;
    }
    this.dispatchEvent(new Event('edit-theme-click'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-theme-snapshot': ThemeSnapshotElement;
  }
}

customElements.define(ThemeSnapshotElement.is, ThemeSnapshotElement);
