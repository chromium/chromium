// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';

import {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
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

export interface ShortcutsElement {}

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
      show_: Boolean,
    };
  }

  private customLinksEnabled_: boolean;
  private show_: boolean;
  private pageHandler_: CustomizeChromePageHandlerInterface;

  constructor() {
    super();
    const {handler} = CustomizeChromeApiProxy.getInstance();
    this.pageHandler_ = handler;
    // TODO(crbug.com/1384278) Auto update most visited settings if they change
    // while the side panel is open.
    this.pageHandler_.getMostVisitedSettings().then(
        ({customLinksEnabled, shortcutsVisible}) => {
          this.customLinksEnabled_ = customLinksEnabled;
          this.show_ = shortcutsVisible;
        });
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  private setMostVisitedSettings_() {
    this.pageHandler_.setMostVisitedSettings(
        this.customLinksEnabled_, /* shortcutsVisible= */ this.show_);
  }

  private onShortcutsRadioSelectionChanged_(e: CustomEvent<{value: string}>) {
    this.customLinksEnabled_ = e.detail.value === 'customLinksOption';
    this.setMostVisitedSettings_();
  }

  private shortcutsRadioSelection_(): string {
    return this.customLinksEnabled_ ? 'customLinksOption' : 'mostVisitedOption';
  }

  private onShowChange_(e: CustomEvent<boolean>) {
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
