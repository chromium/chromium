// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {DrivePickerApiProxyImpl} from './drive_picker_api_proxy.js';
import type {GooglePickerResponse} from './drive_picker_api_proxy.js';
import type {DrivePickerResultHandlerRemote} from './drive_picker_result_handler.mojom-webui.js';

export class DrivePickerHostUntrustedAppElement extends CrLitElement {
  static get is() {
    return 'drive-picker-host-untrusted-app';
  }

  static override get styles() {
    return getCss();
  }

  private browserProxy_ = BrowserProxyImpl.getInstance();
  private drivePickerApiProxy_ = DrivePickerApiProxyImpl.getInstance();
  private resultHandler_: DrivePickerResultHandlerRemote|null = null;
  private listenerIds_: number[] = [];

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds_ = [
      this.browserProxy_.callbackRouter.showDrivePicker.addListener(
          this.showDrivePicker_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => assert(this.browserProxy_.callbackRouter.removeListener(id)));
    this.listenerIds_ = [];
  }

  private async showDrivePicker_(
      resultHandler: DrivePickerResultHandlerRemote,
      keys: {oauthToken: string, apiKey: string, appId: string}) {
    this.resultHandler_ = resultHandler;

    try {
      await this.drivePickerApiProxy_.showPicker(
          keys.oauthToken, keys.apiKey, keys.appId,
          (data: GooglePickerResponse) => this.onPickerCallback_(data));
    } catch (e) {
      return;
    }
  }

  private onPickerCallback_(data: GooglePickerResponse) {
    // TODO(crbug.com/497937568): Handle the selected files,
    // sanitize the data, and send it to the trusted host using
    // this.resultHandler_.
    if (data[google.picker.Response.ACTION] === google.picker.Action.PICKED) {
      console.info('Drive Picker result received', this.resultHandler_);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'drive-picker-host-untrusted-app': DrivePickerHostUntrustedAppElement;
  }
}

customElements.define(
    DrivePickerHostUntrustedAppElement.is, DrivePickerHostUntrustedAppElement);
