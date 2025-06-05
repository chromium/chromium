// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import type {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getCss} from './footer.css.js';
import {getHtml} from './footer.html.js';

export interface FooterElement {
  $: {
    showToggle: CrToggleElement,
  };
}

export class FooterElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-footer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      show_: {type: Boolean},
    };
  }

  protected accessor show_: boolean = false;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private setFooterSettingsListenerId_: number|null = null;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setFooterSettingsListenerId_ =
        this.callbackRouter_.setFooterSettings.addListener(
            (footerVisible: boolean) => {
              this.show_ = footerVisible;
            });
    this.pageHandler_.updateFooterSettings();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setFooterSettingsListenerId_);
    this.callbackRouter_.removeListener(this.setFooterSettingsListenerId_);
  }

  private setShow_(show: boolean) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.SHOW_FOOTER_TOGGLE_CLICKED);
    chrome.metricsPrivate.recordBoolean(
        'NewTabPage.Footer.ToggledVisibility', show);
    this.show_ = show;
    this.setFooterVisible_();
  }

  protected onShowToggleChange_(e: CustomEvent<boolean>) {
    this.setShow_(e.detail);
  }

  protected onShowToggleClick_() {
    this.setShow_(!this.show_);
  }

  private setFooterVisible_() {
    this.pageHandler_.setFooterVisible(this.show_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-footer': FooterElement;
  }
}

customElements.define(FooterElement.is, FooterElement);
