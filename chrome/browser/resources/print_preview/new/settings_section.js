// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-settings-section',

  properties: {
    /** @type {boolean} */
    managed: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @type {boolean} */
    showPolicyOnEnd: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /**
   * @return {boolean} Whether to show the policy icon before the controls.
   * @private
   */
  showStartIcon_: function() {
    return this.managed && !this.showPolicyOnEnd;
  },

  /**
   * @return {boolean} Whether to show the policy icon after the controls.
   * @private
   */
  showEndIcon_: function() {
    return this.managed && this.showPolicyOnEnd;
  },
});
