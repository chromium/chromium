// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_carousel.js';
import '../../components/oobe_slide.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeCrLottie} from '../../components/oobe_cr_lottie.js';
import {ConsumerUpdatePage_ConsumerUpdateStep, ConsumerUpdatePageCallbackRouter, ConsumerUpdatePageHandlerRemote} from '../../mojom-webui/screens_oobe.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './consumer_update.html.js';

const ConsumerUpdateScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

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
 * Enum to represent steps on the consumer update screen.
 */
enum ConsumerUpdateStep {
  CHECKING = 'checking',
  UPDATE = 'update',
  RESTART = 'restart',
  REBOOT = 'reboot',
  CELLULAR = 'cellular',
}

class ConsumerUpdateScreen extends ConsumerUpdateScreenElementBase {
  static get is() {
    return 'consumer-update-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * True if update is forced.
       */
      isSkipButtonHidden: {
        type: Boolean,
        value: true,
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

    };
  }

  static get observers(): string[] {
    return ['playAnimation(uiStep)'];
  }

  private isSkipButtonHidden: boolean;
  private showLowBatteryWarning: boolean;
  private updateStatusMessagePercent: string;
  private updateStatusMessageTimeLeft: string;
  private betterUpdateProgressValue: number;
  private autoTransition: boolean;
  private thresholdIndex: number;
  private callbackRouter: ConsumerUpdatePageCallbackRouter;
  private handler: ConsumerUpdatePageHandlerRemote;

  constructor() {
    super();
    this.callbackRouter = new ConsumerUpdatePageCallbackRouter();
    this.handler = new ConsumerUpdatePageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory
        .establishConsumerUpdateScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver())
        .then((response: any) => {
          this.callbackRouter.$.bindHandle(response.pending.handle);
        });

    this.callbackRouter.showSkipButton.addListener(
        this.showSkipButton.bind(this));
    this.callbackRouter.setLowBatteryWarningVisible.addListener(
        this.setLowBatteryWarningVisible.bind(this));
    this.callbackRouter.setScreenStep.addListener(
        this.setScreenStep.bind(this));
    this.callbackRouter.setUpdateStatusMessage.addListener(
        this.setUpdateStatusMessage.bind(this));
    this.callbackRouter.setAutoTransition.addListener(
        this.setAutoTransition.bind(this));
  }

  override get UI_STEPS() {
    return ConsumerUpdateStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return ConsumerUpdateStep.CHECKING;
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('ConsumerUpdateScreen');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  /**
   * Shows skip button while update in progress.
   */
  showSkipButton(): void {
    this.isSkipButtonHidden = false;
  }

  override onBeforeHide(): void {
    super.onBeforeHide();
    const animation = this.shadowRoot?.querySelector('#checkingAnimation');
    if (animation instanceof OobeCrLottie) {
      animation.playing = false;
    }
  }

  /**
   * Decline to use cellular data.
   */
  private onDeclineCellularClicked(): void {
    this.handler.onDeclineCellularClicked();
  }

  /**
   * Accept to use cellular data.
   */
  private onAcceptCellularClicked(): void {
    this.handler.onAcceptCellularClicked();
  }

  private onSkip(): void {
    this.handler.onSkipClicked();
  }

  /**
   * Shows or hides battery warning message.
   * @param visible Is message visible?
   */
  setLowBatteryWarningVisible(visible: boolean): void {
    this.showLowBatteryWarning = visible;
  }

  /**
   * Sets which dialog should be shown.
   * @param value Current update state.
   */
  setScreenStep(value: ConsumerUpdatePage_ConsumerUpdateStep): void {
    switch (value) {
      case ConsumerUpdatePage_ConsumerUpdateStep.kCheckingForUpdate:
        this.setUIStep(ConsumerUpdateStep.CHECKING);
        break;
      case ConsumerUpdatePage_ConsumerUpdateStep.kUpdateInProgress:
        this.setUIStep(ConsumerUpdateStep.UPDATE);
        break;
      case ConsumerUpdatePage_ConsumerUpdateStep.kRestartInProgress:
        this.setUIStep(ConsumerUpdateStep.RESTART);
        break;
      case ConsumerUpdatePage_ConsumerUpdateStep.kManualReboot:
        this.setUIStep(ConsumerUpdateStep.REBOOT);
        break;
      case ConsumerUpdatePage_ConsumerUpdateStep.kCellularPermission:
        this.setUIStep(ConsumerUpdateStep.CELLULAR);
        break;
    }
  }

  /**
   * Sets percent to be shown in progress bar.
   * @param percent Current progress
   * @param messagePercent Message describing current progress.
   * @param messageTimeLeft Message describing time left.
   */
  setUpdateStatusMessage(
      percent: number, messagePercent: string, messageTimeLeft: string): void {
    // Sets aria-live polite on percent and timeleft container every time
    // new threshold has been achieved otherwise do not initiate spoken
    // feedback update by setting aria-live off.
    const betterUpdatePercent = this.shadowRoot?.
        querySelector('#betterUpdatePercent');
    const betterUpdateTimeleft = this.shadowRoot?.
        querySelector('#betterUpdateTimeleft');
    if (percent >= PERCENT_THRESHOLDS[this.thresholdIndex]) {
      while (percent >= PERCENT_THRESHOLDS[this.thresholdIndex]) {
        this.thresholdIndex = this.thresholdIndex + 1;
      }
      if (betterUpdatePercent instanceof HTMLElement
          && betterUpdateTimeleft instanceof HTMLElement) {
        betterUpdatePercent.setAttribute('aria-live', 'polite');
        betterUpdateTimeleft.setAttribute('aria-live', 'polite');
      }
    } else {
      if (betterUpdatePercent instanceof HTMLElement
          && betterUpdateTimeleft instanceof HTMLElement) {
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
  setAutoTransition(enabled: boolean): void {
    this.autoTransition = enabled;
  }

  /**
   * Gets whether carousel should auto transit slides.
   * @param step Which UIState is shown now.
   * @param autoTransition Is auto transition allowed.
   */
  private getAutoTransition(step: ConsumerUpdateStep,
      autoTransition: boolean): boolean {
    return step === ConsumerUpdateStep.UPDATE && autoTransition;
  }

  private onBackClicked(): void {
    this.handler.onBackClicked();
  }

  /**
   * @param uiStep which UIState is shown now.
   */
  private playAnimation(uiStep: ConsumerUpdateStep): void {
    const animation = this.shadowRoot?.querySelector('#checkingAnimation');
    if (animation instanceof OobeCrLottie) {
      animation.playing = (uiStep === ConsumerUpdateStep.CHECKING);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ConsumerUpdateScreen.is]: ConsumerUpdateScreen;
  }
}

customElements.define(ConsumerUpdateScreen.is, ConsumerUpdateScreen);
