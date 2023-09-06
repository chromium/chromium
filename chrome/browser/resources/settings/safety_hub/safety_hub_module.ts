// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-module' is the settings page that presents the safety
 * state of Chrome.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './safety_hub_module.html.js';

export interface SiteInfo {
  origin: string;
  detail: string;
}

export interface SiteInfoWithTarget extends SiteInfo {
  target: EventTarget;
}

export class SettingsSafetyHubModuleElement extends PolymerElement {
  static get is() {
    return 'settings-safety-hub-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // List of domains in the list. Each site has origin and detail field.
      sites: {
        type: Array,
        value: () => [],
      },

      // The string for the header label.
      header: String,

      // The strinπg for the subheader label.
      subheader: String,

      // The icon for button of the list item.
      buttonIcon: String,

      // Whether the more action button is visible.
      moreActionVisible: {
        type: Boolean,
        value: false,
      },
    };
  }

  sites: SiteInfo[];
  header: string;
  subheader: string;
  buttonIcon: string;
  moreActionVisible: boolean;

  private onItemButtonClick_(e: DomRepeatEvent<SiteInfo>) {
    const item = e.model.item;
    this.dispatchEvent(new CustomEvent(
        'sh-module-item-button-click',
        {bubbles: true, composed: true, detail: item}));
  }

  private onMoreActionClick_(e: DomRepeatEvent<SiteInfo>) {
    const item: SiteInfoWithTarget = {...e.model.item, target: e.target!};
    this.dispatchEvent(new CustomEvent('sh-module-more-action-button-click', {
      bubbles: true,
      composed: true,
      detail: item,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-module': SettingsSafetyHubModuleElement;
  }
}

customElements.define(
    SettingsSafetyHubModuleElement.is, SettingsSafetyHubModuleElement);
