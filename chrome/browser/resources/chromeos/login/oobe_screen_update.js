// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe update screen implementation.
 */

login.createScreen('UpdateScreen', 'update', function() {
  var USER_ACTION_CANCEL_UPDATE_SHORTCUT = 'cancel-update';

  return {
    EXTERNAL_API: [
      'setEstimatedTimeLeft',
      'showEstimatedTimeLeft',
      'setUpdateCompleted',
      'showUpdateCurtain',
      'setProgressMessage',
      'setUpdateProgress',
      'setRequiresPermissionForCellular',
      'setCancelUpdateShortcutEnabled',
    ],

    /** @param {boolean} enabled */
    setCancelUpdateShortcutEnabled: function(enabled) {
      $('oobe-update-md').cancelAllowed = enabled;
    },

    /**
     * Returns default event target element.
     * @type {Object}
     */
    get defaultControl() {
      return $('oobe-update-md');
    },

    /**
     * Cancels the screen.
     */
    cancel: function() {
      $('oobe-update-md')
          .setCancelHint(loadTimeData.getString('cancelledUpdateMessage'));
      this.send(
          login.Screen.CALLBACK_USER_ACTED, USER_ACTION_CANCEL_UPDATE_SHORTCUT);
    },

    /**
     * Sets update's progress bar value.
     * @param {number} progress Percentage of the progress bar.
     */
    setUpdateProgress: function(progress) {
      $('oobe-update-md').progressValue = progress;
    },

    setRequiresPermissionForCellular: function(requiresPermission) {
      $('oobe-update-md').requiresPermissionForCellular = requiresPermission;
    },

    /**
     * Shows or hides downloading ETA message.
     * @param {boolean} visible Are ETA message visible?
     */
    showEstimatedTimeLeft: function(visible) {
      $('oobe-update-md').estimatedTimeLeftShown = visible;
      $('oobe-update-md').progressMessageShown = !visible;
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
      $('oobe-update-md').estimatedTimeLeft =
          loadTimeData.getStringF('downloading', message);
    },

    /**
     * Sets message below progress bar. Hide the message by setting an empty
     * string.
     * @param {string} message Message that should be shown.
     */
    setProgressMessage: function(message) {
      var visible = !!message;
      $('oobe-update-md').progressMessage = message;
      $('oobe-update-md').estimatedTimeLeftShown = !visible;
      $('oobe-update-md').progressMessageShown = visible;
    },

    /**
     * Marks update completed. Shows "update completed" message.
     * @param {boolean} is_completed True if update process is completed.
     */
    setUpdateCompleted: function(is_completed) {
      $('oobe-update-md').updateCompleted = is_completed;
    },

    /**
     * Shows or hides update curtain.
     * @param {boolean} visible Are curtains visible?
     */
    showUpdateCurtain: function(visible) {
      $('oobe-update-md').checkingForUpdate = visible;
    },
  };
});
