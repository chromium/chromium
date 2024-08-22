// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import './strings.m.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cards.css.js';
import {getHtml} from './cards.html.js';
import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import type {CustomizeChromePageHandlerInterface, ModuleSettings} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface CardsElement {
  $: {
    showToggleContainer: HTMLElement,
  };
}

/*
 * Element that lets the user configure module status and settings. From a UI
 * standpoint, we refer to modules as cards.
 */
export class CardsElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-cards';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** The list of modules that can be enabled or disabled on the NTP. */
      modules_: {type: Array},

      /** Whether the modules are customizable or not. */
      show_: {type: Boolean},

      /** Whether the modules are managed by admin policies or not. */
      managedByPolicy_: {type: Boolean},

      initialized_: {type: Boolean},
    };
  }

  protected modules_: ModuleSettings[] = [];
  protected show_: boolean = false;
  protected managedByPolicy_: boolean = false;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private setModulesSettingsListenerId_: number|null = null;
  protected initialized_: boolean = false;

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

  private setShow_(show: boolean) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.SHOW_CARDS_TOGGLE_CLICKED);
    this.show_ = show;
    this.pageHandler_.setModulesVisible(this.show_);
  }

  protected onShowChange_(e: CustomEvent<boolean>) {
    this.setShow_(e.detail);
  }

  protected onShowToggleClick_() {
    if (this.managedByPolicy_) {
      return;
    }

    this.setShow_(!this.show_);
  }

  private setModuleStatus(index: number, enabled: boolean) {
    const module = this.modules_[index]!;
    module.enabled = enabled;
    this.requestUpdate();
    const id = module.id;
    this.pageHandler_.setModuleDisabled(id, !enabled);
    const metricBase = `NewTabPage.Modules.${enabled ? 'Enabled' : 'Disabled'}`;
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(metricBase, id);
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        `${metricBase}.Customize`, id);
  }

  protected onCardCheckboxChange_(e: CustomEvent<boolean>) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    this.setModuleStatus(index, /*checked= */ e.detail);
  }

  protected onCardClick_(e: Event) {
    if (this.managedByPolicy_) {
      return;
    }

    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const module = this.modules_[index]!;
    this.setModuleStatus(index, !module.enabled);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-cards': CardsElement;
  }
}

customElements.define(CardsElement.is, CardsElement);
