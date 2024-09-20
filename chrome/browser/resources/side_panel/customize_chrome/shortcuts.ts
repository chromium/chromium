// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import './button_label.js';

import type {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import type {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import type {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getCss} from './shortcuts.css.js';
import {getHtml} from './shortcuts.html.js';

export interface ShortcutsElement {
  $: {
    showToggleContainer: HTMLElement,
    showToggle: CrToggleElement,
    radioSelection: CrRadioGroupElement,
    customLinksContainer: HTMLElement,
    customLinksButton: CrRadioButtonElement,
    mostVisitedButton: CrRadioButtonElement,
    mostVisitedContainer: HTMLElement,
  };
}

export class ShortcutsElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-shortcuts';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      customLinksEnabled_: {type: Boolean},
      initialized_: {type: Boolean},
      radioSelection_: {type: String},
      show_: {type: Boolean},
    };
  }

  private customLinksEnabled_: boolean = false;
  protected initialized_: boolean = false;
  protected radioSelection_: string|undefined = undefined;
  protected show_: boolean = false;

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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('customLinksEnabled_')) {
      this.radioSelection_ =
          this.customLinksEnabled_ ? 'customLinksOption' : 'mostVisitedOption';
    }
  }

  private setMostVisitedSettings_() {
    this.pageHandler_.setMostVisitedSettings(
        this.customLinksEnabled_, /* shortcutsVisible= */ this.show_);
  }

  private setShow_(show: boolean) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.SHOW_SHORTCUTS_TOGGLE_CLICKED);
    this.show_ = show;
    this.setMostVisitedSettings_();
  }

  protected onShowToggleChange_(e: CustomEvent<boolean>) {
    this.setShow_(e.detail);
  }

  protected onShowToggleClick_() {
    this.setShow_(!this.show_);
  }

  protected onRadioSelectionChanged_(e: CustomEvent<{value: string}>) {
    if (e.detail.value === this.radioSelection_) {
      return;
    }
    this.customLinksEnabled_ = e.detail.value === 'customLinksOption';
    this.setMostVisitedSettings_();
  }

  private setCustomLinksEnabled_(option: string) {
    if (this.radioSelection_ === option) {
      return;
    }
    recordCustomizeChromeAction(
        CustomizeChromeAction.SHOW_SHORTCUTS_TOGGLE_CLICKED);
    this.customLinksEnabled_ = option === 'customLinksOption';
    this.setMostVisitedSettings_();
  }

  protected onCustomLinksClick_() {
    this.setCustomLinksEnabled_('customLinksOption');
  }

  protected onMostVisitedClick_() {
    this.setCustomLinksEnabled_('mostVisitedOption');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-shortcuts': ShortcutsElement;
  }
}

customElements.define(ShortcutsElement.is, ShortcutsElement);
