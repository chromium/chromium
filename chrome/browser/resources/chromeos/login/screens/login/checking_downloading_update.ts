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
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

import {getTemplate} from './checking_downloading_update.html.js';

const CheckingDownloadingUpdateBase =
    OobeDialogHostMixin(OobeI18nMixin(PolymerElement));

export class CheckingDownloadingUpdate extends CheckingDownloadingUpdateBase {
  static get is() {
    return 'checking-downloading-update' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
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
      estimatedTimeLeft: {
        type: Number,
        value: 0,
      },

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
      checkingForUpdatesKey: {
        type: String,
      },

      /**
       * ID of the localized string shown while update is being downloaded.
       */
      downloadingUpdatesKey: {
        type: String,
      },

      /**
       * Message "3 minutes left".
       */
      estimatedTimeLeftMsg: {
        type: String,
        computed: 'computeEstimatedTimeLeftMsg(estimatedTimeLeft)',
      },

      /**
       * Message showing either estimated time left or default update status".
       */
      progressMessage: {
        type: String,
        computed:
            'computeProgressMessage(hasEstimate, defaultProgressMessage, ' +
            'estimatedTimeLeftMsg)',
      },
    };
  }

  private checkingForUpdate: boolean;
  private progressValue: number;
  private estimatedTimeLeft: number;
  private hasEstimate: boolean;
  private defaultProgressMessage: string;
  private updateCompleted: boolean;
  private cancelAllowed: boolean;
  private checkingForUpdatesKey: string;
  private downloadingUpdatesKey: string;
  private estimatedTimeLeftMsg: string;
  private progressMessage: string;

  static get observers(): string[] {
    return ['playAnimation(checkingForUpdate)'];
  }

  private computeProgressMessage(hasEstimate: boolean,
      defaultProgressMessage: string, estimatedTimeLeftMsg: string): string {
    if (hasEstimate) {
      return estimatedTimeLeftMsg;
    }
    return defaultProgressMessage;
  }

  /**
   * Sets estimated time left until download will complete.
   */
  private computeEstimatedTimeLeftMsg(estimatedTimeLeft: number): string {
    const seconds = estimatedTimeLeft;
    const minutes = Math.ceil(seconds / 60);
    let message = '';
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
   * @param checkingForUpdate If the screen is currently checking
   * for updates.
   * @param updateCompleted If update is completed and all
   * intermediate status elements are hidden.
   */
  private isCheckingOrUpdateCompleted(checkingForUpdate: boolean,
      updateCompleted: boolean): boolean {
    return checkingForUpdate || updateCompleted;
  }

  /**
   * @param checkingForUpdate If the screen is currently checking for
   *     updates.
   */
  private playAnimation(checkingForUpdate: boolean) {
    const animation = this.shadowRoot?.querySelector('#checkingAnimation');
    if (animation instanceof OobeCrLottie) {
      animation.playing = checkingForUpdate;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CheckingDownloadingUpdate.is]: CheckingDownloadingUpdate;
  }
}

customElements.define(CheckingDownloadingUpdate.is, CheckingDownloadingUpdate);
