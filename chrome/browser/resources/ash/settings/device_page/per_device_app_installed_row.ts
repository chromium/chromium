// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-app-installed-row' is responsible for displaying companion app
 * information for an installed app.
 */

import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {CompanionAppInfo, InputDeviceSettingsProviderInterface} from './input_device_settings_types.js';
import {getTemplate} from './per_device_app_installed_row.html.js';

const PerDeviceAppInstalledRowElementBase = I18nMixin(PolymerElement);

export class PerDeviceAppInstalledRowElement extends
    PerDeviceAppInstalledRowElementBase {
  static get is() {
    return 'per-device-app-installed-row' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      appInfo: {
        type: Object,
      },

      openAppLabel: {
        type: String,
        computed: 'computeOpenAppLabel(appInfo.*)',
      },
    };
  }

  appInfo: CompanionAppInfo;
  openAppLabel: string;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();

  private computeOpenAppLabel(): string {
    return this.i18n('openAppLabel', this.appInfo.appName);
  }

  private onCompanionAppRowClick(): void {
    this.inputDeviceSettingsProvider.launchCompanionApp(this.appInfo.packageId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PerDeviceAppInstalledRowElement.is]: PerDeviceAppInstalledRowElement;
  }
}

customElements.define(
    PerDeviceAppInstalledRowElement.is, PerDeviceAppInstalledRowElement);
