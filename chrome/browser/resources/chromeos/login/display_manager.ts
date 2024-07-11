// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Display manager for WebUI OOBE and login.
 */

import {$, ensureTransitionEndEvent} from '//resources/ash/common/util.js';
import {assert, assertInstanceof} from '//resources/js/assert.js';
import {afterNextRender} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DisplayType, OobeUiState, SCREEN_DEVICE_DISABLED, SCREEN_WELCOME} from './components/display_manager_types.js';
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
 * A display manager that manages initialization of screens,
 * transitions, error messages display.
 */
export class DisplayManager {
  screens: string[];
  private currentStepId: null|string;
  private keyboardFlowOn: boolean;
  private virtualKeyboardShown: boolean;
  private displayType: DisplayType;
  private oobeConfiguration: undefined|OobeTypes.OobeConfiguration;
  private demoModeStartListener: null|MultiTapDetector;

  constructor() {
    /**
     * Registered screens.
     */
    this.screens = [];

    /**
     * Current OOBE step id.
     */
    this.currentStepId = null;

    /**
     * Whether keyboard navigation flow is enforced.
     */
    this.keyboardFlowOn = false;

    /**
     * Whether the virtual keyboard is displayed.
     */
    this.virtualKeyboardShown = false;

    /**
     * Type of UI.
     */
    this.displayType = DisplayType.UNKNOWN;

    /**
     * Stored OOBE configuration for newly registered screens.
     */
    this.oobeConfiguration = undefined;

    /**
     * Detects multi-tap gesture that invokes demo mode setup in OOBE.
     */
    this.demoModeStartListener = null;
  }

  setVirtualKeyboardShown(shown: boolean): void {
    this.virtualKeyboardShown = shown;
    document.documentElement.toggleAttribute('virtual-keyboard', shown);
  }

  setDisplayType(displayType: DisplayType): void {
    this.displayType = displayType;
    document.documentElement.setAttribute('screen', displayType);
  }

  /**
   * Gets current screen element.
   */
  get currentScreen(): HTMLElement|null {
    if (this.currentStepId === null) {
      return null;
    }
    return $(this.currentStepId);
  }

  /**
   * Sets the current height of the shelf area.
   * @param height current shelf height
   */
  setShelfHeight(height: number): void {
    document.documentElement.style.setProperty(
        '--shelf-area-height-base', height + 'px');
  }

  setOrientation(isHorizontal: boolean): void {
    if (isHorizontal) {
      document.documentElement.setAttribute('orientation', 'horizontal');
    } else {
      document.documentElement.setAttribute('orientation', 'vertical');
    }
  }

  setDialogSize(width: number, height: number): void {
    document.documentElement.style.setProperty(
        '--oobe-oobe-dialog-height-base', height + 'px');
    document.documentElement.style.setProperty(
        '--oobe-oobe-dialog-width-base', width + 'px');
  }

  /**
   * Forces keyboard based OOBE navigation.
   * @param value True if keyboard navigation flow is forced.
   */
  set forceKeyboardFlow(value: boolean) {
    this.keyboardFlowOn = value;
    if (value) {
      globalOobeKeyboard.initializeKeyboardFlow();
    }
  }

  /**
   * Returns true if keyboard flow is enabled.
   */
  get forceKeyboardFlow(): boolean {
    return this.keyboardFlowOn;
  }

  /**
   * Returns current OOBE configuration.
   */
  getOobeConfiguration(): OobeTypes.OobeConfiguration|undefined {
    return this.oobeConfiguration;
  }

  /**
   * Toggles system info visibility.
   */
  toggleSystemInfo(): void {
    $('version-labels').toggleAttribute('hidden');
  }

  /**
   * Handle the cancel accelerator.
   */
  handleCancel(): void {
    if (this.currentScreen && ('cancel' in this.currentScreen) &&
        typeof this.currentScreen.cancel === 'function') {
      this.currentScreen.cancel();
    }
  }

  /**
   * Switches to the next OOBE step.
   * @param nextStepIndex Index of the next step.
   */
  private toggleStep(nextStepId: string, screenData: any): void {
    const oldStep = this.currentScreen;
    const newStep = $(nextStepId);
    assertInstanceof(
        newStep, HTMLElement, 'No screen with such id: ' + nextStepId);
    const innerContainer = $('inner-container');
    const oobeContainer = $('oobe');
    const isBootAnimationEnabled =
        loadTimeData.getBoolean('isBootAnimationEnabled');

    if (oldStep) {
      if ('onBeforeHide' in oldStep &&
          typeof oldStep.onBeforeHide === 'function') {
        oldStep.onBeforeHide();
      }

      if ('defaultControl' in oldStep &&
          oldStep.defaultControl instanceof HTMLElement &&
          'onBeforeHide' in oldStep.defaultControl &&
          typeof oldStep.defaultControl.onBeforeHide === 'function') {
        oldStep.defaultControl.onBeforeHide();
      }
    }

    $('oobe').className = nextStepId;

    // Need to do this before calling newStep.onBeforeShow() so that new step
    // is back in DOM tree and has correct offsetHeight / offsetWidth.
    newStep.hidden = false;

    if ('getOobeUIInitialState' in newStep &&
        typeof newStep.getOobeUIInitialState === 'function') {
      this.setOobeUiState(newStep.getOobeUIInitialState());
    } else {
      this.setOobeUiState(OobeUiState.HIDDEN);
    }

    if ('onBeforeShow' in newStep &&
        typeof newStep.onBeforeShow === 'function') {
      newStep.onBeforeShow(screenData);
    }

    // Default control to be focused (if specified).
    if ('defaultControl' in newStep &&
        newStep.defaultControl instanceof HTMLElement &&
        'onBeforeShow' in newStep.defaultControl &&
        typeof newStep.defaultControl.onBeforeShow === 'function') {
      newStep.defaultControl.onBeforeShow(screenData);
    }

    newStep.classList.remove('hidden');

    // Start fading animation for login display or reset screen.
    oldStep?.classList.add('faded');
    newStep.classList.remove('faded');

    let defaultControl: HTMLElement|null = null;
    if ('defaultControl' in newStep &&
        newStep.defaultControl instanceof HTMLElement) {
      defaultControl = newStep.defaultControl;
    }
    if (this.currentStepId !== nextStepId && oldStep &&
        !oldStep.classList.contains('hidden')) {
      oldStep.classList.add('hidden');
      oldStep.hidden = true;
      if (defaultControl) {
        defaultControl.focus();
      }
    } else {
      // First screen on OOBE launch.
      if (this.isOobeUi() && innerContainer.classList.contains('down')) {
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
    this.currentStepId = nextStepId;

    const currentScreen = this.currentScreen;
    assert(currentScreen, 'currentScreen must exist at this point');
    $('oobe').dispatchEvent(
        new CustomEvent('screenchanged', {detail: currentScreen.id}));
    chrome.send('updateCurrentScreen', [currentScreen.id]);

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
  sendBackdropLoaded(): void {
    chrome.send('backdropLoaded');
  }

  /**
   * Show screen of given screen id.
   */
  showScreen(screen: {id: string, data: any}): void {
    // Do not allow any other screen to clobber the device disabled screen.
    const currentScreen = this.currentScreen;
    if (currentScreen && currentScreen.id === SCREEN_DEVICE_DISABLED) {
      return;
    }

    this.toggleStep(screen.id, screen.data);
  }

  /**
   * Register an oobe screen.
   * @param el Decorated screen element.
   */
  registerScreen(el: Element): void {
    const screenId = el.id;
    assert(screenId);
    assert(!this.screens.includes(screenId), 'Duplicate screen ID.');
    assert(
        this.screens.length > 0 || screenId !== SCREEN_DEVICE_DISABLED,
        'Can not register Device disabled screen as the first');

    this.screens.push(screenId);

    if ('updateOobeConfiguration' in el &&
        typeof el.updateOobeConfiguration === 'function' &&
        this.oobeConfiguration) {
      el.updateOobeConfiguration(this.oobeConfiguration);
    }
  }

  /**
   * Updates localized content of the screens like headers, buttons and links.
   * Should be executed on language change.
   */
  updateLocalizedContent(): void {
    for (let i = 0; i < this.screens.length; ++i) {
      const screenId = this.screens[i];
      const screen = $(screenId);
      if ('updateLocalizedContent' in screen &&
          typeof screen.updateLocalizedContent === 'function') {
        screen.updateLocalizedContent();
      }
    }
    const dynamicElements = document.getElementsByClassName('i18n-dynamic');
    for (const child of dynamicElements) {
      if ('i18nUpdateLocale' in child &&
          typeof child.i18nUpdateLocale === 'function') {
        child.i18nUpdateLocale();
      }
    }
  }

  /**
   * Updates Oobe configuration for screens.
   * @param configuration OOBE configuration.
   */
  updateOobeConfiguration(configuration: OobeTypes.OobeConfiguration): void {
    this.oobeConfiguration = configuration;
    for (let i = 0; i < this.screens.length; ++i) {
      const screenId = this.screens[i];
      const screen = $(screenId);
      if ('updateOobeConfiguration' in screen &&
          typeof screen.updateOobeConfiguration === 'function') {
        screen.updateOobeConfiguration(configuration);
      }
    }
  }

  /**
   * Trigger of play down animation for current screen step.
   */
  triggerDown(): void {
    const innerContainer = $('inner-container');
    if (!this.isOobeUi() || !innerContainer.classList.contains('down')) {
      return;
    }

    innerContainer.classList.remove('down');
    innerContainer.addEventListener('transitionend', () => {
      // Refresh defaultControl. It could have changed.
      const step = this.currentScreen;
      innerContainer.classList.add('down-finished');
      if (step && 'defaultControl' in step &&
          step.defaultControl instanceof HTMLElement) {
        step.defaultControl.focus();
      }
    }, /*AddEventListenerOptions=*/ {once: true});
    ensureTransitionEndEvent(innerContainer, MAX_SCREEN_TRANSITION_DURATION);
  }

  /**
   * Initializes demo mode start listener.
   */
  initializeDemoModeMultiTapListener(): void {
    if (this.displayType === DisplayType.OOBE) {
      this.demoModeStartListener =
          new MultiTapDetector($('outer-container'), 10, () => {
            const currentScreen = this.currentScreen;
            if (currentScreen && currentScreen.id === SCREEN_WELCOME) {
              assert(
                  'onSetupDemoModeGesture' in currentScreen &&
                  typeof currentScreen.onSetupDemoModeGesture === 'function');
              currentScreen.onSetupDemoModeGesture();
            }
          });
    }
  }

  /**
   * Returns true if Oobe UI is shown.
   */
  isOobeUi(): boolean {
    return document.body.classList.contains('oobe-display');
  }

  /**
   * Notifies the C++ handler in views login that the OOBE signin state has
   * been updated. This information is primarily used by the login shelf to
   * update button visibility state.
   * @param state The state (see OobeUiState) of the OOBE UI.
   */
  setOobeUiState(state: number): void {
    chrome.send('updateOobeUIState', [state]);
  }

  /**
   * Initializes display manager.
   */
  initialize(): void {
    // Display type set in HTML property.
    let givenDisplayType = document.documentElement.getAttribute('screen');
    if (!givenDisplayType) {
      // Extracting display type from URL.
      givenDisplayType = window.location.pathname.substr(1);
    }
    assert(givenDisplayType);
    Object.getOwnPropertyNames(DisplayType).forEach(type => {
      if (DisplayType[type as keyof typeof DisplayType] === givenDisplayType) {
        this.setDisplayType(givenDisplayType);
      }
    });
    if (this.displayType === DisplayType.UNKNOWN) {
      console.error(
          'Unknown display type "' + givenDisplayType + '". Setting default.');
      this.setDisplayType(DisplayType.LOGIN);
    }
    this.initializeDemoModeMultiTapListener();
  }


  /**
   * Sets text content for a div with |labelId|.
   * @param labelId Id of the label div.
   * @param labelText Text for the label.
   */
  static setLabelText(labelId: string, labelText: string): void {
    $(labelId).textContent = labelText;
  }

  /**
   * Sets the text content of the Bluetooth device info message.
   * @param bluetoothName The Bluetooth device name text.
   */
  static setBluetoothDeviceInfo(bluetoothName: string): void {
    $('bluetooth-name').hidden = false;
    $('bluetooth-name').textContent = bluetoothName;
  }
}
