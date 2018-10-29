// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe update screen implementation.
 */

login.createScreen('UpdateScreen', 'update', function() {
  var USER_ACTION_CANCEL_UPDATE_SHORTCUT = 'cancel-update';
  var CONTEXT_KEY_TIME_LEFT_SEC = 'time-left-sec';
  var CONTEXT_KEY_SHOW_TIME_LEFT = 'show-time-left';
  var CONTEXT_KEY_UPDATE_COMPLETED = 'update-completed';
  var CONTEXT_KEY_SHOW_CURTAIN = 'show-curtain';
  var CONTEXT_KEY_SHOW_PROGRESS_MESSAGE = 'show-progress-msg';
  var CONTEXT_KEY_PROGRESS = 'progress';
  var CONTEXT_KEY_PROGRESS_MESSAGE = 'progress-msg';
  var CONTEXT_KEY_CANCEL_UPDATE_SHORTCUT_ENABLED = 'cancel-update-enabled';
  var CONTEXT_KEY_REQUIRES_PERMISSION_FOR_CELLULAR =
      'requires-permission-for-cellular';

  return {
    EXTERNAL_API: [],

    /** @override */
    decorate: function() {
      var self = this;

      this.context.addObserver(
          CONTEXT_KEY_TIME_LEFT_SEC, function(time_left_sec) {
            self.setEstimatedTimeLeft(time_left_sec);
          });
      this.context.addObserver(
          CONTEXT_KEY_SHOW_TIME_LEFT, function(show_time_left) {
            self.showEstimatedTimeLeft(show_time_left);
          });
      this.context.addObserver(
          CONTEXT_KEY_UPDATE_COMPLETED, function(is_completed) {
            self.setUpdateCompleted(is_completed);
          });
      this.context.addObserver(
          CONTEXT_KEY_SHOW_CURTAIN, function(show_curtain) {
            self.showUpdateCurtain(show_curtain);
          });
      this.context.addObserver(
          CONTEXT_KEY_SHOW_PROGRESS_MESSAGE, function(show_progress_msg) {
            self.showProgressMessage(show_progress_msg);
          });
      this.context.addObserver(CONTEXT_KEY_PROGRESS, function(progress) {
        self.setUpdateProgress(progress);
      });
      this.context.addObserver(
          CONTEXT_KEY_PROGRESS_MESSAGE, function(progress_msg) {
            self.setProgressMessage(progress_msg);
          });
      this.context.addObserver(
          CONTEXT_KEY_REQUIRES_PERMISSION_FOR_CELLULAR,
          function(requires_permission) {
            self.setRequiresPermissionForCellular(requires_permission);
          });
      this.context.addObserver(
          CONTEXT_KEY_CANCEL_UPDATE_SHORTCUT_ENABLED, function(enabled) {
            $('oobe-update-md').cancelAllowed = enabled;
            var configuration = Oobe.getInstance().getOobeConfiguration();
            if (!configuration)
              return;
            if (configuration.updateSkipNonCritical && enabled) {
              self.cancel();
            }
          });
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('updateScreenTitle');
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
     * Shows or hides info message below progress bar.
     * @param {boolean} visible Are message visible?
     */
    showProgressMessage: function(visible) {
      $('oobe-update-md').estimatedTimeLeftShown = !visible;
      $('oobe-update-md').progressMessageShown = visible;
    },

    /**
     * Sets message below progress bar.
     * @param {string} message Message that should be shown.
     */
    setProgressMessage: function(message) {
      $('oobe-update-md').progressMessage = message;
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
