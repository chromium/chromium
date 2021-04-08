// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="utils.js">
// <include src="setting_zippy.js">
// <include src="voice_match_entry.js">
// <include src="browser_proxy.js">
// <include src="assistant_get_more.js">
// <include src="assistant_loading.js">
// <include src="assistant_related_info.js">
// <include src="assistant_third_party.js">
// <include src="assistant_value_prop.js">
// <include src="assistant_voice_match.js">

/**
 * @fileoverview Polymer element for displaying material design assistant
 * ready screen.
 *
 */

'use strict';

(function() {

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  LOADING: 'loading',
  VALUE_PROP: 'value-prop',
  RELATED_INFO: 'related-info',
  THIRD_PARTY: 'third-party',
  VOICE_MATCH: 'voice-match',
  GET_MORE: 'get-more',
};

Polymer({
  is: 'assistant-optin-flow-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, MultiStepBehavior],

  /** @private {?assistant.BrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = assistant.BrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    window.addEventListener(
        'orientationchange', this.onWindowResized_.bind(this));
    window.addEventListener('resize', this.onWindowResized_.bind(this));
  },

  /** @override */
  detached() {
    window.removeEventListener(
        'orientationchange', this.onWindowResized_.bind(this));
    window.removeEventListener('resize', this.onWindowResized_.bind(this));
  },

  /**
   * Indicates the type of the opt-in flow.
   */
  FlowType: {
    // The whole consent flow.
    CONSENT_FLOW: 0,
    // The voice match enrollment flow.
    SPEAKER_ID_ENROLLMENT: 1,
    // The voice match retrain flow.
    SPEAKER_ID_RETRAIN: 2,
  },

  defaultUIStep() {
    return UIState.LOADING;
  },

  UI_STEPS: UIState,

  /**
   * Signal from host to show the screen.
   * @param {?string} type The type of the flow.
   * @param {?string} captionBarHeight The height of the caption bar.
   */
  onShow(type, captionBarHeight) {
    captionBarHeight = captionBarHeight ? captionBarHeight + 'px' : '0px';
    this.style.setProperty('--caption-bar-height', captionBarHeight);
    this.onWindowResized_();

    type = type ? type : this.FlowType.CONSENT_FLOW.toString();
    var flowType = Number(type);
    switch (flowType) {
      case this.FlowType.CONSENT_FLOW:
      case this.FlowType.SPEAKER_ID_ENROLLMENT:
      case this.FlowType.SPEAKER_ID_RETRAIN:
        this.flowType = flowType;
        break;
      default:
        console.error('Invalid flow type, using default.');
        this.flowType = this.FlowType.CONSENT_FLOW;
        break;
    }

    this.boundShowLoadingScreen = this.showLoadingScreen.bind(this);
    this.boundOnScreenLoadingError = this.onScreenLoadingError.bind(this);
    this.boundOnScreenLoaded = this.onScreenLoaded.bind(this);

    this.$.loading.onBeforeShow();
    this.$.loading.addEventListener('reload', this.onReload.bind(this));

    switch (this.flowType) {
      case this.FlowType.SPEAKER_ID_ENROLLMENT:
      case this.FlowType.SPEAKER_ID_RETRAIN:
        this.$.voiceMatch.isFirstScreen = true;
        this.showStep(UIState.VOICE_MATCH);
        break;
      default:
        this.showStep(UIState.VALUE_PROP);
    }
    this.browserProxy_.initialized([this.flowType]);
  },

  /**
   * Reloads localized strings.
   * @param {!Object} data New dictionary with i18n values.
   */
  reloadContent(data) {
    this.voiceMatchEnforcedOff = data['voiceMatchEnforcedOff'];
    this.voiceMatchDisabled = loadTimeData.getBoolean('voiceMatchDisabled');
    this.betterAssistantEnabled =
        loadTimeData.getBoolean('betterAssistantEnabled');
    data['flowType'] = this.flowType;
    this.$.valueProp.reloadContent(data);
    this.$.relatedInfo.reloadContent(data);
    this.$.thirdParty.reloadContent(data);
    this.$.getMore.reloadContent(data);
  },

  /**
   * Add a setting zippy object in the corresponding screen.
   * @param {string} type type of the setting zippy.
   * @param {!Object} data String and url for the setting zippy.
   */
  addSettingZippy(type, data) {
    switch (type) {
      case 'settings':
        this.$.valueProp.addSettingZippy(data);
        break;
      case 'disclosure':
        this.$.thirdParty.addSettingZippy(data);
        break;
      case 'get-more':
        this.$.getMore.addSettingZippy(data);
        break;
      default:
        console.error('Undefined zippy data type: ' + type);
    }
  },

  /**
   * Show the next screen in the flow.
   */
  showNextScreen() {
    switch (this.currentStep) {
      case UIState.VALUE_PROP:
        if (this.betterAssistantEnabled) {
          this.showStep(UIState.RELATED_INFO);
        } else {
          this.showStep(UIState.THIRD_PARTY);
        }
        break;
      case UIState.RELATED_INFO:
        if (this.voiceMatchEnforcedOff || this.voiceMatchDisabled) {
          this.browserProxy_.flowFinished();
        } else {
          this.showStep(UIState.VOICE_MATCH);
        }
        break;
      case UIState.THIRD_PARTY:
        if (this.voiceMatchEnforcedOff || this.voiceMatchDisabled) {
          this.showStep(UIState.GET_MORE);
        } else {
          this.showStep(UIState.VOICE_MATCH);
        }
        break;
      case UIState.VOICE_MATCH:
        if (this.flowType == this.FlowType.SPEAKER_ID_ENROLLMENT ||
            this.flowType == this.FlowType.SPEAKER_ID_RETRAIN ||
            this.betterAssistantEnabled) {
          this.browserProxy_.flowFinished();
        } else {
          this.showStep(UIState.GET_MORE);
        }
        break;
      case UIState.GET_MORE:
        this.browserProxy_.flowFinished();
        break;
      default:
        console.error('Undefined');
        this.browserProxy_.dialogClose();
    }
  },

  /**
   * Called when the Voice match state is updated.
   * @param {string} state the voice match state.
   */
  onVoiceMatchUpdate(state) {
    if (this.currentStep !== UIState.VOICE_MATCH) {
      return;
    }
    switch (state) {
      case 'listen':
        this.$.voiceMatch.listenForHotword();
        break;
      case 'process':
        this.$.voiceMatch.processingHotword();
        break;
      case 'done':
        this.$.voiceMatch.voiceMatchDone();
        break;
      case 'failure':
        this.onScreenLoadingError();
        break;
      default:
        break;
    }
  },

  /**
   * Show the given step.
   *
   * @param {UIState} step The step to be shown.
   */
  showStep(step) {
    if (this.currentStep == step) {
      return;
    }
    if (this.currentStep) {
      this.applyToStepElements((screen) => {
        screen.removeEventListener('loading', this.boundShowLoadingScreen);
        screen.removeEventListener('error', this.boundOnScreenLoadingError);
        screen.removeEventListener('loaded', this.boundOnScreenLoaded);
      });
    }
    this.setUIStep(step);
    this.currentStep = step;
    this.applyToStepElements((screen) => {
      screen.addEventListener('loading', this.boundShowLoadingScreen);
      screen.addEventListener('error', this.boundOnScreenLoadingError);
      screen.addEventListener('loaded', this.boundOnScreenLoaded);
      screen.onShow();
    });
  },

  /**
   * Show the loading screen.
   */
  showLoadingScreen() {
    this.setUIStep(UIState.LOADING);
    this.$.loading.onShow();
  },

  /**
   * Called when the screen failed to load.
   */
  onScreenLoadingError() {
    this.setUIStep(UIState.LOADING);
    this.$.loading.onErrorOccurred();
  },

  /**
   * Called when all the content of current screen has been loaded.
   */
  onScreenLoaded() {
    this.setUIStep(this.currentStep);
    this.$.loading.onPageLoaded();
  },

  /**
   * Called when user request the screen to be reloaded.
   */
  onReload() {
    this.applyToStepElements((screen) => {
      screen.reloadPage();
    }, this.currentStep);
  },

  /**
   * Called during initialization, when the window is resized, or the window's
   * orientation is updated.
   */
  onWindowResized_() {
    // Dialog size and orientation value needs to be updated for in-session
    // assistant dialog.
    if (!document.documentElement.hasAttribute('screen')) {
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-height-base', window.innerHeight + 'px');
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-width-base', window.innerWidth + 'px');
      if (loadTimeData.valueExists('newLayoutEnabled') &&
          loadTimeData.getBoolean('newLayoutEnabled')) {
        if (window.innerWidth > window.innerHeight) {
          document.documentElement.setAttribute('orientation', 'horizontal');
        } else {
          document.documentElement.setAttribute('orientation', 'vertical');
        }
      }
    }
    // In landscape mode, animation element should reside in subtitle slot which
    // is shown at the bottom left of the screen. In portrait mode, animation
    // element should reside in content slot which allows scrolling with the
    // rest of the content.
    const slot = window.innerWidth > window.innerHeight ? 'subtitle' : 'content';
    this.$.valueProp.getAnimationContainer().slot = slot;
    this.$.relatedInfo.getAnimationContainer().slot = slot;
  },
});
})();
