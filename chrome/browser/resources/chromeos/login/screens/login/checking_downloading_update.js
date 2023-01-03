// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 *
 * UI for checking and downloading updates as part of the update process.
 * 'indeterminate' paper-progress will recalculate styles on every frame
 * when OOBE is loaded (even when another screen is open).
 * So we make it 'indeterminate' only when the checking for updates dialog is
 * shown, and make set to false when dialog is hidden.
 *
 * Example:
 *    <checking-downloading-update> </checking-downloading-update>
 *
 * Attributes:
 *  'checkingForUpdate' - Whether the screen is currently checking for updates.
 *                        Shows the checking for updates dialog and hides the
 *                        downloading dialog.
 *  'progressValue' - Progress bar percent value.
 *  'estimatedTimeLeft' - Time left in seconds for the update to complete
 *                        download.
 *  'hasEstimate' - True if estimated time left is to be shown.
 *  'defaultProgressMessage' - Message showing either estimated time left or
 *                             default update status.
 *  'updateCompleted' - True if update is completed and probably manual action
 *                      is required.
 *  'cancelAllowed' - True if update cancellation is allowed.
 *  'checkingForUpdatesKey' - ID of localized string shown while checking for
 *                            updates.
 *  'downloadingUpdatesKey' - ID of localized string shown while update is being
 *                           downloaded.
 *  'cancelHintKey' - ID of the localized string for update cancellation
 *                    message.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const CheckingDownloadingUpdateBase =
    mixinBehaviors([OobeI18nBehavior, OobeDialogHostBehavior], PolymerElement);

/**
 * @polymer
 */
export class CheckingDownloadingUpdate extends CheckingDownloadingUpdateBase {
  static get is() {
    return 'checking-downloading-update';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Shows "Checking for update ..." section and hides "Updating..."
       * section.
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
    };
  }

  static get observers() {
    return ['playAnimation_(checkingForUpdate)'];
  }

  computeProgressMessage_(
      hasEstimate, defaultProgressMessage, estimatedTimeLeftMsg_) {
    if (hasEstimate) {
      return estimatedTimeLeftMsg_;
    }
    return defaultProgressMessage;
  }

  /**
   * Sets estimated time left until download will complete.
   */
  computeEstimatedTimeLeftMsg_(estimatedTimeLeft) {
    const seconds = estimatedTimeLeft;
    const minutes = Math.ceil(seconds / 60);
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
  }

  /**
   * Calculates visibility of the updating dialog.
   * @param {Boolean} checkingForUpdate If the screen is currently checking
   * for updates.
   * @param {Boolean} updateCompleted If update is completed and all
   * intermediate status elements are hidden.
   */
  isCheckingOrUpdateCompleted_(checkingForUpdate, updateCompleted) {
    return checkingForUpdate || updateCompleted;
  }

  /**
   * @private
   * @param {Boolean} checkingForUpdate If the screen is currently checking for
   *     updates.
   */
  playAnimation_(checkingForUpdate) {
    this.$.checkingAnimation.playing = checkingForUpdate;
  }
}

customElements.define(CheckingDownloadingUpdate.is, CheckingDownloadingUpdate);
