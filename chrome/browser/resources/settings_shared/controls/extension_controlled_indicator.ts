// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ExtensionControlBrowserProxyImpl} from '../extension_control_browser_proxy.js';

import {getCss} from './extension_controlled_indicator.css.js';
import {getHtml} from './extension_controlled_indicator.html.js';

export class ExtensionControlledIndicatorElement extends CrLitElement {
  static get is() {
    return 'extension-controlled-indicator';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      extensionCanBeDisabled: {type: Boolean},
      extensionId: {type: String},
      extensionName: {type: String},
      extensionNameOnlyInLabel: {type: Boolean},
    };
  }

  accessor extensionCanBeDisabled: boolean = false;
  accessor extensionId: string = '';
  accessor extensionName: string = '';
  accessor extensionNameOnlyInLabel: boolean = false;

  protected getLabel_(): string {
    if (this.extensionNameOnlyInLabel) {
      return this.extensionName;
    }
    return loadTimeData.getStringF('controlledByExtension', this.extensionName);
  }

  protected onManageClick_() {
    const manageUrl = 'chrome://extensions/?id=' + this.extensionId;
    OpenWindowProxyImpl.getInstance().openUrl(manageUrl);
  }

  protected onDisableClick_() {
    assert(this.extensionCanBeDisabled);
    ExtensionControlBrowserProxyImpl.getInstance().disableExtension(
        this.extensionId);
    this.fire('disable-extension-click');
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
