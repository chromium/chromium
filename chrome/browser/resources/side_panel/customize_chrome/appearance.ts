// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './colors.js';
import './theme_snapshot.js';
import './hover_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './appearance.html.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface AppearanceElement {
  $: {
    editThemeButton: HTMLButtonElement,
    setClassicChromeButton: HTMLElement,
  };
}

export class AppearanceElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-appearance';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      theme_: Object,

      showClassicChromeButton_: {
        type: Boolean,
        value: false,
        computed: 'computeShowClassicChromeButton_(theme_)',
      },
    };
  }

  private theme_: Theme|undefined = undefined;
  private setThemeListenerId_: number|null = null;
  private showClassicChromeButton_: boolean;

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

  private computeShowClassicChromeButton_(): boolean {
    return !!(this.theme_ && this.theme_.backgroundImage);
  }

  private onEditThemeClicked_() {
    this.dispatchEvent(new Event('edit-theme-click'));
  }

  private onSetClassicChromeClicked_() {
    this.pageHandler_.setClassicChromeDefaultTheme();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-appearance': AppearanceElement;
  }
}

customElements.define(AppearanceElement.is, AppearanceElement);
