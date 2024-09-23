// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design OOBE.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_i18n_dropdown.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {CrInputElement} from '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeA11yOption} from '../../components/oobe_a11y_option.js';
import {getSelectedTitle} from '../../components/oobe_select.js';
import {OobeTypes} from '../../components/oobe_types.js';
import {Oobe} from '../../cr_ui.js';
import {traceWelcomeAnimationPlay} from '../../oobe_trace.js';

import {getTemplate} from './welcome.html.js';
import {OobeWelcomeDialog} from './welcome_dialog.js';

const DEFAULT_CHROMEVOX_HINT_LOCALE: string = 'en-US';

/**
 * The extension ID of the speech engine (Google Speech Synthesis) used to
 * give the default ChromeVox hint.
 */
const DEFAULT_CHROMEVOX_HINT_VOICE_EXTENSION_ID: string =
    'gjjabgpgjpampikjhjpfhneeoapjbjaf';

// The help topic regarding language packs.
const HELP_LANGUAGE_PACKS = 11383012;

/**
 * UI mode for the dialog.
 */
enum WelcomeScreenState {
  GREETING = 'greeting',
  LANGUAGE = 'language',
  ACCESSIBILITY = 'accessibility',
  TIMEZONE = 'timezone',
  ADVANCED_OPTIONS = 'advanced-options',
}

export interface A11yStatuses {
  highContrastEnabled: boolean;
  spokenFeedbackEnabled: boolean;
  screenMagnifierEnabled: boolean;
  largeCursorEnabled: boolean;
  virtualKeyboardEnabled: boolean;
}

const OobeWelcomeScreenBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

/**
 * Data that is passed to the screen during onBeforeShow.
 */
export interface WelcomeScreenData {
  isDeveloperMode: boolean;
}

export class OobeWelcomeScreen extends OobeWelcomeScreenBase {
  static get is() {
    return 'oobe-welcome-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Currently selected system language (display name).
       */
      currentLanguage: String,

      /**
       * Currently selected input method (display name).
       */
      currentKeyboard: String,

      /**
       * List of languages for language selector dropdown.
       */
      languages: {
        type: Array,
        observer: 'onLanguagesChanged',
      },

      /**
       * List of keyboards for keyboard selector dropdown.
       */
      keyboards: {
        type: Array,
        observer: 'onKeyboardsChanged',
      },

      /**
       * Accessibility options status.
       */
      a11yStatus: {
        type: Object,
      },

      /**
       * A list of timezones for Timezone Selection screen.
       */
      timezones: {
        type: Object,
      },

      /**
       * If UI uses forced keyboard navigation.
       */
      highlightStrength: {
        type: String,
      },

      /**
       * Controls displaying of "Enable debugging features" link.
       */
      debuggingLinkVisible: {
        type: Boolean,
      },

      /**
       * Used to save the function instance created when for binded
       * maybeGiveChromeVoxHint.
       *  {function(this:SpeechSynthesis, Event): *|null|undefined}
       */
      voicesChangedListenerMaybeGiveChromeVoxHint: {
        type: Function,
      },

      /**
       * The id of the timer that's set when setting a timeout on
       * giveChromeVoxHint.
       * Only gets set if the initial call to maybeGiveChromeVoxHint fails.
       *  {number|undefined}
       */
      defaultChromeVoxHintTimeoutId: {
        type: Number,
      },

      /**
       * The time in MS to wait before giving the ChromeVox hint in English.
       * Declared as a property so it can be modified in a test.
       *  {number}
       * @const
       */
      DEFAULT_CHROMEVOX_HINT_TIMEOUT_MS: {
        type: Number,
      },

      /**
       * Tracks if we've given the ChromeVox hint yet.
       */
      chromeVoxHintGiven: {
        type: Boolean,
      },

      /**
       * If it is a meet device.
       */
      isMeet: {
        type: Boolean,
        value: function() {
          return (
              loadTimeData.valueExists('deviceFlowType') &&
              loadTimeData.getString('deviceFlowType') === 'meet');
        },
        readOnly: true,
      },

      /**
       * If device requisition is configurable.
       */
      isDeviceRequisitionConfigurable: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('isDeviceRequisitionConfigurable');
        },
        readOnly: true,
      },
    };
  }

  private currentLanguage: string;
  private currentKeyboard: string;
  private languages: OobeTypes.LanguageDsc[];
  private keyboards: OobeTypes.InputMethodsDsc[];
  private a11yStatus: A11yStatuses;
  private timezones: OobeTypes.TimezoneDsc[];
  private highlightStrength: string;
  private debuggingLinkVisible: boolean;
  private voicesChangedListenerMaybeGiveChromeVoxHint: (() => void)|undefined;
  private defaultChromeVoxHintTimeoutId: number|undefined;
  // eslint-disable-next-line @typescript-eslint/naming-convention
  private DEFAULT_CHROMEVOX_HINT_TIMEOUT_MS: number;
  private chromeVoxHintGiven: boolean;
  private isMeet: boolean;
  private isDeviceRequisitionConfigurable: boolean;
  private configurationApplied: boolean;

  constructor() {
    super();

    this.currentLanguage = '';
    this.currentKeyboard = '';
    this.highlightStrength = '';
    this.debuggingLinkVisible = false;
    this.voicesChangedListenerMaybeGiveChromeVoxHint = undefined;
    this.defaultChromeVoxHintTimeoutId = undefined;
    this.DEFAULT_CHROMEVOX_HINT_TIMEOUT_MS = 40 * 1000;
    this.chromeVoxHintGiven = false;

    this.configurationApplied = false;
  }

  override get EXTERNAL_API() {
    return [
      'onInputMethodIdSetFromBackend',
      'refreshA11yInfo',
      'showDemoModeConfirmationDialog',
      'showEditRequisitionDialog',
      'showRemoraRequisitionDialog',
      'maybeGiveChromeVoxHint',
      'setQuickStartEnabled',
    ];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return WelcomeScreenState.GREETING;
  }

  override get UI_STEPS() {
    return WelcomeScreenState;
  }

  override ready() {
    super.ready();
    this.addEventListener(
        'cros-lottie-playing', this.measureAnimationPlayDelay, {once: true});
    this.initializeLoginScreen('WelcomeScreen');
    this.updateLocalizedContent();
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   * @param data Screen init payload.
   */
  override onBeforeShow(data: WelcomeScreenData): void {
    super.onBeforeShow(data);
    this.debuggingLinkVisible =
        data && 'isDeveloperMode' in data && data['isDeveloperMode'];

    window.setTimeout(() => void this.applyOobeConfiguration(), 0);
  }

  private getWelcomeScreenDialog(): OobeWelcomeDialog {
    const dialog = this.shadowRoot?.querySelector('#welcomeScreen');
    assert(dialog instanceof OobeWelcomeDialog);
    return dialog;
  }

  private measureAnimationPlayDelay(e: Event): void {
    e.stopPropagation();
    traceWelcomeAnimationPlay();
  }

  /**
   * Event handler that is invoked just before the screen is hidden.
   */
  override onBeforeHide(): void {
    super.onBeforeHide();
    this.cleanupChromeVoxHint();
  }

  private cancel(): void {
    if (this.uiStep === WelcomeScreenState.LANGUAGE) {
      this.closeLanguageSection();
      return;
    }

    if (this.uiStep === WelcomeScreenState.ACCESSIBILITY) {
      this.closeAccessibilitySection();
      return;
    }
  }

  /**
   * This is called when UI strings are changed.
   * Overridden from LoginScreenBehavior.
   */
  override updateLocalizedContent(): void {
    this.languages = loadTimeData.getValue('languageList');
    this.keyboards = loadTimeData.getValue('inputMethodsList');
    this.timezones = loadTimeData.getValue('timezoneList');
    this.highlightStrength = loadTimeData.getValue('highlightStrength');

    this.getWelcomeScreenDialog().i18nUpdateLocale();
    this.i18nUpdateLocale();

    const currentLanguage = loadTimeData.getString('language');

    // We might have changed language via configuration. In this case
    // we need to proceed with rest of configuration after language change
    // was fully resolved.
    const configuration = Oobe.getInstance().getOobeConfiguration();
    if (configuration && configuration.language &&
        configuration.language === currentLanguage) {
      window.setTimeout(() => void this.applyOobeConfiguration(), 0);
    }
  }

  /**
   * Called when OOBE configuration is loaded.
   * Overridden from LoginScreenBehavior.
   */
  override updateOobeConfiguration(): void {
    if (!this.configurationApplied) {
      window.setTimeout(() => void this.applyOobeConfiguration(), 0);
    }
  }

  /**
   * Called when dialog is shown for the first time.
   */
  private applyOobeConfiguration(): void {
    if (this.configurationApplied) {
      return;
    }
    const configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration) {
      return;
    }

    if (configuration.language) {
      const currentLanguage = loadTimeData.getString('language');
      if (currentLanguage !== configuration.language) {
        this.applySelectedLanguage(configuration.language);
        // Trigger language change without marking it as applied.
        // applyOobeConfiguration will be called again once language change
        // was applied.
        return;
      }
    }
    if (configuration.inputMethod) {
      this.applySelectedLkeyboard(configuration.inputMethod);
    }

    if (configuration.welcomeNext) {
      this.onWelcomeNextButtonClicked();
    }

    if (configuration.enableDemoMode) {
      this.userActed('setupDemoModeGesture');
    }

    this.configurationApplied = true;
  }

  /**
   * Returns true if timezone button should be visible.
   */
  private isTimezoneButtonVisible(highlightStrength: string): boolean {
    return highlightStrength === 'strong';
  }

  /**
   * Handle "Next" button for "Welcome" screen.
   *
   */
  private onWelcomeNextButtonClicked(): void {
    this.userActed('continue');
  }

  /**
   * Handles "enable-debugging" link for "Welcome" screen.
   *
   */
  private onEnableDebuggingClicked(): void {
    this.userActed('enableDebugging');
  }

  /**
   * Handle "launch-advanced-options" button for "Welcome" screen.
   *
   */
  private onWelcomeLaunchAdvancedOptions(): void {
    this.cancelChromeVoxHint();
    this.setUIStep(WelcomeScreenState.ADVANCED_OPTIONS);
  }

  /**
   * Handle "Language" button for "Welcome" screen.
   *
   */
  private onWelcomeSelectLanguageButtonClicked(): void {
    this.cancelChromeVoxHint();
    this.setUIStep(WelcomeScreenState.LANGUAGE);
  }

  /**
   * Handle "Accessibility" button for "Welcome" screen.
   *
   */
  private onWelcomeAccessibilityButtonClicked(): void {
    this.cancelChromeVoxHint();
    this.setUIStep(WelcomeScreenState.ACCESSIBILITY);
  }

  /**
   * Handle "Timezone" button for "Welcome" screen.
   *
   */
  private onWelcomeTimezoneButtonClicked(): void {
    this.cancelChromeVoxHint();
    this.setUIStep(WelcomeScreenState.TIMEZONE);
  }

  /**
   * Handle language selection.
   *
   */
  private onLanguageSelected(event: CustomEvent<OobeTypes.LanguageDsc>): void {
    const item = event.detail;
    const languageId = item.value;
    this.currentLanguage = item.title;
    this.applySelectedLanguage(languageId);
  }

  /**
   * Switch UI language.
   *
   */
  private applySelectedLanguage(languageId: string): void {
    this.userActed(['setLocaleId', languageId]);
  }

  /**
   * Handle keyboard layout selection.
   */
  private onKeyboardSelected(event: CustomEvent<OobeTypes.InputMethodsDsc>):
      void {
    const item = event.detail;
    const inputMethodId = item.value;
    this.currentKeyboard = item.title;
    this.applySelectedLkeyboard(inputMethodId);
  }

  /**
   * Switch keyboard layout.
   *
   */
  private applySelectedLkeyboard(inputMethodId: string): void {
    this.userActed(['setInputMethodId', inputMethodId]);
  }

  private onLanguagesChanged(): void {
    this.currentLanguage = getSelectedTitle(this.languages);
  }

  onInputMethodIdSetFromBackend(keyboardId: string): void {
    let found = false;
    for (let i = 0; i < this.keyboards.length; ++i) {
      if (this.keyboards[i].value !== keyboardId) {
        this.keyboards[i].selected = false;
        continue;
      }
      this.keyboards[i].selected = true;
      found = true;
    }
    if (!found) {
      return;
    }

    // Force i18n-dropdown to refresh.
    this.keyboards = this.keyboards.slice();
    this.onKeyboardsChanged();
  }

  /**
   * Refreshes a11y menu state.
   * @param data New dictionary with a11y features
   *     state.
   */
  refreshA11yInfo(data: A11yStatuses): void {
    this.a11yStatus = data;
    if (data.spokenFeedbackEnabled) {
      this.closeChromeVoxHint();
    }
  }

  private getDemoModeConfirmationDialog(): OobeModalDialog {
    const dialog = this.shadowRoot?.querySelector('#demoModeConfirmationDialog');
    assert(dialog instanceof OobeModalDialog);
    return dialog;
  }

  /**
   * On-tap event handler for demo mode confirmation dialog cancel button.
   */
  private onDemoModeDialogCancel(): void {
    this.getDemoModeConfirmationDialog().hideDialog();
  }

  /**
   * On-tap event handler for demo mode confirmation dialog confirm button.
   */
  private onDemoModeDialogConfirm(): void {
    this.userActed('setupDemoMode');
    this.getDemoModeConfirmationDialog().hideDialog();
  }

  /**
   * Shows confirmation dialog for starting Demo mode
   */
  showDemoModeConfirmationDialog(): void {
    // Ensure the ChromeVox hint dialog is closed.
    this.closeChromeVoxHint();
    this.getDemoModeConfirmationDialog().showDialog();
  }

  private onSetupDemoModeGesture(): void {
    this.userActed('setupDemoModeGesture');
  }

  private getEditRequisitionDialog(): OobeModalDialog {
    const dialog = this.shadowRoot?.querySelector('#editRequisitionDialog');
    assert(dialog instanceof OobeModalDialog);
    return dialog;
  }

  private getEditRequisitionInput(): CrInputElement {
    const input = this.shadowRoot?.querySelector('#editRequisitioninput');
    assert(input instanceof CrInputElement);
    return input;
  }

  /**
   * Shows the device requisition prompt.
   */
  showEditRequisitionDialog(): void {
    this.getEditRequisitionDialog().showDialog();
    this.getEditRequisitionInput().focus();
  }

  private onEditRequisitionCancel(): void {
    this.userActed(['setDeviceRequisition', 'none']);
    this.getEditRequisitionDialog().hideDialog();
  }

  private onEditRequisitionConfirm(): void {
    const requisition = this.getEditRequisitionInput().value;
    this.userActed(['setDeviceRequisition', requisition]);
    this.getEditRequisitionDialog().hideDialog();
  }

  private getRemoraRequisitionDialog(): OobeModalDialog {
    const dialog = this.shadowRoot?.querySelector('#remoraRequisitionDialog');
    assert(dialog instanceof OobeModalDialog);
    return dialog;
  }

  /**
   * Shows the special remora/shark device requisition prompt.
   */
  showRemoraRequisitionDialog(): void {
    this.getRemoraRequisitionDialog().showDialog();
  }

  /**
   * Shows the special remora/shark device requisition prompt.
   */
  private onRemoraCancel(): void {
    this.userActed(['setDeviceRequisition', 'none']);
    this.getRemoraRequisitionDialog().hideDialog();
  }

  /**
   * Shows the special remora/shark device requisition prompt.
   */
  private onRemoraConfirm(): void {
    this.userActed(['setDeviceRequisition', 'remora']);
    this.getRemoraRequisitionDialog().hideDialog();
  }

  private onKeyboardsChanged(): void {
    this.currentKeyboard = getSelectedTitle(this.keyboards);
  }

  /** ******************** Language section ******************* */

  /**
   * Handle "OK" button for "LanguageSelection" screen.
   *
   */
  private closeLanguageSection(): void {
    this.setUIStep(WelcomeScreenState.GREETING);
  }

  /**
   * On-tap event handler for "learn more" link about language packs.
   *
   */
  private onLanguageLearnMoreLinkClicked(e: Event): void {
    chrome.send('launchHelpApp', [HELP_LANGUAGE_PACKS]);

    const elem = this.shadowRoot?.querySelector('#languagesLearnMore');
    assert(elem instanceof HTMLAnchorElement);
    elem.focus();
    e.stopPropagation();
  }

  /** ******************** Accessibility section ******************* */

  /**
   * Handle "OK" button for "Accessibility Options" screen.
   *
   */
  private closeAccessibilitySection(): void {
    this.setUIStep(WelcomeScreenState.GREETING);
  }

  /**
   * Handle all accessibility buttons.
   * Note that each <oobe-a11y-option> has chromeMessage attribute
   * containing Chromium callback name.
   *
   */
  private onA11yOptionChanged(event: CustomEvent<OobeA11yOption>): void {
    assert(event.currentTarget instanceof OobeA11yOption);
    const a11ytarget: OobeA11yOption = event.currentTarget;
    if (a11ytarget.checked) {
      this.userActed(a11ytarget.id + '-enable');
    } else {
      this.userActed(a11ytarget.id + '-disable');
    }
  }

  /** ******************** Timezone section ******************* */

  /**
   * Handle "OK" button for "Timezone Selection" screen.
   *
   */
  private closeTimezoneSection(): void {
    this.setUIStep(WelcomeScreenState.GREETING);
  }

  /**
   * Handle timezone selection.
   *
   */
  private onTimezoneSelected(event: CustomEvent<OobeTypes.TimezoneDsc>): void {
    const item = event.detail;
    if (!item) {
      return;
    }

    this.userActed(['setTimezoneId', item.value]);
  }

  /** ******************** AdvancedOptions section ******************* */

  /**
   * Handle "OK" button for "AdvancedOptions Selection" screen.
   *
   */
  private closeAdvancedOptionsSection(): void {
    this.setUIStep(WelcomeScreenState.GREETING);
  }

  /**
   * Handle click on "Set up as CFM device" option.
   *
   */
  private onCfmBootstrappingClicked(): void {
    this.userActed('activateRemoraRequisition');
  }

  /**
   * Handle click on "Device requisition" option.
   *
   */
  private onDeviceRequisitionClicked(): void {
    this.userActed('editDeviceRequisition');
  }

  /** ******************** ChromeVox hint section ******************* */

  private onChromeVoxHintAccepted(): void {
    this.userActed('activateChromeVoxFromHint');
  }

  private onChromeVoxHintDismissed(): void {
    this.userActed('dismissChromeVoxHint');
    chrome.tts.isSpeaking((speaking) => {
      if (speaking) {
        chrome.tts.stop();
      }
    });
  }

  private showChromeVoxHint(): void {
    this.getWelcomeScreenDialog().showChromeVoxHint();
  }

  private closeChromeVoxHint(): void {
    this.getWelcomeScreenDialog().closeChromeVoxHint();
  }

  private cancelChromeVoxHint(): void {
    this.userActed('cancelChromeVoxHint');
    this.cleanupChromeVoxHint();
  }

  /**
   * Initially called from WelcomeScreenHandler.
   * If we find a matching voice for the current locale, show the ChromeVox hint
   * dialog and give a spoken announcement with instructions for activating
   * ChromeVox. If we can't find a matching voice, call this function again
   * whenever a SpeechSynthesis voiceschanged event fires.
   */
  maybeGiveChromeVoxHint(): void {
    chrome.tts.getVoices((voices) => {
      const locale = loadTimeData.getString('language');
      const voiceName = this.findVoiceForLocale(voices, locale);
      if (!voiceName) {
        this.onVoiceNotLoaded();
        return;
      }

      const ttsOptions: chrome.tts.TtsOptions = ({
        lang: locale,
        voiceName,
      });
      this.giveChromeVoxHint(locale, ttsOptions, false);
    });
  }


  /**
   * Returns a voice name from |voices| that matches |locale|.
   * Returns undefined if no voice can be found.
   * Both |locale| and |voice.lang| will be in the form 'language-region'.
   * Examples include 'en', 'en-US', 'fr', and 'fr-CA'.
   */
  private findVoiceForLocale(voices: chrome.tts.TtsVoice[],
      locale: string): string | undefined {
    const language = locale.toLowerCase().split('-')[0];
    const voice = voices.find((voice) => {
      return !!(
          voice.lang && voice.lang.toLowerCase().split('-')[0] === language);
    });
    return voice ? voice.voiceName : undefined;
  }

  /**
   * Called if we couldn't find a voice in which to announce the ChromeVox
   * hint.
   * Registers a voiceschanged listener that tries to give the hint when new
   * voices are loaded. Also sets a timeout that gives the hint in the default
   * locale as a last resort.
   */
  private onVoiceNotLoaded(): void {
    if (this.voicesChangedListenerMaybeGiveChromeVoxHint === undefined) {
      // Add voiceschanged listener that tries to give the hint when new voices
      // are loaded.
      this.voicesChangedListenerMaybeGiveChromeVoxHint = () =>
          this.maybeGiveChromeVoxHint();
      window.speechSynthesis.addEventListener(
          'voiceschanged', this.voicesChangedListenerMaybeGiveChromeVoxHint,
          false);
    }

    if (!this.defaultChromeVoxHintTimeoutId) {
      // Set a timeout that gives the ChromeVox hint in the default locale.
      const ttsOptions: chrome.tts.TtsOptions = ({
        lang: DEFAULT_CHROMEVOX_HINT_LOCALE,
        extensionId: DEFAULT_CHROMEVOX_HINT_VOICE_EXTENSION_ID,
      });
      this.defaultChromeVoxHintTimeoutId = window.setTimeout(
          () => this.giveChromeVoxHint(
              DEFAULT_CHROMEVOX_HINT_LOCALE, ttsOptions, true),
          this.DEFAULT_CHROMEVOX_HINT_TIMEOUT_MS);
    }
  }

  /**
   * Shows the ChromeVox hint dialog and plays the spoken announcement. Gives
   * the spoken announcement with the provided options.
   */
  private giveChromeVoxHint(_locale: string, options: chrome.tts.TtsOptions,
      isDefaultHint: boolean): void {
    if (this.chromeVoxHintGiven) {
      // Only give the hint once.
      // Due to event listeners/timeouts, there is the chance that this gets
      // called multiple times.
      return;
    }

    this.chromeVoxHintGiven = true;
    if (isDefaultHint) {
      console.warn(
          'No voice available for ' + loadTimeData.getString('language') +
          ', giving default hint in English.');
    }
    this.cleanupChromeVoxHint();
    // |msgId| depends on both feature enabled status and tablet mode.
    const msgId = document.documentElement.hasAttribute('tablet') ?
        'chromeVoxHintAnnouncementTextTabletExpanded' :
        'chromeVoxHintAnnouncementTextLaptopExpanded';

    const message = this.i18n(msgId);
    chrome.tts.speak(message, options, () => {
      this.showChromeVoxHint();
      chrome.send('WelcomeScreen.recordChromeVoxHintSpokenSuccess');
    });
  }

  /**
   * Clear timeout and remove voiceschanged listener.
   */
  private cleanupChromeVoxHint(): void {
    if (this.defaultChromeVoxHintTimeoutId) {
      window.clearTimeout(this.defaultChromeVoxHintTimeoutId);
    }
    if (this.voicesChangedListenerMaybeGiveChromeVoxHint !== undefined) {
      window.speechSynthesis.removeEventListener(
          'voiceschanged',
          (this.voicesChangedListenerMaybeGiveChromeVoxHint),
          /* useCapture */ false);
      this.voicesChangedListenerMaybeGiveChromeVoxHint = undefined;
    }
  }

  /**
   * If it is possible to set up CFM.
   */
  private hideCfmSetupButton(isDeviceRequisitionConfigurable: boolean,
      isMeet: boolean): boolean {
    return !isDeviceRequisitionConfigurable && !isMeet;
  }

  /** ******************** Quick Start section ******************* */

  setQuickStartEnabled(): void {
    this.getWelcomeScreenDialog().isQuickStartEnabled = true;
  }

  /**
   * Handle "Quick Start" button for "Welcome" screen.
   *
   */
  private onActivateQuickStart(): void {
    this.userActed('quickStartClicked');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeWelcomeScreen.is]: OobeWelcomeScreen;
  }
}

customElements.define(OobeWelcomeScreen.is, OobeWelcomeScreen);
