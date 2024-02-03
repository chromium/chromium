// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file provides a custom UI element for indicating that some OS settings
 * are controlled by an extension hosted in Lacros. For extensions hosted in
 * Ash, please use <extension-controlled-indicator> instead.
 *
 * Note that this element is only available in Ash.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LacrosExtensionControlBrowserProxyImpl} from './lacros_extension_control_browser_proxy.js';
import {getTemplate} from './lacros_extension_controlled_indicator.html.js';

export class LacrosExtensionControlledIndicatorElement extends PolymerElement {
  static get is() {
    return 'lacros-extension-controlled-indicator' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      extensionId: String,
      extensionName: String,
    };
  }

  extensionId: string;
  extensionName: string;

  private getLabel_(): string {
    return loadTimeData.getStringF('controlledByExtension', this.extensionName);
  }

  private onManageClick_(): void {
    LacrosExtensionControlBrowserProxyImpl.getInstance().manageLacrosExtension(
        this.extensionId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [LacrosExtensionControlledIndicatorElement.is]:
        LacrosExtensionControlledIndicatorElement;
  }
}

customElements.define(
    LacrosExtensionControlledIndicatorElement.is,
    LacrosExtensionControlledIndicatorElement);
