// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-install-row' is responsible for displaying companion app
 * information and initiating the app installation flow.
 */

import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CompanionAppInfo} from './input_device_settings_types.js';
import {getTemplate} from './per_device_install_row.html.js';

const PerDeviceInstallRowElementBase = I18nMixin(PolymerElement);

export class PerDeviceInstallRowElement extends PerDeviceInstallRowElementBase {
  static get is() {
    return 'per-device-install-row' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      appInfo: {
        type: Object,
      },

      installAppLabel: {
        type: String,
        value: '',
        computed: 'computeInstallAppLabel(appInfo.*)',
      },
    };
  }

  appInfo: CompanionAppInfo;
  installAppLabel: string;

  private onInstallCompanionAppButtonClicked(): void {
    window.open(this.appInfo.actionLink);
  }

  private computeInstallAppLabel(): string {
    return this.i18n('installAppLabel', this.appInfo.appName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PerDeviceInstallRowElement.is]: PerDeviceInstallRowElement;
  }
}

customElements.define(
    PerDeviceInstallRowElement.is, PerDeviceInstallRowElement);
