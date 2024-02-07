// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './http_message_object.html.js';
import type {HttpMessage} from './types.js';
import {Direction, Rpc} from './types.js';

class HttpMessageObjectElement extends PolymerElement {
  static get is() {
    return 'http-message-object';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Underlying HTTPMessage data for this item. Contains read-only fields
       * from the NearbyShare backend, as well as fields computed by http tab.
       * Type: {!HttpMessage}
       */
      item: {
        type: Object,
        observer: 'itemChanged_',
      },

    };
  }

  item: HttpMessage;

  /**
   * Sets the Http message style based on whether it is a response or request.
   */
  private itemChanged_(): void {
    let classStyle = '';
    if (this.item.direction === Direction.REQUEST) {
      classStyle = 'request';
    } else if (this.item.direction === Direction.RESPONSE) {
      classStyle = 'response';
    }
    this.shadowRoot!.querySelector('item')!.className = classStyle;
  }

  /**
   * Sets the string representation of RPC type.
   */
  private rpcToString_(rpc: number): string|undefined {
    switch (rpc) {
      case Rpc.CERTIFICATE:
        return 'ListPublicCertificates RPC';
      case Rpc.CONTACT:
        return 'ListContactPeople RPC';
      case Rpc.DEVICE:
        return 'UpdateDevice RPC';
      case Rpc.DEVICE_STATE:
        return 'GetDeviceState RPC';
      default:
        return;
    }
  }

  /**
   * Returns the string representation of RPC type.
   */
  private directionToString_(direction: number): string|undefined {
    switch (direction) {
      case Direction.REQUEST:
        return 'Request';
      case Direction.RESPONSE:
        return 'Response';
      default:
        return;
    }
  }

  /**
   * Sets the string representation of time.
   */
  formatTime(time: number): string {
    const d = new Date(time);
    return d.toLocaleTimeString();
  }
}

customElements.define(HttpMessageObjectElement.is, HttpMessageObjectElement);
