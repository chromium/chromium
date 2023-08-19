// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/tangible_sync_style_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {DefaultBrowserBrowserProxy, DefaultBrowserBrowserProxyImpl} from './browser_proxy.js';

const DefaultBrowserAppElementBase = WebUiListenerMixin(PolymerElement);

export class DefaultBrowserAppElement extends DefaultBrowserAppElementBase {
  static get is() {
    return 'default-browser-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      anyButtonClicked_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private anyButtonClicked_: boolean;
  private browserProxy_: DefaultBrowserBrowserProxy =
      DefaultBrowserBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'reset-default-browser-buttons', this.resetButtons_.bind(this));
  }

  private onConfirmDefaultBrowserClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.setAsDefaultBrowser();
  }

  private onSkipDefaultBrowserClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.skipDefaultBrowser();
  }

  private resetButtons_() {
    this.anyButtonClicked_ = false;
  }

  /**
   * Disable buttons if a button was clicked.
   */
  private areButtonsDisabled_() {
    return this.anyButtonClicked_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'default-browser-app': DefaultBrowserAppElement;
  }
}

customElements.define(DefaultBrowserAppElement.is, DefaultBrowserAppElement);
