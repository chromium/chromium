// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {ProtoWrapper} from '//resources/mojo/mojo/public/mojom/base/proto_wrapper.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {DrivePickerApiProxyImpl} from './drive_picker_api_proxy.js';
import {DrivePickerError} from './drive_picker_result_handler.mojom-webui.js';
import type {DriveFile, DrivePickerResultHandlerRemote} from './drive_picker_result_handler.mojom-webui.js';

const CONSENT_KIT_ORIGIN = 'https://consent.google.com';
const HANDSHAKE_MESSAGE = 'cpw';
const IFRAME_MESSAGE_KEY = 'base64webEncodedIframeMessage';
const PRIVACY_FLOW_RESULT_KEY = 'base64webEncodedPrivacyFlowResult';
const HANDSHAKE_TIMEOUT_MS = 5000;

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
  private handshakeTimeoutId_: number|null = null;
  private port_: MessagePort|null = null;

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds_ = [
      this.browserProxy_.callbackRouter.showDrivePicker.addListener(
          (handler: DrivePickerResultHandlerRemote,
           keys: {oauthToken: string, apiKey: string, appId: string}) =>
              this.showDrivePicker_(handler, keys)),
      this.browserProxy_.callbackRouter.loadConsentKitUrl.addListener(
          (url: string) => this.loadConsentKitUrl_(url)),
    ];
    window.addEventListener('message', this.handleConsentKitMessage_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => assert(this.browserProxy_.callbackRouter.removeListener(id)));
    this.listenerIds_ = [];
    window.removeEventListener('message', this.handleConsentKitMessage_);
    if (this.port_) {
      this.port_.close();
      this.port_ = null;
    }
    this.clearHandshakeTimeout_();
  }

  private async showDrivePicker_(
      resultHandler: DrivePickerResultHandlerRemote,
      keys: {oauthToken: string, apiKey: string, appId: string}) {
    this.resultHandler_ = resultHandler;

    const container = this.shadowRoot.querySelector('#picker-container');
    if (container) {
      container.replaceChildren();
    }

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

  private loadConsentKitUrl_(consentKitUrl: string) {
    const iframe = document.createElement('iframe');
    iframe.src = consentKitUrl;
    iframe.setAttribute('scrolling', 'no');

    const container = this.shadowRoot.querySelector('#picker-container');
    if (container) {
      container.replaceChildren(iframe);
    }

    this.clearHandshakeTimeout_();
    this.handshakeTimeoutId_ = setTimeout(() => {
      console.error(`ConsentKit: Handshake timeout fired after ${
          HANDSHAKE_TIMEOUT_MS}ms`);
      this.browserProxy_.handler.onConsentKitError('Handshake timeout');
      this.clearHandshakeTimeout_();
    }, HANDSHAKE_TIMEOUT_MS);
  }

  private clearHandshakeTimeout_() {
    if (this.handshakeTimeoutId_ !== null) {
      clearTimeout(this.handshakeTimeoutId_);
      this.handshakeTimeoutId_ = null;
    }
  }

  private handleConsentKitMessage_ = (event: MessageEvent) => {
    if (event.origin !== CONSENT_KIT_ORIGIN) {
      return;
    }

    const data = event.data;
    if (!data) {
      return;
    }

    if (data === HANDSHAKE_MESSAGE) {
      this.clearHandshakeTimeout_();
      if (event.ports && event.ports.length > 0) {
        if (this.port_) {
          this.port_.close();
        }
        const port = event.ports[0];
        if (port) {
          this.port_ = port;
          this.port_.onmessage = (portEvent: MessageEvent) => {
            this.handleConsentKitPortMessage_(portEvent);
          };
        }
      }
      return;
    }

    // When enable_post_message_communication = true, ConsentKit sends the
    // IframeMessage protobuf directly as a base64url string in event.data!
    if (typeof data === 'string') {
      this.clearHandshakeTimeout_();
      this.decodeAndSendBase64Proto_(data, /*isPrivacyFlowResult=*/ false);
      return;
    }

    if (typeof data !== 'object') {
      return;
    }

    this.clearHandshakeTimeout_();

    if (IFRAME_MESSAGE_KEY in data) {
      const base64Message = data[IFRAME_MESSAGE_KEY];
      if (typeof base64Message === 'string') {
        this.decodeAndSendBase64Proto_(
            base64Message, /*isPrivacyFlowResult=*/ false);
      }
    } else if (PRIVACY_FLOW_RESULT_KEY in data) {
      const base64Result = data[PRIVACY_FLOW_RESULT_KEY];
      if (typeof base64Result === 'string') {
        this.decodeAndSendBase64Proto_(
            base64Result, /*isPrivacyFlowResult=*/ true);
      }
    }
  };

  private decodeAndSendBase64Proto_(
      base64String: string, isPrivacyFlowResult: boolean) {
    try {
      let standardBase64 = base64String.replace(/-/g, '+').replace(/_/g, '/');
      while (standardBase64.length % 4) {
        standardBase64 += '=';
      }
      const binaryString = atob(standardBase64);
      const bytes = new Uint8Array(binaryString.length);
      for (let i = 0; i < binaryString.length; i++) {
        bytes[i] = binaryString.charCodeAt(i);
      }
      const wrapper: ProtoWrapper = {
        protoName: isPrivacyFlowResult ? 'identity_consent.PrivacyFlowResult' :
                                         'identity_consent.IframeMessage',
        smuggled: {
          bytes: Array.from(bytes),
        },
      };
      if (isPrivacyFlowResult) {
        this.browserProxy_.handler.onConsentKitPrivacyFlowResult(wrapper);
      } else {
        this.browserProxy_.handler.onConsentKitIframeMessage(wrapper);
      }
    } catch (e) {
      console.error(
          'ConsentKit failed to decode base64 proto:', base64String, e);
      this.browserProxy_.handler.onConsentKitError('Proto decoding failed');
    }
  }

  private handleConsentKitPortMessage_(event: MessageEvent) {
    const data = event.data;
    if (data instanceof Uint8Array) {
      const wrapper: ProtoWrapper = {
        protoName: 'identity_consent.IframeMessage',
        smuggled: {
          bytes: Array.from(data),
        },
      };
      this.browserProxy_.handler.onConsentKitIframeMessage(wrapper);
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
