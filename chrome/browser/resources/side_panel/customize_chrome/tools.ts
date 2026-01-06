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
import {getCss} from './tools.css.js';
import {getHtml} from './tools.html.js';

export interface ToolChipsElement {
  $: {
    showChipsToggle: CrToggleElement,
  };
}

export class ToolChipsElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-tools';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isChipsEnabled_: {type: Boolean},
    };
  }

  protected accessor isChipsEnabled_: boolean = false;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private setToolsSettingsListenerId_: number|null = null;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
  }

  // This function gets called when the element is attached to the DOM, which
  // allows the element to actively listen for changes to the tools visibility
  // from other sources such as from other tabs or devices.
  override connectedCallback() {
    super.connectedCallback();

    this.setToolsSettingsListenerId_ =
        this.callbackRouter_.setToolsSettings.addListener(
            (isEnabled: boolean) => this.isChipsEnabled_ = isEnabled);

    this.pageHandler_.updateToolChipsSettings();
  }

  // This function gets called when the element is detached from the DOM, so we
  // need to remove any listeners here.
  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setToolsSettingsListenerId_);
    this.callbackRouter_.removeListener(this.setToolsSettingsListenerId_);
  }

  // This function updates the state of the toggle on this instance and sends
  // a Mojo call to the CC page handler to update the pref and broadcast the
  // change to other instances of the CC page handler.
  private setChipsEnabled_(isEnabled: boolean) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.SHOW_ACTION_CHIPS_TOGGLE_CLICKED);
    chrome.metricsPrivate.recordBoolean(
        'NewTabPage.ActionChips.ToggledVisibility', isEnabled);
    this.isChipsEnabled_ = isEnabled;
    this.pageHandler_.setToolChipsVisible(this.isChipsEnabled_);
  }

  // This function gets called whenever the toggle is changed.
  protected onShowToggleChange_(e: CustomEvent<boolean>) {
    this.setChipsEnabled_(e.detail);
  }

  // This function is called when the container around the toggle is clicked,
  // which in turn changes the state of the toggle and calls the above function.
  protected onShowToggleClick_() {
    this.setChipsEnabled_(!this.isChipsEnabled_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-tools': ToolChipsElement;
  }
}

customElements.define(ToolChipsElement.is, ToolChipsElement);
