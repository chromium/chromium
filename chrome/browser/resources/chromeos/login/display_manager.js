// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Display manager for WebUI OOBE and login.
 */

import {assert} from '//resources/ash/common/assert.js';
import {$, ensureTransitionEndEvent} from '//resources/ash/common/util.js';
import {afterNextRender} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DISPLAY_TYPE, OOBE_UI_STATE, SCREEN_DEVICE_DISABLED, SCREEN_WELCOME} from './components/display_manager_types.js';
import {globalOobeKeyboard} from './components/keyboard_utils_oobe.js';
import {OobeTypes} from './components/oobe_types.js';
import {loadTimeData} from './i18n_setup.js';
import {MultiTapDetector} from './multi_tap_detector.js';

/**
 * Maximum time in milliseconds to wait for step transition to finish.
 * The value is used as the duration for ensureTransitionEndEvent below.
 * It needs to be inline with the step screen transition duration time
 * defined in css file. The current value in css is 1,150ms. To avoid emulated
 * transitionend fired before real one, +50ms is used.
 */
const MAX_SCREEN_TRANSITION_DURATION = 1200;

/**
 * Maximum delay to call triggerDown from cpp logic. If the logic fails,
 * triggerDown should be called after this duration to unblock CUJ.
 */
const TRIGGERDOWN_FALLBACK_DELAY = 10000;

/**
 * As Polymer behaviors do not provide true inheritance, when two behaviors
 * would declare same method one of them will be hidden. Also, if element
 * re-declares the method it needs explicitly iterate over behaviors and call
 * method on them. This function simplifies such interaction by calling
 * method on element and all behaviors in the same order as lifecycle
 * callbacks are called.
 * @param {Element} element
 * @param {string} name function name
 * @param {...*} args arguments for the function
 *
 * @suppress {missingProperties}
 * element.behaviors
 * TODO(crbug.com/1229130) - Remove this suppression.
 */
export function invokePolymerMethod(element, name, ...args) {
  const method = element[name];
  if (!method || typeof method !== 'function') {
    return;
  }
  method.apply(element, args);
  if (!element.behaviors) {
    return;
  }

  // If element has behaviors call functions on them in reverse order,
  // ignoring case when method on element was derived from behavior.
  for (let i = element.behaviors.length - 1; i >= 0; i--) {
    const behavior = element.behaviors[i];
    const behaviorMethod = behavior[name];
    if (!behaviorMethod || typeof behaviorMethod !== 'function') {
      continue;
    }
    if (behaviorMethod == method) {
      continue;
    }
    behaviorMethod.apply(element, args);
  }
}

  /**
   * A display manager that manages initialization of screens,
   * transitions, error messages display.
   */
  export class DisplayManager {
    constructor() {
      /**
       * Registered screens.
       */
      this.screens_ = [];

      /**
       * Current OOBE step, index in the screens array.
       * @type {number}
       */
      this.currentStep_ = 0;

      /**
       * Whether keyboard navigation flow is enforced.
       * @type {boolean}
       */
      this.forceKeyboardFlow_ = false;

      /**
       * Whether the virtual keyboard is displayed.
       * @type {boolean}
       */
      this.virtualKeyboardShown_ = false;

      /**
       * Type of UI.
       * @type {string}
       */
      this.displayType_ = DISPLAY_TYPE.UNKNOWN;

      /**
       * Stored OOBE configuration for newly registered screens.
       * @type {OobeTypes.OobeConfiguration|undefined}
       */
      this.oobe_configuration_ = undefined;

      /**
       * Detects multi-tap gesture that invokes demo mode setup in OOBE.
       * @type {?MultiTapDetector}
       * @private
       */
      this.demoModeStartListener_ = null;
    }

    set virtualKeyboardShown(shown) {
      this.virtualKeyboardShown_ = shown;
      document.documentElement.setAttribute('virtual-keyboard', shown);
    }

    get displayType() {
      return this.displayType_;
    }

    set displayType(displayType) {
      this.displayType_ = displayType;
      document.documentElement.setAttribute('screen', displayType);
    }

    /**
     * Gets current screen element.
     * @type {HTMLElement}
     */
    get currentScreen() {
      return $(this.screens_[this.currentStep_]);
    }

    /**
     * Sets the current height of the shelf area.
     * @param {number} height current shelf height
     */
    setShelfHeight(height) {
      document.documentElement.style.setProperty(
          '--shelf-area-height-base', height + 'px');
    }

    setOrientation(isHorizontal) {
      if (isHorizontal) {
        document.documentElement.setAttribute('orientation', 'horizontal');
      } else {
        document.documentElement.setAttribute('orientation', 'vertical');
      }
    }

    setDialogSize(width, height) {
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-height-base', height + 'px');
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-width-base', width + 'px');
    }

    /**
     * Forces keyboard based OOBE navigation.
     * @param {boolean} value True if keyboard navigation flow is forced.
     */
    set forceKeyboardFlow(value) {
      this.forceKeyboardFlow_ = value;
      if (value) {
        globalOobeKeyboard.initializeKeyboardFlow();
      }
    }

    /**
     * Returns true if keyboard flow is enabled.
     * @return {boolean}
     */
    get forceKeyboardFlow() {
      return this.forceKeyboardFlow_;
    }

    /**
     * Returns current OOBE configuration.
     * @return {OobeTypes.OobeConfiguration|undefined}
     */
    getOobeConfiguration() {
      return this.oobe_configuration_;
    }

    /**
     * Toggles system info visibility.
     */
    toggleSystemInfo() {
      $('version-labels').hidden = !$('version-labels').hidden;
    }

    /**
     * Handle the cancel accelerator.
     */
    handleCancel() {
      const currentStepId = this.screens_[this.currentStep_];
      if (this.currentScreen && this.currentScreen.cancel) {
        this.currentScreen.cancel();
      }
    }

    /**
     * Switches to the next OOBE step.
     * @param {number} nextStepIndex Index of the next step.
     *
     * @suppress {missingProperties}
     * newStep.defaultControl
     * TODO(crbug.com/1229130) - Remove this suppression.
     */
    toggleStep_(nextStepIndex, screenData) {
      const currentStepId = this.screens_[this.currentStep_];
      const nextStepId = this.screens_[nextStepIndex];
      const oldStep = $(currentStepId);
      const newStep = $(nextStepId);
      const innerContainer = $('inner-container');
      const oobeContainer = $('oobe');
      const isBootAnimationEnabled =
          loadTimeData.getBoolean('isBootAnimationEnabled');

      invokePolymerMethod(oldStep, 'onBeforeHide');

      if (oldStep.defaultControl) {
        invokePolymerMethod(oldStep.defaultControl, 'onBeforeHide');
      }

      $('oobe').className = nextStepId;

      // Need to do this before calling newStep.onBeforeShow() so that new step
      // is back in DOM tree and has correct offsetHeight / offsetWidth.
      newStep.hidden = false;

      if (newStep.getOobeUIInitialState) {
        this.setOobeUIState(newStep.getOobeUIInitialState());
      } else {
        this.setOobeUIState(OOBE_UI_STATE.HIDDEN);
      }

      invokePolymerMethod(newStep, 'onBeforeShow', screenData);

      if (newStep.defaultControl) {
        invokePolymerMethod(newStep.defaultControl, 'onBeforeShow', screenData);
      }

      newStep.classList.remove('hidden');

      // Start fading animation for login display or reset screen.
      oldStep.classList.add('faded');
      newStep.classList.remove('faded');

      // Default control to be focused (if specified).
      const defaultControl = newStep.defaultControl;

      if (this.currentStep_ != nextStepIndex &&
          !oldStep.classList.contains('hidden')) {
        oldStep.classList.add('hidden');
        oldStep.hidden = true;
        if (defaultControl) {
          defaultControl.focus();
        }
      } else {
        // First screen on OOBE launch.
        if (this.isOobeUI() && innerContainer.classList.contains('down')) {
          if (isBootAnimationEnabled &&
              oobeContainer.classList.contains('connect')) {
            setTimeout(this.triggerDown.bind(this), TRIGGERDOWN_FALLBACK_DELAY);
          } else {
            this.triggerDown();
          }
        } else {
          if (defaultControl) {
            defaultControl.focus();
          }
        }
      }
      this.currentStep_ = nextStepIndex;

      // Call onAfterShow after currentStep_ so that the step can have a
      // post-set hook.
      invokePolymerMethod(newStep, 'onAfterShow', screenData);

      $('oobe').dispatchEvent(
          new CustomEvent('screenchanged', {detail: this.currentScreen.id}));
      chrome.send('updateCurrentScreen', [this.currentScreen.id]);

      // Post a task to finish initial animation once a frame is rendered.
      // Posting the task makes sure it will be executed after all other
      // pending calls happening in JS that might delay the paint event.
      if (isBootAnimationEnabled && innerContainer.classList.contains('down')) {
        afterNextRender(this, () => this.sendBackdropLoaded());
      }
    }

    /**
     * Notify browser that backdrop is ready.
     */
    sendBackdropLoaded() {
      chrome.send('backdropLoaded');
    }


    /**
     * Show screen of given screen id.
     * @param {Object} screen Screen params dict, e.g. {id: screenId, data: {}}.
     */
    showScreen(screen) {
      // Do not allow any other screen to clobber the device disabled screen.
      if (this.currentScreen.id == SCREEN_DEVICE_DISABLED) {
        return;
      }

      const screenId = screen.id;

      const data = screen.data;
      const index = this.getScreenIndex_(screenId);
      if (index >= 0) {
        this.toggleStep_(index, data);
      }
    }

    /**
     * Gets index of given screen id in screens_.
     * @param {string} screenId Id of the screen to look up.
     * @private
     */
    getScreenIndex_(screenId) {
      for (let i = 0; i < this.screens_.length; ++i) {
        if (this.screens_[i] == screenId) {
          return i;
        }
      }
      return -1;
    }

    /**
     * Register an oobe screen.
     * @param {Element} el Decorated screen element.
     */
    registerScreen(el) {
      const screenId = el.id;
      assert(screenId);
      assert(!this.screens_.includes(screenId), 'Duplicate screen ID.');
      assert(
          this.screens_.length > 0 || screenId !== SCREEN_DEVICE_DISABLED,
          'Can not register Device disabled screen as the first');

      this.screens_.push(screenId);

      if (el.updateOobeConfiguration && this.oobe_configuration_) {
        el.updateOobeConfiguration(this.oobe_configuration_);
      }
    }

    /**
     * Updates localized content of the screens like headers, buttons and links.
     * Should be executed on language change.
     */
    updateLocalizedContent_() {
      for (let i = 0; i < this.screens_.length; ++i) {
        const screenId = this.screens_[i];
        const screen = $(screenId);
        if (screen.updateLocalizedContent) {
          screen.updateLocalizedContent();
        }
      }
      const dynamicElements = document.getElementsByClassName('i18n-dynamic');
      for (const child of dynamicElements) {
        if (typeof (child.i18nUpdateLocale) === 'function') {
          child.i18nUpdateLocale();
        }
      }
      const isInTabletMode = loadTimeData.getBoolean('isInTabletMode');
      this.setTabletModeState_(isInTabletMode);
    }

    /**
     * Updates Oobe configuration for screens.
     * @param {!OobeTypes.OobeConfiguration} configuration OOBE configuration.
     */
    updateOobeConfiguration_(configuration) {
      this.oobe_configuration_ = configuration;
      for (let i = 0; i < this.screens_.length; ++i) {
        const screenId = this.screens_[i];
        const screen = $(screenId);
        if (screen.updateOobeConfiguration) {
          screen.updateOobeConfiguration(configuration);
        }
      }
    }

    /**
     * Updates "device in tablet mode" state when tablet mode is changed.
     * @param {boolean} isInTabletMode True when in tablet mode.
     */
    setTabletModeState_(isInTabletMode) {
      document.documentElement.setAttribute('tablet', isInTabletMode);
      for (let i = 0; i < this.screens_.length; ++i) {
        const screenId = this.screens_[i];
        const screen = $(screenId);
        if (screen.setTabletModeState) {
          screen.setTabletModeState(isInTabletMode);
        }
      }
    }

    /**
     * Trigger of play down animation for current screen step.
     */
    triggerDown() {
      const innerContainer = $('inner-container');
      if (!this.isOobeUI() || !innerContainer.classList.contains('down')) {
        return;
      }

      innerContainer.classList.remove('down');
      innerContainer.addEventListener('transitionend', () => {
        // Refresh defaultControl. It could have changed.
        const stepId = this.screens_[this.currentStep_];
        const step = $(stepId);
        const defaultControl = step.defaultControl;
        innerContainer.classList.add('down-finished');
        if (defaultControl) {
          defaultControl.focus();
        }
      }, /*AddEventListenerOptions=*/ {once: true});
      ensureTransitionEndEvent(innerContainer, MAX_SCREEN_TRANSITION_DURATION);
    }

    /** Initializes demo mode start listener.
     * @suppress {missingProperties}
     * currentScreen.onSetupDemoModeGesture()
     * TODO(crbug.com/1229130) - Remove this suppression.
     */
    initializeDemoModeMultiTapListener() {
      if (this.displayType_ == DISPLAY_TYPE.OOBE) {
        this.demoModeStartListener_ =
            new MultiTapDetector($('outer-container'), 10, () => {
              const currentScreen = this.currentScreen;
              if (currentScreen.id === SCREEN_WELCOME) {
                currentScreen.onSetupDemoModeGesture();
              }
            });
      }
    }

    /**
     * Returns true if Oobe UI is shown.
     * @return {boolean}
     */
    isOobeUI() {
      return document.body.classList.contains('oobe-display');
    }

    /**
     * Notifies the C++ handler in views login that the OOBE signin state has
     * been updated. This information is primarily used by the login shelf to
     * update button visibility state.
     * @param {number} state The state (see OOBE_UI_STATE) of the OOBE UI.
     */
    setOobeUIState(state) {
      chrome.send('updateOobeUIState', [state]);
    }

    /**
     * Initializes display manager.
     */
    initialize() {
      let givenDisplayType = DISPLAY_TYPE.UNKNOWN;
      if (document.documentElement.hasAttribute('screen')) {
        // Display type set in HTML property.
        givenDisplayType = document.documentElement.getAttribute('screen');
      } else {
        // Extracting display type from URL.
        givenDisplayType = window.location.pathname.substr(1);
      }
      Object.getOwnPropertyNames(DISPLAY_TYPE).forEach( type => {
        if (DISPLAY_TYPE[type] == givenDisplayType) {
          this.displayType = givenDisplayType;
        }
      });
      if (this.displayType == DISPLAY_TYPE.UNKNOWN) {
        console.error(
            'Unknown display type "' + givenDisplayType +
            '". Setting default.');
        this.displayType = DISPLAY_TYPE.LOGIN;
      }
      this.initializeDemoModeMultiTapListener();
    }


    /**
     * Sets text content for a div with |labelId|.
     * @param {string} labelId Id of the label div.
     * @param {string} labelText Text for the label.
     */
    static setLabelText(labelId, labelText) {
      $(labelId).textContent = labelText;
    }

    /**
     * Sets the text content of the Bluetooth device info message.
     * @param {string} bluetoothName The Bluetooth device name text.
     */
    static setBluetoothDeviceInfo(bluetoothName) {
      $('bluetooth-name').hidden = false;
      $('bluetooth-name').textContent = bluetoothName;
    }
  }
