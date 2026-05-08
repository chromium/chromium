// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {PageCallbackRouter} from './drive_picker_host_untrusted.mojom-webui.js';
import type {DrivePickerUntrustedHostHandlerRemote} from './drive_picker_host_untrusted.mojom-webui.js';
import type {DrivePickerResultHandlerRemote} from './drive_picker_result_handler.mojom-webui.js';

export class DrivePickerHostUntrustedAppElement extends CrLitElement {
  static get is() {
    return 'drive-picker-host-untrusted-app';
  }

  static override get styles() {
    return getCss();
  }

  private callbackRouter_: PageCallbackRouter = new PageCallbackRouter();
  private handler_: DrivePickerUntrustedHostHandlerRemote =
      BrowserProxyImpl.getInstance().handler;

  override render() {
    return getHtml.bind(this)();
  }

  override firstUpdated() {
    this.handler_.bindPage(this.callbackRouter_.$.bindNewPipeAndPassRemote());

    this.callbackRouter_.showDrivePicker.addListener(
        (resultHandler: DrivePickerResultHandlerRemote) => {
          this.showDrivePicker(resultHandler);
        });
  }

  showDrivePicker(_resultHandler: DrivePickerResultHandlerRemote) {
    // TODO: crbug.com/497937568 - Implement the Drive Picker UI and relay
    // results to the resultHandler.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'drive-picker-host-untrusted-app': DrivePickerHostUntrustedAppElement;
  }
}

customElements.define(
    DrivePickerHostUntrustedAppElement.is, DrivePickerHostUntrustedAppElement);
