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

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';

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
      /** The ID of the extension controlling the associated field */
      extensionId: String,
      /** The name of the extension controlling the associated field */
      extensionName: String,
    };
  }

  extensionId: string;
  extensionName: string;

  private getLabel_(): string {
    return this.i18n('controlledByExtension', this.extensionName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extension-controlled-icon': ExtensionControlledIconElement;
  }
}

customElements.define(
    ExtensionControlledIconElement.is, ExtensionControlledIconElement);
