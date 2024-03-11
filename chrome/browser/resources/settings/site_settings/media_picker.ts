// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'media-picker' handles showing the dropdown allowing users to select the
 * default camera/microphone.
 */
import 'chrome://resources/cr_elements/md_select.css.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './media_picker.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import type {MediaPickerEntry} from './site_settings_prefs_browser_proxy.js';

interface MediaPickerElement {
  $: {
    mediaPicker: HTMLSelectElement,
    picker: HTMLElement,
  };
}

const MediaPickerElementBase =
    SiteSettingsMixin(WebUiListenerMixin(PolymerElement));

class MediaPickerElement extends MediaPickerElementBase {
  static get is() {
    return 'media-picker';
  }

  static get template() {
    return getTemplate();
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
       */
      devices: Array,
    };
  }

  type: string;
  label: string;
  devices: MediaPickerEntry[];

  override ready() {
    super.ready();

    this.addWebUiListener(
        'updateDevicesMenu',
        (type: string, devices: MediaPickerEntry[], selectedDevice: string) =>
            this.updateDevicesMenu_(type, devices, selectedDevice));
    this.browserProxy.initializeCaptureDevices(this.type);
  }

  /**
   * Updates the microphone/camera devices menu with the given entries.
   * @param type The device type.
   * @param devices List of available devices.
   * @param defaultDevice The unique id of the current default device.
   */
  private updateDevicesMenu_(
      type: string, devices: MediaPickerEntry[], selectedDevice: string) {
    if (type !== this.type) {
      return;
    }

    this.$.picker.hidden = devices.length === 0;
    if (devices.length > 0) {
      this.devices = devices;

      // Wait for <select> to be populated.
      microTask.run(() => {
        this.$.mediaPicker.value = selectedDevice;
      });
    }
  }

  /**
   * A handler for when an item is selected in the media picker.
   */
  private onChange_() {
    this.browserProxy.setPreferredCaptureDevice(
        this.type, this.$.mediaPicker.value);
  }
}

customElements.define(MediaPickerElement.is, MediaPickerElement);
