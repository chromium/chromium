// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-default-browser-page' is the settings page that contains
 * settings to change the default browser (i.e. which the OS will open).
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../settings_page/settings_section.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {getCss as getCrSharedStyle} from 'chrome://resources/cr_elements/cr_shared_style_lit.css.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';

import {loadTimeData} from '../i18n_setup.js';
import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {getCss as getSettingsSharedLit} from '../settings_shared_lit.css.js';

import type {DefaultBrowserBrowserProxy, DefaultBrowserInfo} from './default_browser_browser_proxy.js';
import {DefaultBrowserBrowserProxyImpl} from './default_browser_browser_proxy.js';
import {getHtml} from './default_browser_page.html.js';

const SettingsDefaultBrowserPageElementBase =
    WebUiListenerMixinLit(CrLitElement);

export class SettingsDefaultBrowserPageElement extends
    SettingsDefaultBrowserPageElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-default-browser-page';
  }

  static override get styles() {
    return [getCrSharedStyle(), getSettingsSharedLit()];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      canPin_: {type: Boolean},
      isDefault_: {type: Boolean},
      isSecondaryInstall_: {type: Boolean},
      isUnknownError_: {type: Boolean},
      maySetDefaultBrowser_: {type: Boolean},
    };
  }

  private accessor canPin_: boolean = false;
  protected accessor isDefault_: boolean = false;
  protected accessor isSecondaryInstall_: boolean = false;
  protected accessor isUnknownError_: boolean = false;
  protected accessor maySetDefaultBrowser_: boolean = false;

  private browserProxy_: DefaultBrowserBrowserProxy =
      DefaultBrowserBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'browser-default-state-changed',
        this.updateDefaultBrowserState_.bind(this));

    this.browserProxy_.requestDefaultBrowserState().then(
        this.updateDefaultBrowserState_.bind(this));
  }

  private updateDefaultBrowserState_(defaultBrowserState: DefaultBrowserInfo) {
    this.canPin_ = defaultBrowserState.canPin;
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

  protected getMakeDefaultLabel(): string {
    return loadTimeData.getString(
        this.canPin_ ? 'defaultBrowserMakeDefaultAndPin' :
                       'defaultBrowserMakeDefault');
  }

  protected onSetDefaultBrowserClick_() {
    this.browserProxy_.setAsDefaultBrowser(this.canPin_);
  }

  // SettingsPlugin implementation
  async searchContents(query: string) {
    const searchRequest = await getSearchManager().search(query, this);
    return searchRequest.getSearchResult();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-default-browser-page': SettingsDefaultBrowserPageElement;
  }
}

customElements.define(
    SettingsDefaultBrowserPageElement.is, SettingsDefaultBrowserPageElement);
