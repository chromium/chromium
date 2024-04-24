// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App, AppParentalControlsHandlerInterface} from '../../mojom-webui/app_parental_controls_handler.mojom-webui.js';

import {getTemplate} from './app_parental_controls_subpage.html.js';
import {getAppParentalControlsProvider} from './mojo_interface_provider.js';

export class SettingsAppParentalControlsSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-app-parental-controls-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appList_: {
        type: Array,
        value: [],
      },
    };
  }

  private appList_: App[];
  private mojoInterfaceProvider: AppParentalControlsHandlerInterface;

  constructor() {
    super();
    this.mojoInterfaceProvider = getAppParentalControlsProvider();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.mojoInterfaceProvider.getApps().then((result) => {
      this.appList_ = result.apps;
    });
  }

  private alphabeticalSort_(first: App, second: App): number {
    return first.title!.localeCompare(second.title!);
  }
}

customElements.define(
    SettingsAppParentalControlsSubpageElement.is,
    SettingsAppParentalControlsSubpageElement);
