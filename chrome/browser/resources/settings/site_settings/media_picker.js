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

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, microTask, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SiteSettingsMixin, SiteSettingsMixinInterface} from './site_settings_mixin.js';
import {MediaPickerEntry} from './site_settings_prefs_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {SiteSettingsMixinInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const MediaPickerElementBase =
    mixinBehaviors([WebUIListenerBehavior], SiteSettingsMixin(PolymerElement));

/** @polymer */
class MediaPickerElement extends MediaPickerElementBase {
  static get is() {
    return 'media-picker';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
    };
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'updateDevicesMenu', this.updateDevicesMenu_.bind(this));
    this.browserProxy.getDefaultCaptureDevices(this.type);
  }

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
      microTask.run(() => {
        this.$.mediaPicker.value = defaultDevice;
      });
    }
  }

  /**
   * A handler for when an item is selected in the media picker.
   * @private
   */
  onChange_() {
    this.browserProxy.setDefaultCaptureDevice(
        this.type, this.$.mediaPicker.value);
  }
}

customElements.define(MediaPickerElement.is, MediaPickerElement);
