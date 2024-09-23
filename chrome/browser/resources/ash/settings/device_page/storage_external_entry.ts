// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-external-entry' is the polymer element for showing a certain
 * external storage device with a toggle switch. When the switch is ON,
 * the storage's uuid will be saved to a preference.
 */
import '/shared/settings/prefs/prefs.js';
import '../settings_shared.css.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {getTemplate} from './storage_external_entry.html.js';

const StorageExternalEntryElementBase =
    PrefsMixin(WebUiListenerMixin(PolymerElement));

class StorageExternalEntryElement extends StorageExternalEntryElementBase {
  static get is() {
    return 'storage-external-entry';
  }

  static get template() {
    return getTemplate();
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

      visiblePref_: {
        type: Object,
        value() {
          return {};
        },
      },
    };
  }

  static get observers() {
    return [
      'updateVisible_(prefs.arc.visible_external_storages.*)',
    ];
  }

  uuid: string;
  private visiblePref_: chrome.settingsPrivate.PrefObject<boolean>;

  /**
   * Handler for when the toggle button for this entry is clicked by a user.
   */
  private onVisibleChange_(event: Event): void {
    const isVisible = !!(event.target as SettingsToggleButtonElement).checked;
    if (isVisible) {
      this.appendPrefListItem('arc.visible_external_storages', this.uuid);
    } else {
      this.deletePrefListItem('arc.visible_external_storages', this.uuid);
    }
  }

  /**
   * Updates |visiblePref_| by reading the preference and check if it contains
   * UUID of this storage.
   */
  private updateVisible_(): void {
    const uuids = this.getPref<string[]>('arc.visible_external_storages').value;
    const isVisible = uuids.some((id) => id === this.uuid);
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: isVisible,
    };
    this.visiblePref_ = pref;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'storage-external-entry': StorageExternalEntryElement;
  }
}

customElements.define(
    StorageExternalEntryElement.is, StorageExternalEntryElement);
