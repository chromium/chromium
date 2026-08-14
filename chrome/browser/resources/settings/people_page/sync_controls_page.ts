// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './sync_controls.js';
import '../settings_page/settings_subpage.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getHtml} from './sync_controls_page.html.js';

const SettingsSyncControlsPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(CrLitElement));

export class SettingsSyncControlsPageElement extends
    SettingsSyncControlsPageElementBase {
  static get is() {
    return 'settings-sync-controls-page';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      syncStatus_: {type: Object},
    };
  }

  protected accessor syncStatus_: SyncStatus|null = null;

  override connectedCallback() {
    super.connectedCallback();

    SyncBrowserProxyImpl.getInstance().getSyncStatus().then(
        this.onSyncStatusChanged_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.onSyncStatusChanged_.bind(this));
  }

  private onSyncStatusChanged_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sync-controls-page': SettingsSyncControlsPageElement;
  }
}

customElements.define(
    SettingsSyncControlsPageElement.is, SettingsSyncControlsPageElement);
