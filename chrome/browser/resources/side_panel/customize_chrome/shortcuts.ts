// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import './button_label.js';
import '/strings.m.js';

import type {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import type {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getCss} from './shortcuts.css.js';
import {getHtml} from './shortcuts.html.js';
import {TileType} from './tile_type.mojom-webui.js';

const SHORTCUT_CONFIGS = [
  {
    type: TileType.kCustomLinks,
    title: loadTimeData.getString('myShortcuts'),
    description: loadTimeData.getString('shortcutsCurated'),
    buttonName: 'customLinksOption',
    containerName: 'customLinksContainer',
  },
  {
    type: TileType.kTopSites,
    title: loadTimeData.getString('topSites'),
    description: loadTimeData.getString('shortcutsSuggested'),
    buttonName: 'topSitesOption',
    containerName: 'topSitesContainer',
  },
];

export interface ShortcutsElement {
  $: {
    showToggleContainer: HTMLElement,
    showToggle: CrToggleElement,
    radioSelection: CrRadioGroupElement,
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
      shortcutsType_: {type: Object},
      initialized_: {type: Boolean},
      radioSelection_: {type: String},
      show_: {type: Boolean},
      shortcutConfigs_: {type: Array},
    };
  }

  private accessor shortcutsType_: TileType = TileType.kCustomLinks;
  protected accessor initialized_: boolean = false;
  protected accessor radioSelection_: string|undefined = undefined;
  protected accessor show_: boolean = false;
  protected accessor shortcutConfigs_ = SHORTCUT_CONFIGS;

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
            (shortcutsType: TileType, shortcutsVisible: boolean) => {
              this.shortcutsType_ = shortcutsType;
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

    if (changedPrivateProperties.has('shortcutsType_')) {
      const config = this.shortcutConfigs_.find(
          config => config.type === this.shortcutsType_);
      this.radioSelection_ = config ? config.buttonName : undefined;
    }
  }

  private setMostVisitedSettings_() {
    this.pageHandler_.setMostVisitedSettings(
        this.shortcutsType_,
        /* shortcutsVisible= */ this.show_);
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
    const config = this.shortcutConfigs_.find(
        config => config.buttonName === e.detail.value);
    assert(config);
    this.shortcutsType_ = config.type;
    this.setMostVisitedSettings_();
  }

  protected onOptionClick_(type: TileType) {
    if (this.shortcutsType_ === type) {
      return;
    }
    recordCustomizeChromeAction(
        CustomizeChromeAction.SHOW_SHORTCUTS_TOGGLE_CLICKED);
    this.shortcutsType_ = type;
    this.setMostVisitedSettings_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-shortcuts': ShortcutsElement;
  }
}

customElements.define(ShortcutsElement.is, ShortcutsElement);
