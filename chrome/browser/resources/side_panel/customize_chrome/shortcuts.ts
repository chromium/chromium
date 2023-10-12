// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './button_label.js';

import {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './shortcuts.html.js';

export interface ShortcutsElement {
  $: {
    showShortcutsToggle: CrToggleElement,
    shortcutsRadioSelection: CrRadioGroupElement,
    customLinksButton: CrRadioButtonElement,
    mostVisitedButton: CrRadioButtonElement,
  };
}

export class ShortcutsElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-shortcuts';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      customLinksEnabled_: Boolean,
      shortcutsRadioSelection_: {
        type: String,
        computed: 'computeShortcutsRadioSelection_(customLinksEnabled_)',
      },
      show_: Boolean,
      initialized_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private customLinksEnabled_: boolean;
  private shortcutsRadioSelection_: string|undefined = undefined;
  private show_: boolean;
  private initialized_: boolean;

  private setMostVisitedSettingsListenerId_: number|null = null;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setMostVisitedSettingsListenerId_ =
        this.callbackRouter_.setMostVisitedSettings.addListener(
            (customLinksEnabled: boolean, shortcutsVisible: boolean) => {
              this.customLinksEnabled_ = customLinksEnabled;
              this.show_ = shortcutsVisible;
              this.initialized_ = true;
            });
    this.pageHandler_.updateMostVisitedSettings();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setMostVisitedSettingsListenerId_);
    this.callbackRouter_.removeListener(this.setMostVisitedSettingsListenerId_);
  }

  private setMostVisitedSettings_() {
    this.pageHandler_.setMostVisitedSettings(
        this.customLinksEnabled_, /* shortcutsVisible= */ this.show_);
  }

  private onShortcutsRadioSelectionChanged_(e: CustomEvent<{value: string}>) {
    this.customLinksEnabled_ = e.detail.value === 'customLinksOption';
    this.setMostVisitedSettings_();
  }

  private computeShortcutsRadioSelection_(): string {
    return this.customLinksEnabled_ ? 'customLinksOption' : 'mostVisitedOption';
  }

  private onShowShortcutsToggleChange_(e: CustomEvent<boolean>) {
    this.show_ = e.detail;
    this.setMostVisitedSettings_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-shortcuts': ShortcutsElement;
  }
}

customElements.define(ShortcutsElement.is, ShortcutsElement);
