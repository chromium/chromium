// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './safety_hub_module.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './safety_hub_entry_point.html.js';
// clang-format on

export interface SettingsSafetyHubEntryPointElement {
  $: {
    button: HTMLElement,
  };
}

const SettingsSafetyHubEntryPointElementBase = I18nMixin(PolymerElement);

export class SettingsSafetyHubEntryPointElement extends
    SettingsSafetyHubEntryPointElementBase {
  static get is() {
    return 'settings-safety-hub-entry-point';
  }

  static get template() {
    return getTemplate();
  }

  private onClick_() {
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-entry-point': SettingsSafetyHubEntryPointElement;
  }
}

customElements.define(
    SettingsSafetyHubEntryPointElement.is, SettingsSafetyHubEntryPointElement);
