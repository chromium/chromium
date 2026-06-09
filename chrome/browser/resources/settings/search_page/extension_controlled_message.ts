// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {assert} from '//resources/js/assert.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ExtensionControlBrowserProxyImpl} from '/shared/settings/extension_control_browser_proxy.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';

import {getTemplate} from './extension_controlled_message.html.js';

const ExtensionControlledMessageElementBase = I18nMixin(PolymerElement);

export class ExtensionControlledMessageElement extends
    ExtensionControlledMessageElementBase {
  static get is() {
    return 'extension-controlled-message';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      extensionName: String,
      extensionId: String,
      extensionCanBeDisabled: Boolean,
    };
  }

  declare extensionName: string;
  declare extensionCanBeDisabled: boolean;
  declare extensionId: string;

  private getDisclaimerHtml_(name: string): TrustedHTML {
    const disclaimerStringId = this.extensionCanBeDisabled ?
        'controlledByExtensionWithDisableOption' :
        'controlledByExtensionWithoutDisableOption';

    return this.i18nAdvanced(disclaimerStringId, {
      substitutions: [name, this.i18n('opensInNewTab')],
      attrs: ['id', 'aria-description'],
    });
  }

  private onDisclaimerClick_(e: Event) {
    const target = e.target as HTMLElement;
    e.preventDefault();

    if (target.id === 'disableLink') {
      this.onDisableClick_();
      return;
    }

    if (target.id === 'manageLink') {
      this.onManageClick_();
    }
  }

  private onManageClick_() {
    const manageUrl = 'chrome://extensions/?id=' + this.extensionId;
    OpenWindowProxyImpl.getInstance().openUrl(manageUrl);
  }

  private onDisableClick_() {
    assert(this.extensionCanBeDisabled);
    ExtensionControlBrowserProxyImpl.getInstance().disableExtension(
        this.extensionId);
    this.dispatchEvent(new CustomEvent(
        'disable-extension-click', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extension-controlled-message': ExtensionControlledMessageElement;
  }
}

customElements.define(
    ExtensionControlledMessageElement.is, ExtensionControlledMessageElement);
