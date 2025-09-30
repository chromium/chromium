// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-your-saved-info-page' is the entry point for users to see
 * and manage their saved info.
 */

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {SavedInfoHandlerImpl} from './saved_info_handler_proxy.js';
import {getTemplate} from './your_saved_info_page.html.js';


const SettingsYourSavedInfoPageElementBase =
    WebUiListenerMixin(SettingsViewMixin(PrefsMixin(PolymerElement)));

export class SettingsYourSavedInfoPageElement extends
    SettingsYourSavedInfoPageElementBase {
  static get is() {
    return 'settings-your-saved-info-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      passwordsCount: Number,
      passkeysCount: Number,
    };
  }

  declare prefs: {[key: string]: any};
  declare passwordsCount: number|undefined;
  declare passkeysCount: number|undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.setupDataTypeCounters();
  }

  private setupDataTypeCounters() {
    // Password and passkey counts.
    const setPasswordCount =
      (count: { passwordCount: number, passkeyCount: number }) => {
        this.passwordsCount = count.passwordCount;
        this.passkeysCount = count.passkeyCount;
      };
    this.addWebUiListener('password-count-changed', setPasswordCount);
    SavedInfoHandlerImpl.getInstance().getPasswordCount().then(
      setPasswordCount);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-your-saved-info-page': SettingsYourSavedInfoPageElement;
  }
}

customElements.define(
    SettingsYourSavedInfoPageElement.is, SettingsYourSavedInfoPageElement);
