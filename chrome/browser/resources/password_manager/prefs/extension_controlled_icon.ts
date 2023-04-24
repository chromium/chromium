// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `extension-controlled-icon` is an icon that indicates that a given field is
 * managed by an extension and therefore disabled for the user. It mimics the
 * `extension_controlled_indicator` class that shows a similar indicator in the
 * settings code.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {assert} from '//resources/js/assert_ts.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

import {ExtensionControlBrowserProxyImpl} from './extension_control_browser_proxy.js';
import {getTemplate} from './extension_controlled_icon.html.js';

const ExtensionControlledIconElementBase = I18nMixin(PolymerElement);

export class ExtensionControlledIconElement extends
    ExtensionControlledIconElementBase {
  static get is() {
    return 'extension-controlled-icon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
         Whether the extension can be disabled; false when, e.g., the extension
         is set by policy
       */
      extensionCanBeDisabled: Boolean,
      /** The ID of the extension controlling the associated field */
      extensionId: String,
      /** The name of the extension controlling the associated field */
      extensionName: String,
    };
  }

  extensionCanBeDisabled: boolean;
  extensionId: string;
  extensionName: string;

  private getLabel_(): string {
    return this.i18n('controlledByExtension', this.extensionName);
  }

  private onManageClick_(e: Event) {
    e.stopPropagation();
    const manageUrl = 'chrome://extensions/?id=' + this.extensionId;
    OpenWindowProxyImpl.getInstance().openUrl(manageUrl);
  }

  private onDisableClick_(e: Event) {
    e.stopPropagation();
    assert(this.extensionCanBeDisabled);
    ExtensionControlBrowserProxyImpl.getInstance().disableExtension(
        this.extensionId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extension-controlled-icon': ExtensionControlledIconElement;
  }
}

customElements.define(
    ExtensionControlledIconElement.is, ExtensionControlledIconElement);
