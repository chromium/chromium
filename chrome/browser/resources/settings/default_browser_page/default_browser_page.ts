// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-default-browser-page' is the settings page that contains
 * settings to change the default browser (i.e. which the OS will open).
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../icons.html.js';
import '../settings_shared.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DefaultBrowserBrowserProxy, DefaultBrowserInfo} from './default_browser_browser_proxy.js';
import {DefaultBrowserBrowserProxyImpl} from './default_browser_browser_proxy.js';
import {getTemplate} from './default_browser_page.html.js';

const SettingsDefaultBrowserPageElementBase =
    WebUiListenerMixin(PolymerElement);

export class SettingsDefaultBrowserPageElement extends
    SettingsDefaultBrowserPageElementBase {
  static get is() {
    return 'settings-default-browser-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isDefault_: Boolean,
      isSecondaryInstall_: Boolean,
      isUnknownError_: Boolean,
      maySetDefaultBrowser_: Boolean,
    };
  }

  private isDefault_: boolean;
  private isSecondaryInstall_: boolean;
  private isUnknownError_: boolean;
  private maySetDefaultBrowser_: boolean;
  private browserProxy_: DefaultBrowserBrowserProxy =
      DefaultBrowserBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.addWebUiListener(
        'browser-default-state-changed',
        this.updateDefaultBrowserState_.bind(this));

    this.browserProxy_.requestDefaultBrowserState().then(
        this.updateDefaultBrowserState_.bind(this));
  }

  private updateDefaultBrowserState_(defaultBrowserState: DefaultBrowserInfo) {
    this.isDefault_ = false;
    this.isSecondaryInstall_ = false;
    this.isUnknownError_ = false;
    this.maySetDefaultBrowser_ = false;

    if (defaultBrowserState.isDefault) {
      this.isDefault_ = true;
    } else if (!defaultBrowserState.canBeDefault) {
      this.isSecondaryInstall_ = true;
    } else if (
        !defaultBrowserState.isDisabledByPolicy &&
        !defaultBrowserState.isUnknownError) {
      this.maySetDefaultBrowser_ = true;
    } else {
      this.isUnknownError_ = true;
    }
  }

  private onSetDefaultBrowserClick_() {
    this.browserProxy_.setAsDefaultBrowser();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-default-browser-page': SettingsDefaultBrowserPageElement;
  }
}

customElements.define(
    SettingsDefaultBrowserPageElement.is, SettingsDefaultBrowserPageElement);
