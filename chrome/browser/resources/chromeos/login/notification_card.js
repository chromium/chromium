// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
let NotificationCardType = {
  FAIL: 'fail',
  SUCCESS: 'success',
};

Polymer({
  is: 'notification-card',

  properties: {
    buttonLabel: {type: String, value: ''},

    linkLabel: {type: String, value: ''},

    type: {type: String, value: ''}
  },

  /**
   * @param {NotificationCardType} type
   * @private
   */
  iconNameByType_: function(type) {
    if (type == NotificationCardType.FAIL)
      return 'cr:warning';
    if (type == NotificationCardType.SUCCESS)
      return 'notification-card:done';
    console.error('Unknown type "' + type + '".');
    return '';
  },

  /** @private */
  buttonClicked_: function() {
    this.fire('buttonclick');
  },

  /**
   * @param {Event} e
   * @private
   */
  linkClicked_: function(e) {
    this.fire('linkclick');
    e.preventDefault();
  },

  /** @type {Element} */
  get submitButton() {
    return this.$.submitButton;
  }
});
