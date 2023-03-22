// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './contact_object.html.js';
import {ContactUpdate} from './types.js';

Polymer({
  is: 'contact-object',

  _template: getTemplate(),

  properties: {
    /**
     * Underlying ContactUpdate data for this item. Contains read-only fields
     * from the NearbyShare back-end.
     * Type: {!ContactUpdate}
     */
    contactUpdate: {
      type: Object,
    },
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
