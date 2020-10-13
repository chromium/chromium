// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './mini_page.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/** Element that lets the user configure modules settings. */
class CustomizeModulesElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-modules';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      hide_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  constructor() {
    super();
    /** @private {?number} */
    this.setModulesVisibleListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setModulesVisibleListenerId_ =
        BrowserProxy.getInstance().callbackRouter.setModulesVisible.addListener(
            visible => {
              this.hide_ = !visible;
            });
    BrowserProxy.getInstance().handler.updateModulesVisible();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    BrowserProxy.getInstance().callbackRouter.removeListener(
        assert(this.setModulesVisibleListenerId_));
  }

  apply() {
    BrowserProxy.getInstance().handler.setModulesVisible(!this.hide_);
  }

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onHideChange_(e) {
    this.hide_ = e.detail;
  }
}

customElements.define(CustomizeModulesElement.is, CustomizeModulesElement);
