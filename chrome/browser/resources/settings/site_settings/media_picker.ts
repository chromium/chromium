// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'media-picker' handles showing the dropdown allowing users to select the
 * default camera/microphone.
 */
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './media_picker.css.js';
import {getHtml} from './media_picker.html.js';
import type {MediaPickerEntry} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

export interface MediaPickerElement {
  $: {
    mediaPicker: HTMLSelectElement,
    picker: HTMLElement,
  };
}

const MediaPickerElementBase =
    SiteSettingsMixinLit(WebUiListenerMixinLit(CrLitElement));

export class MediaPickerElement extends MediaPickerElementBase {
  static get is() {
    return 'media-picker';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The type of media picker, either 'camera' or 'mic'.
       */
      type: {type: String},

      /** Label for a11y purposes. */
      label: {type: String},

      /**
       * The devices available to pick from.
       */
      devices: {type: Array},

      selectedDevice_: {type: String},
    };
  }

  accessor type: string = '';
  accessor label: string = '';
  accessor devices: MediaPickerEntry[] = [];
  protected accessor selectedDevice_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'updateDevicesMenu',
        (type: string, devices: MediaPickerEntry[], selectedDevice: string) =>
            this.updateDevicesMenu_(type, devices, selectedDevice));
    this.browserProxy.initializeCaptureDevices(this.type);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedProperties.has('devices') ||
        changedPrivateProperties.has('selectedDevice_')) {
      this.$.mediaPicker.value = this.selectedDevice_;
    }
  }

  /**
   * Updates the microphone/camera devices menu with the given entries.
   * @param type The device type.
   * @param devices List of available devices.
   * @param selectedDevice The unique id of the current default device.
   */
  private updateDevicesMenu_(
      type: string, devices: MediaPickerEntry[], selectedDevice: string) {
    if (type !== this.type) {
      return;
    }

    this.$.picker.hidden = devices.length === 0;
    if (devices.length > 0) {
      this.devices = devices;
      this.selectedDevice_ = selectedDevice;
    }
  }

  /**
   * A handler for when an item is selected in the media picker.
   */
  protected onChange_() {
    this.selectedDevice_ = this.$.mediaPicker.value;
    this.browserProxy.setPreferredCaptureDevice(
        this.type, this.$.mediaPicker.value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'media-picker': MediaPickerElement;
  }
}

customElements.define(MediaPickerElement.is, MediaPickerElement);
