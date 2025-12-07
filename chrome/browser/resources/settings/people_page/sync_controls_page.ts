// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './sync_controls.js';
import '../settings_page/settings_subpage.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './sync_controls_page.html.js';

const SettingsSyncControlsPageElementBase =
    SettingsViewMixin(WebUiListenerMixin(PolymerElement));

export class SettingsSyncControlsPageElement extends
    SettingsSyncControlsPageElementBase {
  static get is() {
    return 'settings-sync-controls-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      syncStatus_: {type: Object},
    };
  }

  declare private syncStatus_: SyncStatus|null;

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
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sync-controls-page': SettingsSyncControlsPageElement;
  }
}

customElements.define(
    SettingsSyncControlsPageElement.is, SettingsSyncControlsPageElement);
