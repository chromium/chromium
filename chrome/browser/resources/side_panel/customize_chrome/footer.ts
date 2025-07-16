// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';

import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import type {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, ManagementNoticeState} from './customize_chrome.mojom-webui.js';
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
      /** Whether the footer is shown. */
      checked_: {type: Boolean},

      /**
         Whether the footer is managed by enterprise custom label or logo
         policy.
       */
      managedByPolicy_: {type: Boolean},
    };
  }

  protected accessor managedByPolicy_: boolean = false;
  protected accessor checked_: boolean = false;
  protected canShowManagement_: boolean = false;

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
            (visible: boolean, _: boolean,
             managementNoticeState: ManagementNoticeState) => {
              // Checked if the footer is visible by user choice  or if it is enabled by policy.
              this.checked_ = visible || managementNoticeState.enabledByPolicy;
              this.managedByPolicy_ = managementNoticeState.enabledByPolicy;
              this.canShowManagement_ = managementNoticeState.canBeShown;
            });
    this.pageHandler_.updateFooterSettings();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setFooterSettingsListenerId_);
    this.callbackRouter_.removeListener(this.setFooterSettingsListenerId_);
  }

  private setChecked_(checked: boolean) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.SHOW_FOOTER_TOGGLE_CLICKED);
    chrome.metricsPrivate.recordBoolean(
        'NewTabPage.Footer.ToggledVisibility', checked);
    if (this.canShowManagement_) {
      chrome.metricsPrivate.recordBoolean(
          'NewTabPage.Footer.ToggledVisibility.Enterprise', checked);
    } else {
      chrome.metricsPrivate.recordBoolean(
          'NewTabPage.Footer.ToggledVisibility.Consumer', checked);
    }
    this.checked_ = checked;
    this.setFooterVisible_();
  }

  protected onShowToggleChange_(e: CustomEvent<boolean>) {
    this.setChecked_(e.detail);
  }

  protected onShowToggleClick_() {
    if (this.managedByPolicy_) {
      return;
    }
    this.setChecked_(!this.checked_);
  }

  private setFooterVisible_() {
    this.pageHandler_.setFooterVisible(this.checked_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-footer': FooterElement;
  }
}

customElements.define(FooterElement.is, FooterElement);
