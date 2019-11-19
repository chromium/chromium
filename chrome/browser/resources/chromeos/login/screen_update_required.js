// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview "Update is required to sign in" screen.
 */

login.createScreen('UpdateRequiredScreen', 'update-required', function() {
  /* Possible UI states of the error screen. */
  /** @const */ var UI_STATE = {
    UPDATE_REQUIRED_MESSAGE: 'update-required-message',
    UPDATE_PROCESS: 'update-process',
    UPDATE_NEED_PERMISSION: 'update-need-permission',
    UPDATE_COMPLETED_NEED_REBOOT: 'update-completed-need-reboot',
    UPDATE_ERROR: 'update-error',
    EOL: 'eol'
  };

  // Array of the possible UI states of the screen. Must be in the
  // same order as UpdateRequiredView::UIState enum values.
  /** @const */ var UI_STATES = [
    UI_STATE.UPDATE_REQUIRED_MESSAGE, UI_STATE.UPDATE_PROCESS,
    UI_STATE.UPDATE_NEED_PERMISSION, UI_STATE.UPDATE_COMPLETED_NEED_REBOOT,
    UI_STATE.UPDATE_ERROR, UI_STATE.EOL
  ];

  return {
    EXTERNAL_API: [
      'setIsConnected', 'setUpdateProgressUnavailable',
      'setUpdateProgressValue', 'setUpdateProgressMessage',
      'setEstimatedTimeLeftVisible', 'setEstimatedTimeLeft', 'setUIState'
    ],

    /** @param {boolean} connected */
    setIsConnected: function(connected) {
      $('update-required-card').isConnected = connected;
    },

    /**
     * @param {boolean} unavailable.
     */
    setUpdateProgressUnavailable: function(unavailable) {
      $('update-required-card').updateProgressUnavailable = unavailable;
    },

    /**
     * Sets update's progress bar value.
     * @param {number} progress Percentage of the progress bar.
     */
    setUpdateProgressValue: function(progress) {
      $('update-required-card').updateProgressValue = progress;
    },

    /**
     * Sets message below progress bar.
     * @param {string} message Message that should be shown.
     */
    setUpdateProgressMessage: function(message) {
      $('update-required-card').updateProgressMessage = message;
    },

    /**
     * Shows or hides downloading ETA message.
     * @param {boolean} visible Are ETA message visible?
     */
    setEstimatedTimeLeftVisible: function(visible) {
      $('update-required-card').estimatedTimeLeftVisible = visible;
    },

    /**
     * Sets estimated time left until download will complete.
     * @param {number} seconds Time left in seconds.
     */
    setEstimatedTimeLeft: function(seconds) {
      var minutes = Math.ceil(seconds / 60);
      var message = '';
      if (minutes > 60) {
        message = loadTimeData.getString('downloadingTimeLeftLong');
      } else if (minutes > 55) {
        message = loadTimeData.getString('downloadingTimeLeftStatusOneHour');
      } else if (minutes > 20) {
        message = loadTimeData.getStringF(
            'downloadingTimeLeftStatusMinutes', Math.ceil(minutes / 5) * 5);
      } else if (minutes > 1) {
        message = loadTimeData.getStringF(
            'downloadingTimeLeftStatusMinutes', minutes);
      } else {
        message = loadTimeData.getString('downloadingTimeLeftSmall');
      }
      $('update-required-card').estimatedTimeLeft =
          loadTimeData.getStringF('downloading', message);
    },

    /**
     * Sets current UI state of the screen.
     * @param {number} ui_state New UI state of the screen.
     */
    setUIState: function(ui_state) {
      $('update-required-card').ui_state = UI_STATES[ui_state];
    },
  };
});
