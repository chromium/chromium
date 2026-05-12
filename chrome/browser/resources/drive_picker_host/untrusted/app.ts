// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {DrivePickerApiProxyImpl} from './drive_picker_api_proxy.js';
import {DrivePickerError} from './drive_picker_result_handler.mojom-webui.js';
import type {DriveFile, DrivePickerResultHandlerRemote} from './drive_picker_result_handler.mojom-webui.js';

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
    this.reportError_(DrivePickerError.kMojoDisconnected);
    this.listenerIds_.forEach(
        id => assert(this.browserProxy_.callbackRouter.removeListener(id)));
    this.listenerIds_ = [];
  }

  private async showDrivePicker_(
      resultHandler: DrivePickerResultHandlerRemote,
      keys: {oauthToken: string, apiKey: string, appId: string}) {
    this.resultHandler_ = resultHandler;

    try {
      const result = await this.drivePickerApiProxy_.showPicker(
          keys.oauthToken, keys.apiKey, keys.appId);

      if (!this.resultHandler_) {
        return;
      }

      if (result === 'CANCEL') {
        this.resultHandler_.onCancel();
      } else {
        this.resultHandler_.onSelection(result as DriveFile[]);
      }
      this.resultHandler_ = null;
    } catch (e) {
      if (typeof e === 'number') {
        this.reportError_(e as DrivePickerError);
      } else {
        this.reportError_(DrivePickerError.kUnknown);
      }
    }
  }

  /**
   * Reports a flow error and resets the result handler.
   */
  private reportError_(error: DrivePickerError) {
    this.resultHandler_?.onError(error);
    this.resultHandler_ = null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'drive-picker-host-untrusted-app': DrivePickerHostUntrustedAppElement;
  }
}

customElements.define(
    DrivePickerHostUntrustedAppElement.is, DrivePickerHostUntrustedAppElement);
