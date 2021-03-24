// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design OOBE.
 */

'use strict';

(function() {

/** @const {string} */
const DEFAULT_CHROMEVOX_HINT_LOCALE = 'en-US';

/**
 * The extension ID of the speech engine (Google Speech Synthesis) used to
 * give the default ChromeVox hint.
 * @const {string}
 */
const DEFAULT_CHROMEVOX_HINT_VOICE_EXTENSION_ID =
    'gjjabgpgjpampikjhjpfhneeoapjbjaf';

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  GREETING: 'greeting',
  LANGUAGE: 'language',
  ACCESSIBILITY: 'accessibility',
  TIMEZONE: 'timezone',
  ADVANCED_OPTIONS: 'advanced-options',
};

Polymer({
  is: 'oobe-welcome-element',

  behaviors: [
    OobeI18nBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  properties: {
    /**
     * Currently selected system language (display name).
     */
    currentLanguage: {
      type: String,
      value: '',
    },

    /**
     * Currently selected input method (display name).
     */
    currentKeyboard: {
      type: String,
      value: '',
    },

    /**
     * List of languages for language selector dropdown.
     * @type {!Array<OobeTypes.LanguageDsc>}
     */
    languages: {
      type: Array,
      observer: 'onLanguagesChanged_',
    },

    /**
     * List of keyboards for keyboard selector dropdown.
     * @type {!Array<OobeTypes.IMEDsc>}
     */
    keyboards: {
      type: Array,
      observer: 'onKeyboardsChanged_',
    },

    /**
     * Accessibility options status.
     * @type {!OobeTypes.A11yStatuses}
     */
    a11yStatus: {
      type: Object,
    },

    /**
     * A list of timezones for Timezone Selection screen.
     * @type {!Array<OobeTypes.TimezoneDsc>}
     */
    timezones: {
      type: Object,
      value: [],
    },

    /**
     * If UI uses forced keyboard navigation.
     */
    highlightStrength: {
      type: String,
      value: '',
    },

    /**
     * Controls displaying of "Enable debugging features" link.
     */
    debuggingLinkVisible_: Boolean,

    /**
     * Used to save the function instance created when doing
     * this.maybeGiveChromeVoxHint.bind(this).
     * @private {function(this:SpeechSynthesis, Event): *|null|undefined}
     */
    voicesChangedListenerMaybeGiveChromeVoxHint_: {type: Function},

    /**
     * The id of the timer that's set when setting a timeout on
     * giveChromeVoxHint.
     * Only gets set if the initial call to maybeGiveChromeVoxHint fails.
     * @private {number|undefined}
     */
    defaultChromeVoxHintTimeoutId_: {type: Number},

    /**
     * The time in MS to wait before giving the ChromeVox hint in English.
     * Declared as a property so it can be modified in a test.
     * @private {number}
     * @const
     */
    DEFAULT_CHROMEVOX_HINT_TIMEOUT_MS_: {type: Number, value: 40 * 1000},

    /**
     * Tracks if we've given the ChromeVox hint yet.
     * @private
     */
    chromeVoxHintGiven_: {type: Boolean, value: false}
  },

  /** Overridden from LoginScreenBehavior. */
  EXTERNAL_API: [
    'onInputMethodIdSetFromBackend',
    'refreshA11yInfo',
    'showDemoModeConfirmationDialog',
    'showEditRequisitionDialog',
    'showRemoraRequisitionDialog',
    'maybeGiveChromeVoxHint',
  ],

  /**
   * Flag that ensures that OOBE configuration is applied only once.
   * @private {boolean}
   */
  configuration_applied_: false,

  defaultUIStep() {
    return UIState.GREETING;
  },

  UI_STEPS: UIState,

  /** @override */
  ready() {
    this.initializeLoginScreen('WelcomeScreen', {
      resetAllowed: true,
    });
    this.updateLocalizedContent();
  },

  /**
   * Event handler that is invoked just before the screen is shown.
   * TODO (https://crbug.com/948932): Define this type.
   * @param {Object} data Screen init payload.
   */
  onBeforeShow(data) {
    this.debuggingLinkVisible_ =
        data && 'isDeveloperMode' in data && data['isDeveloperMode'];

    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /**
   * Event handler that is invoked just before the screen is hidden.
   */
  onBeforeHide() {
    this.cleanupChromeVoxHint_();
  },

  /**
   * This is called when UI strings are changed.
   * Overridden from LoginScreenBehavior.
   */
  updateLocalizedContent() {
    this.languages = /** @type {!Array<OobeTypes.LanguageDsc>} */ (
        loadTimeData.getValue('languageList'));
    this.keyboards = /** @type {!Array<OobeTypes.IMEDsc>} */ (
        loadTimeData.getValue('inputMethodsList'));
    this.timezones = /** @type {!Array<OobeTypes.TimezoneDsc>} */ (
        loadTimeData.getValue('timezoneList'));
    this.highlightStrength =
        /** @type {string} */ (loadTimeData.getValue('highlightStrength'));

    this.$.welcomeScreen.i18nUpdateLocale();
    this.i18nUpdateLocale();

    var currentLanguage = loadTimeData.getString('language');

    // We might have changed language via configuration. In this case
    // we need to proceed with rest of configuration after language change
    // was fully resolved.
    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (configuration && configuration.language &&
        configuration.language == currentLanguage) {
      window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
    }
  },

  /**
   * Called when OOBE configuration is loaded.
   * Overridden from LoginScreenBehavior.
   * @param {!OobeTypes.OobeConfiguration} configuration
   */
  updateOobeConfiguration(configuration) {
    if (!this.configuration_applied_)
      window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /**
   * Called when dialog is shown for the first time.
   * @private
   */
  applyOobeConfiguration_() {
    if (this.configuration_applied_)
      return;
    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration)
      return;

    if (configuration.language) {
      var currentLanguage = loadTimeData.getString('language');
      if (currentLanguage != configuration.language) {
        this.applySelectedLanguage_(configuration.language);
        // Trigger language change without marking it as applied.
        // applyOobeConfiguration will be called again once language change
        // was applied.
        return;
      }
    }
    if (configuration.inputMethod)
      this.applySelectedLkeyboard_(configuration.inputMethod);

    if (configuration.welcomeNext)
      this.onWelcomeNextButtonClicked_();

    if (configuration.enableDemoMode) {
      this.userActed('setupDemoModeGesture');
    }

    this.configuration_applied_ = true;
  },

  /**
   * Updates "device in tablet mode" state when tablet mode is changed.
   * Overridden from LoginScreenBehavior.
   * @param {boolean} isInTabletMode True when in tablet mode.
   */
  setTabletModeState(isInTabletMode) {
    this.$.welcomeScreen.isInTabletMode = isInTabletMode;
  },

  /**
   * Window-resize event listener (delivered through the display_manager).
   */
  onWindowResize() {
    this.$.welcomeScreen.onWindowResize();
  },

  /**
   * Returns true if timezone button should be visible.
   * @private
   */
  isTimezoneButtonVisible_(highlightStrength) {
    return highlightStrength === 'strong';
  },

  /**
   * Handle "Next" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeNextButtonClicked_() {
    this.userActed('continue');
  },

  /**
   * Handles "enable-debugging" link for "Welcome" screen.
   *
   * @private
   */
  onEnableDebuggingClicked_() {
    this.userActed('enableDebugging');
  },

  /**
   * Handle "launch-advanced-options" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeLaunchAdvancedOptions_() {
    this.cancelChromeVoxHint_();
    this.setUIStep(UIState.ADVANCED_OPTIONS);
  },

  /**
   * Handle "Language" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeSelectLanguageButtonClicked_() {
    this.cancelChromeVoxHint_();
    this.setUIStep(UIState.LANGUAGE);
  },

  /**
   * Handle "Accessibility" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeAccessibilityButtonClicked_() {
    this.cancelChromeVoxHint_();
    this.setUIStep(UIState.ACCESSIBILITY);
  },

  /**
   * Handle "Timezone" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeTimezoneButtonClicked_() {
    this.cancelChromeVoxHint_();
    this.setUIStep(UIState.TIMEZONE);
  },

  /**
   * Handle language selection.
   *
   * @param {!CustomEvent<!OobeTypes.LanguageDsc>} event
   * @private
   */
  onLanguageSelected_(event) {
    var item = event.detail;
    var languageId = item.value;
    this.currentLanguage = item.title;
    this.applySelectedLanguage_(languageId);
  },

  /**
   * Switch UI language.
   *
   * @param {string} languageId
   * @private
   */
  applySelectedLanguage_(languageId) {
    chrome.send('WelcomeScreen.setLocaleId', [languageId]);
  },

  /**
   * Handle keyboard layout selection.
   *
   * @param {!CustomEvent<!OobeTypes.IMEDsc>} event
   * @private
   */
  onKeyboardSelected_(event) {
    var item = event.detail;
    var inputMethodId = item.value;
    this.currentKeyboard = item.title;
    this.applySelectedLkeyboard_(inputMethodId);
  },

  /**
   * Switch keyboard layout.
   *
   * @param {string} inputMethodId
   * @private
   */
  applySelectedLkeyboard_(inputMethodId) {
    chrome.send('WelcomeScreen.setInputMethodId', [inputMethodId]);
  },

  onLanguagesChanged_() {
    this.currentLanguage =
        getSelectedTitle(/** @type {!SelectListType} */ (this.languages));
  },

  onInputMethodIdSetFromBackend(keyboard_id) {
    var found = false;
    for (var i = 0; i < this.keyboards.length; ++i) {
      if (this.keyboards[i].value != keyboard_id) {
        this.keyboards[i].selected = false;
        continue;
      }
      this.keyboards[i].selected = true;
      found = true;
    }
    if (!found)
      return;

    // Force i18n-dropdown to refresh.
    this.keyboards = this.keyboards.slice();
    this.onKeyboardsChanged_();
  },

  /**
   * Refreshes a11y menu state.
   * @param {!OobeTypes.A11yStatuses} data New dictionary with a11y features
   *     state.
   */
  refreshA11yInfo(data) {
    this.a11yStatus = data;
    if (data.spokenFeedbackEnabled) {
      this.closeChromeVoxHint_();
    }
  },

  /**
   * On-tap event handler for demo mode confirmation dialog cancel button.
   * @private
   */
  onDemoModeDialogCancelTap_() {
    this.$.demoModeConfirmationDialog.hideDialog();
  },

  /**
   * On-tap event handler for demo mode confirmation dialog confirm button.
   * @private
   */
  onDemoModeDialogConfirmTap_() {
    this.userActed('setupDemoMode');
    this.$.demoModeConfirmationDialog.hideDialog();
  },

  /**
   * Shows confirmation dialog for starting Demo mode
   */
  showDemoModeConfirmationDialog() {
    // Ensure the ChromeVox hint dialog is closed.
    this.closeChromeVoxHint_();
    this.$.demoModeConfirmationDialog.showDialog();
  },

  onSetupDemoModeGesture() {
    this.userActed('setupDemoModeGesture');
  },

  /**
   * Shows the device requisition prompt.
   */
  showEditRequisitionDialog(requisition) {
    this.$.editRequisitionDialog.showDialog();
    this.$.editRequisitionInput.focus();
  },

  onEditRequisitionCancel_() {
    chrome.send('WelcomeScreen.setDeviceRequisition', ['none']);
    this.$.editRequisitionDialog.hideDialog();
  },

  onEditRequisitionConfirm_() {
    const requisition = this.$.editRequisitionInput.value;
    chrome.send('WelcomeScreen.setDeviceRequisition', [requisition]);
    this.$.editRequisitionDialog.hideDialog();
  },

  /**
   * Shows the special remora/shark device requisition prompt.
   */
  showRemoraRequisitionDialog() {
    this.$.remoraRequisitionDialog.showDialog();
  },

  onRemoraCancel_() {
    chrome.send('WelcomeScreen.setDeviceRequisition', ['none']);
    this.$.remoraRequisitionDialog.hideDialog();
  },

  onRemoraConfirm_() {
    chrome.send('WelcomeScreen.setDeviceRequisition', ['remora']);
    this.$.remoraRequisitionDialog.hideDialog();
  },

  onKeyboardsChanged_() {
    this.currentKeyboard = getSelectedTitle(this.keyboards);
  },

  /**
   * Handle "OK" button for "LanguageSelection" screen.
   *
   * @private
   */
  closeLanguageSection_() {
    this.setUIStep(UIState.GREETING);
  },

  /** ******************** Accessibility section ******************* */

  /**
   * Handle "OK" button for "Accessibility Options" screen.
   *
   * @private
   */
  closeAccessibilitySection_() {
    this.setUIStep(UIState.GREETING);
  },

  /**
   * Handle all accessibility buttons.
   * Note that each <oobe-a11y-option> has chromeMessage attribute
   * containing Chromium callback name.
   *
   * @private
   * @param {!Event} event
   */
  onA11yOptionChanged_(event) {
    var a11ytarget = /** @type {{chromeMessage: string, checked: boolean}} */ (
        event.currentTarget);
    if (a11ytarget.checked) {
      this.userActed(a11ytarget.id + '-enable');
    } else {
      this.userActed(a11ytarget.id + '-disable');
    }
  },

  /** ******************** Timezone section ******************* */

  /**
   * Handle "OK" button for "Timezone Selection" screen.
   *
   * @private
   */
  closeTimezoneSection_() {
    this.setUIStep(UIState.GREETING);
  },

  /**
   * Handle timezone selection.
   *
   * @param {!CustomEvent<!OobeTypes.Timezone>} event
   * @private
   */
  onTimezoneSelected_(event) {
    var item = event.detail;
    if (!item)
      return;

    chrome.send('WelcomeScreen.setTimezoneId', [item.value]);
  },

  /** ******************** AdvancedOptions section ******************* */

  /**
   * Handle "OK" button for "AdvancedOptions Selection" screen.
   *
   * @private
   */
  closeAdvancedOptionsSection_() {
    this.setUIStep(UIState.GREETING);
  },

  /**
   * Handle click on "Set up as CFM device" option.
   *
   * @private
   */
  onCFMBootstrappingClicked_() {
    this.userActed('activateRemoraRequisition');
  },

  /**
   * Handle click on "Device requisition" option.
   *
   * @private
   */
  onDeviceRequisitionClicked_() {
    this.userActed('editDeviceRequisition');
  },

  /** ******************** ChromeVox hint section ******************* */

  /** @private */
  onChromeVoxHintAccepted_() {
    this.userActed('activateChromeVoxFromHint');
  },

  /** @private */
  onChromeVoxHintDismissed_() {
    this.userActed('dismissChromeVoxHint');
  },

  /**
   * @suppress {missingProperties}
   * @private
   */
  showChromeVoxHint_() {
    this.$.welcomeScreen.showChromeVoxHint();
  },

  /**
   * @suppress {missingProperties}
   * @private
   */
  closeChromeVoxHint_() {
    this.$.welcomeScreen.closeChromeVoxHint();
  },

  /** @private */
  cancelChromeVoxHint_() {
    this.userActed('cancelChromeVoxHint');
    this.cleanupChromeVoxHint_();
  },

  /**
   * Initially called from WelcomeScreenHandler.
   * If we find a matching voice for the current locale, show the ChromeVox hint
   * dialog and give a spoken announcement with instructions for activating
   * ChromeVox. If we can't find a matching voice, call this function again
   * whenever a SpeechSynthesis voiceschanged event fires.
   */
  maybeGiveChromeVoxHint() {
    chrome.tts.getVoices((voices) => {
      const locale = loadTimeData.getString('language');
      const voiceName = this.findVoiceForLocale_(voices, locale);
      if (!voiceName) {
        this.onVoiceNotLoaded_();
        return;
      }

      const ttsOptions =
          /** @type {!chrome.tts.TtsOptions} */ ({lang: locale, voiceName});
      this.giveChromeVoxHint_(locale, ttsOptions, false);
    });
  },

  /**
   * Returns a voice name from |voices| that matches |locale|.
   * Returns undefined if no voice can be found.
   * Both |locale| and |voice.lang| will be in the form 'language-region'.
   * Examples include 'en', 'en-US', 'fr', and 'fr-CA'.
   * @param {Array<!chrome.tts.TtsVoice>} voices
   * @param {string} locale
   * @return {string|undefined}
   * @private
   */
  findVoiceForLocale_(voices, locale) {
    const language = locale.toLowerCase().split('-')[0];
    const voice = voices.find(voice => {
      return !!(
          voice.lang && voice.lang.toLowerCase().split('-')[0] === language);
    });
    return voice ? voice.voiceName : undefined;
  },

  /**
   * Called if we couldn't find a voice in which to announce the ChromeVox
   * hint.
   * Registers a voiceschanged listener that tries to give the hint when new
   * voices are loaded. Also sets a timeout that gives the hint in the default
   * locale as a last resort.
   * @private
   */
  onVoiceNotLoaded_() {
    if (this.voicesChangedListenerMaybeGiveChromeVoxHint_ === undefined) {
      // Add voiceschanged listener that tries to give the hint when new voices
      // are loaded.
      this.voicesChangedListenerMaybeGiveChromeVoxHint_ =
          this.maybeGiveChromeVoxHint.bind(this);
      window.speechSynthesis.addEventListener(
          'voiceschanged', this.voicesChangedListenerMaybeGiveChromeVoxHint_,
          false);
    }

    if (!this.defaultChromeVoxHintTimeoutId_) {
      // Set a timeout that gives the ChromeVox hint in the default locale.
      const ttsOptions = /** @type {!chrome.tts.TtsOptions} */ ({
        lang: DEFAULT_CHROMEVOX_HINT_LOCALE,
        extensionId: DEFAULT_CHROMEVOX_HINT_VOICE_EXTENSION_ID
      });
      this.defaultChromeVoxHintTimeoutId_ = window.setTimeout(
          this.giveChromeVoxHint_.bind(
              this, DEFAULT_CHROMEVOX_HINT_LOCALE, ttsOptions, true),
          this.DEFAULT_CHROMEVOX_HINT_TIMEOUT_MS_);
    }
  },

  /**
   * Shows the ChromeVox hint dialog and plays the spoken announcement. Gives
   * the spoken announcement with the provided options.
   * @param {string} locale
   * @param {!chrome.tts.TtsOptions} options
   * @param {boolean} isDefaultHint
   * @private
   */
  giveChromeVoxHint_(locale, options, isDefaultHint) {
    if (this.chromeVoxHintGiven_) {
      // Only give the hint once.
      // Due to event listeners/timeouts, there is the chance that this gets
      // called multiple times.
      return;
    }

    this.chromeVoxHintGiven_ = true;
    if (isDefaultHint) {
      console.warn(
          'No voice available for ' + loadTimeData.getString('language') +
          ', giving default hint in English.');
    }
    this.cleanupChromeVoxHint_();
    const msgId = this.$.welcomeScreen.isInTabletMode ?
        'chromeVoxHintAnnouncementTextTablet' :
        'chromeVoxHintAnnouncementTextLaptop';
    const message = this.i18n(msgId);
    chrome.tts.speak(message, options, () => {
      this.showChromeVoxHint_();
      chrome.send('WelcomeScreen.recordChromeVoxHintSpokenSuccess');
    });
  },

  /**
   * Clear timeout and remove voiceschanged listener.
   * @private
   */
  cleanupChromeVoxHint_() {
    if (this.defaultChromeVoxHintTimeoutId_) {
      window.clearTimeout(this.defaultChromeVoxHintTimeoutId_);
    }
    window.speechSynthesis.removeEventListener(
        'voiceschanged',
        /** @type {function(this:SpeechSynthesis, Event): *} */
        (this.voicesChangedListenerMaybeGiveChromeVoxHint_),
        /* useCapture */ false);
    this.voicesChangedListenerMaybeGiveChromeVoxHint_ = null;
  }
});
})();
