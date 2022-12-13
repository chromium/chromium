// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for dealing with Cellular setup subflows.
 * It includes some methods and property shared between subflows.
 */

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';

import {ButtonBarState} from './cellular_types.js';

/** @polymerBehavior */
export const SubflowBehavior = {
  properties: {

    /**
     * Button bar button state.
     * @type {!ButtonBarState}
     */
    buttonState: {
      type: Object,
      notify: true,
    },
  },

  /**
   * Initialize the subflow.
   */
  initSubflow() {
    assertNotReached();
  },

  /**
   * Handles forward navigation within subpage.
   */
  navigateForward() {
    assertNotReached();
  },

  /**
   * Handles backward navigation within subpage.
   */
  navigateBackward() {
    assertNotReached();
  },
};
