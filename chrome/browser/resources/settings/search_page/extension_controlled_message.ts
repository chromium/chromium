// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {assert} from '//resources/js/assert.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {ExtensionControlBrowserProxyImpl} from '/shared/settings/extension_control_browser_proxy.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';

import {getCss} from './extension_controlled_message.css.js';
import {getHtml} from './extension_controlled_message.html.js';

const ExtensionControlledMessageElementBase = I18nMixinLit(CrLitElement);

export class ExtensionControlledMessageElement extends
    ExtensionControlledMessageElementBase {
  static get is() {
    return 'extension-controlled-message';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      extensionName: {type: String},
      extensionId: {type: String},
      extensionCanBeDisabled: {type: Boolean},
    };
  }

  accessor extensionName: string;
  accessor extensionCanBeDisabled: boolean;
  accessor extensionId: string;

  protected getDisclaimerHtml_(): TrustedHTML {
    const disclaimerStringId = this.extensionCanBeDisabled ?
        'controlledByExtensionWithDisableOption' :
        'controlledByExtensionWithoutDisableOption';

    return this.i18nAdvanced(disclaimerStringId, {
      substitutions: [this.extensionName, this.i18n('opensInNewTab')],
      attrs: ['id', 'aria-description'],
    });
  }

  protected onDisclaimerClick_(e: Event) {
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
    this.fire('disable-extension-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extension-controlled-message': ExtensionControlledMessageElement;
  }
}

customElements.define(
    ExtensionControlledMessageElement.is, ExtensionControlledMessageElement);
