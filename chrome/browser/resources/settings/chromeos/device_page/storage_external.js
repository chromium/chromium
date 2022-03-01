// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-storage-external' is the settings subpage for external storage
 * settings.
 */

import '//resources/cr_components/localized_link/localized_link.js';
import './storage_external_entry.js';
import '../../prefs/prefs.js';
import '../../settings_shared_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BatteryStatus, DevicePageBrowserProxy, DevicePageBrowserProxyImpl, ExternalStorage, getDisplayApi, IdleBehavior, LidClosedBehavior, NoteAppInfo, NoteAppLockScreenSupport, PowerManagementSettings, PowerSource, StorageSpaceState} from './device_page_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-storage-external',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of the plugged-in external storages.
     * @private {Array<!ExternalStorage>}
     */
    externalStorages_: {
      type: Array,
      value() {
        return [];
      }
    },

    /** @private {!chrome.settingsPrivate.PrefObject} */
    externalStorageVisiblePref_: {
      type: Object,
      value() {
        return /** @type {!chrome.settingsPrivate.PrefObject} */ ({});
      },
    },
  },

  /** @private {?DevicePageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.browserProxy_.setExternalStoragesUpdatedCallback(
        this.handleExternalStoragesUpdated_.bind(this));
    this.browserProxy_.updateExternalStorages();
  },

  /**
   * @param {Array<!ExternalStorage>} storages
   * @private
   */
  handleExternalStoragesUpdated_(storages) {
    this.externalStorages_ = storages;
  },

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
  },
});
