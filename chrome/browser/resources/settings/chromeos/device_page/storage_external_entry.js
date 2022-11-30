// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-external-entry' is the polymer element for showing a certain
 * external storage device with a toggle switch. When the switch is ON,
 * the storage's uuid will be saved to a preference.
 */
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 */
const StorageExternalEntryElementBase =
    mixinBehaviors([WebUIListenerBehavior, PrefsBehavior], PolymerElement);

/** @polymer */
class StorageExternalEntryElement extends StorageExternalEntryElementBase {
  static get is() {
    return 'storage-external-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * FileSystem UUID of an external storage.
       */
      uuid: String,

      /**
       * Label of an external storage.
       */
      label: String,

      /** @private {chrome.settingsPrivate.PrefObject} */
      visiblePref_: {
        type: Object,
        value() {
          return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
        },
      },
    };
  }

  static get observers() {
    return [
      'updateVisible_(prefs.arc.visible_external_storages.*)',
    ];
  }

  /**
   * Handler for when the toggle button for this entry is clicked by a user.
   * @param {!Event} event
   * @private
   */
  onVisibleChange_(event) {
    const visible = !!event.target.checked;
    if (visible) {
      this.appendPrefListItem('arc.visible_external_storages', this.uuid);
    } else {
      this.deletePrefListItem('arc.visible_external_storages', this.uuid);
    }
    chrome.metricsPrivate.recordBoolean(
        'Arc.ExternalStorage.SetVisible', visible);
  }

  /**
   * Updates |visiblePref_| by reading the preference and check if it contains
   * UUID of this storage.
   * @private
   */
  updateVisible_() {
    const uuids = /** @type {!Array<string>} */ (
        this.getPref('arc.visible_external_storages').value);
    const visible = uuids.some((id) => id === this.uuid);
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: visible,
    };
    this.visiblePref_ = pref;
  }
}

customElements.define(
    StorageExternalEntryElement.is, StorageExternalEntryElement);
