// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-pointers' is the settings subpage with mouse and touchpad settings.
 */
Polymer({
  is: 'settings-pointers',

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    hasMouse: Boolean,

    hasTouchpad: Boolean,

    /**
     * TODO(michaelpg): settings-slider should optionally take a min and max so
     * we don't have to generate a simple range of natural numbers ourselves.
     * @type {!Array<number>}
     * @private
     */
    sensitivityValues_: {
      type: Array,
      value: [1, 2, 3, 4, 5],
      readOnly: true,
    },

    /**
     * TODO(zentaro): Remove this conditional once the feature is launched.
     * @private
     */
    allowDisableAcceleration_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('allowDisableMouseAcceleration');
      },
    },
  },

  // Used to correctly identify when the mouse button has been released.
  // crbug.com/686949.
  receivedMouseSwapButtonsDown_: false,

  /**
   * Mouse and touchpad sections are only subsections if they are both present.
   * @param {boolean} hasMouse
   * @param {boolean} hasTouchpad
   * @return {string}
   * @private
   */
  getSubsectionClass_: function(hasMouse, hasTouchpad) {
    return hasMouse && hasTouchpad ? 'subsection' : '';
  },

  /** @private */
  onMouseSwapButtonsDown_: function() {
    this.receivedMouseSwapButtonsDown_ = true;
  },

  /** @private */
  onMouseSwapButtonsUp_: function() {
    this.receivedMouseSwapButtonsDown_ = false;
    /** @type {!SettingsToggleButtonElement} */ (this.$.mouseSwapButton)
        .sendPrefChange();
  },

  /** @private */
  onMouseSwapButtonsChange_: function(event) {
    if (!this.receivedMouseSwapButtonsDown_) {
      /** @type {!SettingsToggleButtonElement} */ (this.$.mouseSwapButton)
          .sendPrefChange();
    }
  },
});
