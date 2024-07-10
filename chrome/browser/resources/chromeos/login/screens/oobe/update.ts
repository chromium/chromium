// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../components/oobe_cr_lottie.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_carousel.js';
import '../../components/oobe_slide.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

import {getTemplate} from './update.html.js';


const USER_ACTION_ACCEPT_UPDATE_OVER_CELLUAR = 'update-accept-cellular';
const USER_ACTION_REJECT_UPDATE_OVER_CELLUAR = 'update-reject-cellular';
const USER_ACTION_CANCEL_UPDATE_SHORTCUT = 'cancel-update';
const USER_ACTION_OPT_OUT_INFO_NEXT = 'opt-out-info-next';

const UNREACHABLE_PERCENT = 1000;
// Thresholds which are used to determine when update status announcement should
// take place. Last element is not reachable to simplify implementation.
const PERCENT_THRESHOLDS = [
  0,
  10,
  20,
  30,
  40,
  50,
  60,
  70,
  80,
  90,
  95,
  98,
  99,
  100,
  UNREACHABLE_PERCENT,
];

/**
 * Enum for the UI states corresponding to sub steps inside update screen.
 * These values must be kept in sync with string constants in
 * update_screen_handler.cc.
 */
enum UpdateUiState {
  CHECKING = 'checking',
  CHECKING_SOFTWARE = 'checking-software',
  UPDATE = 'update',
  RESTART = 'restart',
  REBOOT = 'reboot',
  CELLULAR = 'cellular',
  OPT_OUT_INFO = 'opt-out-info',
}

const UpdateBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

interface UpdateScreenData {
  isOptOutEnabled: boolean;
}

export class Update extends UpdateBase {
  static get is() {
    return 'update-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * True if update is fully completed and manual action is required.
       */
      manualRebootNeeded: {
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
       * ID of the localized string for update cancellation message.
       */
      cancelHint: {
        type: String,
        value: 'cancelUpdateHint',
      },

      /**
       * Shows battery warning message during Downloading stage.
       */
      showLowBatteryWarning: {
        type: Boolean,
        value: false,
      },

      /**
       * Message like "3% complete".
       */
      updateStatusMessagePercent: {
        type: String,
        value: '',
      },

      /**
       * Message like "About 5 minutes left".
       */
      updateStatusMessageTimeLeft: {
        type: String,
        value: '',
      },

      /**
       * Progress bar percent that is used in BetterUpdate version of the
       * screen.
       */
      betterUpdateProgressValue: {
        type: Number,
        value: 0,
      },

      /**
       * Whether auto-transition is enabled or not.
       */
      autoTransition: {
        type: Boolean,
        value: true,
      },

      /**
       * Index of threshold that has been already achieved.
       */
      thresholdIndex: {
        type: Number,
        value: 0,
      },

      /**
       * Whether a user can opt out from auto-updates.
       */
      isOptOutEnabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether to show the loading UI different for
       * checking update stage
       */
      isOobeSoftwareUpdateEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isOobeSoftwareUpdateEnabled');
        },
      },
    };
  }

  private manualRebootNeeded: boolean;
  private cancelAllowed: boolean;
  private cancelHint: string;
  private showLowBatteryWarning: boolean;
  private updateStatusMessagePercent: string;
  private updateStatusMessageTimeLeft: string;
  private betterUpdateProgressValue: number;
  private autoTransition: boolean;
  private thresholdIndex: number;
  private isOptOutEnabled: boolean;
  private isOobeSoftwareUpdateEnabled: boolean;

  static get observers(): string[] {
    return ['playAnimation(uiStep)'];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    if (this.isOobeSoftwareUpdateEnabled) {
      return UpdateUiState.CHECKING_SOFTWARE;
    } else {
      return UpdateUiState.CHECKING;
    }
  }

  override get UI_STEPS() {
    return UpdateUiState;
  }

  override get EXTERNAL_API(): string[] {
    return ['setCancelUpdateShortcutEnabled',
            'showLowBatteryWarningMessage',
            'setUpdateState',
            'setUpdateStatus',
            'setAutoTransition',
          ];
  }


  override ready(): void {
    super.ready();
    this.initializeLoginScreen('UpdateScreen');
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   * @param data Screen init payload.
   */
  override onBeforeShow(data: UpdateScreenData): void {
    super.onBeforeShow(data);
    if (data && 'isOptOutEnabled' in data) {
      this.isOptOutEnabled = data['isOptOutEnabled'];
    }
  }

  /**
   * Cancels the screen.
   */
  private cancel(): void {
    this.userActed(USER_ACTION_CANCEL_UPDATE_SHORTCUT);
  }

  private onBackClicked(): void {
    this.userActed(USER_ACTION_REJECT_UPDATE_OVER_CELLUAR);
  }

  private onNextClicked(): void {
    this.userActed(USER_ACTION_ACCEPT_UPDATE_OVER_CELLUAR);
  }

  private onOptOutInfoNext(): void {
    this.userActed(USER_ACTION_OPT_OUT_INFO_NEXT);
  }

  setCancelUpdateShortcutEnabled(enabled: boolean): void {
    this.cancelAllowed = enabled;
  }

  /**
   * Shows or hides battery warning message.
   * @param visible Is message visible?
   */
  showLowBatteryWarningMessage(visible: boolean): void {
    this.showLowBatteryWarning = visible;
  }

  /**
   * Sets which dialog should be shown.
   * @param value Current update state.
   */
  setUpdateState(value: UpdateUiState): void {
    if (value === 'checking' && this.isOobeSoftwareUpdateEnabled) {
      this.setUIStep(UpdateUiState.CHECKING_SOFTWARE);
    } else {
      this.setUIStep(value);
    }
  }

  /**
   * Sets percent to be shown in progress bar.
   * @param percent Current progress
   * @param messagePercent Message describing current progress.
   * @param messageTimeLeft Message describing time left.
   */
  setUpdateStatus(percent: number, messagePercent: string,
      messageTimeLeft: string): void {
    // Sets aria-live polite on percent and timeleft container every time new
    // threshold has been achieved otherwise do not initiate spoken feedback
    // update by setting aria-live off.
    const betterUpdatePercent = this.shadowRoot?.
        querySelector('#betterUpdatePercent');
    const betterUpdateTimeleft = this.shadowRoot?.
        querySelector('#betterUpdateTimeleft');
    if (percent >= PERCENT_THRESHOLDS[this.thresholdIndex]) {
      while (percent >= PERCENT_THRESHOLDS[this.thresholdIndex]) {
        this.thresholdIndex = this.thresholdIndex + 1;
      }
      if (betterUpdatePercent instanceof HTMLElement
          && betterUpdateTimeleft instanceof HTMLElement){
        betterUpdatePercent.setAttribute('aria-live', 'polite');
        betterUpdateTimeleft.setAttribute('aria-live', 'polite');
      }
    } else {
      if (betterUpdatePercent instanceof HTMLElement
          && betterUpdateTimeleft instanceof HTMLElement){
        betterUpdatePercent.setAttribute('aria-live', 'off');
        betterUpdateTimeleft.setAttribute('aria-live', 'off');
      }
    }
    this.betterUpdateProgressValue = percent;
    this.updateStatusMessagePercent = messagePercent;
    this.updateStatusMessageTimeLeft = messageTimeLeft;
  }

  /**
   * Sets whether carousel should auto transit slides.
   */
  setAutoTransition(value: boolean): void {
    this.autoTransition = value;
  }

  /**
   * Gets whether carousel should auto transit slides.
   * @param step Which UIState is shown now.
   * @param autoTransition Is auto transition allowed.
   */
  private getAutoTransition(step: UpdateUiState,
      autoTransition: boolean): boolean {
    return step === UpdateUiState.UPDATE && autoTransition;
  }

  /**
   * Computes the title of the first slide in carousel during update.
   */
  private getUpdateSlideTitle(locale: string,
      isOptOutEnabled: boolean): string {
    return this.i18nDynamic(locale,
        isOptOutEnabled ? 'slideUpdateAdditionalSettingsTitle' :
                          'slideUpdateTitle');
  }

  /**
   * Computes the text of the first slide in carousel during update.
   */
  private getUpdateSlideText(locale: string, isOptOutEnabled: boolean): string {
    return this.i18nDynamic(locale,
        isOptOutEnabled ? 'slideUpdateAdditionalSettingsText' :
                          'slideUpdateText');
  }

  /**
   * @param uiStep which UiState is shown now.
   */
  private playAnimation(uiStep: UpdateUiState): void {
    const animation = this.shadowRoot?.querySelector('#checkingAnimation');
    if (animation instanceof OobeCrLottie) {
      animation.playing = (uiStep === UpdateUiState.CHECKING);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [Update.is]: Update;
  }
}

customElements.define(Update.is, Update);
