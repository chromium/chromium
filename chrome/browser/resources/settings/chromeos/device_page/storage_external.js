// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-storage-external' is the settings subpage for external storage
 * settings.
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './storage_external_entry.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, ExternalStorage} from './device_page_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsStorageExternalElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
class SettingsStorageExternalElement extends
    SettingsStorageExternalElementBase {
  static get is() {
    return 'settings-storage-external';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * List of the plugged-in external storages.
       * @private {Array<!ExternalStorage>}
       */
      externalStorages_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** @private {!chrome.settingsPrivate.PrefObject} */
      externalStorageVisiblePref_: {
        type: Object,
        value() {
          return /** @type {!chrome.settingsPrivate.PrefObject} */ ({});
        },
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!DevicePageBrowserProxy} */
    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.setExternalStoragesUpdatedCallback(
        this.handleExternalStoragesUpdated_.bind(this));
    this.browserProxy_.updateExternalStorages();
  }

  /**
   * @param {Array<!ExternalStorage>} storages
   * @private
   */
  handleExternalStoragesUpdated_(storages) {
    this.externalStorages_ = storages;
  }

  /**
   * @param {Array<!ExternalStorage>} externalStorages
   * @return {string}
   * @private
   */
  computeStorageListHeader_(externalStorages) {
    return this.i18n(
        !externalStorages || externalStorages.length === 0 ?
            'storageExternalStorageEmptyListHeader' :
            'storageExternalStorageListHeader');
  }
}

customElements.define(
    SettingsStorageExternalElement.is, SettingsStorageExternalElement);
