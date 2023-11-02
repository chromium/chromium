// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Direction, Rpc} from './types.js';

Polymer({
  is: 'http-message-object',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Underlying HTTPMessage data for this item. Contains read-only fields
     * from the NearbyShare backend, as well as fields computed by http tab.
     * Type: {!HttpMessage}
     */
    item: {
      type: Object,
      observer: 'itemChanged_',
    },
  },

  /**
   * Sets the Http message style based on whether it is a response or request.
   * @private
   */
  itemChanged_() {
    let classStyle = '';
    if (this.item.direction === Direction.REQUEST) {
      classStyle = 'request';
    } else if (this.item.direction === Direction.RESPONSE) {
      classStyle = 'response';
    }
    this.$['item'].className = classStyle;
  },

  /**
   * Sets the string representation of RPC type.
   * @private
   * @param {number} rpc
   * @return
   */
  rpcToString_(rpc) {
    switch (rpc) {
      case Rpc.CERTIFICATE:
        return 'ListPublicCertificates RPC';
        break;
      case Rpc.CONTACT:
        return 'ListContactPeople RPC';
        break;
      case Rpc.DEVICE:
        return 'UpdateDevice RPC';
        break;
      case Rpc.DEVICE_STATE:
        return 'GetDeviceState RPC';
        break;
      default:
        break;
    }
  },

  /**
   * Returns the string representation of RPC type.
   * @private
   * @param {number} direction
   * @return
   */
  directionToString_(direction) {
    switch (direction) {
      case Direction.REQUEST:
        return 'Request';
        break;
      case Direction.RESPONSE:
        return 'Response';
        break;
      default:
        break;
    }
  },

  /**
   * Sets the string representation of time.
   * @private
   * @param {number} time
   * @return
   */
  formatTime_(time) {
    const d = new Date(time);
    return d.toLocaleTimeString();
  },
});
