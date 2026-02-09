// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app_refresh.css.js';
import {getHtml} from './app_refresh.html.js';
import type {DefaultBrowserBrowserProxy} from './browser_proxy.js';
import {DefaultBrowserBrowserProxyImpl} from './browser_proxy.js';

const AppRefreshElementBase = WebUiListenerMixinLit(CrLitElement);

export class AppRefreshElement extends AppRefreshElementBase {
  static get is() {
    return 'default-browser-app-refresh';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      anyButtonClicked_: {type: Boolean},
    };
  }

  protected accessor anyButtonClicked_: boolean = false;
  private browserProxy_: DefaultBrowserBrowserProxy =
      DefaultBrowserBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'reset-default-browser-buttons', this.resetButtons_.bind(this));
  }

  protected onConfirmDefaultBrowserClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.setAsDefaultBrowser();
  }

  protected onSkipDefaultBrowserClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.skipDefaultBrowser();
  }

  private resetButtons_() {
    this.anyButtonClicked_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'default-browser-app-refresh': AppRefreshElement;
  }
}

customElements.define(AppRefreshElement.is, AppRefreshElement);
