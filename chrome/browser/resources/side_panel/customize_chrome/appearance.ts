// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './colors.js';
import './theme_snapshot.js';
import './hover_button.js';
import './strings.m.js'; // Required by <managed-dialog>.
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
    chromeColors: HTMLElement,
    editThemeButton: HTMLButtonElement,
    themeSnapshot: HTMLElement,
    setClassicChromeButton: HTMLButtonElement,
    thirdPartyLinkButton: HTMLButtonElement,
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
      themeButtonClass_: String,

      thirdPartyThemeId_: {
        type: String,
        computed: 'computeThirdPartyThemeId_(theme_)',
        reflectToAttribute: true,
      },

      thirdPartyThemeName_: {
        type: String,
        computed: 'computeThirdPartyThemeName_(theme_)',
        reflectToAttribute: true,
      },

      // Prevents side panel from showing theme snapshot and colors before
      // thirdPartyThemeName_ is determined if third party theme is installed.
      showFirstPartyThemeView_: {
        type: Boolean,
        value: false,
        computed: 'computeShowFirstPartyThemeView_(theme_)',
      },

      showClassicChromeButton_: {
        type: Boolean,
        value: false,
        computed: 'computeShowClassicChromeButton_(theme_)',
      },

      showManagedDialog_: Boolean,
    };
  }

  private theme_: Theme|undefined = undefined;
  private themeButtonClass_: string;
  private thirdPartyThemeId_: string|null = null;
  private thirdPartyThemeName_: string|null = null;
  private showClassicChromeButton_: boolean;
  private showFirstPartyThemeView_: boolean;
  private showManagedDialog_: boolean;

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
    this.themeButtonClass_ =
        document.documentElement.hasAttribute('chrome-refresh-2023') ?
        'floating-button' :
        'action-button';
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

  focusOnThemeButton() {
    this.$.editThemeButton.focus();
  }

  private computeThirdPartyThemeId_(): string|null {
    if (this.theme_ && this.theme_.thirdPartyThemeInfo) {
      return this.theme_.thirdPartyThemeInfo.id;
    } else {
      return null;
    }
  }

  private computeThirdPartyThemeName_(): string|null {
    if (this.theme_ && this.theme_.thirdPartyThemeInfo) {
      return this.theme_.thirdPartyThemeInfo.name;
    } else {
      return null;
    }
  }

  private computeShowFirstPartyThemeView_(): boolean {
    return !!this.theme_ && !this.theme_.thirdPartyThemeInfo;
  }

  private computeShowClassicChromeButton_(): boolean {
    return !!(
        this.theme_ &&
        (this.theme_.backgroundImage || this.theme_.thirdPartyThemeInfo));
  }

  private onEditThemeClicked_() {
    if (this.handleClickForManagedThemes_()) {
      return;
    }
    this.dispatchEvent(new Event('edit-theme-click'));
  }

  private onThirdPartyLinkButtonClick_() {
    if (this.thirdPartyThemeId_) {
      this.pageHandler_.openThirdPartyThemePage(this.thirdPartyThemeId_);
    }
  }

  private onSetClassicChromeClicked_() {
    if (this.handleClickForManagedThemes_()) {
      return;
    }
    this.pageHandler_.removeBackgroundImage();
    this.pageHandler_.setDefaultColor();
  }

  private onManagedDialogClosed_() {
    this.showManagedDialog_ = false;
  }

  private handleClickForManagedThemes_(): boolean {
    if (!this.theme_ || !this.theme_.backgroundManagedByPolicy) {
      return false;
    }
    this.showManagedDialog_ = true;
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-appearance': AppearanceElement;
  }
}

customElements.define(AppearanceElement.is, AppearanceElement);
