// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ExtensionControlBrowserProxyImpl} from '/shared/settings/extension_control_browser_proxy.js';

import {getTemplate} from './extension_controlled_indicator.html.js';

export class ExtensionControlledIndicatorElement extends PolymerElement {
  static get is() {
    return 'extension-controlled-indicator';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      extensionCanBeDisabled: Boolean,
      extensionId: String,
      extensionName: String,
    };
  }

  extensionCanBeDisabled: boolean;
  extensionId: string;
  extensionName: string;

  private getLabel_(): string {
    return loadTimeData.getStringF('controlledByExtension', this.extensionName);
  }

  private onManageClick_(): void {
    const manageUrl = 'chrome://extensions/?id=' + this.extensionId;
    OpenWindowProxyImpl.getInstance().openUrl(manageUrl);
  }

  private onDisableClick_(): void {
    assert(this.extensionCanBeDisabled);
    ExtensionControlBrowserProxyImpl.getInstance().disableExtension(
        this.extensionId);
    this.dispatchEvent(
        new CustomEvent('extension-disable', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extension-controlled-indicator': ExtensionControlledIndicatorElement;
  }
}

customElements.define(
    ExtensionControlledIndicatorElement.is,
    ExtensionControlledIndicatorElement);
