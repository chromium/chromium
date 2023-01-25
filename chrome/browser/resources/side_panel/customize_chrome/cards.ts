// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './strings.m.js';

import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cards.html.js';
import {ChromeCartProxy} from './chrome_cart_proxy.js';
import {CustomizeChromePageHandlerInterface, ModuleSettings} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

/*
 * Element that lets the user configure module status and settings. From a UI
 * standpoint, we refer to modules as cards.
 */
export class CardsElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-cards';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The list of modules that can be enabled or disabled on the NTP. */
      modules_: Array,

      /** Whether the modules are customizable or not. */
      show_: Boolean,

      /** Whether the modules are managed by admin policies or not. */
      managedByPolicy_: Boolean,

      // Discount checkbox is a workaround for crbug.com/1199465 and will be
      // removed after module customization is better defined. Please avoid
      // using similar pattern for other features.
      discountCheckbox_: {
        type: Boolean,
        value: false,
      },

      discountCheckboxEligible_: {
        type: Boolean,
        value: false,
      },

      initialized_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return ['modulesChanged_(modules_.*)'];
  }

  private modules_: ModuleSettings[];
  private show_: boolean;
  private managedByPolicy_: boolean;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private setModulesSettingsListenerId_: number|null = null;
  private discountCheckbox_: boolean;
  private discountCheckboxEligible_: boolean;
  private initialized_: boolean;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setModulesSettingsListenerId_ =
        CustomizeChromeApiProxy.getInstance()
            .callbackRouter.setModulesSettings.addListener(
                (modulesSettings: ModuleSettings[], managed: boolean,
                 visible: boolean) => {
                  this.show_ = visible;
                  this.managedByPolicy_ = managed;
                  this.modules_ = modulesSettings;
                  this.initialized_ = true;
                });
    this.pageHandler_.updateModulesSettings();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.setModulesSettingsListenerId_!);
  }

  private modulesChanged_() {
    if (this.modules_.some(module => module.id === 'chrome_cart')) {
      ChromeCartProxy.getHandler().getDiscountToggleVisible().then(
          ({toggleVisible}) => {
            this.discountCheckboxEligible_ = toggleVisible;
          });

      ChromeCartProxy.getHandler().getDiscountEnabled().then(({enabled}) => {
        this.discountCheckbox_ = enabled;
      });
    }
  }

  private onShowChange_(e: CustomEvent<boolean>) {
    this.show_ = e.detail;
    this.pageHandler_.setModulesVisible(this.show_);
  }

  private onCardStatusChange_(e: DomRepeatEvent<ModuleSettings, CustomEvent>) {
    const id: string = e.model.item.id;
    const checked: boolean = e.detail;
    this.pageHandler_.setModuleDisabled(id, !checked);
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        'NewTabPage.Modules.' + (checked ? 'Enabled' : 'Disabled'), id);
  }

  private showDiscountCheckbox_(
      id: string, checked: boolean, eligible: boolean): boolean {
    return id === 'chrome_cart' && checked && eligible;
  }

  private onDiscountCheckboxChange_() {
    if (this.discountCheckboxEligible_) {
      ChromeCartProxy.getHandler().setDiscountEnabled(this.discountCheckbox_);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-cards': CardsElement;
  }
}

customElements.define(CardsElement.is, CardsElement);
