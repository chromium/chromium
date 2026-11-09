// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-device-tab-visibility-page' is the settings subpage for choosing
 * which synced devices appear in the cross-device tab list.
 */
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../icons.html.js';
import '../settings_page/settings_subpage.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {DeviceTabVisibilityBrowserProxy, DeviceTabVisibilityEntry} from './device_tab_visibility_browser_proxy.js';
import {DeviceTabVisibilityBrowserProxyImpl, SyncedDeviceFormFactor} from './device_tab_visibility_browser_proxy.js';
import {getCss} from './device_tab_visibility_page.css.js';
import {getHtml} from './device_tab_visibility_page.html.js';

const SettingsDeviceTabVisibilityPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(CrLitElement));

export class SettingsDeviceTabVisibilityPageElement extends
    SettingsDeviceTabVisibilityPageElementBase {
  static get is() {
    return 'settings-device-tab-visibility-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      devices_: {type: Array},
    };
  }

  protected accessor devices_: DeviceTabVisibilityEntry[] = [];

  private browserProxy_: DeviceTabVisibilityBrowserProxy =
      DeviceTabVisibilityBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'device-tab-visibility-list-changed',
        this.onDeviceTabVisibilityListChanged_.bind(this));
    this.browserProxy_.getDeviceTabVisibilityList().then(
        this.onDeviceTabVisibilityListChanged_.bind(this));
  }

  private onDeviceTabVisibilityListChanged_(
      devices: DeviceTabVisibilityEntry[]) {
    this.devices_ = devices;
  }

  protected getDeviceIcon_(formFactor: SyncedDeviceFormFactor): string {
    switch (formFactor) {
      case SyncedDeviceFormFactor.PHONE:
        return 'settings:smartphone';
      case SyncedDeviceFormFactor.TABLET:
        return 'settings:tablet';
      case SyncedDeviceFormFactor.UNKNOWN:
      case SyncedDeviceFormFactor.DESKTOP:
      case SyncedDeviceFormFactor.AUTOMOTIVE:
      case SyncedDeviceFormFactor.WEARABLE:
      case SyncedDeviceFormFactor.TV:
        return 'cr:computer';
      default:
        assertNotReachedCase(formFactor);
    }
  }

  protected onDeviceVisibilityToggleChange_(e: CustomEvent<boolean>) {
    const toggle = e.currentTarget as HTMLElement;
    const sessionTag = toggle.dataset['sessionTag'];
    assert(sessionTag);
    this.browserProxy_.setDeviceTabVisibility(sessionTag, e.detail);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-device-tab-visibility-page':
        SettingsDeviceTabVisibilityPageElement;
  }
}

customElements.define(
    SettingsDeviceTabVisibilityPageElement.is,
    SettingsDeviceTabVisibilityPageElement);
