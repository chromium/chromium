// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cards.html.js';
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
    };
  }

  private modules_: ModuleSettings[];
  private show_: boolean;
  private managedByPolicy_: boolean;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private setModulesSettingsListenerId_: number|null = null;

  // TODO:(crbug.com/1401492): Add chrome cart discount consent support.

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
                });
    this.pageHandler_.updateModulesSettings();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.setModulesSettingsListenerId_!);
  }

  private onShowChange_(e: CustomEvent<boolean>) {
    this.show_ = e.detail;
    this.pageHandler_.setModulesVisible(this.show_);
  }

  private onCardStatusChange_(e: DomRepeatEvent<ModuleSettings, CustomEvent>) {
    const id: string = e.model.item.id;
    const checked: boolean = e.detail;
    this.pageHandler_.setModuleDisabled(id, !checked);

    // TODO(crbug.com/1384258): Add metrics.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-cards': CardsElement;
  }
}

customElements.define(CardsElement.is, CardsElement);
