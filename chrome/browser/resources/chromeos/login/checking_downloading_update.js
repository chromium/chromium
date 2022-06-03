// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

Polymer({
  is: 'checking-downloading-update',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Shows "Checking for update ..." section and hides "Updating..." section.
     */
    checkingForUpdate: {
      type: Boolean,
      value: true,
    },

    /**
     * Progress bar percent.
     */
    progressValue: {
      type: Number,
      value: 0,
    },

    /**
     * Estimated time left in seconds.
     */
    estimatedTimeLeft: {type: Number, value: 0},

    /**
     * Shows estimatedTimeLeft.
     */
    hasEstimate: {
      type: Boolean,
      value: false,
    },

    /**
     * Message "33 percent done".
     */
    defaultProgressMessage: {
      type: String,
    },

    /**
     * True if update is fully completed and, probably manual action is
     * required.
     */
    updateCompleted: {
      type: Boolean,
      value: false,
    },

    /**
     * If update cancellation is allowed.
     */
    cancelAllowed: {
      type: Boolean,
      value: false,
    },

    /**
     * ID of the localized string shown while checking for updates.
     */
    checkingForUpdatesKey: String,

    /**
     * ID of the localized string shown while update is being downloaded.
     */
    downloadingUpdatesKey: String,

    /**
     * ID of the localized string for update cancellation message.
     */
    cancelHintKey: String,

    /**
     * Message "3 minutes left".
     */
    estimatedTimeLeftMsg_: {
      type: String,
      computed: 'computeEstimatedTimeLeftMsg_(estimatedTimeLeft)',
    },

    /**
     * Message showing either estimated time left or default update status".
     */
    progressMessage_: {
      type: String,
      computed:
          'computeProgressMessage_(hasEstimate, defaultProgressMessage, ' +
          'estimatedTimeLeftMsg_)',
    },
  },

  computeProgressMessage_(
      hasEstimate, defaultProgressMessage, estimatedTimeLeftMsg_) {
    if (hasEstimate)
      return estimatedTimeLeftMsg_;
    return defaultProgressMessage;
  },

  /**
   * Sets estimated time left until download will complete.
   */
  computeEstimatedTimeLeftMsg_(estimatedTimeLeft) {
    let seconds = estimatedTimeLeft;
    let minutes = Math.ceil(seconds / 60);
    var message = '';
    if (minutes > 60) {
      message = loadTimeData.getString('downloadingTimeLeftLong');
    } else if (minutes > 55) {
      message = loadTimeData.getString('downloadingTimeLeftStatusOneHour');
    } else if (minutes > 20) {
      message = loadTimeData.getStringF(
          'downloadingTimeLeftStatusMinutes', Math.ceil(minutes / 5) * 5);
    } else if (minutes > 1) {
      message =
          loadTimeData.getStringF('downloadingTimeLeftStatusMinutes', minutes);
    } else {
      message = loadTimeData.getString('downloadingTimeLeftSmall');
    }
    return loadTimeData.getStringF('downloading', message);
  },

  /**
   * Calculates visibility of the updating dialog.
   * @param {Boolean} checkingForUpdate If the screen is currently checking
   * for updates.
   * @param {Boolean} updateCompleted If update is completed and all
   * intermediate status elements are hidden.
   */
  isCheckingOrUpdateCompleted_(checkingForUpdate, updateCompleted) {
    return checkingForUpdate || updateCompleted;
  },
});
