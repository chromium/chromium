// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-toggle-row',

  properties: {
    /**
     * @type {string}
     */
    icon: String,
    /**
     * @type {string}
     */
    label: String,
    /**
     * @type {boolean}
     */
    managed: {type: Boolean, value: false, reflectToAttribute: true},
    /**
     * @type {boolean}
     */
    value: {type: Boolean, value: false, reflectToAttribute: true},
    /**
     * @type {string}
     */
    description: String,
  },

  listeners: {
    click: 'onClick_',
  },

  /**
   * @returns {boolean} true if the toggle is checked.
   */
  isChecked() {
    return this.$.toggle.checked;
  },

  /**
   * @param {boolean} value What to set the toggle to.
   */
  setToggle(value) {
    this.$.toggle.checked = value;
  },

  /**
   * @param {MouseEvent} event
   * @private
   */
  onClick_(event) {
    event.stopPropagation();
    this.$['toggle'].click();
  },
});
