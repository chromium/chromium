// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'media-picker' handles showing the dropdown allowing users to select the
 * default camera/microphone.
 */
import 'chrome://resources/cr_elements/md_select_css.m.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SiteSettingsBehavior} from './site_settings_behavior.js';
import {MediaPickerEntry} from './site_settings_prefs_browser_proxy.js';

Polymer({
  is: 'media-picker',

  _template: html`{__html_template__}`,

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The type of media picker, either 'camera' or 'mic'.
     */
    type: String,

    /** Label for a11y purposes. */
    label: String,

    /**
     * The devices available to pick from.
     * @type {Array<MediaPickerEntry>}
     */
    devices: Array,
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'updateDevicesMenu', this.updateDevicesMenu_.bind(this));
    this.browserProxy.getDefaultCaptureDevices(this.type);
  },

  /**
   * Updates the microphone/camera devices menu with the given entries.
   * @param {string} type The device type.
   * @param {!Array<MediaPickerEntry>} devices List of available devices.
   * @param {string} defaultDevice The unique id of the current default device.
   */
  updateDevicesMenu_(type, devices, defaultDevice) {
    if (type !== this.type) {
      return;
    }

    this.$.picker.hidden = devices.length === 0;
    if (devices.length > 0) {
      this.devices = devices;

      // Wait for <select> to be populated.
      this.async(() => {
        this.$.mediaPicker.value = defaultDevice;
      });
    }
  },

  /**
   * A handler for when an item is selected in the media picker.
   * @private
   */
  onChange_() {
    this.browserProxy.setDefaultCaptureDevice(
        this.type, this.$.mediaPicker.value);
  },
});
