// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TimestampedMessage} from './types.js';
import {getTemplate} from './ui_trigger_list_object.html.js';

Polymer({
  is: 'ui-trigger-object',

  _template: getTemplate(),

  properties: {
    /**
     * Underlying StatusCode data for this item. Contains read-only fields
     * from the NearbyShare back-end.
     * Type: {!TimestampedMessage}
     */
    timestampedMessage: {
      type: Object,
    },
  },

  /**
   * Sets the string representation of time.
   * @private
   * @param {number} time
   * @return {string}
   */
  formatTime_(time) {
    const d = new Date(time);
    return d.toLocaleTimeString();
  },
});
