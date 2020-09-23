// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-page-template is used as a template for pages. It
 * provide a consistent setup for all pages with title, sub-title, body slot
 * and button options.
 */
Polymer({
  is: 'nearby-page-template',

  properties: {
    /** @type {?string} */
    title: {
      type: String,
    },

    /** @type {?string} */
    subTitle: {
      type: String,
    },

    /** @type {?string} */
    actionButtonLabel: {
      type: String,
    },

    /** @type {string} */
    actionButtonEventName: {
      type: String,
      value: 'action'
    },

    actionDisabled: {
      type: Boolean,
      value: false,
    },

    /** @type {?string} */
    cancelButtonLabel: {
      type: String,
    },

    /** @type {string} */
    cancelButtonEventName: {
      type: String,
      value: 'cancel',
    },

    /** @type {?string} */
    utilityButtonLabel: {
      type: String,
    },

    /** @type {string} */
    utilityButtonEventName: {
      type: String,
      value: 'utility',
    }
  },

  /** @private */
  onActionClick_() {
    this.fire(this.actionButtonEventName);
  },

  /** @private */
  onCancelClick_() {
    this.fire(this.cancelButtonEventName);
  },

  /** @private */
  onUtilityClick_() {
    this.fire(this.utilityButtonEventName);
  },
});
